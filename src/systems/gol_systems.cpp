#include "gol_systems.h"
// Keep Flecs-based GoL systems aligned with the vendored Flecs version
#include <cstdlib>

// Temporary component to accumulate neighbor counts each tick
struct NeighborCount
{
    int n;
};

static inline int neighbor_offsets[8][2] = {
    {-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

void register_gol_systems(flecs::world &world)
{
    // Ensure components are registered
    world.component<Cell>()
        .member<int>("x")
        .member<int>("y");
    world.component<Alive>();
    world.component<NeighborCount>().member<int>("n");

    // System: reset neighbor counts to 0
    world.system<NeighborCount>("ResetNeighborCounts")
        .kind(flecs::OnUpdate)
        .each([](NeighborCount &nc)
              { nc.n = 0; });

    // Query for all cells to assist neighbor accumulation
    auto all_cells = world.query_builder<Cell>().cached().build();

    // For all alive cells, increment neighbor count of neighbors (brute-force)
    world.system<const Cell>("AccumulateNeighborsBrute")
        .kind(flecs::OnUpdate)
        .with<Alive>()
        .each([&](flecs::entity /*alive_e*/, const Cell &alive_cell)
              {
            int cx = alive_cell.x;
            int cy = alive_cell.y;
            all_cells.each([&](flecs::entity neigh, Cell &cn) {
                int dx = cn.x - cx; int dy = cn.y - cy;
                if ((dx || dy) && dx >= -1 && dx <= 1 && dy >= -1 && dy <= 1) {
                    auto &nc = neigh.get_mut<NeighborCount>();
                    nc.n += 1;
                    neigh.modified<NeighborCount>();
                }
            }); });

    // Apply Life rules
    world.system<Cell, NeighborCount>("ApplyLifeRules")
        .kind(flecs::PostUpdate)
        .each([](flecs::entity e, Cell &, NeighborCount &nc)
              {
            bool is_alive = e.has<Alive>();
            int n = nc.n;
            if (is_alive) {
                if (n < 2 || n > 3) {
                    e.remove<Alive>();
                }
            } else {
                if (n == 3) {
                    e.add<Alive>();
                }
            } });
}

void init_gol_grid(flecs::world &world, int width, int height, int seed, float alive_probability)
{
    world.defer_begin();
    std::srand(seed);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            auto e = world.entity().set<Cell>({x, y});
            float r = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
            if (r < alive_probability)
            {
                e.add<Alive>();
            }
        }
    }
    world.defer_end();
}

void collect_alive_cells(flecs::world &world, std::vector<CellPos> &out, int *out_max_x, int *out_max_y)
{
    out.clear();
    int max_x = 0, max_y = 0;
    auto q = world.query_builder<const Cell>().with<Alive>().cached().build();
    q.each([&](flecs::entity, const Cell &c)
           {
        out.push_back({c.x, c.y});
        if (c.x > max_x) max_x = c.x;
        if (c.y > max_y) max_y = c.y; });
    if (out_max_x)
        *out_max_x = max_x;
    if (out_max_y)
        *out_max_y = max_y;
}
