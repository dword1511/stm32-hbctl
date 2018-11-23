#ifndef __HBCTL_BSP_H__
#define __HBCTL_BSP_H__

#include <libopencm3/stm32/gpio.h>


#define GPIO_PORT_CS  GPIOA
#define GPIO_PIN_CSP  GPIO5
#define CS_CH_P       5
#define GPIO_PIN_CSN  GPIO4
#define CS_CH_N       4

#define CS_R_MOHM     100
#define CS_VREF_MV    3300 /* For better results, user VREFINT */

/* Auxiliary ADC port, unused for now */
#define GPIO_PORT_VIN GPIOB
#define GPIO_PIN_VIN  GPIO1
#define CS_CH_VIN     9


#define GPIO_PORT_BUT GPIOB
#define GPIO_PIN_BUT  GPIO8
#define GPIO_PORT_LED GPIOA
#define GPIO_PIN_LED1 GPIO6
#define GPIO_PIN_LED2 GPIO7
#define HCI_LED_ALIVE GPIO_PIN_LED1
#define HCI_LED_RUN   GPIO_PIN_LED2
#define HCI_LED_ALL   (GPIO_PIN_LED1 | GPIO_PIN_LED2)


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


#endif /* __HBCTL_BSP_H__ */
