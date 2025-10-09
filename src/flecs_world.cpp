
#include "flecs_world.h"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include "systems/game_of_life.h"
#include <godot_cpp/classes/project_settings.hpp>
#include <filesystem>

#include <algorithm>
#include <chrono>

using namespace godot;

void FlecsWorld::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_age"), &FlecsWorld::get_age);
    ClassDB::bind_method(D_METHOD("get_alive_map"), &FlecsWorld::get_alive_map);
    ClassDB::bind_method(D_METHOD("get_size_factor"), &FlecsWorld::get_size_factor);
    ClassDB::bind_method(D_METHOD("set_size_factor", "p_size_factor"), &FlecsWorld::set_size_factor);
    ClassDB::bind_method(D_METHOD("initialize_gol"), &FlecsWorld::initialize_game_of_life);
    ClassDB::bind_method(D_METHOD("get_gol_texture"), &FlecsWorld::get_gol_texture);
    // Make size_factor editable in the Editor with range 0.25 - 4.0
    ADD_PROPERTY(
        PropertyInfo(Variant::FLOAT, "size_factor", PROPERTY_HINT_RANGE, "0.2,4.0,0.2", PROPERTY_USAGE_DEFAULT),
        "set_size_factor",
        "get_size_factor");
}

FlecsWorld::FlecsWorld()
{
    age = 0.0;
    set_size_factor(size_factor);
}

double FlecsWorld::get_age() const
{
    return age;
}

double FlecsWorld::get_size_factor() const
{
    return size_factor;
}

void FlecsWorld::set_size_factor(double p_size_factor)
{
    size_factor = std::clamp(p_size_factor, 0.25, 4.0);
    const Vector2i project_viewport_size = Vector2i(
        godot::ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_width"),
        godot::ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_height"));
    size = Vector2i(
        static_cast<int>(project_viewport_size.x * size_factor),
        static_cast<int>(project_viewport_size.y * size_factor));
}

void FlecsWorld::_ready()
{
    // Print world size for verification
    godot::UtilityFunctions::print("FlecsWorld initialised. size=", size);
    // Ensure initialization happens once even if called from GDScript explicitly.
    initialize_game_of_life();
}

void FlecsWorld::_physics_process(double delta)
{
    age += delta;
    { // Time how long a single ECS progress step takes
        using clock = std::chrono::high_resolution_clock;
        auto t0 = clock::now();
        world.progress();
        auto t1 = clock::now();
        // Report as milliseconds (floating point) for easier reading
        auto dur = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
}

godot::Array FlecsWorld::get_alive_map()
{
    using namespace godot;
    Array rows;

    // Ensure size is valid
    if (size.x <= 0 || size.y <= 0)
    {
        return rows; // empty
    }

    // Collect alive cell positions from flecs world
    std::vector<CellPos> alive_positions;
    collect_alive_cells(world, alive_positions);

    // Create a 2D buffer of bytes (row-major, y then x)
    // We'll create one PackedByteArray per row for easy access in GDScript
    for (int y = 0; y < size.y; ++y)
    {
        PackedByteArray row;
        row.resize(size.x);
        // initialize to 0
        for (int x = 0; x < size.x; ++x)
            row.set(x, uint8_t(0));
        rows.append(row);
    }

    // Mark alive cells (clamp positions within bounds). We copy-modify-set each
    // row to avoid unsafe reference semantics when accessing Array elements.
    for (const auto &p : alive_positions)
    {
        if (p.x >= 0 && p.x < size.x && p.y >= 0 && p.y < size.y)
        {
            PackedByteArray row = rows[p.y];
            row.set(p.x, uint8_t(1));
            rows.set(p.y, row);
        }
    }

    return rows;
}

godot::Ref<godot::ImageTexture> FlecsWorld::get_gol_texture()
{
    using namespace godot;

    if (size.x <= 0 || size.y <= 0)
    {
        return Ref<ImageTexture>();
    }

    // Collect alive cell positions
    std::vector<CellPos> alive_positions;
    collect_alive_cells(world, alive_positions);

    const int w = size.x;
    const int h = size.y;
    const int total = w * h;

    // Build flat L8 buffer (one byte per pixel)
    PackedByteArray data;
    data.resize(total);
    // initialize to 0
    for (int i = 0; i < total; ++i)
        data.set(i, uint8_t(0));

    for (const auto &p : alive_positions)
    {
        if (p.x >= 0 && p.x < w && p.y >= 0 && p.y < h)
        {
            int idx = p.y * w + p.x;
            data.set(idx, uint8_t(255));
        }
    }

    // Create or update the Image (L8)
    if (gol_image.is_null())
    {
        gol_image = Image::create_from_data(w, h, false, Image::FORMAT_L8, data);
    }
    else
    {
        gol_image->set_data(w, h, false, Image::FORMAT_L8, data);
    }

    // Create or update the ImageTexture
    if (gol_texture.is_null())
    {
        gol_texture = ImageTexture::create_from_image(gol_image);
    }
    else
    {
        gol_texture->set_image(gol_image);
    }

    return gol_texture;
}

FlecsWorld::~FlecsWorld()
{
    // Add your cleanup here.
}

void FlecsWorld::initialize_game_of_life()
{
    if (initialized)
        return;

    initialized = true;

    // Register Game of Life components/systems and initialize the grid so
    // that alive cells exist and can be returned by get_alive_map().
    register_gol_systems(world);
    // Keep ECS single-threaded while we mutate neighbor counts from systems
    // to avoid data races. We can revisit multi-threading later.
    world.set_threads(1);

    // Enable the Flecs Explorer
    world.set<flecs::Rest>({});
    world.import <flecs::stats>();

    // Initialize grid with a modest alive probability. Use a fixed seed for
    // reproducibility; callers can re-seed or re-init via exposed methods.
    int seed = 42;
    float alive_probability = 0.1f;
    init_gol_grid(world, size.x, size.y, seed, alive_probability);
    godot::UtilityFunctions::print("FlecsWorld initialized grid with seed=", seed, " prob=", alive_probability);

    // Diagnostic: report initial number of alive cells created by init_gol_grid
    std::vector<CellPos> alive_positions;
    collect_alive_cells(world, alive_positions);
}
