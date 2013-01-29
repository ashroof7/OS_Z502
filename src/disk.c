/*
 * disk.c
 *
 *  Created on: Jan 29, 2013
 *      Author: Ashraf Saleh
 */
#include "disk.h"
#include "global.h"
#include "z502.h"
#include "syscalls.h"


void disk_action(INT32 disk_id, INT32 sector, INT32* buffer_ptr, INT32 RW){
			CALL(MEM_WRITE(Z502DiskSetSector, &sector));
			CALL(MEM_WRITE(Z502DiskSetBuffer,(INT32*) buffer_ptr));
			CALL(MEM_WRITE(Z502DiskSetAction, &RW ));
			// mark sector as used
			setSector(disk_id, sector);
			// start Disk
			CALL(MEM_WRITE(Z502DiskStart, &DISK_START));
}

void add_disk_request(INT32 disk_id, INT32 sector, INT32* buffer_ptr, INT32 RW){
	if (next_req_ins == (next_req_remove))
			printf("** ERROR: # of disk request is too large to handle\n");
	requests[next_req_ins] = (disk_request){disk_id,sector,buffer_ptr,RW };
	next_req_ins =(next_req_ins+1)%MAX_DISK_REQUESTS;

}

disk_request* remove_disk_request(){
	next_req_remove = (next_req_remove +1)%MAX_DISK_REQUESTS;
	if (next_req_remove == next_req_ins)
		printf("** ERROR: out of disk requests (trying to get a request from an empty Q)\n");
	disk_request* ret;
	ret = &requests[next_req_remove];
	return ret;
}
