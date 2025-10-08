#ifndef _BBBLED_TIMER_H
#define _BBBLED_TIMER_H

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct k_timer timer_t;
typedef k_timeout_t timeout_t;

typedef k_timer_expiry_t timer_expiry_cb_t;
typedef k_timer_stop_t  timer_stop_cb_t;

void timer_init(timer_t *timer, timer_expiry_cb_t expiry_fn, timer_stop_cb_t stop_fn, void *data);

void timer_start(timer_t *timer, timeout_t duration, timeout_t period);

void timer_stop(timer_t *timer);


#ifdef __cplusplus
}
#endif

#endif /* _BBBLED_TIMER_H */