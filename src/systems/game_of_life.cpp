#include "game_of_life.h"
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <thread>
// Godot utility functions for debug prints
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

// Temporary per-cell component to store neighbor counts each tick.
// Only written for the entity being iterated to enable parallelization.
struct NeighborCount
{
    int n;
};

// Temporary per-cell component to store next-state decision.
struct NextAlive
{
    bool v;
};

static inline int neighbor_offsets[8][2] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

// Simple global index from (x,y) -> entity for fast neighbor lookup
static std::unordered_map<long long, flecs::entity> g_cell_index;
static int g_grid_w = 0, g_grid_h = 0;
static std::vector<uint8_t> g_alive_cur;  // row-major, 0/1
static std::vector<uint8_t> g_alive_next; // row-major, 0/1
static inline long long key_xy(int x, int y)
{
    return (static_cast<long long>(y) << 32) | (static_cast<unsigned int>(x));
}

void register_gol_systems(flecs::world &world)
{
    // Ensure components are registered (names align with FlecsScript)
    world.component<Cell>()
        .member<int>("x")
        .member<int>("y");
    world.component<Alive>();
    world.component<NeighborCount>().member<int>("n");
    world.component<NextAlive>().member<bool>("v");

    // Phase A: compute next state into flat buffer letting Flecs handle threading.
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
            // Count neighbors with bounds checks (can be optimized with padding later)
            for (int i = 0; i < 8; ++i) {
                const int nx = x + neighbor_offsets[i][0];
                const int ny = y + neighbor_offsets[i][1];
                if ((unsigned)nx < (unsigned)W && (unsigned)ny < (unsigned)H) {
                    count += g_alive_cur[(size_t)ny * (size_t)W + (size_t)nx];
                }
            }
            const size_t idx = (size_t)y * (size_t)W + (size_t)x;
            const uint8_t alive = g_alive_cur[idx];
            g_alive_next[idx] = alive ? (count == 2 || count == 3) : (count == 3); });
    // Phase B: swap buffers once (single-threaded system).
    world.system<>("SwapBuffers")
        .kind(flecs::PostUpdate)
        .each([]()
              { std::swap(g_alive_cur, g_alive_next); });

    // Phase C: synchronize Alive tag from buffer (parallel over entities).
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

void init_gol_grid(flecs::world &world, int width, int height, int seed, float alive_probability)
{
    g_cell_index.clear();
    g_grid_w = width;
    g_grid_h = height;
    g_alive_cur.assign((size_t)width * (size_t)height, 0);
    g_alive_next.assign((size_t)width * (size_t)height, 0);

    world.defer_begin();
    std::srand(seed);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            auto e = world.entity()
                         .set<Cell>({x, y})
                         .set<NeighborCount>({0})
                         .set<NextAlive>({false});
            float r = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            const bool alive = (r < alive_probability);
            if (alive)
            {
                e.add<Alive>();
            }
            g_alive_cur[(size_t)y * (size_t)width + (size_t)x] = alive ? 1 : 0;
            g_cell_index.emplace(key_xy(x, y), e);
        }
    }
    world.defer_end();

    // Ensure at least a small visible pattern exists for visual verification
    auto force_alive = [&](int fx, int fy)
    {
        auto it = g_cell_index.find(key_xy(fx, fy));
        if (it != g_cell_index.end())
        {
            it->second.add<Alive>();
        }
    };
    force_alive(1, 1);
    force_alive(2, 1);
    force_alive(3, 1);
    if (width > 3 && height > 2)
    {
        g_alive_cur[1 * width + 1] = 1;
        g_alive_cur[1 * width + 2] = 1;
        g_alive_cur[1 * width + 3] = 1;
    }
}

void collect_alive_cells(flecs::world &world, std::vector<CellPos> &out, int *out_max_x, int *out_max_y)
{
    out.clear();
    int max_x = 0, max_y = 0;
    const int W = g_grid_w;
    const int H = g_grid_h;
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
                if (x > max_x)
                    max_x = x;
                if (y > max_y)
                    max_y = y;
            }
        }
    }
    if (out_max_x)
        *out_max_x = max_x;
    if (out_max_y)
        *out_max_y = max_y;
}

// Accessors for optimized buffer
const uint8_t *gol_alive_data() { return g_alive_cur.empty() ? nullptr : g_alive_cur.data(); }
int gol_width() { return g_grid_w; }
int gol_height() { return g_grid_h; }
