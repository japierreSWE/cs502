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
#include "processManager.h"

/**
 * Initializes the disk manager by preparing the queues
 * for each disk.
 */
void initDiskManager() {

	diskQueueIds = (int *)calloc(MAX_NUMBER_OF_DISKS, sizeof(int));

	//get the queue id's for each disk
	for(int i = 0; i<MAX_NUMBER_OF_DISKS; i++) {

		char* name = (char *)calloc(6, sizeof(char));
		name = "disk";
		char number[1];
		sprintf(number, "%d", i);
		strcat(name, number);
		diskQueueIds[i] = QCreate(name);

	}

}

/**
 * Writes to a disk with a given ID at a given sector.
 * Parameters:
 * diskID: the diskID to write to.
 * sector: the sector to write to.
 * writeBuffer: the address of outgoing data.
 */
void writeToDisk(long diskID, long sector, char* writeBuffer) {

	//TODO: check that disk is free and wait if in use.

	//ask disk to write.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502DiskWrite;
	mmio.Field1 = diskID;
	mmio.Field2 = sector;
	mmio.Field3 = (long)writeBuffer;

	MEM_WRITE(Z502Disk, &mmio);

	if(mmio.Field4 == ERR_BAD_PARAM) {
		printf("Invalid parameter for disk write.\n");
	} else if(mmio.Field4 == ERR_DISK_IN_USE) {
		printf("Disk write attempted when disk is already in use.\n");
	}

	//TODO: add to disk queue.

	//wait for disk to write.
	idle();

}

/**
 * Reads from a disk with a given ID at a given sector.
 * Parameters:
 * diskID: the diskID to read from.
 * sector: the sector to read from.
 * readBuffer: the address to send the data to.
 */
void readFromDisk(long diskID, long sector, char* readBuffer) {

	//TODO: check that disk is free and wait if in use.

	//ask for disk read.
	MEMORY_MAPPED_IO mmio;
	mmio.Mode = Z502DiskRead;
	mmio.Field1 = diskID;
	mmio.Field2 = sector;
	mmio.Field3 = (long)readBuffer;

	MEM_WRITE(Z502Disk, &mmio);

	if(mmio.Field4 == ERR_BAD_PARAM) {
		printf("ERROR: Invalid parameter for disk write.\n");
	} else if(mmio.Field4 == ERR_DISK_IN_USE) {
		printf("ERROR: Disk write attempted when disk is already in use.\n");
	} else if(mmio.Field4 == ERR_NO_PREVIOUS_WRITE) {
		printf("ERROR: Read from a sector that hasn't been written to.\n");
	}

	//wait to read from disk.
	idle();

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
