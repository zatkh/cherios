/*-
 * Copyright (c) 2017 Lawrence Esswood
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "vmem.h"
#include "stdio.h"

static ptable_t top_table, L1_0, L2_0;

static void init_vmem(void) {
    top_table = get_top_level_table();
    assert(top_table != NULL);
    CHERI_PRINT_CAP(top_table);
    L1_0 = memmget_create_table(top_table, 0);
    assert(L1_0 != NULL);
    CHERI_PRINT_CAP(L1_0);
    L2_0 = memmget_create_table(L1_0, 0);
    assert(L2_0 != NULL);
    CHERI_PRINT_CAP(L2_0);
    for(size_t index = 0; index != PAGE_TABLE_ENTS; index++) {
        int res = memget_create_mapping(L2_0, index);
        assert(res == 0);
    }


    /* Now we get the first reservation which NEEDS virtual mem */
    first_reservation.reservation = make_first_reservation();
    first_reservation.next = NULL;
    first_reservation.prev = NULL;
}

static void init_book(void) {
    book = get_book();
}

void minit(void) {
    printf("Getting book\n");
    init_book();
    printf("Starting up virtual memory and reservation system\n");
    init_vmem();

}