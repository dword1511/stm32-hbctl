#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>

#include "uart.h"

#define GPIO_BANK_BUT GPIOA
#define GPIO_BUT      GPIO0
#define GPIO_BANK_LED GPIOC
#define GPIO_LED1     GPIO8
#define GPIO_LED2     GPIO9

#define BAUD_RATE     115200

static void swcap_setup_gpio(void) {
  /* Setup TIM1 channel 1 (channel 2/3 pin occupied by UART, channel 4 is single-ended) */
  gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, GPIO7 | GPIO8);
  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO7 | GPIO8);
  gpio_set_af(GPIOA, GPIO_AF2, GPIO7 | GPIO8);

  /* In case of EMI */
  gpio_port_config_lock(GPIOA, GPIO7 | GPIO8);
}

static void swcap_setup_timer(uint16_t period, uint16_t duty, uint16_t deadtime) {
  rcc_periph_clock_enable(RCC_TIM1);

  timer_reset(TIM1);
  timer_disable_oc_output(TIM1, TIM_OC1);
  timer_disable_oc_output(TIM1, TIM_OC1N);
  timer_disable_oc_output(TIM1, TIM_OC2);
  timer_disable_oc_output(TIM1, TIM_OC2N);
  timer_disable_oc_output(TIM1, TIM_OC3);
  timer_disable_oc_output(TIM1, TIM_OC3N);
  timer_disable_oc_output(TIM1, TIM_OC4);

  timer_set_mode(TIM1, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_EDGE, TIM_CR1_DIR_UP);
  timer_set_prescaler(TIM1, 0);
  timer_continuous_mode(TIM1);

  timer_set_period(TIM1, period);
  timer_set_deadtime(TIM1, deadtime);

  timer_set_break_lock(TIM1, TIM_BDTR_LOCK_OFF);
  timer_disable_break(TIM1);

  timer_disable_oc_clear(TIM1, TIM_OC1);
  timer_enable_oc_preload(TIM1, TIM_OC1);
  timer_set_oc_mode(TIM1, TIM_OC1, TIM_OCM_PWM1);
  timer_set_oc_value(TIM1, TIM_OC1, duty);

  timer_enable_break_main_output(TIM1); /* Required for advanced timer to work */
  timer_enable_oc_output(TIM1, TIM_OC1);
  timer_enable_oc_output(TIM1, TIM_OC1N);
  timer_enable_counter(TIM1);
}

/* TIM1 as Switched Capacitor Clock Source */
static void swcap_setup(uint32_t freq, uint16_t duty, uint16_t deadtime) {
  uint16_t period = rcc_apb2_frequency / freq - 1;
  uint16_t ocv = (((uint32_t)duty) * ((uint32_t)period)) / 0xffff;
  /* Period / duty calculation is OK */

  /* GPIO / AF should have been enabled by this point */
  swcap_setup_gpio();
  swcap_setup_timer(period, ocv, deadtime);
}

static void button_setup(void) {
  gpio_mode_setup(GPIO_BANK_BUT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_BUT);

  /* In case of EMI */
  gpio_port_config_lock(GPIO_BANK_BUT, GPIO_BUT);
}

static void button_wait(void) {
  uint32_t i;

  while (!gpio_get(GPIO_BANK_BUT, GPIO_BUT));

  /* De-bounce */
  for (i = 0; i < 100000; i ++) {
    if (gpio_get(GPIO_BANK_BUT, GPIO_BUT)) {
      i = 0;
    }
  }
}

static void led_setup(void) {
  gpio_set_output_options(GPIO_BANK_LED, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_LED1 | GPIO_LED2);
  gpio_mode_setup(GPIO_BANK_LED, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_LED1 | GPIO_LED2);

  /* In case of EMI */
  gpio_port_config_lock(GPIO_BANK_LED, GPIO_LED1 | GPIO_LED2);
}

/* Interrupts */

static void halt_normal(void) {
  while (true);
}

static void halt(void) {
  uint32_t i;

  while (true) {
    for (i = 0; i < 10000000; i ++) {
      asm("nop\n");
    }
    gpio_toggle(GPIO_BANK_LED, GPIO_LED1);
    gpio_toggle(GPIO_BANK_LED, GPIO_LED2);
  }
}

void nmi_handler(void)
__attribute__ ((alias ("halt")));

void hard_fault_handler(void)
__attribute__ ((alias ("halt")));

#define DEAT_TIME 13 /* Suggested minimum: 270ns */
#define FREQ_1    300000
#define FREQ_2    250000
#define FREQ_3    100000

int main(void) {
  uint32_t freq = FREQ_1;
  uint32_t i;

  /* Enable GPIOA and GPIOB */
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_GPIOC);

  led_setup();

  gpio_set(GPIO_BANK_LED, GPIO_LED1);
  rcc_clock_setup_in_hsi_out_48mhz();
  swcap_setup(freq, 32768, DEAT_TIME);
  gpio_set(GPIO_BANK_LED, GPIO_LED2);

  uart_setup(BAUD_RATE);
  uart_printf("\r\n\r\n%s, compiled on %s %s, gcc %d.%d.%d\r\n", __FILE__, __DATE__, __TIME__, __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__);
  uart_printf("f1/f2/f3 = %d.%03d/%d.%03d/%d.%03d kHz\r\n", FREQ_1 / 1000, FREQ_1 % 1000, FREQ_2 / 1000, FREQ_2 % 1000, FREQ_3 / 1000, FREQ_3 % 1000);
  button_setup();

  /* TODO: communication */
  while (true) {
    button_wait();
    gpio_clear(GPIO_BANK_LED, GPIO_LED1 | GPIO_LED2);
    for (i = 0; i < 1000000; i ++) {
      asm("nop\n");
    }

    switch (freq) {
      case FREQ_1: {
        freq = FREQ_2;
        gpio_set(GPIO_BANK_LED, GPIO_LED1);
        break;
      }
      case FREQ_2: {
        freq = FREQ_3;
        gpio_set(GPIO_BANK_LED, GPIO_LED2);
        break;
      }
      default: {
        freq = FREQ_1;
        gpio_set(GPIO_BANK_LED, GPIO_LED1 | GPIO_LED2);
      }
    }

    swcap_setup(freq, 32768, DEAT_TIME);
  }

  halt_normal();
  return 0;
}
