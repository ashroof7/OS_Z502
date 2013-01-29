#include "memory.h"

void free_pages_init(){
    int i;
    for(i = 0; i < PHYS_MEM_PGS - 1; i++)
        frame_table[i].next = &frame_table[i+1];

    free_pages = &frame_table[0];
}

page_frame* allocate_page(){
    if(!free_pages){
        return 0;
    }

    page_frame* r = free_pages;
    free_pages = free_pages->next;
    r->ref_count++;
    return r;
}
