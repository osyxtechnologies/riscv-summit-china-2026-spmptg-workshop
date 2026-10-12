/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>

int main(void)
{
	printf("\nHello World!\nRunning Zephyr w/SPMP on top of the %s platform\n", CONFIG_BOARD);
	return 0;
}
