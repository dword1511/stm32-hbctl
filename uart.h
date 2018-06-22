#ifndef __UART_XPRINTF_H__
#define __UART_XPRINTF_H__

#include <stdarg.h>
#include <stdlib.h>

extern void     uart_setup(uint32_t baud);
extern void     uart_putc(char c);
extern void     uart_puts(const char* str);
extern void     uart_printf(const char*  fmt, ...);
extern void     uart_vprintf(const char* fmt, va_list arp);
extern void     uart_hline(void);
extern void     uart_dump(uint8_t *buf, size_t len);

#endif /* __UART_XPRINTF_H__ */
