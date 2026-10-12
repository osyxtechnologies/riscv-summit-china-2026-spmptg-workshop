/*
 * Copyright (c) 2020 BayLibre, SAS
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * This benchmark is for the test of malloc.
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <zephyr/sys_clock.h>
#include <zephyr/sys/libc-hooks.h>


#define FILE_SIZE (1<<10UL)
#define USER_STACKSIZE	2048

struct k_thread user_thread;
K_THREAD_STACK_DEFINE(user_stack, USER_STACKSIZE);


uint32_t lz77_compress (uint8_t *uncompressed_text, uint32_t uncompressed_size, uint8_t *compressed_text, uint8_t pointer_length_width)
{
    uint16_t pointer_pos, temp_pointer_pos, output_pointer, pointer_length, temp_pointer_length;
    uint32_t compressed_pointer, output_size, coding_pos, output_lookahead_ref, look_behind, look_ahead;
    uint16_t pointer_pos_max, pointer_length_max;
    pointer_pos_max = 1 << (16 - pointer_length_width);
    pointer_length_max = 1 << pointer_length_width;

    *((uint32_t *) compressed_text) = uncompressed_size;
    *(compressed_text + 4) = pointer_length_width;
    compressed_pointer = output_size = 5;
    
    for(coding_pos = 0; coding_pos < uncompressed_size; ++coding_pos)
    {
        pointer_pos = 0;
        pointer_length = 0;
        for(temp_pointer_pos = 1; (temp_pointer_pos < pointer_pos_max) && (temp_pointer_pos <= coding_pos); ++temp_pointer_pos)
        {
            look_behind = coding_pos - temp_pointer_pos;
            look_ahead = coding_pos;
            for(temp_pointer_length = 0; uncompressed_text[look_ahead++] == uncompressed_text[look_behind++]; ++temp_pointer_length)
                if(temp_pointer_length == pointer_length_max)
                    break;
            if(temp_pointer_length > pointer_length)
            {
                pointer_pos = temp_pointer_pos;
                pointer_length = temp_pointer_length;
                if(pointer_length == pointer_length_max)
                    break;
            }
        }
        coding_pos += pointer_length;
        if((coding_pos == uncompressed_size) && pointer_length)
        {
            output_pointer = (pointer_length == 1) ? 0 : ((pointer_pos << pointer_length_width) | (pointer_length - 2));
            output_lookahead_ref = coding_pos - 1;
        }
        else
        {
            output_pointer = (pointer_pos << pointer_length_width) | (pointer_length ? (pointer_length - 1) : 0);
            output_lookahead_ref = coding_pos;
        }
        *((uint16_t *) (compressed_text + compressed_pointer)) = output_pointer;
        compressed_pointer += 2;
        *(compressed_text + compressed_pointer++) = *(uncompressed_text + output_lookahead_ref);
        output_size += 3;
    }

    return output_size;
}

uint32_t lz77_decompress (uint8_t *compressed_text, uint8_t *uncompressed_text)
{
    uint8_t pointer_length_width;
    uint16_t input_pointer, pointer_length, pointer_pos, pointer_length_mask;
    uint32_t compressed_pointer, coding_pos, pointer_offset, uncompressed_size;

    uncompressed_size = *((uint32_t *) compressed_text);
    pointer_length_width = *(compressed_text + 4);
    compressed_pointer = 5;

    pointer_length_mask = (1 << pointer_length_width) - 1;

    for(coding_pos = 0; coding_pos < uncompressed_size; ++coding_pos)
    {
        input_pointer = *((uint16_t *) (compressed_text + compressed_pointer));
        compressed_pointer += 2;
        pointer_pos = input_pointer >> pointer_length_width;
        pointer_length = pointer_pos ? ((input_pointer & pointer_length_mask) + 1) : 0;
        if(pointer_pos)
            for(pointer_offset = coding_pos - pointer_pos; pointer_length > 0; --pointer_length)
                uncompressed_text[coding_pos++] = uncompressed_text[pointer_offset++];
        *(uncompressed_text + coding_pos) = *(compressed_text + compressed_pointer++);
    }

    return coding_pos;
}


uint32_t file_lz77_compress (size_t malloc_size, uint8_t pointer_length_width)
{
    uint32_t uncompressed_size = FILE_SIZE;

    uint8_t *uncompressed_text = malloc(uncompressed_size);

    char c = 0x01;
	for (int j = 0; j < uncompressed_size; j += 1) {
		uncompressed_text[j] = (c ^= c * 7) % 256;
	}
    
    uint8_t *compressed_text;

    uint32_t compressed_size;

    compressed_text = malloc(malloc_size);

    compressed_size = lz77_compress(uncompressed_text, uncompressed_size, compressed_text, pointer_length_width);

    return compressed_size;
}

int lz77_main (void)
{
    uint32_t res = file_lz77_compress(FILE_SIZE, 4);
    return res;
}


static void user_function(void *p1, void *p2, void *p3) {

	uint64_t start_time = k_uptime_get();

	lz77_main();

	uint64_t stop_time = k_uptime_get();


    printf("time:%lld ms\n", start_time);
	printf("timex:%lld ms\n", stop_time);
}


int main(void)
{
    /*
    If you want to use malloc in userspace, z_malloc_partition which is enabled in <zephyr/sys/libc-hooks.h> has to be involved  in current memory domain.
    For more information about memory management and memory protection in Zephyr, please refer to the following links:
    https://docs.zephyrproject.org/latest/develop/languages/c/index.html
    https://docs.zephyrproject.org/latest/kernel/usermode/memory_domain.html#pre-defined-memory-partitions
    By the way, do remember to set memory domains and partitions before ret to userspace!
    You can change CONFIG_MINIMAL_LIBC_MALLOC_ARENA_SIZE in prj.conf to configure the size of z_malloc_partition.
    */
    k_mem_domain_add_partition(&k_mem_domain_default, &z_malloc_partition);
	k_thread_create(&user_thread, user_stack, USER_STACKSIZE,
			user_function, NULL, NULL, NULL,
			-1, K_USER, K_MSEC(0));
}