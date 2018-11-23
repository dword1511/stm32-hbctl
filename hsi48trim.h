#ifndef __HBCTL_HSI48TRIM_H__
#define __HBCTL_HSI48TRIM_H__

#include <stdint.h>


#define HSI48TRIM_MAX (1 << 6)


extern void     hsi48trim_setup(void);
extern void     hsi48trim_set(uint16_t trim);
extern uint16_t hsi48trim_get(void);
extern uint16_t hsi48trim_goto_next(void);

#endif /* __HBCTL_HSI48TRIM_H__ */
