/* Adapted from xprintf: http://elm-chan.org/fsw/strf/xprintf.html */

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>

#include "uart.h"


#define GPIO_BANK_UART    GPIOA
#define GPIO_UART_TX      GPIO9
#define GPIO_UART_RX      GPIO10
#define GPIO_AF_UART      GPIO_AF1
#define UART              USART1
#define RCC_UART          RCC_USART1


/* Standard xprintf functions */

void uart_putc(char c) {
  usart_send_blocking(UART, c);
}

void uart_puts(const char* str) {
  while (*str) {
    uart_putc(*str++);
  }
}

void uart_vprintf(const char* fmt, va_list arp) {
  unsigned int r, i, j, w, f;
  unsigned long v;
  char s[16], c, d, *p;

  while (true) {
    c = *fmt++;

    if (!c) {
      break;
    }

    if (c != '%') {
        uart_putc(c);
        continue;
    }

    f = 0;
    c = *fmt++;
    if (c == '0') {
      f = 1; c = *fmt++;
    } else {
      if (c == '-') {
        f = 2; c = *fmt++;
      }
    }
    for (w = 0; c >= '0' && c <= '9'; c = *fmt++) {
      w = w * 10 + c - '0';
    }
    if (c == 'l' || c == 'L') {
      f |= 4; c = *fmt++;
    }
    if (!c) {
      break;
    }
    d = c;
    if (d >= 'a') {
      d -= 0x20;
    }

    switch (d) {
      case 'S':
        p = va_arg(arp, char*);
        for (j = 0; p[j]; j++) ;
        while (!(f & 2) && j++ < w) {
          uart_putc(' ');
        }
        uart_puts(p);
        while (j++ < w) {
          uart_putc(' ');
        }
        continue;
      case 'C':
        uart_putc((char)va_arg(arp, int));
        continue;
      case 'B':
        r = 2;
        break;
      case 'O':
        r = 8;
        break;
      case 'D':
      case 'U':
        r = 10;
        break;
      case 'X':
        r = 16;
        break;
      default:
        /* Unknown type (passthrough) */
        uart_putc(c);
        continue;
    }

    v = (f & 4) ? va_arg(arp, long) : ((d == 'D') ? (long)va_arg(arp, int) : (long)va_arg(arp, unsigned int));
    if (d == 'D' && (v & 0x80000000)) {
      v = 0 - v;
      f |= 8;
    }
    i = 0;
    do {
      d = (char)(v % r);
      v /= r;
      if (d > 9) {
        d += (c == 'x') ? 0x27 : 0x07;
      }
      s[i++] = d + '0';
    } while (v && i < sizeof(s));
    if (f & 8) {
      s[i++] = '-';
    }
    j = i;
    d = (f & 1) ? '0' : ' ';
    while (!(f & 2) && j++ < w) {
      uart_putc(d);
    }
    do {
      uart_putc(s[--i]);
    } while(i);
    while (j++ < w) {
      uart_putc(' ');
    }
  }
}

void uart_printf(const char*  fmt, ...) {
  va_list arp;
  va_start(arp, fmt);
  uart_vprintf(fmt, arp);
  va_end(arp);
}

/* Application specific */

static void uart_setup_gpio(void) {
  //gpio_set_output_options(GPIO_BANK_UART, GPIO_OTYPE_OD, GPIO_OSPEED_2MHZ, GPIO_UART_TX);
  gpio_set_output_options(GPIO_BANK_UART, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_UART_TX); /* PP for ST-Link */
  gpio_set_af(GPIO_BANK_UART, GPIO_AF_UART, GPIO_UART_TX);
  gpio_mode_setup(GPIO_BANK_UART, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO_UART_TX);
}

void uart_setup(uint32_t baud) {
  rcc_periph_clock_enable(RCC_UART);

  /* GPIOA, AFIO should have been enabled by this point */
  uart_setup_gpio();

  usart_set_baudrate(UART, baud);
  usart_set_databits(UART, 8);
  usart_set_parity(UART, USART_PARITY_NONE);
  usart_set_stopbits(UART, USART_STOPBITS_1);
  usart_set_mode(UART, USART_MODE_TX);
  usart_set_flow_control(UART, USART_FLOWCONTROL_NONE);

  usart_enable(UART);
}

void uart_hline(void) {
  uart_printf("====================\r\n");
}

void uart_dump(uint8_t *buf, size_t len) {
  size_t i;

  uart_printf("\r\nBuffer @ %08x, size %u\r\n", (int)buf, len);

  for (i = 0; i < len - 15; i += 16) {
    uart_printf("%08x    %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
      i,
      buf[i], buf[i + 1], buf[i + 2], buf[i + 3], buf[i + 4], buf[i + 5], buf[i + 6], buf[i + 7],
      buf[i + 8], buf[i + 9], buf[i + 10], buf[i + 11], buf[i + 12], buf[i + 13], buf[i + 14], buf[i + 15]
    );
  }

  if (i < len) {
    uart_printf("%08x    ", i);
    for (; i < len - 3; i += 4) {
      uart_printf("%02x %02x %02x %02x  ", buf[i], buf[i + 1], buf[i + 2], buf[i + 3]);
    }
    for (; i < len; i ++) {
      uart_printf("%02x", buf[i]);
    }
  }
}
