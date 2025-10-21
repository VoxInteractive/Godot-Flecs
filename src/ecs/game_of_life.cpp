#include <unordered_map>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/time.hpp>
#include "game_of_life.h"

using namespace godot;

static inline int neighbour_offsets[8][2] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

// Simple global index from (x,y) -> entity for fast neighbour lookup.
static std::unordered_map<long long, flecs::entity> g_cell_index;
static int g_grid_w = 0, g_grid_h = 0;
static std::vector<uint8_t> g_alive_cur;  // row-major, 0/1. Current state.
static std::vector<uint8_t> g_alive_next; // row-major, 0/1. Next state buffer.

static inline long long key_xy(int x, int y)
{
    return (static_cast<long long>(y) << 32) | (static_cast<unsigned int>(x));
}

void register_game_of_life_systems(flecs::world &world)
{
    // Register the components
    world.component<Cell>()
        .member<int>("x")
        .member<int>("y");
    world.component<Alive>();

    // Compute next state into a flat buffer, letting Flecs handle threading
    world.system<const Cell>("SimulateBuffers")
        .kind(flecs::OnUpdate)
        .multi_threaded()
        .each([](flecs::entity /*e*/, const Cell &c)
              {
            const int W = g_grid_w;
            const int H = g_grid_h;
            if (W == 0 || H == 0 || g_alive_cur.size() != (size_t)W * (size_t)H) return;
            const int x = c.x;
            const int y = c.y;
            int count = 0;
            // Count neighbours with bounds checks
            for (int i = 0; i < 8; ++i) {
                const int nx = x + neighbour_offsets[i][0];
                const int ny = y + neighbour_offsets[i][1];
                if ((unsigned)nx < (unsigned)W && (unsigned)ny < (unsigned)H) {
                    count += g_alive_cur[(size_t)ny * (size_t)W + (size_t)nx];
                }
            }
            const size_t idx = (size_t)y * (size_t)W + (size_t)x;
            const uint8_t alive = g_alive_cur[idx];
            g_alive_next[idx] = alive ? (count == 2 || count == 3) : (count == 3); });

    // Swap buffers once (single-threaded system)
    // This runs single-threaded after the simulation to ensure no race conditions
    world.system<>("SwapBuffers")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter &it)
             { std::swap(g_alive_cur, g_alive_next); });

    // Synchronize Alive tag from buffer in parallel
    world.system<Cell>("SyncAliveTags")
        .kind(flecs::PostUpdate)
        .multi_threaded()
        .each([](flecs::entity e, Cell &c)
              {
            const int W = g_grid_w;
            const int H = g_grid_h;
            if (W == 0 || H == 0 || g_alive_cur.size() != (size_t)W * (size_t)H) return;
            const bool alive = g_alive_cur[(size_t)c.y * (size_t)W + (size_t)c.x] != 0;
            const bool has_alive = e.has<Alive>();
            if (alive && !has_alive) e.add<Alive>();
            else if (!alive && has_alive) e.remove<Alive>(); });
}

void init_game_of_life_grid(flecs::world &world, int width, int height, int seed, float alive_probability)
{
    g_cell_index.clear();
    g_grid_w = width;
    g_grid_h = height;
    g_alive_cur.assign((size_t)width * (size_t)height, 0);
    g_alive_next.assign((size_t)width * (size_t)height, 0);

    // Use Godot's random number generator for better seeding.
    auto rng = memnew(RandomNumberGenerator);
    if (seed == 0)
    {
        rng->randomize(); // Use a time-based seed
    }
    rng->set_seed(seed);

    // Rebuild the grid: defer and first remove any existing Cell entities,
    // then create the new set for the requested width/height.
    world.defer_begin();
    world.each<Cell>([](flecs::entity e, Cell &)
                     { e.destruct(); });

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            auto e = world.entity().set<Cell>({x, y});
            const bool alive = (rng->randf() < alive_probability);
            if (alive)
            {
                e.add<Alive>();
            }
            g_alive_cur[(size_t)y * (size_t)width + (size_t)x] = alive ? 1 : 0;
            g_cell_index.emplace(key_xy(x, y), e);
        }
    }
    world.defer_end();

    memdelete(rng);
}

void collect_alive_cells(flecs::world &world, std::vector<CellPos> &out, int *out_max_x, int *out_max_y)
{
    out.clear();
    int max_x = 0, max_y = 0;

    world.each([&](const Cell &c, const Alive &)
               {
        out.push_back({c.x, c.y});
        if (c.x > max_x)
            max_x = c.x;
        if (c.y > max_y)
            max_y = c.y; });

    if (out_max_x)
        *out_max_x = max_x;
    if (out_max_y)
        *out_max_y = max_y;
}

// Accessors for the optimised buffer
const uint8_t *game_of_life_alive_data() { return g_alive_cur.empty() ? nullptr : g_alive_cur.data(); }
int game_of_life_width() { return g_grid_w; }
int game_of_life_height() { return g_grid_h; }
