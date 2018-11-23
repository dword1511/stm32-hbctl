#ifndef __HBCTL_TICK_H__
#define __HBCTL_TICK_H__

#include <stdint.h>


extern void     tick_setup(uint32_t target_period_ms);
extern void     tick_delay_ms(uint32_t ms);
extern uint64_t tick_get_time_ms(void);
extern void     tick_routine_cb(void);
extern void     tick_routine_confirm_done(void);

#endif /* __HBCTL_TICK_H__ */
