/*
 * fileSystem.c
 *
 *  Created on: Oct 26, 2019
 *      Author: jean-philippe
 */

#include "moreGlobals.h"
#include "global.h"
#include "syscalls.h"
#include "protos.h"
#include "diskManager.h"
#include <stdlib.h>

int rootSector = 0x11;
int bitmapSector = 0x01;
int bitmapSize; //# of sectors the bitmap takes up.

/**
 * Formats a disk for it to be used
 * for file management.
 * Parameters:
 * diskID: the ID of the disk to format.
 * Returns:
 * 0 if successful, -1 if an error occurred.
 */
int formatDisk(int diskID) {

	if(diskID < 0 || diskID >= MAX_NUMBER_OF_DISKS) {
		return -1;
	}

	currentProcess()->currentDisk = diskID;

	char* sectorZeroBuffer = malloc(PGSIZE * sizeof(char));

	sectorZeroBuffer[0] = 'Z';

	char diskIDChar = 0x00 + diskID;

	sectorZeroBuffer[1] = diskIDChar;

	//prepare required chars at end of string.
	sectorZeroBuffer[15] = 46;
	sectorZeroBuffer[12] = '\0';
	sectorZeroBuffer[13] = '\0';
	sectorZeroBuffer[14] = '\0';

	//put in disk length.
	sectorZeroBuffer[3] = 0x08;
	sectorZeroBuffer[2] = 0x00;

	//put in bitmap size.
	sectorZeroBuffer[4] = 0x04;

	//initialize bitmap size.
	bitmapSize = 4*sectorZeroBuffer[4];

	//put in swap size.
	sectorZeroBuffer[5] = 0x80;

	//put in bitmap location.
	sectorZeroBuffer[7] = 0x00;
	sectorZeroBuffer[6] = 0x01;

	//put in root dir location.
	sectorZeroBuffer[9] = 0x00;
	sectorZeroBuffer[8] = 0x11;

	//put in swap location.
	sectorZeroBuffer[11] = 0x06;
	sectorZeroBuffer[10] = 0x00;

	writeToDisk(diskID, 0, sectorZeroBuffer);

	//we use this for all sectors other than the 0th one.
	//write part of bitmap covering the swap space.
	char* tempBuffer = malloc(PGSIZE * sizeof(char));

	for(int i = 0; i<PGSIZE; i++) {
		tempBuffer[i] = 0xFF;
	}

	writeToDisk(diskID, 0x0D, tempBuffer);
	writeToDisk(diskID, 0x0E, tempBuffer);
	writeToDisk(diskID, 0x0F, tempBuffer);
	writeToDisk(diskID, 0x10, tempBuffer);

	//part of bitmap for lines 0-1B
	tempBuffer[3] = 0xE0;

	for(int i = 4; i<PGSIZE; i++) {
		tempBuffer[i] = 0x00;
	}

	writeToDisk(diskID, bitmapSector, tempBuffer);


	//now we make the buffer contain root's header
	tempBuffer[0] = 0x00;
	tempBuffer[1] = 'r';
	tempBuffer[2] = 'o';
	tempBuffer[3] = 'o';
	tempBuffer[4] = 't';
	tempBuffer[5] = '\0';
	tempBuffer[6] = '\0';
	tempBuffer[7] = '\0';

	//put time in the header.
	long currTime = getTimeOfDay();
	char* timeBytes = (char*)&currTime;

	tempBuffer[10] = timeBytes[2];
	tempBuffer[9] = timeBytes[1];
	tempBuffer[8] = timeBytes[0];

	//put file description in the header.
	int fileDescription = 0;
	fileDescription += 1; //this is a directory.

	fileDescription += (2 << 1); //add index level

	fileDescription += (31 << 3); //parent inode, which is 31 for root

	tempBuffer[11] = fileDescription;

	//index location.
	tempBuffer[13] = 0x00;
	tempBuffer[12] = 0x12;

	tempBuffer[15] = 0x00;
	tempBuffer[14] = 0x00;

	writeToDisk(diskID, rootSector, tempBuffer);


	//now we make the index sector.
	tempBuffer[0] = 0x13;
	tempBuffer[1] = 0x00;
	tempBuffer[2] = 0x14;
	tempBuffer[3] = 0x00;
	tempBuffer[4] = 0x15;
	tempBuffer[5] = 0x00;
	tempBuffer[6] = 0x16;
	tempBuffer[7] = 0x00;
	tempBuffer[8] = 0x17;
	tempBuffer[9] = 0x00;
	tempBuffer[10] = 0x18;
	tempBuffer[11] = 0x00;
	tempBuffer[12] = 0x19;
	tempBuffer[13] = 0x00;
	tempBuffer[14] = 0x1A;
	tempBuffer[15] = 0x00;

	writeToDisk(diskID, 0x12, tempBuffer);

	//we don't need these anymore
	free(tempBuffer);
	free(sectorZeroBuffer);

	return 0;

}

/*
 * Finds the first open sector available
 * by looking for a 0 bit in the bitmap.
 * Returns the index of that bit, ie. the
 * sector to use.
 * Returns -1 if there are no available sectors.
 */
long findOpenSector() {

	int index = 0;
	char* tempBuffer = malloc(PGSIZE * sizeof(char));
	int foundAvailable = 0;

	for(int i = bitmapSector; i<bitmapSector+bitmapSize; i++) {

		readFromDisk(currentProcess()->currentDisk, i, tempBuffer);

		for(int j = 0; j<PGSIZE; j++) {

			for(int k = 0; k<4; k++) {

				int masked = tempBuffer[j] & (1 << k);

				if(masked >> k == 1) {
					return index;
				}

				++index;

			}

		}

	}

	return -1;

}

/**
 * Opens a directory, making it the current directory.
 * Parameters:
 * diskID: the diskID to open the directory in.
 * if -1, find the directory in the current directory.
 */
int openDir(int diskID, char* directoryName) {

	if(diskID < 0 || diskID >= MAX_NUMBER_OF_DISKS) {
		return -1;
	}

	//if this is our first dir, basically.
	if(currentProcess()->currentDirectorySector == -1 && strcmp(directoryName,"root") == 0) {

		currentProcess()->currentDirectorySector = rootSector;

	}

	return 0;

}
