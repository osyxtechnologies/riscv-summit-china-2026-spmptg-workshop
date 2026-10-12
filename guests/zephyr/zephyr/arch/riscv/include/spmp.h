/*
 * Copyright (c) 2022 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SPMP_H_
#define SPMP_H_

void z_riscv_spmp_init(void);
void z_riscv_spmp_stackguard_prepare(struct k_thread *thread);
void z_riscv_spmp_stackguard_enable(struct k_thread *thread);
void z_riscv_spmp_usermode_init(struct k_thread *thread);
void z_riscv_spmp_usermode_prepare(struct k_thread *thread);
void z_riscv_spmp_usermode_enable(struct k_thread *thread);
void z_riscv_spmp_usermode_commit(struct k_thread *thread);
 
 #endif /* SPMP_H_ */
