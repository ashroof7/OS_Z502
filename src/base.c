/************************************************************************

 This code forms the base of the operating system you will
 build.  It has only the barest rudiments of what you will
 eventually construct; yet it contains the interfaces that
 allow test.c and z502.c to be successfully built together.

 Revision History:
 1.0 August 1990
 1.1 December 1990: Portability attempted.
 1.3 July     1992: More Portability enhancements.
 Add call to sample_code.
 1.4 December 1992: Limit (temporarily) printout in
 interrupt handler.  More portability.
 2.0 January  2000: A number of small changes.
 2.1 May      2001: Bug fixes and clear STAT_VECTOR
 2.2 July     2002: Make code appropriate for undergrads.
 Default program start is in test0.
 3.0 August   2004: Modified to support memory mapped IO
 3.1 August   2004: hardware interrupt runs on separate thread
 3.11 August  2004: Support for OS level locking
 ************************************************************************/

#include 			  "scheduler.h"
#include 			  "base.h"
#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             <string.h>
#include 			  "z502.h"

extern char MEMORY[];
extern BOOL   POP_THE_STACK;
extern UINT16 *Z502_PAGE_TBL_ADDR;
extern INT16 Z502_PAGE_TBL_LENGTH;
extern INT16 Z502_PROGRAM_COUNTER;
extern INT16 Z502_INTERRUPT_MASK;
extern INT32 SYS_CALL_CALL_TYPE;
extern INT16 Z502_MODE;
extern Z502_ARG Z502_ARG1;
extern Z502_ARG Z502_ARG2;
extern Z502_ARG Z502_ARG3;
extern Z502_ARG Z502_ARG4;
extern Z502_ARG Z502_ARG5;
extern Z502_ARG Z502_ARG6;

extern void *TO_VECTOR[];
extern INT32 CALLING_ARGC;
extern char **CALLING_ARGV;


char *call_names[] = { "mem_read ", "mem_write", "read_mod ", "get_time ",
		"sleep    ", "get_pid  ", "create   ", "term_proc", "suspend  ",
		"resume   ", "ch_prior ", "send     ", "receive  ", "disk_read",
		"disk_wrt ", "def_sh_ar", "dispatch" };

void notify_ipc(int mid) {
	if (ipc_q.count) {
		PCB *receiver = ipc_q.h;
		while (receiver) {
			if (mailbox[mid].receiver == -1
					|| mailbox[mid].receiver == receiver->id) {
				if (receiver->ipc.waiting_on_pid == -1
						|| receiver->ipc.waiting_on_pid
								== mailbox[mid].sender) {
					CALL(queue_remove(receiver,receiver->q));
					CALL(sc_schedule(receiver));
					if (receiver->ipc.buffer_size
							< mailbox[mid].message_length) {
						receiver->ipc.fail = ERR_BAD_PARAM;
					} else {
						receiver->ipc.msg = &mailbox[mid];
						break;
					}
				}
			}
			receiver = receiver->qn;
		}
	}
}

void timer_ISR() {
	INT32 Time;
	int i;
	ZCALL( MEM_READ( Z502ClockStatus, &Time ));
	unsigned int nxt_interrupt = TIMER_INF;
	for (i = 0; i < MAX_PROCESSES; i++)
		if (process_table[i].status == PCB_BLOCKED) {
			if (process_table[i].blocked_until <= Time) {
				process_table[i].status = PCB_READY;
				process_table[i].blocked_until = -1;
				CALL(queue_enqueue(&process_table[i], &ready_q));
			} else if (process_table[i].blocked_until < nxt_interrupt) {
				nxt_interrupt = process_table[i].blocked_until;
			}
		}

	nxt_timer_interrupt = nxt_interrupt;
	if (nxt_interrupt == TIMER_INF)
		return;

	ZCALL( MEM_READ( Z502ClockStatus, &Time ));
	nxt_interrupt -= Time;
	INT32 setter = (INT32) nxt_interrupt;
	ZCALL( MEM_WRITE(Z502TimerStart, &setter));

}

/************************************************************************
 INTERRUPT_HANDLER
 When the Z502 gets a hardware interrupt, it transfers control to
 this routine in the OS.
 ************************************************************************/
void interrupt_handler(void) {

	INT32 device_id;
	INT32 status;
	INT32 Index = 0;

	pthread_mutex_lock(&mutex);
	// Get cause of interrupt
	MEM_READ(Z502InterruptDevice, &device_id);
	// Set this device as target of our query
	MEM_WRITE(Z502InterruptDevice, &device_id);
	// Now read the status of this device
	MEM_READ(Z502InterruptStatus, &status);

	switch (device_id) {
	case TIMER_INTERRUPT :
		printf("in time interrupt handler\n");
		CALL(timer_ISR());
		break;
	}

	// Clear out this device - we're done with it
	MEM_WRITE(Z502InterruptClear, &Index);
	isr_done = 1;

	pthread_cond_signal(&int_cond);
	pthread_mutex_unlock(&mutex);
} /* End of interrupt_handler */
/************************************************************************
 FAULT_HANDLER
 The beginning of the OS502.  Used to receive hardware faults.
 ************************************************************************/

void fault_handler(void) {
	INT32 device_id;
	INT32 status;
	INT32 Index = 0;

	// Get cause of interrupt
	MEM_READ(Z502InterruptDevice, &device_id);
	// Set this device as target of our query
	MEM_WRITE(Z502InterruptDevice, &device_id);
	// Now read the status of this device
	MEM_READ(Z502InterruptStatus, &status);

	printf("Fault_handler: Found vector type %d with value %d\n", device_id,
			status);

	CALL(terminate_process(process_table));
	CALL(sc_dispatch());

	// Clear out this device - we're done with it
	MEM_WRITE(Z502InterruptClear, &Index);
} /* End of fault_handler */

/************************************************************************
 SVC
 The beginning of the OS502.  Used to receive software interrupts.
 All system calls come to this point in the code and are to be
 handled by the student written code here.
 ************************************************************************/

void svc(void) {
	INT16 call_type;
	static INT16 do_print = 10;
	INT32 Time;
	call_type = (INT16) SYS_CALL_CALL_TYPE;
	//	if (do_print > 0) {
	printf("SVC handler: %s %8ld %8ld %8ld %8ld %8ld %8ld\n",
			call_names[call_type], Z502_ARG1.VAL, Z502_ARG2.VAL, Z502_ARG3.VAL,
			Z502_ARG4.VAL, Z502_ARG5.VAL, Z502_ARG6.VAL);
	printf("SVC handler: called by process %s\n",
			(current_process) ? (current_process->name) : ("idle_context"));
	do_print--;
	//	}

	PCB* pcb = 0;
	int pid;

	switch (call_type) {
	//get time service call
	case SYSNUM_GET_TIME_OF_DAY:
		printf("SYSNUM_GET_TIME_OF_DAY\n");
		ZCALL( MEM_READ( Z502ClockStatus, &Time ));
		*(INT32*) Z502_ARG1.PTR = Time;
		break;

	case SYSNUM_SLEEP:
		printf("SYSNUM_SLEEP\n");
		ZCALL( MEM_READ( Z502ClockStatus, &Time ));
		if (Time + Z502_ARG1.VAL < nxt_timer_interrupt) {
			nxt_timer_interrupt = Time + Z502_ARG1.VAL;
			ZCALL(MEM_WRITE(Z502TimerStart,(INT32*)&Z502_ARG1.VAL));
		}
		current_process->blocked_until = Time + (INT32) Z502_ARG1.VAL;
		current_process->status = PCB_BLOCKED;
//		current_process->q = &timer_q;
//		queue_enqueue(current_process, &ready_q);
		//FIXME remove from ready_q (already in dispatch)
		// insert into timer_q
		CALL(sc_dispatch());
		break;

	case SYSNUM_CREATE_PROCESS:
		printf("SYSNUM_CREATE_PROCESS\n");
		//process already exists

		CALL(get_process_id_by_name((char *) Z502_ARG1.PTR, &pid));
		if ((INT32) Z502_ARG3.VAL < 0 || pid > -1) {
			*(long *) Z502_ARG4.PTR = -1;
			*(long *) Z502_ARG5.PTR = ERR_BAD_PARAM;
			break;
		}
		CALL(
				allocate_process((char *) Z502_ARG1.PTR, (int) Z502_ARG3.VAL, &pcb));
		// can't find a space to allocate the new process
		if (pcb == 0) {
			//TODO throw exception "end reached"
			*(long *) Z502_ARG4.PTR = -1;
			*(long *) Z502_ARG5.PTR = ERR_PROCESS_CREATION;
			break;
		}

		pcb->p = current_process;
		pcb->rs = current_process->lc;
		current_process->lc = pcb;
		ZCALL( Z502_MAKE_CONTEXT(&pcb->context, Z502_ARG2.PTR, USER_MODE ));
		CALL(sc_schedule(pcb));

		*(long *) Z502_ARG4.PTR = pcb->id;
		*(long *) Z502_ARG5.PTR = ERR_SUCCESS;
		break;

	case SYSNUM_TERMINATE_PROCESS:
		printf("SYSNUM_TERMINATE_PROCESS\n");
		pid = (int) Z502_ARG1.VAL;

		if (pid < -2 || pid >= MAX_PROCESSES) {
			*(long *) Z502_ARG2.PTR = ERR_BAD_PARAM;
			break;
		} else if (pid > -1) {
			pcb = &process_table[pid];

			if (process_table[pid].status == PCB_EMPTY) { // passed process doesn't exist
				*(long *) Z502_ARG2.PTR = ERR_BAD_PARAM;
				break;
			}

			CALL(terminate_process(pcb));
			ZCALL(Z502_DESTROY_CONTEXT(&(pcb->context)));
			*(long *) Z502_ARG2.PTR = ERR_SUCCESS;
			break;
		} else if (pid == -1) {
			pcb = current_process;
			CALL(terminate_process(pcb));
			*(long *) Z502_ARG2.PTR = ERR_SUCCESS;
			CALL(sc_dispatch());
		} else if (pid == -2) {
			pcb = current_process;
			*(long *) Z502_ARG2.PTR = ERR_SUCCESS;
			CALL(terminate_tree(pcb));
			CALL(sc_dispatch());
		}
		break;

	case SYSNUM_GET_PROCESS_ID:
		printf("SYSNUM_GET_PROCESS_ID\n");
		if (!strlen((char *) Z502_ARG1.PTR)) {
			*(INT32 *) Z502_ARG2.PTR = current_process->id;
			*(INT32 *) Z502_ARG3.PTR = ERR_SUCCESS;
			break;
		}

		CALL(get_process_id_by_name((char *) Z502_ARG1.PTR, &pid));
		if (pid > -1)
			*(INT32 *) Z502_ARG3.PTR = ERR_SUCCESS;
		else
			*(INT32 *) Z502_ARG3.PTR = ERR_BAD_PARAM;

		*(INT32 *) Z502_ARG2.PTR = pid;
		break;

	case SYSNUM_CHANGE_PRIORITY:
		printf("SYSNUM_CHANGE_PRIORITY\n");
		if ((INT32) Z502_ARG2.VAL >= MAX_PRIORITY
				|| (INT32) Z502_ARG2.VAL < 0) {
			// illegal passed value of new priority
			*(INT32 *) Z502_ARG3.PTR = ERR_ILLEGAL_PRIORITY;
			break;
		} else if ((INT32) Z502_ARG1.VAL
				< -1|| (INT32) Z502_ARG1.VAL >= MAX_PROCESSES) {*(INT32 *) Z502_ARG3.PTR = ERR_BAD_PARAM;
		break;
	}

	if ((INT32) Z502_ARG1.VAL == -1)
	// change the priority of current process
	current_process->priority = Z502_ARG2.VAL;
	else
	process_table[Z502_ARG1.VAL].priority = Z502_ARG2.VAL;
	*(INT32 *) Z502_ARG3.PTR = ERR_SUCCESS;

	break;

	case SYSNUM_SUSPEND_PROCESS:
	printf("SYSNUM_SUSPEND_PROCESS\n");
	pid = ((INT32) Z502_ARG1.VAL == -1) ?
	(current_process->id) : ((INT32) Z502_ARG1.VAL);

	if (pid < 0 || pid >= MAX_PROCESSES) {
		*(INT32 *) Z502_ARG2.PTR = ERR_BAD_PARAM;
		break;
	}
	if (process_table[pid].status != PCB_RUNNING
			&& process_table[pid].status != PCB_READY) {
		*(INT32 *) Z502_ARG2.PTR = ERR_ILLEGAL_PROCESS_STATE;
		break;
	}

	*(INT32 *) Z502_ARG2.PTR = ERR_SUCCESS;

	if (process_table[pid].status == PCB_RUNNING) {
		process_table[pid].q = &suspended_q;
		process_table[pid].status = PCB_SUSPENDED;
		process_table[pid].return_ptr = Z502_ARG2.PTR;
		CALL(queue_enqueue(&process_table[pid],&suspended_q));
		CALL(sc_dispatch());
	} else {
		CALL(queue_remove(&process_table[pid],process_table[pid].q));
		process_table[pid].q = &suspended_q;
		process_table[pid].return_ptr = 0;
		CALL(queue_enqueue(&process_table[pid],&suspended_q));
		process_table[pid].status = PCB_SUSPENDED;

	}

	break;

	case SYSNUM_RESUME_PROCESS:
	printf("SYSNUM_RESUME_PROCESS\n");
	pid = (INT32) (Z502_ARG1.VAL);

	if (pid < 0 || pid >= MAX_PROCESSES) {
		*(INT32 *) Z502_ARG2.PTR = ERR_BAD_PARAM;
		break;
	}

	if (process_table[pid].status != PCB_SUSPENDED) {
		// if the passed process is not SUSPENDED
		*(INT32 *) Z502_ARG2.PTR = ERR_ILLEGAL_PROCESS_STATE;
		break;
	}

	CALL(queue_remove(&process_table[pid],process_table[pid].q));
	CALL(sc_schedule(&process_table[pid]));
	*(INT32 *) Z502_ARG2.PTR = ERR_SUCCESS;

	break;

	case SYSNUM_SEND_MESSAGE:
	printf("SYSNUM_SEND_MESSAGE\n");

	int wi;
	if((INT32) Z502_ARG3.VAL > MAX_MSG_SIZE) {
		*(long *) Z502_ARG4.PTR = ERR_BAD_PARAM; //MAXIMUM BUFFER SIZE EXCEEDED
		break;
	}

	int rid = (INT32)Z502_ARG1.VAL;
	if (rid >= MAX_PROCESSES || (rid > -1 && process_table[rid].status == PCB_EMPTY)) {
		*(long *) Z502_ARG4.PTR = ERR_BAD_PARAM; //passed pid doesn't exist
		break;
	}

	wi = -1;
	while(++wi < MAILBOX_SIZE && mailbox[wi].status == MSG_BUSY);

	if(wi >= MAILBOX_SIZE) {
		*(long *) Z502_ARG4.PTR = ERR_MSG_MAIL_BOX_FULL; //buffer full
		break;
	}

	mailbox[wi] =(Message) {current_process->id, rid, {}, (INT32)Z502_ARG3.VAL, MSG_BUSY};
	memcpy(mailbox[wi].message, (char *)Z502_ARG2.PTR, Z502_ARG3.VAL);
	notify_ipc(wi);
	*(long *) Z502_ARG4.PTR = ERR_SUCCESS;
	break;

	case SYSNUM_RECEIVE_MESSAGE:
	printf("SYSNUM_RECEIVE_MESSAGE\n");
	int sid = (INT32)Z502_ARG1.VAL;
	if (sid >= MAX_PROCESSES || (sid > -1 && process_table[sid].status == PCB_EMPTY)) {
		*(long *) Z502_ARG6.PTR = ERR_BAD_PARAM; //passed pid doesn't exist
		break;
	}

	if((INT32) Z502_ARG3.VAL > MAX_MSG_SIZE) {
		*(long *) Z502_ARG6.PTR = ERR_BAD_PARAM; //MAXIMUM BUFFER SIZE EXCEEDED
		break;
	}

	wi = -1;
	while(++wi < MAILBOX_SIZE) {
		if(mailbox[wi].status == MSG_BUSY) {
			if(mailbox[wi].receiver == -1 || mailbox[wi].receiver == current_process->id) {
				if(sid == -1 || mailbox[wi].sender == sid) {
					break;
				}
			}
		}
	}

	if(wi < MAILBOX_SIZE) {
		Message *msg = &mailbox[wi];
		if(msg->message_length > (INT32)Z502_ARG3.VAL) {
			*(long *) Z502_ARG6.PTR = ERR_BAD_PARAM; //insufficient buffer size
			break;
		}
		msg->status = MSG_FREE;
		memcpy((char *)Z502_ARG2.PTR, msg->message, msg->message_length);
		*(INT32 *) Z502_ARG4.PTR = msg->message_length;
		*(INT32 *) Z502_ARG5.PTR = msg->sender;
		*(long *) Z502_ARG6.PTR = ERR_SUCCESS;
		break;
	}

	//we should block
	current_process->status = PCB_BLOCKED_IPC;
	CALL(queue_enqueue(current_process,&ipc_q));
	current_process->ipc = (IPC) {sid, (char *) Z502_ARG2.PTR,(INT32)Z502_ARG3.VAL,
		0, 0};
	CALL(sc_dispatch());

	break;

	case SYSNUM_DISPATCH:
	printf("SYSNUM_DISPATCH\n");
	CALL(sc_dispatch());
	break;
	default:
	printf("ERROR! call_type not recognized!\n");
	printf("Call_type is - %i\n", call_type);
	break;
} //end of switch

}		// End of svc

		/************************************************************************
		 OS_SWITCH_CONTEXT_COMPLETE
		 The hardware, after completing a process switch, calls this routine
		 to see if the OS wants to do anything before starting the user
		 process.
		 ************************************************************************/

void os_switch_context_complete(void) {
	static INT16 do_print = TRUE;

	if (do_print == TRUE) {
		printf("os_switch_context_complete  called before user code.\n");
		do_print = FALSE;
	}

	if (current_process) {
		if (current_process->return_ptr)
			*(long *) ((Z502CONTEXT *) current_process->context)->arg2.PTR =
					current_process->return_val;
		current_process->return_ptr = 0;
		current_process->return_val = 0;

		if (current_process->ipc.fail) {
			*(long *) Z502_ARG6.PTR = current_process->ipc.fail;
		} else if (current_process->ipc.msg) {
			memcpy((char *) Z502_ARG2.PTR, current_process->ipc.msg->message,
					current_process->ipc.msg->message_length);
			*(INT32 *) Z502_ARG4.PTR = current_process->ipc.msg->message_length;
			*(INT32 *) Z502_ARG5.PTR = current_process->ipc.msg->sender;
			*(long *) Z502_ARG6.PTR = ERR_SUCCESS;
			current_process->ipc.msg->status = MSG_FREE;
			current_process->ipc.msg = 0;
		}
	}
} /* End of os_switch_context_complete */

/************************************************************************
 OS_INIT
 This is the first routine called after the simulation begins.  This
 is equivalent to boot code.  All the initial OS components can be
 defined and initialized here.
 ************************************************************************/

void os_init(void) {
	ready_q = (PCB_Queue) { .h = 0, .t = 0, .count = 0, .q_id = 0 };
	suspended_q = (PCB_Queue) { .h = 0, .t = 0, .count = 0, .q_id = 1 };
	timer_q = (PCB_Queue) { .h = 0, .t = 0, .count = 0, .q_id = 2 };
	ipc_q = (PCB_Queue) { .h = 0, .t = 0, .count = 0, .q_id = 3 };

	nxt_timer_interrupt = TIMER_INF;
	next_pcb = 0;
	SC = SC_PRIORITY;
	allocated_processes = 0;

	void *next_context;
	INT32 i;

	pthread_mutex_init(&mutex, NULL );
	pthread_cond_init(&int_cond, NULL );
	/* Demonstrates how calling arguments are passed thru to here       */

	printf("Program called with %d arguments:", CALLING_ARGC);
	for (i = 0; i < CALLING_ARGC; i++)
		printf(" %s", CALLING_ARGV[i]);
	printf("\n");
	printf("Calling with argument 'sample' executes the sample program.\n");

	/*          Setup so handlers will come to code in base.c           */

	TO_VECTOR[TO_VECTOR_INT_HANDLER_ADDR ] = (void *) interrupt_handler;
	TO_VECTOR[TO_VECTOR_FAULT_HANDLER_ADDR ] = (void *) fault_handler;
	TO_VECTOR[TO_VECTOR_TRAP_HANDLER_ADDR ] = (void *) svc;

	/*  Determine if the switch was set, and if so go to demo routine.  */

	if ((CALLING_ARGC > 1) && (strcmp(CALLING_ARGV[1], "sample") == 0)) {
		ZCALL(
				Z502_MAKE_CONTEXT( &next_context, (void *)sample_code, KERNEL_MODE ));
		ZCALL( Z502_SWITCH_CONTEXT( SWITCH_CONTEXT_KILL_MODE, &next_context ));
	} /* This routine should never return!!           */

	/*  This should be done by a "os_make_process" routine, so that
	 test0 runs on a process recognized by the operating system.    */
	PCB* init = 0;
	CALL(allocate_process("init", 0, &init));
	init->status = PCB_RUNNING;
	current_process = init;
	ZCALL( Z502_MAKE_CONTEXT(&idle_context, (void *)os_idle, KERNEL_MODE));
	ZCALL( Z502_MAKE_CONTEXT( &init->context, (void *)test1m, USER_MODE ));
	ZCALL( Z502_SWITCH_CONTEXT( SWITCH_CONTEXT_KILL_MODE, &init->context ));

} /* End of os_init       */
