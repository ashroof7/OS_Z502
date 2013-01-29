/*
 * disk.h
 *
 *  Created on: Jan 29, 2013
 *      Author: Ashraf Saleh
 */

#ifndef DISK_H_
#define DISK_H_
#include "global.h"

#define isSectorSet(diskNum, sectorNum) ((bitTable[diskNum][sectorNum/64])&(1<<(sectorNum%64)))
#define setSector(diskNum, sectorNum) ((bitTable[diskNum][sectorNum/64])&=(1<<(sectorNum%64)))
#define clearSector(diskNum, sectorNum) ((bitTable[diskNum][sectorNum/64])&=(~(1<<(sectorNum%64))))
#define MAX_DISK_REQUESTS 120

INT32 DISK_START;

long long bitTable[MAX_NUMBER_OF_DISKS][25];
//where 25 is the number of long long to represent one disk


typedef struct disk_request disk_request;

struct disk_request{
	INT32 id;
	INT32 sector;
	INT32* buf_ptr;
	INT32 rw;
	disk_request* next;
	disk_request* prev;
};

int next_req_ins;
int next_req_remove;


disk_request requests[MAX_DISK_REQUESTS];


void disk_action(INT32 disk_id, INT32 sector, INT32* buffer_ptr, INT32 RW);
void add_disk_request(INT32 disk_id, INT32 sector, INT32* buffer_ptr, INT32 RW);
disk_request* remove_disk_request();

#endif /* DISK_H_ */
