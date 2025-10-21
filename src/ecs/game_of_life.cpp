#include <unordered_map>
#include <godot_cpp/variant/utility_functions.hpp>
#include "game_of_life.h"

using namespace godot;

static inline int neighbor_offsets[8][2] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

// Simple global index from (x,y) -> entity for fast neighbor lookup
static Grid g_grid;
static std::vector<uint8_t> g_alive_cur;  // row-major, 0/1. Current state.
static std::vector<uint8_t> g_alive_next; // row-major, 0/1. Next state buffer.

static inline long long key_xy(int x, int y)
{
    return (static_cast<long long>(y) << 32) | (static_cast<unsigned int>(x));
}

void register_game_of_life_systems(flecs::world &world)
{
    // Register components
    world.component<Cell>();
    world.component<Grid>();

    // System to calculate the next state of the grid.
    // Run single-threaded currently to keep the implementation simple
    // and avoid depending on flecs worker-iter internals.
    world.system<>("SimulateNextState")
        .kind(flecs::OnUpdate)
        .run([](flecs::iter &it)
             {
            const int W = g_grid.width;
            const int H = g_grid.height;

            for (int y = 0; y < H; ++y)
            {
                for (int x = 0; x < W; ++x)
                {
                    int count = 0;
                    // Count live neighbors
                    for (int i = 0; i < 8; ++i)
                    {
                        const int nx = x + neighbor_offsets[i][0];
                        const int ny = y + neighbor_offsets[i][1];
                        if ((unsigned)nx < (unsigned)W && (unsigned)ny < (unsigned)H)
                        {
                            count += g_alive_cur[(size_t)ny * (size_t)W + (size_t)nx];
                        }
                    }
                    const size_t idx = (size_t)y * (size_t)W + (size_t)x;
                    const uint8_t alive = g_alive_cur[idx];
                    g_alive_next[idx] = alive ? (count == 2 || count == 3) : (count == 3);
                }
            } });

    // System to swap the current and next state buffers.
    // This runs single-threaded after the simulation to ensure no race conditions.
    world.system<>("SwapBuffers")
        .kind(flecs::PostUpdate)
        .run([](flecs::iter &it)
             { std::swap(g_alive_cur, g_alive_next); });
}

void init_game_of_life_grid(flecs::world &world, int width, int height, int seed, float alive_probability)
{
    g_grid = {width, height};
    g_alive_cur.assign((size_t)width * (size_t)height, 0);
    g_alive_next.assign((size_t)width * (size_t)height, 0);

    // Use Godot's random number generator for better seeding.
    auto rng = memnew(RandomNumberGenerator);
    if (seed == 0)
    {
        rng->randomize();
        seed = rng->get_seed();
    }
    rng->set_seed(seed);

    for (size_t i = 0; i < g_alive_cur.size(); ++i)
    {
        g_alive_cur[i] = (rng->randf() < alive_probability) ? 1 : 0;
    }

    memdelete(rng);
}

void collect_alive_cells(flecs::world &world, std::vector<CellPos> &out)
{
    out.clear();
    const int W = g_grid.width;
    const int H = g_grid.height;
    if (W <= 0 || H <= 0 || g_alive_cur.size() != (size_t)W * (size_t)H)
        return;

    for (int y = 0; y < H; ++y)
    {
        const size_t row_off = (size_t)y * (size_t)W;
        for (int x = 0; x < W; ++x)
        {
            if (g_alive_cur[row_off + (size_t)x])
            {
                out.push_back({x, y});
            }
        }
    }
}

// Accessors for the optimised buffer
const uint8_t *game_of_life_alive_data() { return g_alive_cur.empty() ? nullptr : g_alive_cur.data(); }
int game_of_life_width() { return g_grid.width; }
int game_of_life_height() { return g_grid.height; }
