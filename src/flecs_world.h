#ifndef FLECS_WORLD_H
#define FLECS_WORLD_H

#include <godot_cpp/classes/sprite2d.hpp>

namespace godot
{

    class FlecsWorld : public Sprite2D
    {
        GDCLASS(FlecsWorld, Sprite2D)

    private:
        double time_passed;
        double time_emit;
        double amplitude;
        double speed;

    protected:
        static void _bind_methods();

    public:
        FlecsWorld();
        ~FlecsWorld();

        void _process(double delta) override;

        // Node properties
        double get_amplitude() const;
        void set_amplitude(const double p_amplitude);

        double get_speed() const;
        void set_speed(const double p_speed);
    };
}

#endif
