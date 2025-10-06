// Helper to get viewport size from ProjectSettings
#ifndef FLECS_WORLD_H
#define FLECS_WORLD_H

#include <godot_cpp/classes/node.hpp>
#include <flecs/distr/flecs.h>

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

        void _ready() override;
        void _physics_process(double delta) override;

    protected:
        static void _bind_methods();

    private:
        flecs::world world;
        double age;
        Vector2i size;
        double size_factor = 1.0;
    };
}

#endif
