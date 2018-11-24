/* Complementary PWM with deadtime using GP Timer (instead of Advanced TIM1) */
/* Implemented with center-aligned counter and 2 PWM channels in different modes */
/* NOTE: this further limits freq resolution... */

#include <libopencm3/cm3/assert.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>


#include "bsp.h"
#include "pwm.h"


//TODO: sync (use ETR to reset counter...)
void pwm_setup(void) {
  gpio_set_output_options(GPIO_PORT_PWM, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, GPIO_PIN_HI | GPIO_PIN_LO);
  gpio_set_af(GPIO_PORT_PWM, GPIO_AF2, GPIO_PIN_SYNC | GPIO_PIN_HI | GPIO_PIN_LO);
  gpio_mode_setup(GPIO_PORT_PWM, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_PIN_HI | GPIO_PIN_LO);
  gpio_mode_setup(GPIO_PORT_PWM, GPIO_MODE_AF, GPIO_PUPD_PULLDOWN, GPIO_PIN_SYNC);

  rcc_periph_clock_enable(PWM_TIM_RCC);
  rcc_periph_reset_pulse(PWM_TIM_RST);

  timer_disable_oc_output(PWM_TIM, TIM_OC1);
  timer_disable_oc_output(PWM_TIM, TIM_OC2);
  timer_disable_oc_output(PWM_TIM, TIM_OC3);
  timer_disable_oc_output(PWM_TIM, TIM_OC4);

  timer_set_mode(PWM_TIM, TIM_CR1_CKD_CK_INT, TIM_CR1_CMS_CENTER_1, TIM_CR1_DIR_UP);
  timer_set_prescaler(PWM_TIM, 0);
  timer_continuous_mode(PWM_TIM);

  timer_disable_oc_clear(PWM_TIM, PWM_OC_HI);
  timer_enable_oc_preload(PWM_TIM, PWM_OC_HI);
  timer_set_oc_mode(PWM_TIM, PWM_OC_HI, TIM_OCM_PWM1); /* Active when TIMx_CNT < TIMx_CCR1 */
  timer_disable_oc_clear(PWM_TIM, PWM_OC_LO);
  timer_enable_oc_preload(PWM_TIM, PWM_OC_LO);
  timer_set_oc_mode(PWM_TIM, PWM_OC_LO, TIM_OCM_PWM2); /* Active when TIMx_CNT >= TIMx_CCR1 */

  pwm_disable();
  /* Output should always be active (otherwise GPIO becomes high-Z) */
  timer_enable_counter(PWM_TIM);
  timer_enable_oc_output(PWM_TIM, PWM_OC_LO);
  timer_enable_oc_output(PWM_TIM, PWM_OC_HI);
}

/* freq in Hz, duty/deadtime in 0 - 255 */
void pwm_config(uint32_t freq, uint8_t duty, uint8_t deadtime) {
  uint16_t period = rcc_apb1_frequency / (freq * 2) - 1;
  uint16_t ocv_hi = (((uint32_t)duty) * ((uint32_t)period)) / 255 + 1;
  uint16_t ocv_lo = ocv_hi + (((uint32_t)deadtime) * ((uint32_t)period)) / 255;

  pwm_disable(); /* Avoid shoot-through */

  cm3_assert(duty + deadtime <= 255);
  cm3_assert(ocv_lo > ocv_hi); /* Low-side's OCV has to be greater than high-side's. Will not hold if deadtime is 0. */

  timer_set_period(PWM_TIM, period);
  timer_set_counter(PWM_TIM, 0);
  timer_set_oc_value(PWM_TIM, PWM_OC_HI, ocv_hi);
  timer_set_oc_value(PWM_TIM, PWM_OC_LO, ocv_lo);

  pwm_enable();
}

uint32_t pwm_get_next_freq(void) {
  uint16_t next_period = TIM_ARR(PWM_TIM) - 1;

  return rcc_apb1_frequency / ((next_period + 1) * 2);
}

void pwm_enable(void) {
  gpio_set(GPIO_PORT_LED, HCI_LED_RUN);

  /* Must be in this sequence to avoid shoot-through */
  timer_set_oc_mode(PWM_TIM, PWM_OC_LO, TIM_OCM_PWM2);
  timer_set_oc_mode(PWM_TIM, PWM_OC_HI, TIM_OCM_PWM1);
}

void pwm_disable(void) {
  /* Must be in this sequence to avoid shoot-through */
  timer_set_oc_mode(PWM_TIM, PWM_OC_HI, TIM_OCM_INACTIVE);
  timer_set_oc_mode(PWM_TIM, PWM_OC_LO, TIM_OCM_ACTIVE);

  gpio_clear(GPIO_PORT_LED, HCI_LED_RUN);
}
