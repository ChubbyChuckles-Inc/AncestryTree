#include "expansion.h"
#include "test_framework.h"

TEST(test_expansion_state_basic)
{
    ExpansionState expansion;
    expansion_state_reset(&expansion);
    ASSERT_FALSE(expansion_is_active(&expansion));
}
