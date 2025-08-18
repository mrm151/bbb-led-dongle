#include "timer.h"


void timer_init(timer_t timer, timer_expiry_cb_t expiry_fn, timer_stop_cb_t stop_fn, void *data)
{
    k_timer_init(timer, expiry_fn, stop_fn);
    k_timer_user_data_set(timer, data);
}

void timer_start(timer_t timer, timeout_t duration, timeout_t period)
{
    k_timer_start(timer, duration, period);
}

void timer_stop(timer_t timer)
{
    k_timer_stop(timer);
}