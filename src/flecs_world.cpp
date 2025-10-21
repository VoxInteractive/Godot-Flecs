#include <string>
#include <chrono>
#include <algorithm>

#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/os.hpp>

#include "flecs_world.h"
#include "ecs/game_of_life.h"

using namespace godot;

void FlecsWorld::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("get_alive_map"), &FlecsWorld::get_alive_map);
    ClassDB::bind_method(D_METHOD("get_resolution_factor"), &FlecsWorld::get_resolution_factor);
    ClassDB::bind_method(D_METHOD("set_resolution_factor", "p_resolution_factor"), &FlecsWorld::set_resolution_factor);
    ClassDB::bind_method(D_METHOD("get_seed"), &FlecsWorld::get_seed);
    ClassDB::bind_method(D_METHOD("set_seed", "p_seed"), &FlecsWorld::set_seed);
    ClassDB::bind_method(D_METHOD("initialize_game_of_life"), &FlecsWorld::initialize_game_of_life);
    ClassDB::bind_method(D_METHOD("get_game_of_life_texture"), &FlecsWorld::get_game_of_life_texture);

    ADD_PROPERTY(
        PropertyInfo(Variant::FLOAT, "resolution_factor", PROPERTY_HINT_RANGE, "0.1,1.0,0.1", PROPERTY_USAGE_DEFAULT),
        "set_resolution_factor",
        "get_resolution_factor");

    ADD_PROPERTY(
        PropertyInfo(Variant::INT, "seed", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT),
        "set_seed",
        "get_seed");
}

FlecsWorld::FlecsWorld()
{
    set_resolution_factor(resolution_factor);
}

double FlecsWorld::get_resolution_factor() const
{
    return resolution_factor;
}

int FlecsWorld::get_seed() const
{
    return seed;
}

void FlecsWorld::set_seed(int p_seed)
{
    seed = p_seed;
    if (initialised)
    {
        recreate_world_and_grid();
    }
}

void FlecsWorld::set_resolution_factor(double p_resolution_factor)
{
    // Clamp to [0.1, 1.0]
    // 1.0 => one texture pixel per screen pixel (full-resolution grid)
    // 0.5 => one texture pixel spans 2x2 screen pixels (half-resolution grid)
    resolution_factor = std::clamp(p_resolution_factor, 0.1, 1.0);
    const Vector2i project_viewport_size = Vector2i(
        godot::ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_width"),
        godot::ProjectSettings::get_singleton()->get_setting("display/window/size/viewport_height"));
    size = Vector2i(
        static_cast<int>(project_viewport_size.x * resolution_factor),
        static_cast<int>(project_viewport_size.y * resolution_factor));

    // If already initialised, recreate the ECS world and grid to match the new size
    if (initialised)
    {
        recreate_world_and_grid();
    }
}

void FlecsWorld::_ready()
{
    godot::UtilityFunctions::print("FlecsWorld initialised. size=", size);
    // Ensure initialization happens once even if called from GDScript explicitly
    initialize_game_of_life();
}

void FlecsWorld::_physics_process(double delta)
{
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
        rows.append(PackedByteArray());
        rows.get(y).operator godot::PackedByteArray().resize(size.x);
        // resize() default-initializes to zero, so no need to clear manually.
    }

    // Mark alive cells. Get a write pointer to each row's PackedByteArray
    // to modify it in-place, which is much more efficient than copy-modify-set.
    for (const auto &p : alive_positions)
    {
        // Bounds check
        if ((unsigned)p.y < (unsigned)size.y && (unsigned)p.x < (unsigned)size.x)
        {
            PackedByteArray row = rows[p.y]; // This is a variant conversion
            uint8_t *row_ptr = row.ptrw();
            row_ptr[p.x] = 1;
        }
    }

    return rows;
}

godot::Ref<godot::ImageTexture> FlecsWorld::get_game_of_life_texture()
{
    using namespace godot;

    if (size.x <= 0 || size.y <= 0)
    {
        return Ref<ImageTexture>();
    }

    const int w = size.x;
    const int h = size.y;
    const int total = w * h;

    // Build a flat L8 buffer (one byte per pixel)
    PackedByteArray data;
    data.resize(total);
    uint8_t *dst = data.ptrw();

    // Collect alive positions via ECS query and mark pixels
    memset(dst, 0, (size_t)total);
    std::vector<CellPos> alive_positions;
    collect_alive_cells(world, alive_positions);
    for (const auto &p : alive_positions)
    {
        if ((unsigned)p.x < (unsigned)w && (unsigned)p.y < (unsigned)h)
        {
            dst[p.y * w + p.x] = 255;
        }
    }

    // Create or update the Image (L8)
    if (game_of_life_image.is_null())
    {
        game_of_life_image = Image::create_from_data(w, h, false, Image::FORMAT_L8, data);
    }
    else
    {
        game_of_life_image->set_data(w, h, false, Image::FORMAT_L8, data);
    }

    // Create or update the ImageTexture
    if (game_of_life_texture.is_null())
    {
        game_of_life_texture = ImageTexture::create_from_image(game_of_life_image);
    }
    else
    {
        game_of_life_texture->set_image(game_of_life_image);
    }

    return game_of_life_texture;
}

FlecsWorld::~FlecsWorld()
{
    // Add your cleanup here.
}

void FlecsWorld::initialize_game_of_life()
{
    if (initialised)
        return;

    initialised = true;
    recreate_world_and_grid();
}

void FlecsWorld::recreate_world_and_grid()
{
    // Reset cached textures first before they are recreated with the new size
    if (game_of_life_image.is_valid())
        game_of_life_image.unref();
    if (game_of_life_texture.is_valid())
        game_of_life_texture.unref();

    // Recreate ECS world by assigning a fresh instance.
    world = flecs::world{};

    // Register the components/systems and initialise the grid so
    // that alive cells exist and can be returned by get_alive_map()
    register_game_of_life_systems(world);

    // Use (available logical processors) - 1 for throughput
    int cpu_count = OS::get_singleton()->get_processor_count();
    world.set_threads(cpu_count - 1);

    // Enable the Flecs Explorer and stats available at
    // https://www.flecs.dev/explorer/?page=stats&host=localhost
    world.set<flecs::Rest>({});
    world.import <flecs::stats>();

    // Initialise with the configured seed and a modest alive probability
    float alive_probability = 0.1f;
    init_game_of_life_grid(world, size.x, size.y, seed, alive_probability);

    long long total_cells = static_cast<long long>(size.x) * static_cast<long long>(size.y);

    godot::UtilityFunctions::print(
        "FlecsWorld initialised grid with seed=", seed,
        " alive_probability=", alive_probability,
        " (", size.x, "x", size.y, ")",
        " total_cells=", godot::String(std::to_string(total_cells).c_str()));
}
