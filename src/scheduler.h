/*
 * scheduler.h
 *
 *  Created on: Jan 28, 2013
 *      Author: Ashraf Saleh
 */

#ifndef SCHEDULER_H_
#define SCHEDULER_H_

#include "base.h"

#define MAX_PROCESSES 20
#define MAX_PROCESS_NAME 20
#define MAX_PRIORITY_LEVEL 5

#define PCB_EMPTY 0
#define PCB_READY 1
#define PCB_RUNNING 2
#define PCB_BLOCKED 3
#define PCB_SUSPENDED 4
#define PCB_BLOCKED_IPC 5
#define PCB_BLOCKED_IO 6

#define MAX_PRIORITY 100

#define SC_FCFS 0
#define SC_PRIORITY 1


typedef union {
	int resume_time;
} PCB_EXTRA;

struct PCB_Queue;

struct PCB {
	int id;
	char name[MAX_PROCESS_NAME];
	int status;
	int priority;
	int blocked_until;
	void *return_ptr;
	long return_val;
	void *context;
	struct PCB* qp;
	struct PCB* qn;
	struct PCB* p;
	struct PCB* lc;
	struct PCB* rs;
	struct PCB_Queue* q;
	IPC ipc;
	Message private_msg[MAILBOX_SIZE];
	Message public_msg[MAILBOX_SIZE];
	int nxt_pub_r;
	int nxt_pub_w;
};

struct PCB_Queue {
	struct PCB* h;
	struct PCB* t;
	int count;
	int q_id;
};

typedef struct PCB PCB;
typedef struct PCB_Queue PCB_Queue;

//TODO implement remove max in O(log (n))
PCB_Queue ready_q_pr[MAX_PRIORITY_LEVEL];
// PCB_Queue* suspended_q_pr[MAX_PRIORITY_LEVEL];
PCB_Queue timer_q_pr[MAX_PRIORITY_LEVEL];

// are initialized in OS_init at base.c
PCB_Queue ready_q ;
PCB_Queue suspended_q ;
PCB_Queue timer_q ;
PCB_Queue ipc_q ;
PCB_Queue io_q ;

PCB process_table[MAX_PROCESSES];
PCB* current_process;
void* idle_context;
int next_pcb ; // initialized by 0
int SC;// initialized by SC_PRIORITY
int allocated_processes;//initialized by 0

void queue_enqueue(PCB* pcb, PCB_Queue* q);
void queue_dequeue(PCB_Queue* q, PCB** pcb);
void queue_remove(PCB* pcb, PCB_Queue* q);
void queue_remove_max(PCB_Queue* q, PCB** maxPCB);
void sc_dispatch();
void os_idle();
void sc_schedule(PCB *pcb);
void sc_deschedule(PCB *pcb);
void get_process_id_by_name(char* name, int *i);
void get_left_sibling(int pid, int *sid);
void terminate_tree(PCB* pcb);
void terminate_process(PCB* pcb);
void allocate_process(char *name, int priority, PCB **pcb);

#endif /*SCHEDULER_H_*/
