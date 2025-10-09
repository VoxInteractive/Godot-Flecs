// Helper to get viewport size from ProjectSettings
#ifndef FLECS_WORLD_H
#define FLECS_WORLD_H

#include <godot_cpp/classes/node.hpp>
#include <flecs/distr/flecs.h>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>

namespace godot
{

    class FlecsWorld : public Node
    {
        GDCLASS(FlecsWorld, Node)

    public:
        FlecsWorld();
        ~FlecsWorld();

        double get_age() const;
        double get_size_factor() const;
        void set_size_factor(double p_size_factor);
        // Initialize/register Game of Life systems and seed the grid if not already initialized
        void initialize_game_of_life();
        // Return a 2D (width x height) boolean map of alive/dead cells as
        // an Array of PackedByteArray rows (each byte is 0 or 1). Callable
        // from GDScript as `get_alive_map()`.
        godot::Array get_alive_map();

        // Build a Godot Image from the ECS alive/dead map and return an
        // ImageTexture that is created once and updated in-place on
        // subsequent calls. The Image uses Image::FORMAT_L8 (one byte per
        // pixel). Returns an invalid Ref<ImageTexture> if size is zero.
        godot::Ref<godot::ImageTexture> get_gol_texture();

        void _ready() override;
        void _physics_process(double delta) override;

    protected:
        static void _bind_methods();

    private:
        flecs::world world;
        double age;
        Vector2i size;
        double size_factor = 1.0;
        bool initialized = false;
        // Cached image/texture reused across frames to avoid repeated
        // allocations and to enable efficient texture updates.
        godot::Ref<godot::Image> gol_image;
        godot::Ref<godot::ImageTexture> gol_texture;
    };
}

#endif
