#ifndef __HBCTL_CS_H__
#define __HBCTL_CS_H__

#include <stdint.h>


extern void     cs_setup(uint32_t target_duration_ms);
extern uint32_t cs_measure_ma(void);

#endif /* __HBCTL_CS_H__ */
