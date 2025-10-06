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
        FlecsWorld(const Vector2i &p_size);
        ~FlecsWorld();

        void _ready() override;
        void _physics_process(double delta) override;
        double get_age() const;
        void set_size(const Vector2i &p_size);
        Vector2i get_size() const;

    public:
        static Vector2i get_project_viewport_size();

    protected:
        static void _bind_methods();

    private:
        flecs::world world;
        double age;
        Vector2i size;
    };
}

#endif
