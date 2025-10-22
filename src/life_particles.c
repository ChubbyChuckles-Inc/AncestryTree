#include "life_particles.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void life_particles_channel_clear(LifeParticleChannel *channel)
{
    if (!channel)
    {
        return;
    }
    channel->particles        = NULL;
    channel->capacity         = 0U;
    channel->default_lifetime = 0.0f;
    channel->rng_state        = 0U;
    channel->next_index       = 0U;
}

static void life_particles_channel_free(LifeParticleChannel *channel)
{
    if (!channel)
    {
        return;
    }
    free(channel->particles);
    life_particles_channel_clear(channel);
}

static unsigned int life_particles_seed_for_kind(LifeParticleKind kind)
{
    return (kind == LIFE_PARTICLE_KIND_BIRTH) ? 0xA361E12DU : 0xC1D94A73U;
}

static float life_particles_random_unit(LifeParticleChannel *channel)
{
    if (!channel)
    {
        return 0.0f;
    }
    channel->rng_state  = channel->rng_state * 1664525U + 1013904223U;
    unsigned int sample = (channel->rng_state >> 8) & 0x00FFFFFFU;
    return (float)sample / 16777215.0f;
}

static void life_particles_random_direction(LifeParticleChannel *channel, float out_direction[3])
{
    if (!channel || !out_direction)
    {
        return;
    }
    float u          = life_particles_random_unit(channel);
    float v          = life_particles_random_unit(channel);
    float theta      = 6.28318530718f * u;
    float z          = 1.0f - 2.0f * v;
    float r          = sqrtf(fmaxf(0.0f, 1.0f - z * z));
    out_direction[0] = r * cosf(theta);
    out_direction[1] = z;
    out_direction[2] = r * sinf(theta);
}

static bool life_particles_channel_setup(LifeParticleChannel *channel, LifeParticleKind kind,
                                         size_t capacity, float lifetime)
{
    if (!channel)
    {
        return false;
    }
    life_particles_channel_clear(channel);
    if (capacity == 0U)
    {
        return true;
    }
    LifeParticle *particles = (LifeParticle *)calloc(capacity, sizeof(LifeParticle));
    if (!particles)
    {
        return false;
    }
    channel->particles        = particles;
    channel->capacity         = capacity;
    channel->default_lifetime = (lifetime > 0.0f) ? lifetime : 1.0f;
    channel->rng_state        = life_particles_seed_for_kind(kind);
    channel->next_index       = 0U;
    return true;
}

bool life_particles_setup(LifeParticleSystem *system, size_t birth_capacity, size_t death_capacity,
                          float birth_lifetime, float death_lifetime)
{
    if (!system)
    {
        return false;
    }
    memset(system, 0, sizeof(*system));
    if (!life_particles_channel_setup(&system->birth, LIFE_PARTICLE_KIND_BIRTH, birth_capacity,
                                      birth_lifetime))
    {
        life_particles_shutdown(system);
        return false;
    }
    if (!life_particles_channel_setup(&system->death, LIFE_PARTICLE_KIND_DEATH, death_capacity,
                                      death_lifetime))
    {
        life_particles_shutdown(system);
        return false;
    }
    return true;
}

void life_particles_shutdown(LifeParticleSystem *system)
{
    if (!system)
    {
        return;
    }
    life_particles_channel_free(&system->birth);
    life_particles_channel_free(&system->death);
}

void life_particles_reset(LifeParticleSystem *system)
{
    if (!system)
    {
        return;
    }
    if (system->birth.particles && system->birth.capacity > 0U)
    {
        memset(system->birth.particles, 0, system->birth.capacity * sizeof(LifeParticle));
        system->birth.next_index = 0U;
        system->birth.rng_state  = life_particles_seed_for_kind(LIFE_PARTICLE_KIND_BIRTH);
    }
    if (system->death.particles && system->death.capacity > 0U)
    {
        memset(system->death.particles, 0, system->death.capacity * sizeof(LifeParticle));
        system->death.next_index = 0U;
        system->death.rng_state  = life_particles_seed_for_kind(LIFE_PARTICLE_KIND_DEATH);
    }
}

static void life_particles_update_channel(LifeParticleChannel *channel, float delta_time)
{
    if (!channel || !channel->particles || channel->capacity == 0U)
    {
        return;
    }
    if (!(delta_time > 0.0f))
    {
        return;
    }
    for (size_t index = 0U; index < channel->capacity; ++index)
    {
        LifeParticle *particle = &channel->particles[index];
        if (!particle->active)
        {
            continue;
        }
        particle->age += delta_time;
        if (particle->age >= particle->lifetime)
        {
            particle->active = 0U;
            continue;
        }
        particle->position[0] += particle->velocity[0] * delta_time;
        particle->position[1] += particle->velocity[1] * delta_time;
        particle->position[2] += particle->velocity[2] * delta_time;
        /* Apply a slight velocity dampening to keep particles near the node. */
        particle->velocity[0] *= 0.92f;
        particle->velocity[2] *= 0.92f;
    }
}

void life_particles_update(LifeParticleSystem *system, float delta_time)
{
    if (!system)
    {
        return;
    }
    life_particles_update_channel(&system->birth, delta_time);
    life_particles_update_channel(&system->death, delta_time);
}

static bool life_particles_spawn_channel(LifeParticleChannel *channel, const float origin[3],
                                         float radius, float speed_min, float speed_max,
                                         float vertical_bias, float vertical_sign)
{
    if (!channel || channel->capacity == 0U || !channel->particles)
    {
        return false;
    }
    if (!(speed_min > 0.0f) && !(speed_max > 0.0f))
    {
        return false;
    }
    if (speed_max < speed_min)
    {
        float tmp = speed_min;
        speed_min = speed_max;
        speed_max = tmp;
    }
    float base_speed = speed_min;
    if (speed_max > speed_min)
    {
        float t    = life_particles_random_unit(channel);
        base_speed = speed_min + (speed_max - speed_min) * t;
    }
    size_t index           = channel->next_index;
    channel->next_index    = (channel->next_index + 1U) % channel->capacity;
    LifeParticle *particle = &channel->particles[index];
    particle->age          = 0.0f;
    particle->lifetime     = channel->default_lifetime;
    particle->active       = 1U;
    if (origin)
    {
        particle->position[0] = origin[0];
        particle->position[1] = origin[1];
        particle->position[2] = origin[2];
    }
    else
    {
        particle->position[0] = 0.0f;
        particle->position[1] = 0.0f;
        particle->position[2] = 0.0f;
    }

    float direction[3] = {0.0f, 1.0f, 0.0f};
    life_particles_random_direction(channel, direction);
    direction[1] = fabsf(direction[1]);
    direction[1] = fmaxf(direction[1], vertical_bias);
    direction[1] *= vertical_sign;
    if (vertical_sign < 0.0f)
    {
        direction[1] = -fabsf(direction[1]);
    }
    float length = sqrtf(direction[0] * direction[0] + direction[1] * direction[1] +
                         direction[2] * direction[2]);
    if (!(length > 0.0f))
    {
        direction[0] = 0.0f;
        direction[1] = vertical_sign;
        direction[2] = 0.0f;
        length       = 1.0f;
    }
    float inv_length = 1.0f / length;
    direction[0] *= inv_length;
    direction[1] *= inv_length;
    direction[2] *= inv_length;

    float lateral_jitter = radius * 0.35f;
    float jitter_x       = (life_particles_random_unit(channel) - 0.5f) * lateral_jitter;
    float jitter_z       = (life_particles_random_unit(channel) - 0.5f) * lateral_jitter;
    particle->position[0] += jitter_x;
    particle->position[2] += jitter_z;
    particle->position[1] += vertical_sign > 0.0f ? radius * 0.2f : -radius * 0.2f;

    particle->velocity[0] = direction[0] * base_speed;
    particle->velocity[1] = direction[1] * base_speed;
    particle->velocity[2] = direction[2] * base_speed;
    return true;
}

bool life_particles_spawn_birth(LifeParticleSystem *system, const float origin[3], float radius,
                                float speed_min, float speed_max, float vertical_bias)
{
    if (!system)
    {
        return false;
    }
    if (!(vertical_bias > 0.0f))
    {
        vertical_bias = 0.4f;
    }
    return life_particles_spawn_channel(&system->birth, origin, radius, speed_min, speed_max,
                                        vertical_bias, 1.0f);
}

bool life_particles_spawn_death(LifeParticleSystem *system, const float origin[3], float radius,
                                float speed_min, float speed_max, float vertical_bias)
{
    if (!system)
    {
        return false;
    }
    if (!(vertical_bias > 0.0f))
    {
        vertical_bias = 0.35f;
    }
    return life_particles_spawn_channel(&system->death, origin, radius, speed_min, speed_max,
                                        vertical_bias, -1.0f);
}

size_t life_particles_active_count(const LifeParticleSystem *system, LifeParticleKind kind)
{
    if (!system)
    {
        return 0U;
    }
    const LifeParticleChannel *channel =
        (kind == LIFE_PARTICLE_KIND_BIRTH) ? &system->birth : &system->death;
    if (!channel->particles || channel->capacity == 0U)
    {
        return 0U;
    }
    size_t active = 0U;
    for (size_t index = 0U; index < channel->capacity; ++index)
    {
        if (channel->particles[index].active)
        {
            ++active;
        }
    }
    return active;
}
