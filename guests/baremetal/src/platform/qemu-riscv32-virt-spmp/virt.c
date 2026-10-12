#include <plat.h>
#include <8250_uart.h>
#include <stdio.h>

#define VIRT_UART_BAUDRATE      115200
#define VIRT_UART_SHIFTREG_ADDR 1843200

void uart_init(void)
{
    uart8250_init(PLAT_UART_ADDR, VIRT_UART_SHIFTREG_ADDR,
                  VIRT_UART_BAUDRATE, 0, 1);
}

void uart_putc(char c)
{
    uart8250_putc(c);
}

char uart_getchar(void)
{
    return uart8250_getc();
}

void uart_enable_rxirq(void)
{
    uart8250_enable_rx_int();
}

void uart_clear_rxirq(void)
{
    uart8250_interrupt_handler();
}

void plat_led_init(void) { }

void plat_led_on(void)  { printf("[led] ON\n"); }
void plat_led_off(void) { printf("[led] off\n"); }
