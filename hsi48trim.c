/* HSI48 Trimming */
/* HSI48 has 3% factory error and trimming step is 67kHz (1.4kHz @ 1MHz) */
/* HSI48 has 1% factory error and trimming step is 40kHz (5kHz @ 1MHz) */

#include <libopencm3/cm3/assert.h>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/crs.h>

#include "hsi48trim.h"


void hsi48trim_setup(void) {
  rcc_periph_clock_enable(RCC_CRS);
  rcc_periph_reset_pulse(RST_CRS);

  hsi48trim_set(0);
}

void hsi48trim_set(uint16_t trim) {
  uint32_t reg = CRS_CR & (~CRS_CR_TRIM);

  cm3_assert(trim < HSI48TRIM_MAX);
  reg |= trim << CRS_CR_TRIM_SHIFT;
  CRS_CR = reg;
}

uint16_t hsi48trim_get(void) {
  return (CRS_CR & CRS_CR_TRIM) >> CRS_CR_TRIM_SHIFT;
}

uint16_t hsi48trim_goto_next(void) {
  uint16_t trim = hsi48trim_get();

  trim ++;
  if (trim >= HSI48TRIM_MAX) {
    trim = 0;
  }

  hsi48trim_set(trim);
  return trim;
}
