/*
 * diskManager.c
 *
 *  Created on: Aug 29, 2019
 *      Author: jean-philippe
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "global.h"
#include "syscalls.h"
#include "protos.h"
#include "diskManager.h"
#include "moreGlobals.h"
#include "processManager.h"
#include "dispatcher.h"
#include "fileSystem.h"

/**
 * Initializes the disk manager by creating the disk queue.
 */
void initDiskManager() {

	diskQueueId = QCreate("diskQueue");

}

/**
 * This adds the currently running process to
 * the disk queue, requesting the given diskID
 * Parameters:
 * diskID: the diskID requested by the currently running process.
 * currentlyUsing: whether the process is currently using disk.
 */
void addToDiskQueue(long diskID, int currentlyUsing) {

	DiskRequest* req = malloc(sizeof(DiskRequest));
	req->diskID = diskID;
	req->process = currentProcess();
	req->currentlyUsing = currentlyUsing;

	QInsertOnTail(diskQueueId, req);

}

/**
 * Removes from the disk queue the first
 * disk request that requested the given diskID.
 * Parameters:
 * diskID: the diskID that was requested
 * ignoreCurrentlyUsing: whether we should ignore processes using disk.
 * Returns: the first process found that requested that diskID,
 * or -1 if none found
 */
Process* removeFromDiskQueue(long diskID, int ignoreCurrentlyUsing) {

	int i = 0;
	DiskRequest* req = QWalk(diskQueueId, i);

	//iterate through disk queue
	//until we find a request for diskID.
	//remove, then return it.
	while((int)req != -1) {

		if(req->diskID == diskID) {

			if(ignoreCurrentlyUsing) {

				if(req->currentlyUsing != 1) {
					QRemoveItem(diskQueueId, req);
					return req->process;
				}

			} else {

				QRemoveItem(diskQueueId, req);
				return req->process;

			}

		}

		++i;
		req = QWalk(diskQueueId, i);

	}

	return (Process*)-1;

}

/**
 * Writes to a disk with a given ID at a given sector.
 * Parameters:
 * diskID: the diskID to write to.
 * sector: the sector to write to.
 * writeBuffer: the address of outgoing data.
 */
void writeToDisk(long diskID, long sector, char* writeBuffer) {


	//ask disk to write.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502DiskWrite;
	mmio.Field1 = diskID;
	mmio.Field2 = sector;
	mmio.Field3 = (long)writeBuffer;

	//if the disk is being used, wait.
	/*while(getDiskStatus(diskID) == DEVICE_IN_USE) {
		addToDiskQueue(diskID);
		diskUnlock();
		dispatch();
	}

	diskUnlock();

	diskLock();
	MEM_WRITE(Z502Disk, &mmio);

	//we have to wait for the disk to finish before we continue.
	addToDiskQueue(diskID);
	diskUnlock();
	dispatch();*/

	while(1) {

		diskLock();

		if(getDiskStatus(diskID) == DEVICE_IN_USE) {

			addToDiskQueue(diskID, 0);
			diskUnlock();
			dispatch();

		} else {

			MEM_WRITE(Z502Disk, &mmio);
			addToDiskQueue(diskID, 1);
			diskUnlock();
			dispatch();
			break;

		}

	}

}

/*
 * Whether 2 buffers are equal.
 */
int areEqual(char* buf1, char* buf2) {

	for(int i = 0; i<PGSIZE; i++) {

		if(buf1[i] != buf2[i]) {
			return 0;
		}

	}

	return 1;

}

/**
 * Reads from a disk with a given ID at a given sector.
 * Parameters:
 * diskID: the diskID to read from.
 * sector: the sector to read from.
 * readBuffer: the address to send the data to.
 */
void readFromDisk(long diskID, long sector, char* readBuffer) {

	//ask for disk read.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502DiskRead;
	mmio.Field1 = diskID;
	mmio.Field2 = sector;
	mmio.Field3 = (long)readBuffer;

	//if the disk is being used, wait.
	/*while(getDiskStatus(diskID) == DEVICE_IN_USE) {
		addToDiskQueue(diskID);
		diskUnlock();
		dispatch();
	}

	diskUnlock();

	diskLock();
	MEM_WRITE(Z502Disk, &mmio);

	//we have to wait for the disk to finish before we continue.
	addToDiskQueue(diskID);
	diskUnlock();
	dispatch();*/

	while(1) {

		diskLock();

		if(getDiskStatus(diskID) == DEVICE_IN_USE) {

			//aprintf("Process %d add from read: DISK IN USE.\n", currentProcess()->pid);
			addToDiskQueue(diskID, 0);
			diskUnlock();
			//aprintf("Process %d dispatching: DISK IN USE.\n", currentProcess()->pid);
			dispatch();

		} else {

			MEM_WRITE(Z502Disk, &mmio);
			//aprintf("Process %d add from read: STARTED READ.\n", currentProcess()->pid);
			addToDiskQueue(diskID, 1);
			diskUnlock();
			//aprintf("Process %d dispatching: STARTED READ.\n", currentProcess()->pid);
			dispatch();
			break;

		}

	}

	//while(areEqual(readBuffer, tempBuffer));

}

/*
 * Checks a disk with a given ID. Prints the disk's contents to a file.
 * Parameters:
 * diskID: the ID of the disk to check.
 */
void checkDisk(long diskID) {

	flushDiskContents(diskID);

	//make request to hardware to check the disk.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502CheckDisk;
	mmio.Field1 = diskID;
	mmio.Field2 = 0;
	mmio.Field3 = 0;
	mmio.Field4 = 0;

	MEM_WRITE(Z502Disk, &mmio);

}

/*
 * Checks the status of a given disk.
 * Parameters:
 * diskID: the ID of the disk to check.
 * Returns the status of the disk.
 */
long getDiskStatus(long diskID) {
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502Status;
	mmio.Field1 = diskID;
	mmio.Field2 = 0;
	mmio.Field3 = 0;
	mmio.Field4 = 0;

	MEM_READ(Z502Disk, &mmio);
	return mmio.Field2;

}
