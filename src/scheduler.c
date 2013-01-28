/*
 * scheduler.c
 *
 *  Created on: Jan 28, 2013
 *      Author: Ashraf Saleh
 */

#include 			  "scheduler.h"
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include 			  "z502.h"
#include             <string.h>

extern INT32 SYS_CALL_CALL_TYPE;

void queue_enqueue(PCB* pcb, PCB_Queue* q) {
	if (q->t == NULL) // Q empty
		q->h = q->t = pcb;
	else {
		q->t->qn = pcb;
		pcb->qp = q->t;
		q->t = pcb;
	}

	pcb->q = q;
	q->count++;
}

void queue_dequeue(PCB_Queue* q, PCB** pcb) {
	*pcb = NULL; //NULL be default
	// deques the first element the given Q and setting the pcb*
	if (!(q->h)) // Q is Empty
		return;
	if (q->h == q->t) { //only one PCB
		*pcb = q->h;
		q->h = q->t = NULL;
	} else {
		*pcb = q->h;
		(*pcb)->qn->qp = NULL;
		q->h = (*pcb)->qn;
		(*pcb)->qn = NULL;
	}
	q->count--;
}

void queue_remove(PCB* pcb, PCB_Queue* q) {
	if (q->h->id == pcb->id) {
		q->h = pcb->qn;
	} else {
		pcb->qp->qn = pcb->qn;
	}

	if (q->t->id == pcb->id) {
		q->t = pcb->qp;
	} else {
		pcb->qn->qp = pcb->qp;
	}
	pcb->qn = pcb->qp = NULL;
	q->count--;
}

void queue_remove_max(PCB_Queue* q, PCB** maxPCB) {

	*maxPCB = q->h;
	if (!*maxPCB)
		return;

	PCB* temp = (*maxPCB)->qn;
	while (temp) {
		if (temp->priority > (*maxPCB)->priority)
			*maxPCB = temp;
		temp = temp->qn;
	}

	queue_remove(*maxPCB, q);
}

void sc_dispatch() {
	PCB *nextToRun = 0, *old = 0;
	CALL(queue_remove_max(&ready_q, &nextToRun));

	if (nextToRun == NULL) // no ready processes available
	{
		if (allocated_processes > 0) {
			if (allocated_processes == suspended_q.count) {
				//we only have indefinitely suspended processes, force them to WAKE UP
				//making sure they receive an error
				old = current_process;
				CALL(queue_dequeue(&suspended_q, &current_process));
				current_process->status = PCB_RUNNING;
				current_process->return_val = ERR_FORCED_RESUME;

				printf("*****forcing a suspended process to start!*****\n");
				if (old && old->status == PCB_EMPTY) {
					ZCALL(
							Z502_SWITCH_CONTEXT(SWITCH_CONTEXT_SAVE_MODE, &(current_process->context)));
				} else {
					ZCALL(
							Z502_SWITCH_CONTEXT(SWITCH_CONTEXT_SAVE_MODE, &(current_process->context)));
				}
			} else {
				pthread_mutex_lock(&mutex);
				isr_done = 0;
				pthread_mutex_unlock(&mutex);

				if (current_process && current_process->status == PCB_EMPTY) {
					current_process = 0;
					ZCALL(
							Z502_SWITCH_CONTEXT(SWITCH_CONTEXT_KILL_MODE,&idle_context));
				} else {
					current_process = 0;
					ZCALL(
							Z502_SWITCH_CONTEXT(SWITCH_CONTEXT_SAVE_MODE,&idle_context));
				}

			}
		} else {
			ZCALL(Z502_HALT());
		}
		return;
	}

	old = current_process;
	current_process = nextToRun;
	current_process->status = PCB_RUNNING;
	current_process->return_val = ERR_SUCCESS;
	if (old && old->status == PCB_EMPTY) {
		ZCALL(
				Z502_SWITCH_CONTEXT(SWITCH_CONTEXT_KILL_MODE,&(nextToRun->context)));
	} else {
		ZCALL(
				Z502_SWITCH_CONTEXT(SWITCH_CONTEXT_SAVE_MODE,&(nextToRun->context)));
	}
}

void os_idle() {
	while (1) {
		pthread_mutex_lock(&mutex);
		if (!isr_done)
			ZCALL(Z502_IDLE());
		pthread_mutex_unlock(&mutex);

		pthread_mutex_lock(&mutex);
		if (!isr_done)
			pthread_cond_wait(&int_cond, &mutex);
		pthread_mutex_unlock(&mutex);
		SYS_CALL_CALL_TYPE = SYSNUM_DISPATCH;
		return;
	}
}

void sc_schedule(PCB *pcb) {
	pcb->q = &ready_q;
	pcb->status = PCB_READY;
	CALL(queue_enqueue(pcb, &ready_q));
}

void sc_deschedule(PCB *pcb) {
	pcb->q = 0;
	pcb->status = PCB_SUSPENDED;
	CALL(queue_remove(pcb, &ready_q));
}

void get_process_id_by_name(char* name, int *i) {
	for (*i = 0; *i < MAX_PROCESSES; (*i)++)
		if (process_table[*i].status != PCB_EMPTY)
			if (!strcmp(process_table[*i].name, name))
				return;
	*i = -1;
}

void get_left_sibling(int pid, int *sid) {
	//TODO e3melha O(1) we nadaf el design ya beta3 enta :D
	PCB* temp = process_table[pid].p;
	PCB* ls = 0;

	*sid = -1;

	if (!temp) // if the process doesn't have a parent
		return;
	temp = temp->lc;

	while (temp) {
		if (temp->id == pid) {
			if (ls) {
				*sid = ls->id;
				return;
			} else
				return;
		}
		ls = temp;
		temp = temp->rs;
	}
	return;
}

void terminate_tree(PCB* pcb) {
	// terminates the whole process tree of the passed process including the process itself

	while (pcb->lc)
		CALL(terminate_tree(pcb->lc));

	//terminate self
	int ls;
	CALL(get_left_sibling(pcb->id, &ls));
	if (ls > -1)
		process_table[ls].rs = pcb->rs;
	else if (pcb->p) // the node is the lc of its parent
		pcb->p->lc = pcb->rs;

	allocated_processes--;
	//TODO this is a quick patch, assuming we only use a ready queue
	if (pcb != current_process) {
		if (pcb->status == PCB_READY)
			CALL(queue_remove(pcb, pcb->q));
		ZCALL(Z502_DESTROY_CONTEXT(&(pcb->context)));
	}
	pcb->status = PCB_EMPTY;
}

void terminate_process(PCB* pcb) {

	PCB *last_child = pcb->lc;
	PCB* temp;
	if (last_child) {
		// getting the last (rightmost) child of pcb &
		//changing all the parent of its child to point to the parent of pcb

		//GEDEEEDA di
		if (!pcb->p) { //pcb has no parent ====> remove links between processes
			while (last_child->rs) {
				last_child->p = NULL;
				temp = last_child;
				last_child = last_child->rs;
				temp->rs = NULL;
			}
			last_child->p = NULL;

		} else {
			while (last_child->rs) {
				last_child->p = pcb->p;
				last_child = last_child->rs;
			}
			last_child->p = pcb->p;
		}

	}

	int ls;
	CALL(get_left_sibling(pcb->id, &ls));
	if (ls > -1) { //if pcb has a left sibling also since it has sibing it must have a parent

		if (pcb->lc) { //if pcb has children
			process_table[ls].rs = pcb->lc; //attach pcb's children in its place
			last_child->rs = pcb->rs;
		} else { //if pcb has no children
			process_table[ls].rs = pcb->rs; //attach pcb's left & right sigblings
		}
	} else { // if pcb doesn't have a left sibling (it's a lc of its parent)
		if (pcb->p) {
			if (pcb->lc) { //if pcb has children
				pcb->p->lc = pcb->lc; // set the pcb's parenet lc to pcb's lc
				last_child->rs = pcb->rs;
			} else
				//if pcb has no children
				pcb->p->lc = pcb->rs;
		}
	}

	if (current_process != pcb)
		CALL(queue_remove(pcb, pcb->q));
	pcb->status = PCB_EMPTY;
	allocated_processes--;
}

/*
void terminate_tree(PCB* pcb) {
 return terminate_tree_old(pcb);
 //GEDEEEDA  ya brens
 // terminate siblings and children of siblings
 PCB* temp = pcb->p;
 if (temp == NULL) {
 printf("*** foo2\n");
 //terminate self only;
 temp->status = PCB_EMPTY;
 allocated_processes--;
 if (temp != current_process) {
 CALL(queue_remove(temp, temp->q));
 ZCALL(Z502_DESTROY_CONTEXT(&(temp->context)));
 }

 } else {
 printf("*** ta7t\n");
 temp = temp->lc;

 while (temp) {
 printf("## awel ## %d\n", temp->id);

 if (temp->lc) {
 terminate_tree(temp->lc);
 }

 // kill temp
 temp->status = PCB_EMPTY;
 allocated_processes--;

 if (temp->id != current_process->id) {
 printf("hela hopa \n");
 CALL(queue_remove(temp, temp->q));
 printf("hopa ya thawra\n");
 ZCALL(Z502_DESTROY_CONTEXT(&(temp->context)));
 }

 // set temp to next process on sibling list
 temp = temp->rs;
 }
 printf("brens\n");

 }

 }
 */

void allocate_process(char *name, int priority, PCB **pcb) {
	*pcb = 0;
	//TODO rewrite
	int seek_start = (next_pcb + MAX_PROCESSES - 1) % MAX_PROCESSES;
	while (next_pcb != seek_start) {
		if (process_table[next_pcb].status == PCB_EMPTY) {
			*pcb = &process_table[next_pcb];
			memset(*pcb, 0, sizeof(PCB));
			(*pcb)->id = next_pcb;
			next_pcb = (next_pcb + 1) % MAX_PROCESSES;
			break;
		}
		next_pcb = (next_pcb + 1) % MAX_PROCESSES;
	}

	if (*pcb == 0) {
		return;
	}

	allocated_processes++;
	strncpy((*pcb)->name, name, MAX_PROCESS_NAME);
	(*pcb)->priority = priority;
}
