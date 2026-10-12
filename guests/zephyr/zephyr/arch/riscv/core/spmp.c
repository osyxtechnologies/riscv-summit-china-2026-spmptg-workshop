/*
 * Copyright (c) 2022 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * - RISC-V S-mode Physical Memory Protection (SPMP) provides per-hart S-mode 
 * control registers to allow physical memory access privileges (read, write, 
 * execute) to be specified for each physical memory region. So we use it 
 * for memory protection when kernel is on S-mode. 
 *
 * The SPMP is comprized of a number of entries or slots. This number depends
 * on the hardware design. For each slot there is an address register and
 * a configuration register. While each address register is matched to an
 * actual CSR register, configuration registers are small and therefore
 * several of them are bundled in a few additional CSR registers.
 *
 * SPMP slot configurations are updated in memory to avoid read-modify-write
 * cycles on corresponding CSR registers. Relevant CSR registers are always
 * written in batch from their shadow copy in RAM for better efficiency.
 *
 * In the stackguard case we keep an s-mode copy for each thread. Each user
 * mode threads also has a u-mode copy. This makes faster context switching
 * as precomputed content just have to be written to actual registers with
 * no additional processing.
 *
 * Thread-specific s-mode and u-mode SPMP entries start from the SPMP slot
 * indicated by global_spmp_end_index. Lower slots are used by global entries
 * which are never modified.
 */

#include <kernel_internal.h>
#include <spmp.h>
#include <zephyr/arch/riscv/csr.h>
#include <zephyr/kernel.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/arch/arch_interface.h>
#include <zephyr/arch/riscv/arch.h>

#define LOG_LEVEL CONFIG_MPU_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mpu);

#define CSR_SPMPEN  0x183
#define CSR_SPMPENH 0x193

static unsigned int spmp_slots;
static unsigned int spmp_global_entries;
/* Switch bits set during global init — vsiselect writes reset vspmpen on some
 * hardware, so commit must restore global bits alongside per-thread bits. */
static unsigned long global_spmp_switch[2];

/* WORKSHOP: set (to the needed slot count) when even the kernel's global
 * SPMP entries do not fit in the available slots.  z_riscv_spmp_init() runs before
 * the console is up, so it records this here and spmp_check_global_slots() (PRE_KERNEL_2)
 * reports it where printk() actually works. */
static unsigned int spmp_global_deficit;
static void spmp_report_insufficient(const char *what, unsigned int needed);

static unsigned int probe_spmp_slots(void)
{
	unsigned long saved = csr_read(CSR_SPMPEN);
	unsigned long val;
	unsigned int count;

	csr_write(CSR_SPMPEN, ~0UL);
	val = csr_read(CSR_SPMPEN);
	csr_write(CSR_SPMPEN, saved);

	count = (val == ~0UL) ? (8U * sizeof(unsigned long))
			      : (unsigned int)__builtin_ctzl(~val);

#ifndef CONFIG_64BIT
	if (count == 32U) {
		unsigned long saved_h = csr_read(CSR_SPMPENH);
		csr_write(CSR_SPMPENH, ~0UL);
		val = csr_read(CSR_SPMPENH);
		csr_write(CSR_SPMPENH, saved_h);
		count += (val == ~0UL) ? 32U : (unsigned int)__builtin_ctzl(~val);
	}
#endif

	return MIN(count, CONFIG_SPMP_SLOTS);
}

static int spmp_check_global_slots(void)
{
	/* If global init found too few vsPMP slots, report it now (printk works
	 * here, unlike in the very-early z_riscv_spmp_init()) and halt. */
	if (spmp_global_deficit != 0U) {
		spmp_report_insufficient("kernel global entries", spmp_global_deficit);
	}

	return 0;
}
SYS_INIT(spmp_check_global_slots, PRE_KERNEL_2, 0);


#ifdef CONFIG_64BIT
#define PR_ADDR "0x%016lx"
#else
#define PR_ADDR "0x%08lx"
#endif

#define SPMP_TOR_SUPPORTED !IS_ENABLED(CONFIG_SPMP_NO_TOR)
#define SPMP_NA4_SUPPORTED !IS_ENABLED(CONFIG_SPMP_NO_NA4)
#define SPMP_NAPOT_SUPPORTED !IS_ENABLED(CONFIG_SPMP_NO_NAPOT)

#define SPMPCFG_STRIDE sizeof(unsigned long)

#define SPMP_ADDR(addr) ((addr) >> 2)
#define NAPOT_RANGE(size) (((size)-1) >> 1)
#define SPMP_ADDR_NAPOT(addr, size) SPMP_ADDR(addr | NAPOT_RANGE(size))

#define SPMP_NONE 0

/**
 * @brief Set SPMP shadow register values in memory
 *
 * Register content is built using this function which selects the most
 * appropriate address matching mode automatically. Note that the special
 * case start=0 size=0 is valid and means the whole address range.
 *
 * @param index_p Location of the current SPMP slot index to use. This index
 *                will be updated according to the number of slots used.
 * @param perm SPMP permission flags
 * @param start Start address of the memory area to cover
 * @param size Size of the memory area to cover
 * @param spmp_addr Array of spmpaddr values (starting at entry 0).
 * @param spmp_cfg Array of spmpcfg values (starting at entry 0).
 * @param index_limit Index value representing the size of the provided arrays.
 * @return true on success, false when out of free SPMP slots.
 */
static bool set_spmp_entry(unsigned int *index_p, uint16_t perm,
						uintptr_t start, size_t size,
						unsigned long *spmp_addr, uint16_t *spmp_cfg,
						unsigned long *spmp_switch, unsigned int index_limit) {
	unsigned int index = *index_p;
	bool ok = true;

	__ASSERT((start & 0x3) == 0, "misaligned start address");
	__ASSERT((size & 0x3) == 0, "misaligned size");

	if (index >= index_limit) {
		LOG_ERR("out of SPMP slots");
		ok = false;
	} else if (SPMP_TOR_SUPPORTED &&
			((index == 0 && start == 0) ||
				(index != 0 && spmp_addr[index - 1] == SPMP_ADDR(start)))) {
		/* We can use TOR using only one additional slot */
		spmp_addr[index] = SPMP_ADDR(start + size);
		spmp_cfg[index] = perm | SPMP_TOR;
		spmp_switch[index/(RV_REGSIZE*8)] |= (1UL << ((index%(RV_REGSIZE*8))));
		index += 1;
	} else if (SPMP_NA4_SUPPORTED && size == 4) {
		spmp_addr[index] = SPMP_ADDR(start);
		spmp_cfg[index] = perm | SPMP_NA4;
		spmp_switch[index/(RV_REGSIZE*8)] |= (1UL << ((index%(RV_REGSIZE*8))));
		index += 1;
	} else if (SPMP_NAPOT_SUPPORTED &&
			((size & (size - 1)) == 0) /* power of 2 */ &&
			((start & (size - 1)) == 0) /* naturally aligned */ &&
			(SPMP_NA4_SUPPORTED || (size != 4))) {
		spmp_addr[index] = SPMP_ADDR_NAPOT(start, size);
		spmp_cfg[index] = perm | SPMP_NAPOT;
		spmp_switch[index/(RV_REGSIZE*8)] |= (1UL << ((index%(RV_REGSIZE*8))));
		index += 1;
	} else if (SPMP_TOR_SUPPORTED && index + 1 >= index_limit) {
		LOG_ERR("out of SPMP slots");
		ok = false;
	} else if (SPMP_TOR_SUPPORTED) {
		spmp_addr[index] = SPMP_ADDR(start);
		spmp_cfg[index] = 0;
		index += 1;
		spmp_addr[index] = SPMP_ADDR(start + size);
		spmp_cfg[index] = perm | SPMP_TOR;
		spmp_switch[index/(RV_REGSIZE*8)] |= (1UL << ((index%(RV_REGSIZE*8))));
		index += 1;
	} else {
		LOG_ERR("inappropriate SPMP range (start=%#lx size=%#zx)", start, size);
		ok = false;
	}

	*index_p = index;
	return ok;
}

/**
 * @brief Write a range of SPMP entries to corresponding SPMP registers
 *
 * SPMP registers are accessed with the csr instruction which only takes an
 * immediate value as the actual register. This is performed more efficiently
 * in assembly code (spmp.S) than what is possible with C code.
 *
 * Requirement: start < end && end <= CONFIG_SPMP_SLOTS
 *
 * @param start Start of the SPMP range to be written
 * @param end End (exclusive) of the SPMP range to be written
 * @param clear_trailing_entries True if trailing entries must be turned off
 * @param spmp_addr Array of spmpaddr values (starting at entry 0).
 * @param spmp_cfg Array of spmpcfg values (starting at entry 0).
 * @param spmp_switch spmpswitch value to be written
 */
extern void z_riscv_write_spmp_entries(unsigned int start, unsigned int end,
									const uint16_t *spmp_cfg,
									const unsigned long *spmp_addr,
									const unsigned long *spmp_switch);

/**
 * @brief Write a range of SPMP entries to corresponding SPMP registers
 *
 * This performs some sanity checks before calling z_riscv_write_spmp_entries().
 *
 * @param start Start of the SPMP range to be written
 * @param end End (exclusive) of the SPMP range to be written
 * @param clear_trailing_entries True if trailing entries must be turned off
 * @param spmp_addr Array of spmpaddr values (starting at entry 0).
 * @param spmp_cfg Array of spmpcfg values (starting at entry 0).
 * @param index_limit Index value representing the size of the provided arrays.
 */
static void write_spmp_entries(unsigned int start, unsigned int end,
							bool clear_trailing_entries,
							unsigned long *spmp_addr, uint16_t *spmp_cfg,
							unsigned long *spmp_switch, unsigned int index_limit) {
	__ASSERT(start < end && end <= index_limit &&
				index_limit <= CONFIG_SPMP_SLOTS,
			"bad SPMP range (start=%u end=%u)", start, end);

	/* Be extra paranoid in case assertions are disabled */
	if (start >= end || end > index_limit) {
		k_panic();
	}

	if (clear_trailing_entries) {
		/*
		* There are many config entries per spmpcfg register.
		* Make sure to clear trailing garbage in the last
		* register to be written if any. Remaining registers
		* will be cleared in z_riscv_write_spmp_entries().
		*/
		unsigned int index;

		for (index = end; index % SPMPCFG_STRIDE != 0; index++) {
			spmp_cfg[index] = 0;
			*spmp_switch &= ~(1UL << index);
		}
	}

#ifdef CONFIG_QEMU_TARGET
	/*
	* A QEMU bug may create bad transient SPMP representations causing
	* false access faults to be reported. Work around it by setting
	* spmp registers to zero from the update start point to the end
	* before updating them with new values.
	* The QEMU fix is here with more details about this bug:
	* https://lists.gnu.org/archive/html/qemu-devel/2022-06/msg02800.html
	*/
	static const unsigned long spmp_zero[CONFIG_SPMP_SLOTS] = {
		0,
	};

	z_riscv_write_spmp_entries(start, CONFIG_SPMP_SLOTS,
							spmp_zero, spmp_zero, spmp_zero);
#endif

	z_riscv_write_spmp_entries(start, end,
							spmp_cfg, spmp_addr, spmp_switch);
}

/**
 * @brief Abstract the last 4 arguments to set_spmp_entry() and
 *        write_spmp_entries for s-mode.
 */
#define SPMP_S_MODE(thread)                 \
	thread->arch.s_mode_spmpaddr_regs,      \
		thread->arch.s_mode_spmpcfg_regs,   \
		thread->arch.s_mode_spmpswitch_reg, \
		ARRAY_SIZE(thread->arch.s_mode_spmpaddr_regs)

/**
 * @brief Abstract the last 4 arguments to set_spmp_entry() and
 *        write_spmp_entries for u-mode.
 */
#define SPMP_U_MODE(thread)                  \
	thread->arch.u_mode_pmpaddr_regs,        \
		thread->arch.u_mode_pmpcfg_regs,     \
		thread->arch.u_mode_spmpswitch_regs, \
		ARRAY_SIZE(thread->arch.u_mode_pmpaddr_regs)

/*
* This is used to seed thread SPMP copies with global s-mode cfg entries
* sharing the same cfg register. 
*/
static uint16_t global_spmp_cfg[1];
static unsigned long global_spmp_last_addr;

/* End of global SPMP entry range */
static unsigned int global_spmp_end_index;

/*
 * WORKSHOP capacity check.  `needed` is the number of SPMP slots Zephyr
 * wants to occupy (slots 0..needed-1).  OpenSBI or Bao reserves the
 * topmost slot (spmp_slots-1) for its S-mode allow-all default, so Zephyr
 * is only safe while needed < spmp_slots.
 *
 * spmp_report_insufficient() prints and halts; only call it once printk() is
 * usable (PRE_KERNEL_2 onwards, or from a running thread).
 */
static void spmp_report_insufficient(const char *what, unsigned int needed)
{
	printk("\n*** ZEPHYR ERROR: insufficient SPMP entries (%s) ***\n"
	       "  Zephyr needs %u SPMP slots but the platform provides\n"
	       "  only %u (the top slot is reserved for the S-mode default).\n"
	       "  Check the SPMP entry allocation and QEMU CPU configuration.\n",
	       what, needed, spmp_slots);

	for (;;) {
		/* halt: do not let the guest run with broken memory protection */
	}
}

/* Late (post-console) capacity check: safe to report immediately. */
static void spmp_require_slots(const char *what, unsigned int needed)
{
	if (needed >= spmp_slots) {
		spmp_report_insufficient(what, needed);
	}
}

/**
 * @Brief Initialize the SPMP with global entries on each CPU
 */
void z_riscv_spmp_init(void) {
	spmp_slots = probe_spmp_slots();

	unsigned long spmp_addr[CONFIG_SPMP_SLOTS];
	uint16_t spmp_cfg[CONFIG_SPMP_SLOTS];
	unsigned long spmp_switch[2] = {0, 0};
	unsigned int index = 0;

	/* ROM: Shared-Region (SHARED+U, no L) — both VS and VU get R+X (§19.1.4).
	 * No L: entry locks AFTER spmpen bit is activated (write cfg, run
	 * spmpswitch_store, then lock — here we skip locking for simplicity).
	 * VS-mode accesses general RAM via the allow-all (S-mode-only entry). */
	set_spmp_entry(&index, SPMP_SHARED | SPMP_U | SPMP_R | SPMP_X,
				(uintptr_t)__rom_region_start,
				(size_t)__rom_region_size,
				spmp_addr, spmp_cfg, spmp_switch, ARRAY_SIZE(spmp_addr));

#ifdef CONFIG_NULL_POINTER_EXCEPTION_DETECTION_SPMP
	/*
	* Use a SPMP slot to make region (starting at address 0x0) inaccessible
	* for detecting null pointer dereferencing
	*/
	set_spmp_entry(&index, SPMP_NONE,
				0,
				CONFIG_NULL_POINTER_EXCEPTION_REGION_SIZE,
				spmp_addr, spmp_cfg, spmp_switch, ARRAY_SIZE(spmp_addr));
#endif

#ifdef CONFIG_SPMP_STACK_GUARD
	/*
	* Set the stack guard for this CPU's IRQ stack by making the bottom
	* addresses inaccessible.
	*/
	set_spmp_entry(&index, SPMP_NONE,
				(uintptr_t)z_interrupt_stacks[_current_cpu->id],
				Z_RISCV_STACK_GUARD_SIZE,
				spmp_addr, spmp_cfg, spmp_switch, ARRAY_SIZE(spmp_addr));
#endif

	/* Capacity check BEFORE touching hardware.  When too few entries are available, the
	 * slots above spmp_slots do not exist; do NOT write to them.  This runs
	 * before the console is up, so just record the deficit and let
	 * spmp_check_global_slots() (PRE_KERNEL_2) report it where printk() works.  The
	 * kernel keeps running on the S-mode allow-all (top slot, untouched)
	 * until that report halts it -- no user code runs in between. */
	if (index >= spmp_slots) {
		spmp_global_deficit = index;
		spmp_global_entries = index;
		global_spmp_end_index = index;
		return;
	}

	/* spmpen bits must be activated BEFORE locking (§19.2: locked entry's
	 * spmpen bit becomes read-only at its current value). */
	write_spmp_entries(0, index, true, spmp_addr, spmp_cfg, spmp_switch, ARRAY_SIZE(spmp_addr));

	/* Now lock global entries: cfg/addr become read-only, spmpen bit stays
	 * at 1 (just activated above) so entry remains permanently active.
	 * Write addr BEFORE cfg: on some hardware (e.g. CVA6) writing cfg
	 * (vsireg2) clears addr (vsireg), so addr must be re-written first. */
	for (unsigned int i = 0; i < index; i++) {
		uint16_t cfg = spmp_cfg[i];

		if (((cfg >> 3) & 0x3) != 0) {  /* A != OFF — active entry */
			cfg |= SPMP_L;
			csr_write(0x150, 0x100 + i);    /* vsiselect = entry i */
			csr_write(0x151, spmp_addr[i]); /* vsireg    = addr (must come first) */
			csr_write(0x152, cfg);          /* vsireg2   = cfg | L */
		}
	}

#ifdef CONFIG_SMP
#ifdef CONFIG_SPMP_STACK_GUARD
	/*
	* The IRQ stack guard area is different for each CPU.
	* Make sure TOR entry sharing won't be attempted with it by
	* remembering a bogus address for those entries.
	*/
	spmp_addr[index - 1] = -1L;
#endif

	/* Make sure secondary CPUs produced the same values */
	if (global_spmp_end_index != 0) {
		__ASSERT(global_spmp_end_index == index, "");
		__ASSERT(global_spmp_cfg[0] == spmp_cfg[0], "");
		__ASSERT(global_spmp_last_addr == spmp_addr[index - 1], "");
	}
#endif

	global_spmp_cfg[0] = spmp_cfg[0];
	global_spmp_last_addr = spmp_addr[index - 1];
	global_spmp_end_index = index;
	spmp_global_entries = index;
	global_spmp_switch[0] = spmp_switch[0];
	global_spmp_switch[1] = spmp_switch[1];
}

/**
 * @Brief Initialize the per-thread SPMP register copy with global values.
 */
static inline unsigned int z_riscv_spmp_thread_init(unsigned long *spmp_addr,
												uint16_t *spmp_cfg,
												unsigned long *spmp_switch,
												unsigned int index_limit) {
	ARG_UNUSED(index_limit);
	ARG_UNUSED(spmp_switch);

	/*
	* Retrieve spmpcfg0 partial content from global entries.
	*/
	spmp_cfg[0] = global_spmp_cfg[0];

	/*
	* Retrieve the spmpaddr value matching the last global SPMP slot.
	* This is so that set_spmp_entry() can safely attempt TOR with it.
	*/
	spmp_addr[global_spmp_end_index - 1] = global_spmp_last_addr;

	return global_spmp_end_index;
}

#ifdef CONFIG_SPMP_STACK_GUARD

/**
 * @brief Prepare the SPMP stackguard content for given thread.
 *
 * This is called once during new thread creation.
 */
void z_riscv_spmp_stackguard_prepare(struct k_thread *thread) {
	unsigned int index = z_riscv_spmp_thread_init(SPMP_S_MODE(thread));
	uintptr_t stack_bottom;

	/* make the bottom addresses of our stack inaccessible */
	stack_bottom = thread->stack_info.start - K_KERNEL_STACK_RESERVED;
#ifdef CONFIG_USERSPACE
	if (thread->arch.priv_stack_start != 0) {
		stack_bottom = thread->arch.priv_stack_start;
	} else if (z_stack_is_user_capable(thread->stack_obj)) {
		stack_bottom = thread->stack_info.start - K_THREAD_STACK_RESERVED;
	}
#endif
	set_spmp_entry(&index, SPMP_NONE,
				stack_bottom, Z_RISCV_STACK_GUARD_SIZE,
				SPMP_S_MODE(thread));

	/* remember how many entries we use */
	thread->arch.s_mode_spmp_end_index = index;
}

/**
 * @brief Write SPMP stackguard content to actual SPMP registers
 *
 * This is called on every context switch.
 */
void z_riscv_spmp_stackguard_enable(struct k_thread *thread) {
	LOG_DBG("spmp_stackguard_enable for thread %p", thread);

	/* Write our m-mode MPP entries */
	write_spmp_entries(global_spmp_end_index, thread->arch.s_mode_spmp_end_index,
					false /* no need to clear to the end */,
					SPMP_S_MODE(thread));
}

#endif /* CONFIG_SPMP_STACK_GUARD */

#ifdef CONFIG_USERSPACE

/**
 * @brief Initialize the usermode portion of the SPMP configuration.
 *
 * This is called once during new thread creation.
 */
void z_riscv_spmp_usermode_init(struct k_thread *thread) {
	/* Only indicate that the u-mode SPMP is not prepared yet */
	thread->arch.u_mode_pmp_end_index = 0;
}

/**
 * @brief Prepare the u-mode SPMP content for given thread.
 *
 * This is called once before making the transition to usermode.
 */
void z_riscv_spmp_usermode_prepare(struct k_thread *thread) {
	unsigned int index = z_riscv_spmp_thread_init(SPMP_U_MODE(thread));

	set_spmp_entry(&index, SPMP_U | SPMP_R | SPMP_W,
				thread->stack_info.start, thread->stack_info.size,
				SPMP_U_MODE(thread));

	/* Global + per-user-thread entries must fit below Bao's reserved top
	 * slot.  A user task is what pushes the count up, so this is where an
	 * insufficient slot count is caught. */
	spmp_require_slots("user-mode thread", index);

	thread->arch.u_mode_pmp_domain_offset = index;
	thread->arch.u_mode_pmp_end_index = index;
	thread->arch.u_mode_pmp_update_nr = 0;
}

/**
 * @brief Convert partition information into SPMP entries
 */
static void resync_spmp_domain(struct k_thread *thread,
							struct k_mem_domain *domain) {
	unsigned int index = thread->arch.u_mode_pmp_domain_offset;
	int p_idx, remaining_partitions;
	bool ok;

	k_spinlock_key_t key = k_spin_lock(&z_mem_domain_lock);

	remaining_partitions = domain->num_partitions;
	for (p_idx = 0; remaining_partitions > 0; p_idx++) {
		struct k_mem_partition *part = &domain->partitions[p_idx];

		if (part->size == 0) {
			/* skip empty partition */
			continue;
		}

		remaining_partitions--;

		if (part->size < 4) {
			/* * 4 bytes is the minimum we can map */
			LOG_ERR("non-empty partition too small");
			__ASSERT(false, "");
			continue;
		}

		/* Honor the domain partition's requested user permissions. */
		ok = set_spmp_entry(&index, SPMP_U | part->attr.pmp_attr,
						part->start, part->size, SPMP_U_MODE(thread));
		__ASSERT(ok,
				"no SPMP slot left for %d remaining partitions in domain %p",
				remaining_partitions + 1, domain);
	}

	thread->arch.u_mode_pmp_end_index = index;
	thread->arch.u_mode_pmp_update_nr = domain->arch.pmp_update_nr;

	k_spin_unlock(&z_mem_domain_lock, key);
}

/**
 * @brief Write SPMP usermode content to actual SPMP registers
 *
 * This is called on every context switch.
 */
/* Per-thread (U=1) switch bits currently enabled in spmpen, so the next
 * context switch can clear exactly those and leave every other bit alone
 * (uniprocessor guest). */
static unsigned long active_user_sw[2];

void z_riscv_spmp_usermode_enable(struct k_thread *thread) {
	struct k_mem_domain *domain = thread->mem_domain_info.mem_domain;
	unsigned long new_sw0 = 0;
#ifndef CONFIG_64BIT
	unsigned long new_sw1 = 0;
#endif

	if (thread->arch.u_mode_pmp_end_index != 0) {
		/* write_spmp_entries() below enables this thread's U=1 user-stack
		 * vsPMP entry and re-enables interrupts while we are still executing
		 * on that stack (this runs on the context-switch path for a resuming
		 * user thread).  Set SUM so a trap taken in that window can
		 * save/restore context onto the now-U=1 stack; like
		 * arch_user_mode_enter() we leave SUM set (the epilogue also loads
		 * from that stack, and the eventual return-to-VU path in isr.S clears
		 * it). */
		csr_set(sstatus, SSTATUS_SUM);

		if (thread->arch.u_mode_pmp_update_nr != domain->arch.pmp_update_nr) {
			resync_spmp_domain(thread, domain);
		}

		unsigned long zero_sw[2] = { 0, 0 };

		write_spmp_entries(global_spmp_end_index, thread->arch.u_mode_pmp_end_index,
						true,
						thread->arch.u_mode_pmpaddr_regs,
						thread->arch.u_mode_pmpcfg_regs,
						zero_sw,
						ARRAY_SIZE(thread->arch.u_mode_pmpaddr_regs));

		new_sw0 = thread->arch.u_mode_spmpswitch_regs[0];
#ifndef CONFIG_64BIT
		new_sw1 = thread->arch.u_mode_spmpswitch_regs[1];
#endif
	}

	/* Enable exactly this thread's per-thread entries: clear the bits a
	 * previously-running user thread left enabled and OR in this thread's,
	 * touching nothing else.  A full spmpen overwrite would also clear the
	 * global and hypervisor/boot-reserved enable bits (not tracked here),
	 * which removes the guest's S-mode access and hangs the guest on CVA6.
	 *
	 * Until a user thread has run, active_user_sw is 0, so this is a no-op. */
	csr_write(CSR_SPMPEN,
			(csr_read(CSR_SPMPEN) & ~active_user_sw[0]) | new_sw0);
	active_user_sw[0] = new_sw0;
#ifndef CONFIG_64BIT
	csr_write(CSR_SPMPENH,
			(csr_read(CSR_SPMPENH) & ~active_user_sw[1]) | new_sw1);
	active_user_sw[1] = new_sw1;
#endif
	/* Discard cached permissions from the previous thread, including when
	 * address translation is Bare. SPMP CSR writes alone do not flush QEMU's
	 * cached physical access permissions. */
	__asm__ volatile ("sfence.vma" ::: "memory");
}

void z_riscv_spmp_usermode_commit(struct k_thread *thread)
{
	unsigned long sw0 = global_spmp_switch[0]
			  | thread->arch.u_mode_spmpswitch_regs[0];

	csr_write(CSR_SPMPEN, csr_read(CSR_SPMPEN) | sw0);

#ifndef CONFIG_64BIT
	unsigned long sw1 = global_spmp_switch[1]
			  | thread->arch.u_mode_spmpswitch_regs[1];

	csr_write(CSR_SPMPENH, csr_read(CSR_SPMPENH) | sw1);
#endif
}

int arch_mem_domain_max_partitions_get(void) {
	int available_spmp_slots = CONFIG_SPMP_SLOTS;

	/* remove those slots dedicated to global entries */
	available_spmp_slots -= global_spmp_end_index;

	/*
	* User thread stack mapping:
	* 1 slot if CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT=y,
	* most likely 2 slots otherwise.
	*/
	available_spmp_slots -=
		IS_ENABLED(CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT) ? 1 : 2;

	/*
	* Each partition may require either 1 or 2 SPMP slots depending
	* on a couple factors that are not known in advance. Even when
	* arch_mem_domain_partition_add() is called, we can't tell if a
	* given partition will fit in the remaining SPMP slots of an
	* affected thread if it hasn't executed in usermode yet.
	*
	* Give the most optimistic answer here (which should be pretty
	* accurate if CONFIG_MPU_REQUIRES_POWER_OF_TWO_ALIGNMENT=y) and be
	* prepared to deny availability in resync_spmp_domain() if this
	* estimate was too high.
	*/
	return available_spmp_slots;
}

int arch_mem_domain_init(struct k_mem_domain *domain) {
	domain->arch.pmp_update_nr = 0;
	return 0;
}

int arch_mem_domain_partition_add(struct k_mem_domain *domain,
								uint32_t partition_id) {
	/* Force resynchronization for every thread using this domain */
	domain->arch.pmp_update_nr += 1;
	return 0;
}

int arch_mem_domain_partition_remove(struct k_mem_domain *domain,
									uint32_t partition_id) {
	/* Force resynchronization for every thread usinginit this domain */
	domain->arch.pmp_update_nr += 1;
	return 0;
}

int arch_mem_domain_thread_add(struct k_thread *thread) {
	/* Force resynchronization for this thread */
	thread->arch.u_mode_pmp_update_nr = 0;
	return 0;
}

int arch_mem_domain_thread_remove(struct k_thread *thread) {
	return 0;
}

#define IS_WITHIN(inner_start, inner_size, outer_start, outer_size)    \
	((inner_start) >= (outer_start) && (inner_size) <= (outer_size) && \
	((inner_start) - (outer_start)) <= ((outer_size) - (inner_size)))

int arch_buffer_validate(const void *addr, size_t size, int write) {
	uintptr_t start = (uintptr_t)addr;
	int ret = -1;

	/* Check if this is on the stack */
	if (IS_WITHIN(start, size,
				_current->stack_info.start, _current->stack_info.size)) {
		return 0;
	}

	/* Check if this is within the global read-only area */
	if (!write) {
		uintptr_t ro_start = (uintptr_t)__rom_region_start;
		size_t ro_size = (size_t)__rom_region_size;

		if (IS_WITHIN(start, size, ro_start, ro_size)) {
			return 0;
		}
	}

	/* Look for a matching partition in our memory domain */
	struct k_mem_domain *domain = _current->mem_domain_info.mem_domain;
	int p_idx, remaining_partitions;
	k_spinlock_key_t key = k_spin_lock(&z_mem_domain_lock);

	remaining_partitions = domain->num_partitions;
	for (p_idx = 0; remaining_partitions > 0; p_idx++) {
		struct k_mem_partition *part = &domain->partitions[p_idx];

		if (part->size == 0) {
			/* unused partition */
			continue;
		}

		remaining_partitions--;

		if (!IS_WITHIN(start, size, part->start, part->size)) {
			/* unmatched partition */
			continue;
		}

		/* partition matched: determine access result */
		if ((part->attr.pmp_attr & (write ? SPMP_W : SPMP_R)) != 0) {
			ret = 0;
		}
		break;
	}

	k_spin_unlock(&z_mem_domain_lock, key);
	return ret;
}

#endif /* CONFIG_USERSPACE */
