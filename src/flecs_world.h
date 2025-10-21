// Helper to get viewport size from ProjectSettings
#ifndef FLECS_WORLD_H
#define FLECS_WORLD_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>

#include <flecs/distr/flecs.h>

namespace godot
{

    class FlecsWorld : public Node
    {
        GDCLASS(FlecsWorld, Node)

    public:
        FlecsWorld();
        ~FlecsWorld();

        double get_resolution_factor() const;
        void set_resolution_factor(double p_resolution_factor);

        int get_seed() const;
        void set_seed(int p_seed);

        // Initialize/register Game of Life systems and seed the grid if not already initialised
        void initialize_game_of_life();

        // Return a 2D (width x height) boolean map of alive/dead cells as
        // an Array of PackedByteArray rows (each byte is 0 or 1). Callable
        // from GDScript as `get_alive_map()`
        godot::Array get_alive_map();

        // Build a Godot Image from the ECS alive/dead map and return an
        // ImageTexture that is created once and updated in-place on
        // subsequent calls. The Image uses Image::FORMAT_L8 (one byte per
        // pixel). Returns an invalid Ref<ImageTexture> if size is zero
        godot::Ref<godot::ImageTexture> get_game_of_life_texture();

        void _ready() override;
        void _physics_process(double delta) override;

    protected:
        static void _bind_methods();

    private:
        flecs::world world;
        Vector2i size;
        double resolution_factor = 1.0;

        // Seed for random initialization of the game-of-life grid. Exposed to
        // the editor so users can change seed at runtime and recreate the grid
        int seed = 42;
        bool initialised = false;

        // Recreate ECS world, (re)register systems and initialize the grid
        // with current size and default seed/probability. Also resets cached
        // textures so they are recreated at the new resolution
        void recreate_world_and_grid();

        // Cached image/texture reused across frames to avoid repeated
        // allocations and to enable efficient texture updates
        godot::Ref<godot::Image> game_of_life_image;
        godot::Ref<godot::ImageTexture> game_of_life_texture;
    };
}

#endif
