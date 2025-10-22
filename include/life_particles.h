#ifndef LIFE_PARTICLES_H
#define LIFE_PARTICLES_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum LifeParticleKind
    {
        LIFE_PARTICLE_KIND_BIRTH = 0,
        LIFE_PARTICLE_KIND_DEATH = 1
    } LifeParticleKind;

    typedef struct LifeParticle
    {
        float position[3];
        float velocity[3];
        float age;
        float lifetime;
        unsigned char active;
    } LifeParticle;

    typedef struct LifeParticleChannel
    {
        LifeParticle *particles;
        size_t capacity;
        float default_lifetime;
        unsigned int rng_state;
        size_t next_index;
    } LifeParticleChannel;

    typedef struct LifeParticleSystem
    {
        LifeParticleChannel birth;
        LifeParticleChannel death;
    } LifeParticleSystem;

    bool life_particles_setup(LifeParticleSystem *system, size_t birth_capacity,
                              size_t death_capacity, float birth_lifetime, float death_lifetime);
    void life_particles_shutdown(LifeParticleSystem *system);
    void life_particles_reset(LifeParticleSystem *system);
    void life_particles_update(LifeParticleSystem *system, float delta_time);

    bool life_particles_spawn_birth(LifeParticleSystem *system, const float origin[3], float radius,
                                    float speed_min, float speed_max, float vertical_bias);
    bool life_particles_spawn_death(LifeParticleSystem *system, const float origin[3], float radius,
                                    float speed_min, float speed_max, float vertical_bias);

    size_t life_particles_active_count(const LifeParticleSystem *system, LifeParticleKind kind);

#ifdef __cplusplus
}
#endif

#endif /* LIFE_PARTICLES_H */
