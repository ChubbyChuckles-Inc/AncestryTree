#include "selection_particles.h"

#include "test_framework.h"

#include <math.h>

TEST(test_selection_particles_trigger_burst_activates_all_slots)
{
    SelectionParticleSystem system = {0};
    ASSERT_TRUE(selection_particles_setup(&system, 12U, 1.0f));

    float origin[3] = {1.0f, 2.0f, -3.0f};
    selection_particles_trigger_burst(&system, origin, 1.0f, 1.0f);
    ASSERT_EQ(selection_particles_active_count(&system), 12U);

    for (size_t index = 0U; index < system.capacity; ++index)
    {
        const SelectionParticle *particle = &system.particles[index];
        ASSERT_TRUE(particle->active != 0U);
        ASSERT_FLOAT_NEAR(particle->position[0], origin[0], 0.0001f);
        ASSERT_FLOAT_NEAR(particle->position[1], origin[1], 0.0001f);
        ASSERT_FLOAT_NEAR(particle->position[2], origin[2], 0.0001f);
        float magnitude = sqrtf(particle->velocity[0] * particle->velocity[0] +
                                particle->velocity[1] * particle->velocity[1] +
                                particle->velocity[2] * particle->velocity[2]);
        ASSERT_FLOAT_NEAR(magnitude, 1.0f, 0.0001f);
    }

    selection_particles_shutdown(&system);
}

TEST(test_selection_particles_update_deactivates_when_expired)
{
    SelectionParticleSystem system = {0};
    ASSERT_TRUE(selection_particles_setup(&system, 8U, 0.5f));

    float origin[3] = {0.0f, 0.0f, 0.0f};
    selection_particles_trigger_burst(&system, origin, 1.0f, 1.0f);
    ASSERT_EQ(selection_particles_active_count(&system), 8U);

    selection_particles_update(&system, 0.25f);
    ASSERT_EQ(selection_particles_active_count(&system), 8U);

    selection_particles_update(&system, 0.30f);
    ASSERT_EQ(selection_particles_active_count(&system), 0U);

    selection_particles_shutdown(&system);
}

TEST(test_selection_particles_setup_allows_capacity_reset)
{
    SelectionParticleSystem system = {0};
    ASSERT_TRUE(selection_particles_setup(&system, 4U, 1.0f));
    ASSERT_EQ(system.capacity, 4U);

    selection_particles_trigger_burst(&system, (float[3]){0.0f, 0.0f, 0.0f}, 1.0f, 2.0f);
    ASSERT_EQ(selection_particles_active_count(&system), 4U);

    ASSERT_TRUE(selection_particles_setup(&system, 0U, -1.0f));
    ASSERT_EQ(system.capacity, 0U);
    ASSERT_EQ(system.particles, NULL);
    ASSERT_FLOAT_NEAR(system.default_lifetime, 1.0f, 0.0001f);

    selection_particles_shutdown(&system);
}

void register_selection_particles_tests(TestRegistry *registry)
{
    REGISTER_TEST(registry, test_selection_particles_trigger_burst_activates_all_slots);
    REGISTER_TEST(registry, test_selection_particles_update_deactivates_when_expired);
    REGISTER_TEST(registry, test_selection_particles_setup_allows_capacity_reset);
}
