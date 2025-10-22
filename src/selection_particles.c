#include "selection_particles.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void selection_particles_allocate(SelectionParticleSystem *system, size_t capacity)
{
    if (!system)
    {
        return;
    }
    if (capacity == 0U)
    {
        free(system->particles);
        system->particles = NULL;
        system->capacity  = 0U;
        return;
    }
    SelectionParticle *buffer =
        (SelectionParticle *)realloc(system->particles, capacity * sizeof(SelectionParticle));
    if (!buffer)
    {
        return;
    }
    system->particles = buffer;
    if (system->capacity < capacity)
    {
        size_t begin = system->capacity;
        memset(&system->particles[begin], 0, (capacity - begin) * sizeof(SelectionParticle));
    }
    system->capacity = capacity;
}

bool selection_particles_setup(SelectionParticleSystem *system, size_t capacity, float lifetime)
{
    if (!system)
    {
        return false;
    }
    if (capacity == 0U)
    {
        selection_particles_allocate(system, 0U);
        system->default_lifetime = (lifetime > 0.0f) ? lifetime : 1.0f;
        return true;
    }

    SelectionParticleSystem temp = *system;
    selection_particles_allocate(&temp, capacity);
    if (temp.capacity != capacity)
    {
        return false;
    }
    temp.default_lifetime = (lifetime > 0.0f) ? lifetime : 1.0f;
    for (size_t index = 0U; index < temp.capacity; ++index)
    {
        temp.particles[index].active   = 0U;
        temp.particles[index].age      = 0.0f;
        temp.particles[index].lifetime = temp.default_lifetime;
    }
    *system = temp;
    return true;
}

void selection_particles_shutdown(SelectionParticleSystem *system)
{
    if (!system)
    {
        return;
    }
    free(system->particles);
    system->particles        = NULL;
    system->capacity         = 0U;
    system->default_lifetime = 1.0f;
}

void selection_particles_reset(SelectionParticleSystem *system)
{
    if (!system || !system->particles)
    {
        return;
    }
    for (size_t index = 0U; index < system->capacity; ++index)
    {
        system->particles[index].active   = 0U;
        system->particles[index].age      = 0.0f;
        system->particles[index].lifetime = system->default_lifetime;
    }
}

void selection_particles_trigger_burst(SelectionParticleSystem *system, const float origin[3],
                                       float speed_min, float speed_max)
{
    if (!system || !system->particles || system->capacity == 0U || !origin)
    {
        return;
    }
    if (!(speed_min > 0.0f))
    {
        speed_min = 1.0f;
    }
    if (!(speed_max > speed_min))
    {
        speed_max = speed_min;
    }
    const float speed_range  = speed_max - speed_min;
    const float golden_angle = 2.39996322972865332f; /* ~3 - sqrt(5) */
    const float inv_capacity = (system->capacity > 0U) ? (1.0f / (float)system->capacity) : 0.0f;

    for (size_t index = 0U; index < system->capacity; ++index)
    {
        SelectionParticle *particle = &system->particles[index];
        particle->active            = 1U;
        particle->age               = 0.0f;
        particle->lifetime          = system->default_lifetime;
        particle->position[0]       = origin[0];
        particle->position[1]       = origin[1];
        particle->position[2]       = origin[2];

        float offset = ((float)index + 0.5f) * inv_capacity;
        float y      = 1.0f - 2.0f * offset;
        float radius = sqrtf(fmaxf(0.0f, 1.0f - y * y));
        float theta  = golden_angle * (float)index;
        float direction[3];
        direction[0] = cosf(theta) * radius;
        direction[1] = y;
        direction[2] = sinf(theta) * radius;
        float length = sqrtf(direction[0] * direction[0] + direction[1] * direction[1] +
                             direction[2] * direction[2]);
        if (length < 0.0001f)
        {
            direction[0] = 0.0f;
            direction[1] = 1.0f;
            direction[2] = 0.0f;
            length       = 1.0f;
        }
        float speed_bias      = fmodf(((float)index) * 0.61803398875f, 1.0f);
        float magnitude       = speed_min + speed_range * speed_bias;
        float inv_length      = 1.0f / length;
        particle->velocity[0] = direction[0] * inv_length * magnitude;
        particle->velocity[1] = direction[1] * inv_length * magnitude;
        particle->velocity[2] = direction[2] * inv_length * magnitude;
    }
}

void selection_particles_update(SelectionParticleSystem *system, float delta_time)
{
    if (!system || !system->particles || system->capacity == 0U)
    {
        return;
    }
    if (!(delta_time > 0.0f))
    {
        return;
    }
    for (size_t index = 0U; index < system->capacity; ++index)
    {
        SelectionParticle *particle = &system->particles[index];
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
        particle->velocity[1] += 0.4f * delta_time; /* subtle buoyancy */
    }
}

size_t selection_particles_active_count(const SelectionParticleSystem *system)
{
    if (!system || !system->particles || system->capacity == 0U)
    {
        return 0U;
    }
    size_t active = 0U;
    for (size_t index = 0U; index < system->capacity; ++index)
    {
        if (system->particles[index].active)
        {
            ++active;
        }
    }
    return active;
}
