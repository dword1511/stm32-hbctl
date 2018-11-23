/* Bridge current sense */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/adc.h>
#include <libopencm3/stm32/gpio.h>

#include "bsp.h"
#include "cs.h"


#define ADC_DIGITS    (1 << 12)
#define ADC_CLOCK     12000000
#define ADC_CONV_CYCS 241   /* 239.5 + 1.5 */


static uint32_t oversample = 0;


void cs_setup(uint32_t target_duration_ms) {
  rcc_periph_clock_enable(RCC_ADC1);
  rcc_periph_reset_pulse(RST_ADC1);

  gpio_mode_setup(GPIO_PORT_CS, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_CSP | GPIO_PIN_CSN);

  adc_power_off(ADC1);
  adc_set_clk_source(ADC1, ADC_CLKSOURCE_PCLK_DIV4); /* Set ADC clock to 12 MHz, see also ADC_CLOCK */
  adc_calibrate(ADC1);
  adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5); /* (239.5 + 1.5 ~= 50 ksps), see also ADC_CONV_CYCS */
  adc_set_resolution(ADC1, ADC_RESOLUTION_12BIT); /* See also ADC_DIGITS */
  adc_set_right_aligned(ADC1);
  adc_power_on(ADC1);

  adc_set_single_conversion_mode(ADC1);

  /* Each measurement samples 2 channels */
  oversample = (((ADC_CLOCK / 1000 / 2) * target_duration_ms) / ADC_CONV_CYCS);
}

uint32_t cs_measure_ma(void) {
  unsigned i;
  uint32_t acc_csp = 0;
  uint32_t acc_csn = 0;
  uint32_t acc_cs;
  uint32_t ma;
  /* constants but prefer to store in SRAM */
  uint8_t ch_csp = CS_CH_P;
  uint8_t ch_csn = CS_CH_N;

  /* Oversampling act as FIR LPF */
  for (i = 0; i < oversample; i ++) {
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
  ma = (acc_cs * CS_VREF_MV * 1000) / (ADC_DIGITS * CS_R_MOHM * oversample);

  return ma;
}
