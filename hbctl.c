#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>

#include "bsp.h"
#include "cs.h"
#include "pwm.h"
#include "tick.h"
#include "hsi48trim.h"


#define TICK_PERIOD_MS  10
#define CS_SAMP_T_MS    5 /* Must be smaller than tick interval */
#define OVP_MA          1000
#define F_NOM           1000000
#define F_TOL           200000
#define F_MAX           (F_NOM + F_TOL)
#define F_MIN           (F_NOM - F_TOL)
#define DUTY            127 /* out of 255. Use 127 for soft switching */
#define DEAD_TIME       20
#define DELAY_LONG_MS   500
#define DELAY_SHORT_MS  50


/* Exceptions */
static void halt(void) {
  uint32_t i;

  while (true) {
    for (i = 0; i < 10000000; i ++) {
      asm("nop\n");
    }
    gpio_toggle(GPIO_PORT_LED, HCI_LED_ALL);
  }
}

void nmi_handler(void)
__attribute__ ((alias ("halt")));

void hard_fault_handler(void)
__attribute__ ((alias ("halt")));


/* HCI */
static void delay_a_bit(void) {
  uint32_t i;

  for (i = 0; i < 3000000; i ++) {
    asm("nop\n");
  }
}

static void hci_setup(void) {
  gpio_mode_setup(GPIO_PORT_BUT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_BUT);

  gpio_set_output_options(GPIO_PORT_LED, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, HCI_LED_ALL);
  gpio_mode_setup(GPIO_PORT_LED, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, HCI_LED_ALL);

  delay_a_bit(); /* System tick unavailable yet */
  gpio_set(GPIO_PORT_LED, HCI_LED_ALL);
  delay_a_bit();
  gpio_clear(GPIO_PORT_LED, HCI_LED_ALL);
  delay_a_bit();
  delay_a_bit();
  gpio_set(GPIO_PORT_LED, HCI_LED_ALIVE);
}

static void wait_button(void) {
  uint32_t i = 0;

  while (!gpio_get(GPIO_PORT_BUT, GPIO_PIN_BUT)) {
    asm("wfi");
  }

  /* De-bounce TODO: use tick */
  for (i = 0; i < 30000; i ++) {
    if (gpio_get(GPIO_PORT_BUT, GPIO_PIN_BUT)) {
      i = 0;
    }
  }
}

/* OVP */

void tick_routine_cb(void) {
  /* Over current protection */
  if (cs_measure_ma() > OVP_MA) {
    pwm_disable();
  }

  tick_routine_confirm_done();
}


/* TODO: tune HSI instead of timer */
int main(void) {
  uint32_t reset_reason = RCC_CSR & RCC_CSR_RESET_FLAGS;
  RCC_CSR |= RCC_CSR_RMVF; /* Clear reset flags */

  /* HSI48 does not need to be piped through PLL and can be tuned on-the-fly safely */
  rcc_clock_setup_in_hsi48_out_48mhz();

  /* Enable & reset GPIOA and GPIOB */
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_reset_pulse(RST_GPIOA);
  gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_ALL);
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_reset_pulse(RST_GPIOB);
  gpio_mode_setup(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_ALL);

  hci_setup();

  hsi48trim_setup();
  cs_setup(CS_SAMP_T_MS);
  pwm_setup();
  tick_setup(TICK_PERIOD_MS);

  /* In case of EMI (all GPIO setup done) */
  gpio_port_config_lock(GPIOA, GPIO_ALL);
  gpio_port_config_lock(GPIOB, GPIO_ALL);

  if (reset_reason & (RCC_CSR_WWDGRSTF | RCC_CSR_IWDGRSTF)) {
    /* Notify WDG reset */
    while (true) {
      tick_delay_ms(DELAY_SHORT_MS);
      gpio_toggle(GPIO_PORT_LED, HCI_LED_ALL);
    }
  }

  {
    uint32_t freq = F_MIN;

    while (true) {
      wait_button();

      if (!gpio_get(GPIO_PORT_LED, HCI_LED_RUN)) {
        /* Bridge has been disabled due to POR or OVP, (re)enable per user request */
        pwm_enable();
      } else {
        pwm_disable();
        tick_delay_ms(DELAY_SHORT_MS);

        if (hsi48trim_goto_next() == 0) {
          /* HSI48 trimming has gone through one round, do coarse adjustment */
          freq = pwm_get_next_freq();
          if (freq > F_MAX) {
            freq = F_MIN;
          }
          pwm_config(freq, DUTY, DEAD_TIME);
        } else {
          /* Fine adjustment has been done */
          pwm_enable();
        }
      }
    }
  }

  return 0;
}
