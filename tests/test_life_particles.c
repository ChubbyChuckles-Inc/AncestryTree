#include "life_particles.h"
#include "test_framework.h"

#include <math.h>

TEST(test_life_particles_birth_moves_upwards)
{
    LifeParticleSystem system;
    ASSERT_TRUE(life_particles_setup(&system, 4U, 0U, 1.0f, 1.0f));

    float origin[3] = {0.0f, 1.0f, 0.0f};
    ASSERT_TRUE(life_particles_spawn_birth(&system, origin, 0.6f, 0.5f, 0.5f, 0.8f));
    ASSERT_EQ(life_particles_active_count(&system, LIFE_PARTICLE_KIND_BIRTH), 1U);

    LifeParticle *particle = &system.birth.particles[0];
    ASSERT_NOT_NULL(particle);
    ASSERT_TRUE(particle->active);
    ASSERT_TRUE(particle->velocity[1] > 0.0f);

    life_particles_update(&system, 0.4f);
    ASSERT_TRUE(particle->position[1] > origin[1]);

    life_particles_update(&system, 1.0f);
    ASSERT_FALSE(particle->active);

    life_particles_shutdown(&system);
}

TEST(test_life_particles_death_moves_downwards)
{
    LifeParticleSystem system;
    ASSERT_TRUE(life_particles_setup(&system, 0U, 3U, 1.5f, 0.8f));

    float origin[3] = {0.0f, 2.0f, 0.0f};
    ASSERT_TRUE(life_particles_spawn_death(&system, origin, 0.6f, 0.3f, 0.3f, 0.6f));
    ASSERT_EQ(life_particles_active_count(&system, LIFE_PARTICLE_KIND_DEATH), 1U);

    LifeParticle *particle = &system.death.particles[0];
    ASSERT_NOT_NULL(particle);
    ASSERT_TRUE(particle->active);
    ASSERT_TRUE(particle->velocity[1] < 0.0f);

    life_particles_update(&system, 0.5f);
    ASSERT_TRUE(particle->position[1] < origin[1]);

    life_particles_update(&system, 1.0f);
    ASSERT_FALSE(particle->active);

    life_particles_shutdown(&system);
}

TEST(test_life_particles_reset_clears_channels)
{
    LifeParticleSystem system;
    ASSERT_TRUE(life_particles_setup(&system, 2U, 2U, 1.0f, 1.0f));

    float origin[3] = {0.0f, 0.0f, 0.0f};
    ASSERT_TRUE(life_particles_spawn_birth(&system, origin, 0.5f, 0.4f, 0.4f, 0.6f));
    ASSERT_TRUE(life_particles_spawn_death(&system, origin, 0.5f, 0.4f, 0.4f, 0.6f));
    ASSERT_TRUE(life_particles_active_count(&system, LIFE_PARTICLE_KIND_BIRTH) > 0U);
    ASSERT_TRUE(life_particles_active_count(&system, LIFE_PARTICLE_KIND_DEATH) > 0U);

    life_particles_reset(&system);
    ASSERT_EQ(life_particles_active_count(&system, LIFE_PARTICLE_KIND_BIRTH), 0U);
    ASSERT_EQ(life_particles_active_count(&system, LIFE_PARTICLE_KIND_DEATH), 0U);

    life_particles_shutdown(&system);
}

void register_life_particles_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_life_particles_birth_moves_upwards);
    REGISTER_TEST(registry, test_life_particles_death_moves_downwards);
    REGISTER_TEST(registry, test_life_particles_reset_clears_channels);
}
