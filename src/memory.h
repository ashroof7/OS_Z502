/*
 * memory.h
 *
 *  Created on: Jan 29, 2013
 *      Author: Ashraf Saleh
 */


#ifndef MEMORY_H_
#define MEMORY_H_

#include "scheduler.h"
#include "global.h"

typedef INT16 pte_t ;

pte_t page_tables[MAX_PROCESSES][VIRTUAL_MEM_PGS];

typedef struct page_frame page_frame;

struct page_frame{
    page_frame* next;
    page_frame* prev;
};

typedef struct {
    page_frame* head;
    page_frame* tail;
} LRU_queue;

page_frame frame_table[PHYS_MEM_PGS];

#endif /*  MEMORY_H_ */
