/* Cortex-M SysTick driver with STM32's IWDG */

#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/assert.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/iwdg.h>

#include "bsp.h"
#include "tick.h"


#define BLINK_MS          150


static uint32_t           period_ms = 0;
static volatile uint64_t  uptime_ms = 0;


void tick_setup(uint32_t target_period_ms) {
  unsigned period = (rcc_ahb_frequency * target_period_ms) / 1000 - 1;

  cm3_assert((rcc_ahb_frequency * target_period_ms) > rcc_ahb_frequency); /* No overflow or 0 ms */
  cm3_assert(period < (1 << 24));

  period_ms = target_period_ms;

  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
  systick_set_reload(period);
  systick_clear();

  nvic_set_priority(NVIC_SYSTICK_IRQ, 80);
  systick_interrupt_enable();
  systick_counter_enable();

  rcc_osc_on(RCC_LSI);
  rcc_wait_for_osc_ready(RCC_LSI);
  iwdg_set_period_ms(target_period_ms * 2);
  iwdg_start();
  //iwdg_reset(); /* Kick the dog here does not prevent double reset after DFU */
}

void tick_delay_ms(uint32_t ms) {
  uint32_t target = uptime_ms + ms;

  while (uptime_ms < target) {
    asm("wfi");
  }
}

uint64_t tick_get_time_ms(void) {
  return uptime_ms;
}

void __attribute__((weak)) tick_routine_cb(void) {
  tick_routine_confirm_done();
}

void tick_routine_confirm_done(void) {
  /* IWDG watches for whether the routine is finished on-time. */
  iwdg_reset();
}

void sys_tick_handler(void) {
  tick_routine_cb();

  uptime_ms += period_ms;

  if ((uptime_ms) % BLINK_MS == 0) {
    gpio_toggle(GPIO_PORT_LED, HCI_LED_ALIVE);
  }
}
