#ifndef SELECTION_PARTICLES_H
#define SELECTION_PARTICLES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct SelectionParticle
    {
        float position[3];
        float velocity[3];
        float age;
        float lifetime;
        unsigned char active;
    } SelectionParticle;

    typedef struct SelectionParticleSystem
    {
        SelectionParticle *particles;
        size_t capacity;
        float default_lifetime;
    } SelectionParticleSystem;

    bool selection_particles_setup(SelectionParticleSystem *system, size_t capacity,
                                   float lifetime);
    void selection_particles_shutdown(SelectionParticleSystem *system);
    void selection_particles_reset(SelectionParticleSystem *system);
    void selection_particles_trigger_burst(SelectionParticleSystem *system, const float origin[3],
                                           float speed_min, float speed_max);
    void selection_particles_update(SelectionParticleSystem *system, float delta_time);
    size_t selection_particles_active_count(const SelectionParticleSystem *system);

#ifdef __cplusplus
}
#endif

#endif /* SELECTION_PARTICLES_H */
