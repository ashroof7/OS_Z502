/*
 * base.h
 *
 *  Created on: Jan 28, 2013
 *      Author: Ashraf Saleh
 */

#ifndef BASE_H_
#define BASE_H_

#include <pthread.h>

int isr_done;
pthread_cond_t int_cond;
pthread_mutex_t mutex;

#define SYSNUM_DISPATCH 16

#define TIMER_INF (unsigned int)0 - 1

#define ERR_PROCESS_CREATION 22L
#define ERR_ILLEGAL_PROCESS_STATE 23L
#define ERR_IPC_FAIL 24L
#define ERR_FORCED_RESUME 25L
#define ERR_ILLEGAL_PRIORITY 26L
#define ERR_MSG_MAIL_BOX_FULL 27L

#define MAILBOX_SIZE 8
#define MSG_RCV 0
#define MSG_BUSY 1
#define MSG_FREE MSG_RCV
#define MAX_MSG_SIZE 256

typedef struct {
	int sender;
	int receiver;
	char message[MAX_MSG_SIZE];
	int message_length;
	int status;
} Message;

typedef struct {
	int waiting_on_pid;
	char *buffer;
	int buffer_size;
	Message* msg;
	int fail;
} IPC;

 Message mailbox[MAILBOX_SIZE];

// initialized in os_init = TIMER_INF
 unsigned int nxt_timer_interrupt ;

#endif /* BASE_H_ */


