#include "detail_view.h"
#include "test_framework.h"

TEST(test_detail_view_basic)
{
    DetailViewSystem *dv = detail_view_create();
    ASSERT_TRUE(dv != NULL);
    detail_view_destroy(dv);
}
