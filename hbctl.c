#include <libopencm3/cm3/systick.h>
#include <libopencm3/cm3/nvic.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/iwdg.h>


#define GPIO_PORT_BUT GPIOB
#define GPIO_PIN_BUT  GPIO8
#define GPIO_PORT_LED GPIOA
#define GPIO_PIN_LED1 GPIO6
#define GPIO_PIN_LED2 GPIO7

#define GPIO_PORT_CS  GPIOA
#define GPIO_PIN_CSP  GPIO5
#define ADC_CH_CSP    5
#define GPIO_PIN_CSN  GPIO4
#define ADC_CH_CSN    4
#define GPIO_PORT_VIN GPIOB
#define GPIO_PIN_VIN  GPIO1
#define ADC_CH_VIN    9

#define ADC_R_MOHM    100
#define ADC_VDDA_MV   3300
#define ADC_DIGITS    (1 << 12)
#define ADC_CLOCK     12000000
#define ADC_CONV_CYCS 241   /* 239.5 + 1.5 */
#define ADC_SAMP_T_MS 5
#define ADC_OVERSAMP  (((ADC_CLOCK / 1000 / 2) * ADC_SAMP_T_MS) / ADC_CONV_CYCS) /* Each measurement samples 2 channels */

#define GPIO_PORT_PWM GPIOA
#define GPIO_AF_PWM   GPIO_AF2
#define GPIO_PIN_SYNC GPIO0
#define GPIO_PIN_HI   GPIO1
#define GPIO_PIN_LO   GPIO2
#define PWM_TIM       TIM2
#define PWM_TIM_RCC   RCC_TIM2
#define PWM_TIM_RST   RST_TIM2
#define PWM_OC_HI     TIM_OC2
#define PWM_OC_LO     TIM_OC3


/* Exceptions */
static void halt(void) {
  uint32_t i;

  while (true) {
    for (i = 0; i < 10000000; i ++) {
      asm("nop\n");
    }
    gpio_toggle(GPIO_PORT_LED, GPIO_PIN_LED1 | GPIO_PIN_LED2);
  }
}

void nmi_handler(void)
__attribute__ ((alias ("halt")));

void hard_fault_handler(void)
__attribute__ ((alias ("halt")));


/* ADC */
static void setup_adc(void) {
  rcc_periph_clock_enable(RCC_ADC1);

  gpio_mode_setup(GPIO_PORT_CS, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_CSP | GPIO_PIN_CSN);

  adc_power_off(ADC1);
  adc_set_clk_source(ADC1, ADC_CLKSOURCE_PCLK_DIV4); /* Set ADC clock to 12 MHz, see also ADC_CLOCK */
  adc_calibrate(ADC1);
  adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5); /* (239.5 + 1.5 ~= 50 ksps), see also ADC_CONV_CYCS */
  adc_set_resolution(ADC1, ADC_RESOLUTION_12BIT); /* See also ADC_DIGITS */
  adc_set_right_aligned(ADC1);
  adc_power_on(ADC1);

  adc_set_single_conversion_mode(ADC1);
}

static uint32_t read_adc_ma(void) {
  unsigned i;
  uint32_t acc_csp = 0;
  uint32_t acc_csn = 0;
  uint32_t acc_cs;
  uint32_t ma;
  /* const but prefer to store in SRAM */
  uint8_t ch_csp = ADC_CH_CSP;
  uint8_t ch_csn = ADC_CH_CSN;

  /* Oversampling act as FIR LPF */
  for (i = 0; i < ADC_OVERSAMP; i ++) {
    adc_set_regular_sequence(ADC1, 1, &ch_csp);
    adc_start_conversion_regular(ADC1);
    while (!adc_eoc(ADC1));
    acc_csp += adc_read_regular(ADC1);
    //
    adc_set_regular_sequence(ADC1, 1, &ch_csn);
    adc_start_conversion_regular(ADC1);
    while (!adc_eoc(ADC1));
    acc_csn += adc_read_regular(ADC1);
  }

  /* mV per LSB = ADC_VDDA_MV / ADC_DIGITS, mA per mV = 1000 / ADC_R_MOHM, mA per LSB = ADC_VDDA_MV * 1000 / ADC_DIGITS / ADC_R_MOHM (approx 8 mA in current setup) */
  acc_cs = acc_csp - acc_csn;
  ma = (acc_cs * ADC_VDDA_MV * 1000) / (ADC_DIGITS * ADC_R_MOHM * ADC_OVERSAMP);

  return ma;
}

/* PWM */
/* Complementary PWM with deadtime using GP Timer (instead of Advanced TIM1) */
/* Implemented with center-aligned counter and 2 PWM channels in different modes */
/* NOTE: this further limits freq resolution... */

static void disable_pwm(void) {
  /* Must be in this sequence to avoid shoot-through */
  timer_set_oc_mode(PWM_TIM, PWM_OC_HI, TIM_OCM_INACTIVE);
  timer_set_oc_mode(PWM_TIM, PWM_OC_LO, TIM_OCM_ACTIVE);

  gpio_clear(GPIO_PORT_LED, GPIO_PIN_LED2);
}

static void enable_pwm(void) {
  gpio_set(GPIO_PORT_LED, GPIO_PIN_LED2);

  /* Must be in this sequence to avoid shoot-through */
  timer_set_oc_mode(PWM_TIM, PWM_OC_LO, TIM_OCM_PWM2);
  timer_set_oc_mode(PWM_TIM, PWM_OC_HI, TIM_OCM_PWM1);
}

//TODO: sync (use ETR to reset counter...)
static void setup_pwm(void) {
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

  disable_pwm();
  /* Output should always be active (otherwise GPIO becomes high-Z) */
  timer_enable_counter(PWM_TIM);
  timer_enable_oc_output(PWM_TIM, PWM_OC_LO);
  timer_enable_oc_output(PWM_TIM, PWM_OC_HI);
}

/* freq in Hz, duty/deadtime in 0 - 255 */
static void config_pwm(uint32_t freq, uint8_t duty, uint8_t deadtime) {
  uint16_t period = rcc_apb1_frequency / freq / 2 - 1;
  uint16_t ocv_hi = (((uint32_t)duty) * ((uint32_t)period)) / 255 + 1;
  uint16_t ocv_lo = ocv_hi + (((uint32_t)deadtime) * ((uint32_t)period)) / 255;

  if (duty + deadtime >= 255) {
    /* Error: duty/deadtime too large */
    halt();
  }

  if (ocv_lo <= ocv_hi) {
    /* Error: deadtime too small */
    halt();
  }

  disable_pwm(); /* Avoid shoot-through */

  timer_set_period(PWM_TIM, period);
  timer_set_counter(PWM_TIM, 0);
  timer_set_oc_value(PWM_TIM, PWM_OC_HI, ocv_hi);
  timer_set_oc_value(PWM_TIM, PWM_OC_LO, ocv_lo);

  enable_pwm();
}

/* HCI/OVP */

#define SYSCLK_PERIOD_MS  10
#define OVP_MA            1000
#define DELAY_LONG_MS     500
#define DELAY_SHORT_MS    50
#define BLINK_MS          150


static volatile uint64_t  uptime_ms = 0;


static void delay_ms(uint32_t ms) {
  uint32_t target = uptime_ms + ms;

  while (uptime_ms < target) {
    asm("wfi");
  }
}

static void delay_a_bit(void) {
  uint32_t i;

  for (i = 0; i < 3000000; i ++) {
    asm("nop\n");
  }
}

static void setup_hci(void) {
  gpio_mode_setup(GPIO_PORT_BUT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_PIN_BUT);

  gpio_set_output_options(GPIO_PORT_LED, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_LED1 | GPIO_PIN_LED2);
  gpio_mode_setup(GPIO_PORT_LED, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_LED1 | GPIO_PIN_LED2);

  delay_a_bit(); /* System tick unavailable yet */
  gpio_set(GPIO_PORT_LED, GPIO_PIN_LED1 | GPIO_PIN_LED2);
  delay_a_bit();
  gpio_clear(GPIO_PORT_LED, GPIO_PIN_LED1 | GPIO_PIN_LED2);
  delay_a_bit();
  delay_a_bit();
}

static void wait_button(void) {
  uint32_t i = 0;

  while (!gpio_get(GPIO_PORT_BUT, GPIO_PIN_BUT)) {
    asm("wfi");
  }

  /* De-bounce */
  for (i = 0; i < 30000; i ++) {
    if (gpio_get(GPIO_PORT_BUT, GPIO_PIN_BUT)) {
      i = 0;
    }
  }
}

void sys_tick_handler(void) {
  /* Over current protection */
  if (read_adc_ma() > OVP_MA) {
    disable_pwm();
  }

  uptime_ms += SYSCLK_PERIOD_MS;

  if ((uptime_ms) % BLINK_MS == 0) {
    gpio_toggle(GPIO_PORT_LED, GPIO_PIN_LED1);
  }

  iwdg_reset();
}

static void setup_tick(void) {
  unsigned period = rcc_ahb_frequency * SYSCLK_PERIOD_MS / 1000 - 1;

  systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
  systick_set_reload(period);
  systick_clear();

  nvic_set_priority(NVIC_SYSTICK_IRQ, 80);
  systick_interrupt_enable();
  systick_counter_enable();

  rcc_osc_on(RCC_LSI);
  rcc_wait_for_osc_ready(RCC_LSI);
  iwdg_set_period_ms(SYSCLK_PERIOD_MS * 2);
  iwdg_start();
}


#define F_MAX     1100000
#define F_MIN      900000
#define F_STEP      10000 /* 1/48MHz ~= 21ns. @ 800kHz, delta_f = 13kHz. @ 1200kHz, delta_f = 29kHz. */
#define N_FREQ    ((F_MAX - F_MIN) / F_STEP + 1)
#define DUTY          127 /* out of 255. Use 127 for soft switching */
#define DEAD_TIME      20
/* TODO: tune HSI instead of timer */


int main(void) {
  uint32_t reset_reason = RCC_CSR & RCC_CSR_RESET_FLAGS;
  RCC_CSR |= RCC_CSR_RMVF; /* Clear reset flags */

  rcc_clock_setup_in_hsi_out_48mhz();
  /* Enable GPIOA and GPIOB */
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);

  setup_hci();

  gpio_set(GPIO_PORT_LED, GPIO_PIN_LED1);
  setup_adc();
  setup_tick();
  setup_pwm();

  if (reset_reason & (RCC_CSR_WWDGRSTF | RCC_CSR_IWDGRSTF)) {
    /* Notify WDG reset */
    while (true) {
      delay_ms(DELAY_SHORT_MS);
      gpio_toggle(GPIO_PORT_LED, GPIO_PIN_LED1 | GPIO_PIN_LED2);
    }
  }

  /* In case of EMI (ADC setup already done) */
  gpio_port_config_lock(GPIOA, GPIO_ALL);
  gpio_port_config_lock(GPIOB, GPIO_ALL);

  {
    uint32_t freq = F_MIN;

    wait_button();
    config_pwm(freq, DUTY, DEAD_TIME);

    while (true) {
      wait_button();
      disable_pwm();
      delay_ms(DELAY_SHORT_MS);

      freq += F_STEP;
      if (freq > F_MAX) {
        freq = F_MIN;
      }
      config_pwm(freq, DUTY, DEAD_TIME);
    }
  }

  return 0;
}
