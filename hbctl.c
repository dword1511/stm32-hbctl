#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/adc.h>

#define GPIO_BANK_BUT GPIOA
#define GPIO_BUT      GPIO0
#define GPIO_BANK_LED GPIOC
#define GPIO_LED1     GPIO8
#define GPIO_LED2     GPIO9


static uint8_t  channel_array_1[16] = {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};


static void setup_adc(void) {
  /* Set ADC clock to 14 MHz */
  adc_set_clk_source(ADC1, ADC_CLKSOURCE_ADC);
  rcc_periph_clock_enable(RCC_ADC1);

  /* Configure PA2 and PA4 for analog input */

  gpio_mode_setup(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO1);

  /* Configure PA1, PA3 and PA5 as guard (ground) */
  gpio_set_output_options(GPIOA, GPIO_OTYPE_OD, GPIO_OSPEED_100MHZ, GPIO0 | GPIO2);
  gpio_mode_setup(GPIOA, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO0 | GPIO2);
  gpio_clear(GPIOA, GPIO0 | GPIO2);

  adc_disable_temperature_sensor();
  adc_power_off(ADC1);
  adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5); /* (239.5 + 1.5 = 58 ksps) */
  adc_set_right_aligned(ADC1);
  adc_power_on(ADC1);
  adc_calibrate(ADC1);
  adc_set_regular_sequence(ADC1, 1, channel_array_1);

  adc_set_continuous_conversion_mode(ADC1);
  adc_start_conversion_regular(ADC1);
}

#define ADC_OVERSAMPLE 10

static uint32_t read_adc(void) {
  /* Oversample act as FIR LPF */
  uint32_t adc_acc = 0;
  int i;

  adc_read_regular(ADC1);
  for (i = 0; i < ADC_OVERSAMPLE; i ++) {
    adc_acc += adc_read_regular(ADC1);
  }

  return adc_acc;
}

static void swcap_setup_gpio(void) {
  /* These should be PA9 and PA10 */
  /* Setup TIM1 channel 2 (channel 1 pin occupied by MCO, channel 4 is single-ended) */
  gpio_set_output_options(GPIOA, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, GPIO9);
  gpio_set_output_options(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_100MHZ, GPIO0);
  gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO9);
  gpio_mode_setup(GPIOB, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO0);
  gpio_set_af(GPIOA, GPIO_AF2, GPIO9);
  gpio_set_af(GPIOB, GPIO_AF2, GPIO0);

  /* In case of EMI (ADC setup already done) */
  gpio_port_config_lock(GPIOA, GPIO9);
  gpio_port_config_lock(GPIOB, GPIO0);
}

static void swcap_setup_timer(uint16_t period, uint16_t duty, uint16_t deadtime) {
  rcc_periph_clock_enable(RCC_TIM1);
  rcc_periph_reset_pulse(RST_TIM1);

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
  if (deadtime != 0) {
    timer_set_deadtime(TIM1, deadtime);
  }

  timer_set_break_lock(TIM1, TIM_BDTR_LOCK_OFF);
  timer_disable_break(TIM1);

  timer_disable_oc_clear(TIM1, TIM_OC2);
  timer_enable_oc_preload(TIM1, TIM_OC2);
  timer_set_oc_mode(TIM1, TIM_OC2, TIM_OCM_PWM1);
  timer_set_oc_value(TIM1, TIM_OC2, duty);

  timer_enable_break_main_output(TIM1); /* Required for advanced timer to work */
  timer_enable_oc_output(TIM1, TIM_OC2);
  timer_enable_oc_output(TIM1, TIM_OC2N);
  timer_enable_counter(TIM1);
}

/* TIM1 as Switched Capacitor Clock Source */
/* Use channel 2 or 3 to obtain complementary output without clashing with MCO */
static void swcap_setup(uint32_t freq, uint8_t duty, uint16_t deadtime) {
  uint16_t period = rcc_apb2_frequency / freq - 1;
  uint16_t ocv = (((uint32_t)duty) * ((uint32_t)period)) / 100 + 1;
  /* Period / duty calculation is OK */

  /* GPIOA, AFIO should have been enabled by this point */
  swcap_setup_gpio();
  swcap_setup_timer(period, ocv, deadtime);
}

static void button_setup(void) {
  gpio_mode_setup(GPIO_BANK_BUT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GPIO_BUT);

  /* In case of EMI */
  gpio_port_config_lock(GPIO_BANK_BUT, GPIO_BUT);
}

static void button_wait(void) {
  uint32_t i = 0;

  while (!gpio_get(GPIO_BANK_BUT, GPIO_BUT)) {
    i ++;
    if (((i % 500000) == 1) || ((i % 500000) == 10000)) {
      gpio_toggle(GPIO_BANK_LED, GPIO_LED1 | GPIO_LED2);
    }
  }

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


static void delay_a_bit(void) {
  uint32_t i;

  for (i = 0; i < 100000; i ++) {
    asm("nop\n");
  }
}

#define DEAD_TIME 0 /* Controlled by hardware for L6491 */
#define F_MAX     1200000
#define F_MIN      800000
#define F_STEP      10000

#define CURR_LOW  (4 * ADC_OVERSAMPLE) /* 10mA * 0.33Ohm = 3.3mV, 3.3V / 2 ^ 12-bit = 0.81mV/digit, 3.3mV = 4 digits */

int main(void) {
  uint32_t freq = F_MAX;
  uint32_t adc, adc_prev = 0;

  /* Enable GPIOA and GPIOB */
  rcc_periph_clock_enable(RCC_GPIOA);
  rcc_periph_clock_enable(RCC_GPIOB);
  rcc_periph_clock_enable(RCC_GPIOC);

  led_setup();

  gpio_set(GPIO_BANK_LED, GPIO_LED1);
  rcc_clock_setup_in_hsi_out_48mhz();
  setup_adc();
  button_setup();
  swcap_setup(freq, 50, DEAD_TIME);
  gpio_set(GPIO_BANK_LED, GPIO_LED2);

  while (true) {
    delay_a_bit(); /* Wait for current to settle */
    adc = read_adc();
    if ((adc_prev == 0) || ((adc < adc_prev) && (adc > CURR_LOW))) {
      freq -= F_STEP;
      swcap_setup(freq, 50, DEAD_TIME);
    } else {
      /* Stop switching for a while so FETs recover */
      timer_disable_counter(TIM1);
      freq += F_STEP;
      delay_a_bit();
      swcap_setup(freq, 50, DEAD_TIME);
      timer_enable_counter(TIM1);
    }
    adc_prev = adc;
  }

  halt_normal();
  return 0;
}
