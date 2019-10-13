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

/**
 * Initializes the disk manager by preparing the queues
 * for each disk.
 */
void initDiskManager() {

	diskQueueId = QCreate("diskQueue");

}

/**
 * This adds the currently running process to
 * the disk queue, requesting the given diskID
 * Parameters:
 * diskID: the diskID requested by the currently running process.
 */
void addToDiskQueue(long diskID) {

	DiskRequest* req = malloc(sizeof(DiskRequest));
	req->diskID = diskID;
	req->process = currentProcess();

	diskLock();
	QInsertOnTail(diskQueueId, req);
	diskUnlock();

}

/**
 * Removes from the disk queue the first
 * disk request that requested the given diskID.
 * Parameters:
 * diskID: the diskID that was requested
 * Returns: the first process found that requested that diskID,
 * or -1 if none found
 */
Process* removeFromDiskQueue(long diskID) {

	diskLock();
	int i = 0;
	DiskRequest* req = QWalk(diskQueueId, i);

	while((int)req != -1) {

		if(req->diskID == diskID) {

			QRemoveItem(diskQueueId, req);
			diskUnlock();
			return req->process;
		}

		++i;
		req = QWalk(diskQueueId, i);

	}
	diskUnlock();

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

	while(getDiskStatus(diskID) == DEVICE_IN_USE) {
		addToDiskQueue(diskID);
		dispatch();
	}

	MEM_WRITE(Z502Disk, &mmio);

	//we have to wait for the disk to finish before we continue.
	addToDiskQueue(diskID);
	dispatch();

	if(mmio.Field4 == ERR_BAD_PARAM) {
		aprintf("ERROR: Invalid parameter for disk write.\n");
		exit(0);
	} else if(mmio.Field4 == ERR_DISK_IN_USE) {
		aprintf("ERROR: Disk write attempted when disk is already in use.\n");
		exit(0);
	}

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

	if(getDiskStatus(diskID) == DEVICE_IN_USE) {
		addToDiskQueue(diskID);
		dispatch();
	}

	MEM_WRITE(Z502Disk, &mmio);

	//we have to wait for the disk to finish before we continue.
	addToDiskQueue(diskID);
	dispatch();

	if(mmio.Field4 == ERR_BAD_PARAM) {
		aprintf("ERROR: Invalid parameter for disk read.\n");
	} else if(mmio.Field4 == ERR_DISK_IN_USE) {
		aprintf("ERROR: Disk read attempted when disk is already in use.\n");
	} else if(mmio.Field4 == ERR_NO_PREVIOUS_WRITE) {
		aprintf("ERROR: Read from a sector that hasn't been written to.\n");
	}

}

/*
 * Checks a disk with a given ID. Prints the disk's contents to a file.
 * Parameters:
 * diskID: the ID of the disk to check.
 */
void checkDisk(long diskID) {

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
