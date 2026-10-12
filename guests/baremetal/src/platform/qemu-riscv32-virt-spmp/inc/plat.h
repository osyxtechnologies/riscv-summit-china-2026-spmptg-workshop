#ifndef PLAT_H
#define PLAT_H

#ifndef PLAT_MEM_BASE
#define PLAT_MEM_BASE 0x88200000
#endif
#define PLAT_MEM_SIZE 0x01000000

#define PLAT_TIMER_FREQ (10000000ull) /* 10 MHz (QEMU virt default) */

/* Defaults: UART1 at 0x6000000, APLIC source 12 (used by milestone 02 for the
 * baremetal guest). Override via -DPLAT_UART_ADDR/-DUART_IRQ_ID at build time
 * to retarget UART0 (e.g. milestone 00 single baremetal scenarios). */
#ifndef PLAT_UART_ADDR
#define PLAT_UART_ADDR (0x6000000)
#endif
#ifndef UART_IRQ_ID
#define UART_IRQ_ID    (12)
#endif

#define PLAT_APLIC_CTL_BASE_ADDR  (0xd000000)
#define PLAT_APLIC_MAX_INTERRUPTS (64)
#define PLAT_IMSIC_IF_BASE_ADDR   (0x28000000)
#define PLAT_IMSIC_MAX_INTERRUPTS (255)

#define CPU_EXT_SSTC 1

/* IPC shmem interrupt - APLIC source Bao injects on peer notification.
 * Only meaningful when SHMEM_BASE is passed at build time. */
#define SHMEM_IRQ_ID (3)

#ifndef __ASSEMBLER__
void plat_led_init(void);
void plat_led_on(void);
void plat_led_off(void);
#endif

#endif
