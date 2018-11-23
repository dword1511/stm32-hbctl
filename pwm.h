#ifndef __HBCTL_PWM_H__
#define __HBCTL_PWM_H__

#include <stdint.h>


extern void     pwm_setup(void);
extern void     pwm_config(uint32_t freq, uint8_t duty, uint8_t deadtime);
extern uint32_t pwm_get_next_freq(void);
extern void     pwm_enable(void);
extern void     pwm_disable(void);

#endif /* __HBCTL_PWM_H__ */
