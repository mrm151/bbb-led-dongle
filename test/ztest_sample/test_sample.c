#include <zephyr/ztest.h>

ZTEST(my_suite, test_addition)
{
    zassert_equal(1 + 1, 2, "1 + 1 should equal 2");
}

ZTEST_SUITE(my_suite, NULL, NULL, NULL, NULL, NULL);