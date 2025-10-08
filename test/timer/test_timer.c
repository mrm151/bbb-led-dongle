#include <zephyr/ztest.h>
#include <timer.h>
#include <zephyr/logging/log.h>
#include <errno.h>
#include <stdlib.h>


LOG_MODULE_REGISTER(timer_test, LOG_LEVEL_DBG);


void timer_stop_cb(timer_t timer)
{
    zassert_ok(0);
}

void timer_expired_cb(timer_t timer)
{
    zassert_ok(0);
}


ZTEST(timer_test, init)
{

}


ZTEST_SUITE(timer_test, NULL, NULL, NULL, NULL, NULL);
