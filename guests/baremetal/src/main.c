/** 
 * Bao, a Lightweight Static Partitioning Hypervisor 
 *
 * Copyright (c) Bao Project (www.bao-project.org), 2019-
 *
 * Authors:
 *      Jose Martins <jose.martins@bao-project.org>
 *      Sandro Pinto <sandro.pinto@bao-project.org>
 *
 * Bao is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License version 2 as published by the Free
 * Software Foundation, with a special exception exempting guest code from such
 * license. See the COPYING file in the top-level directory for details. 
 *
 */

#include <core.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cpu.h>
#include <wfi.h>
#include <plat.h>
#include <irq.h>
#include <uart.h>
#include <timer.h>

#define TIMER_INTERVAL (TIME_S(1))

#ifdef SHMEM_BASE

#define SHMEM_CHANNEL_SIZE 0x2000

static char* const baremetal_msg = (char*)SHMEM_BASE;
static char* const zephyr_msg    = (char*)(SHMEM_BASE + SHMEM_CHANNEL_SIZE);

typedef enum { LED_OFF, LED_ON, LED_BLINKING } led_mode_t;
static volatile led_mode_t led_mode = LED_OFF;
static bool blink_state = false;

static void led_cmd_run(const char *cmd)
{
    if (strcmp(cmd, "led on") == 0) {
        led_mode = LED_ON;
        plat_led_on();
    } else if (strcmp(cmd, "led off") == 0) {
        led_mode = LED_OFF;
        plat_led_off();
    } else if (strcmp(cmd, "led blink") == 0) {
        blink_state = false;
        led_mode = LED_BLINKING;
    } else {
        printf("[shmem rx] from zephyr: \"%s\"\n", cmd);
    }
}

static void shmem_handler(unsigned int id)
{
    /* Null-terminate and strip trailing newline before comparing. */
    zephyr_msg[SHMEM_CHANNEL_SIZE - 1] = '\0';
    char *end = strchr(zephyr_msg, '\n');
    if (end != NULL)
        *end = '\0';
    led_cmd_run(zephyr_msg);
}

static void shmem_init(void)
{
    memset(baremetal_msg, 0, SHMEM_CHANNEL_SIZE);
    memset(zephyr_msg, 0, SHMEM_CHANNEL_SIZE);
    irq_set_handler(SHMEM_IRQ_ID, shmem_handler);
    irq_set_prio(SHMEM_IRQ_ID, IRQ_MAX_PRIO);
    irq_enable(SHMEM_IRQ_ID);
}

#endif /* SHMEM_BASE */

static void uart_rx_handler(unsigned int id)
{
    /* uart_getchar() reads RBR, which also clears the 8250 RX-data-ready
     * condition, so no separate uart_clear_rxirq() call is needed. */
    uint8_t c = (uint8_t)uart_getchar();

    if (c >= 0x20 && c < 0x7f) {
        printf("[uart rx] '%c'    0x%02x  %3u\n", c, c, c);
    } else {
        printf("[uart rx] '\\x%02x' 0x%02x  %3u\n", c, c, c);
    }
}

static void timer_handler(unsigned int id)
{
    static unsigned int uptime = 0;

    timer_set(TIMER_INTERVAL);
    printf("[heartbeat] uptime=%us\n", ++uptime);

#ifdef SHMEM_BASE
    if (led_mode == LED_BLINKING) {
        blink_state = !blink_state;
        if (blink_state)
            plat_led_on();
        else
            plat_led_off();
    }
#endif
}

void main(void)
{
    printf("Bao bare-metal guest\n");

#ifdef SHMEM_BASE
    plat_led_init();
    shmem_init();
#endif

    irq_set_handler(UART_IRQ_ID, uart_rx_handler);
    irq_set_handler(TIMER_IRQ_ID, timer_handler);

    uart_enable_rxirq();

    timer_set(TIMER_INTERVAL);
    irq_enable(TIMER_IRQ_ID);
    irq_set_prio(TIMER_IRQ_ID, TIMER_IRQ_PRIO);

    irq_enable(UART_IRQ_ID);
    irq_set_prio(UART_IRQ_ID, UART_IRQ_PRIO);

    timer_enable();

    while (1) {
        wfi();
    }
}
