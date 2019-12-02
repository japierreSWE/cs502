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
#include "processManager.h"
#include "fileSystem.h"
#include "memoryManager.h"
#include <stdlib.h>
#include <string.h>

//A structure representing an open file.
//inode: the file's inode.
//sector: the sector at which its file header is located.
typedef struct {

	long inode;
	int sector;

} OpenFile;

//TODO: find the bug in test29 that causes occaisional freezing

void initDiskContents();
int isUnwritten(unsigned char* buffer);
int isDir(unsigned char* buffer);
long findOpenSector();
int findUnwrittenIndex();
void insertName(char* fileName, unsigned char* buffer);
int hasName(unsigned char* buffer, char* fileName);
OpenFile* isOpen(int inode);
char* getName(unsigned char* buffer);

int rootSector = 0x11; //the sectors of the root directory and bitmap.
int bitmapSector = 0x01;
int bitmapSize; //# of sectors the bitmap takes up.
int currentInode = 0x01; //the next inode we'll use for a file.
int openFilesQueueId;

unsigned char** diskContents; //the contents of the disk stored in memory.

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

	initDiskContents();
	formattedDisk = diskID;

	openFilesQueueId = QCreate("openFilesQ");

	unsigned char* sectorZeroBuffer = malloc(PGSIZE * sizeof(char));

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

	writeToDisk(diskID, 0, (char*)sectorZeroBuffer);

	bufferCopy(sectorZeroBuffer, diskContents[0]);

	//we use this for all sectors other than the 0th one.
	//write part of bitmap covering the swap space.
	unsigned char* tempBuffer = malloc(PGSIZE * sizeof(char));

	for(int i = 0; i<PGSIZE; i++) {
		tempBuffer[i] = 0xFF;
	}

	writeToDisk(diskID, 0x0D, (char*)tempBuffer);
	writeToDisk(diskID, 0x0E, (char*)tempBuffer);
	writeToDisk(diskID, 0x0F, (char*)tempBuffer);
	writeToDisk(diskID, 0x10, (char*)tempBuffer);

	bufferCopy(tempBuffer, diskContents[0x0D]);
	bufferCopy(tempBuffer, diskContents[0x0E]);
	bufferCopy(tempBuffer, diskContents[0x0F]);
	bufferCopy(tempBuffer, diskContents[0x10]);

	//part of bitmap for lines 0-1B
	tempBuffer[3] = 0xE0;

	for(int i = 4; i<PGSIZE; i++) {
		tempBuffer[i] = 0x00;
	}

	writeToDisk(diskID, bitmapSector, (char*)tempBuffer);

	bufferCopy(tempBuffer, diskContents[bitmapSector]);


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

	writeToDisk(diskID, rootSector, (char*)tempBuffer);

	bufferCopy(tempBuffer, diskContents[rootSector]);

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

	writeToDisk(diskID, 0x12, (char*)tempBuffer);

	bufferCopy(tempBuffer, diskContents[0x12]);

	//we don't need these anymore
	free(tempBuffer);
	free(sectorZeroBuffer);

	return 0;

}

/**
 * Copies the contents from the buffer src
 * to the buffer dest.
 * Src and dest are buffers representing
 * disk sectors.
 */
void bufferCopy(unsigned char* src, unsigned char* dest) {

	for(int i = 0; i<PGSIZE; i++) {

		dest[i] = src[i];

	}

}

/**
 * Initializes the pointers used to
 * keep the disk contents in memory.
 */
void initDiskContents() {

	diskContents = calloc(NUMBER_LOGICAL_SECTORS, sizeof(char*));

	for(int i = 0; i<NUMBER_LOGICAL_SECTORS; i++) {

		diskContents[i] = calloc(PGSIZE, sizeof(char));

		//initialize sectors with zeroes.
		for(int j = 0; j<PGSIZE; j++) {

			diskContents[i][j] = '\0';

		}

	}

}

/**
 * Writes the contents of
 * diskContents to disk
 * so that they can be
 * shown in checkDisk.
 */
void flushDiskContents() {

	//diskContentsLock();
	long index = 0;
	//update the bitmap and all
	//sectors that have been indicated
	//in the bitmap.
	for(int i = bitmapSector; i<bitmapSector+bitmapSize; i++) {

		unsigned char* sector = diskContents[i];

		//if we haven't written to this part of the bitmap.
		if(isUnwritten(sector)) {
			return;
		} else {
			writeToDisk(currentProcess()->currentDisk, i, (char*)sector);
		}

		for(int j = 0; j<PGSIZE; j++) {

			for(int k = 0; k<8; k++) {

				int shiftAmount = (7 - k); //starts at 3, goes to 0
				int masked = sector[j] & (1 << shiftAmount);

				//if we find a bit.
				if(masked >> shiftAmount == 1) {

					writeToDisk(currentProcess()->currentDisk, index, (char*)diskContents[index]);

				}

				++index;

			}

		}

	}
	//diskContentsUnlock();
}

/*
 * Finds the first open sector available
 * in the bitmap by looking for a 0 bit
 * Returns the index of that bit, ie. the
 * sector to use.
 * Returns -1 if there are no available sectors.
 */
long findOpenSector() {

	long index = 0;
	unsigned char* tempBuffer = malloc(PGSIZE * sizeof(char));

	for(int i = bitmapSector; i<bitmapSector+bitmapSize; i++) {

		unsigned char* sector = diskContents[i];

		//if we haven't written to this part of the bitmap.
		if(isUnwritten(sector)) {
			sector[0] = sector[0] | (1 << 7);
			bufferCopy(sector, diskContents[i]);
			free(tempBuffer);
			return index;
		}

		for(int j = 0; j<PGSIZE; j++) {

			for(int k = 0; k<8; k++) {

				int shiftAmount = (7 - k); //starts at 7, goes to 0
				int masked = sector[j] & (1 << shiftAmount);

				//if we find a zero bit.
				if(masked >> shiftAmount != 1) {

					//flip the bit, then write.
					sector[j] = sector[j] | (1 << shiftAmount);
					bufferCopy(sector, diskContents[i]);
					free(tempBuffer);
					return index;
				}

				++index;

			}

		}

	}
	free(tempBuffer);
	return -1;

}

/**
 * Finds an index that hasn't been written
 * to yet in the current directory. If there
 * is none in the current index level, it checks
 * if there is an index level after that. If there is
 * none, it creates a new index level and updates the directory
 * accordingly. Otherwise, it checks the next index level.
 * Returns an unwritten index or -1 if there isn't enough space.
 */
int findUnwrittenIndex() {

	unsigned char* tempBuffer = malloc(PGSIZE * sizeof(char));
	unsigned char* dirBuffer = malloc(PGSIZE * sizeof(char));
	int sectorNumber;

	unsigned char* currentDirectory = diskContents[currentProcess()->currentDirectorySector];

	int indexMsb = currentDirectory[13];
	int indexLsb = currentDirectory[12];

	int indexSector = (indexMsb << 8) + indexLsb;

	unsigned char* indexSectorBuffer =diskContents[indexSector];

	for(int i = 0; i<PGSIZE; i=i+2) {

		int msb = indexSectorBuffer[i+1];
		int lsb = indexSectorBuffer[i];

		//shift msb so that it can be combined w/ lsb.
		sectorNumber = (msb << 8) + lsb;

		//looking for an index not already written to.
		if(isUnwritten(diskContents[sectorNumber])) {

			free(tempBuffer);
			free(dirBuffer);
			return sectorNumber;

		}

	}

	return -1;

}

/**
 * Places the name of a file in a file header.
 * Parameters:
 * fileName: a file name < 8 characters long.
 * buffer: a character array to be used as a file header.
 */
void insertName(char* fileName, unsigned char* buffer) {

	int nameCursor = 1;

	for(int i = 0; i<7; i++) {

		if(fileName[i] == '\0') {
			break;
		} else {

			buffer[nameCursor] = fileName[i];
			++nameCursor;

		}

	}

	//fill rest of name field with zeros.
	for(; nameCursor<8; nameCursor++) {
		buffer[nameCursor] = '\0';
	}

}

/**
 * Determines whether a file has a given name.
 * Parameters:
 * fileName: the name we are looking for.
 * buffer: a buffer representing a file header.
 * Returns 1 if the name in buffer is equal to fileName
 * Returns 0 otherwise.
 */
int hasName(unsigned char* buffer, char* fileName) {

	for(int i = 0; fileName[i]!='\0'; i++) {

		if(buffer[i+1] != fileName[i]) return 0;

	}

	return 1;

}

/**
 * Checks a disk sector and determines
 * whether it has been written to.
 * Parameters:
 * buffer: a buffer representing a disk sector.
 * Returns 1 if the buffer isn't written to.
 * Returns 0 otherwise.
 */
int isUnwritten(unsigned char* buffer) {

	for(int i = 0; i<PGSIZE; i++) {

		if(buffer[i] != '\0') {
			return 0;
		}

	}

	return 1;

}

/**
 * Returns whether a file header is a
 * directory.
 * Parameters:
 * buffer: a buffer representing a file header.
 * Returns 1 if the file header is a buffer.
 * Returns 0 otherwise.
 */
int isDir(unsigned char* buffer) {
	char description = buffer[11];

	//directories have their 1st bit set.
	//files don't.
	int result = description & 0x01;

	if(result == 1) return 1;
	else return 0;

}

/**
 * Opens a directory, making it the current directory.
 * Parameters:
 * diskID: the diskID to open the directory in.
 * if -1, find the directory in the current directory.
 * Returns 0 if successful. Returns -1 if an error occurred.
 */
int openDir(int diskID, char* directoryName) {

	//diskID out of range
	if(diskID >= MAX_NUMBER_OF_DISKS) {
		return -1;
	}

	//we don't have a current directory yet, we can't use diskID=-1.
	if(diskID == -1 && currentProcess()->currentDirectorySector == -1) {
		return -1;
	}

	//if this is our first dir, basically.
	if(currentProcess()->currentDirectorySector == -1 && strcmp(directoryName,"root") == 0) {

		currentProcess()->currentDirectorySector = rootSector;
		currentProcess()->currentDisk = diskID;

		return 0;

	}

	unsigned char* dirBuffer = malloc(PGSIZE * sizeof(char));

	int currentSector = currentProcess()->currentDirectorySector;
	unsigned char* tempBuffer = diskContents[currentSector];

	int indexMsb = tempBuffer[13];
	int indexLsb = tempBuffer[12];

	int indexSector = (indexMsb << 8) + indexLsb;

	tempBuffer = diskContents[indexSector];

	int cursor = 0;

	do {

		indexLsb = tempBuffer[cursor];
		indexMsb = tempBuffer[cursor+1];

		indexSector = (indexMsb << 8) + indexLsb;

		dirBuffer = diskContents[indexSector];

		if(isUnwritten(dirBuffer)) {
			//we didn't find the file.
			int createResult = createDir(directoryName);

			if(createResult == 0) return openDir(diskID, directoryName);

			else return -1;

		}


		if(cursor == 14) {

			aprintf("Ran out of space for dirs on open.\n");
			exit(0);

		}
		//the file has the name we're looking for
		//and is a directory.
		else if(hasName(dirBuffer, directoryName) && isDir(dirBuffer)) {

			currentProcess()->currentDirectorySector = indexSector;
			return 0;

		}
		 else {
			//we haven't reached the end yet.
			cursor+=2;
		}

	} while(1);

}

/**
 * Creates a directory with a given
 * name in the current directory.
 * Parameters:
 * name: the name of the directory to be created.
 * Returns 0 if successful. Returns -1 if an error occurred.
 */
int createDir(char* directoryName) {

	if(strlen(directoryName) > 7) {
		return -1;
	}

	unsigned char* tempBuffer = malloc(PGSIZE * sizeof(char));
	unsigned char* dirBuffer = malloc(PGSIZE * sizeof(char));

	int sectorNumber = findUnwrittenIndex();

	if(sectorNumber == -1) {
		aprintf("Create Dir: Not enough space for dir.\n");
		return -1;
	}

	unsigned char* currentDirectory = diskContents[currentProcess()->currentDirectorySector];

	int parentInode = currentDirectory[0];

	//we make the file header.

	//put in inode.
	dirBuffer[0] = currentInode;
	++currentInode;

	//put in name from directoryName
	insertName(directoryName, dirBuffer);

	//put time in the header.
	long currTime = getTimeOfDay();

	dirBuffer[10] = currTime & 0xFF;
	dirBuffer[9] = (currTime >> 8) & 0xFF;
	dirBuffer[8] = (currTime >> 16) & 0xFF;

	int fileDescription = 1;
	fileDescription += (2 << 1);
	fileDescription += (parentInode << 3);

	dirBuffer[11] = fileDescription;

	//find the index sector for the new dir, and put its
	//msb and lsb in the buffer.
	diskContentsLock();
	long newDirIndex = findOpenSector();
	dirBuffer[13] = (newDirIndex >> 8) & 0xFF;
	dirBuffer[12] = newDirIndex & 0xFF;

	dirBuffer[14] = '\0';
	dirBuffer[15] = '\0';

	//write file header
	bufferCopy(dirBuffer, diskContents[sectorNumber]);

	for(int i = 0; i<PGSIZE; i=i+2) {

		long indexLocation = findOpenSector();
		dirBuffer[i+1] = (indexLocation >> 8) & 0xFF;
		dirBuffer[i] = indexLocation & 0xFF;

	}

	//write file index.
	bufferCopy(dirBuffer, diskContents[newDirIndex]);
	diskContentsUnlock();
	free(tempBuffer);
	free(dirBuffer);
	return 0;

}

/**
 * Creates a file with a given
 * name in the current directory.
 * Parameters:
 * name: the name of the file to be created.
 * Returns 0 if successful. Returns -1 if an error occurred.
 *
 * NOTE: files have an index level of 3. Not all sectors are
 * allocated at first, but are allocated as needed. ie. they're
 * allocated based on WRITE_FILE calls.
 */
int createFile(char* fileName) {

	if(strlen(fileName) > 7) {
		return -1;
	}

	unsigned char* tempBuffer = malloc(PGSIZE * sizeof(char));
	unsigned char* fileBuffer = malloc(PGSIZE * sizeof(char));

	int sectorNumber = findUnwrittenIndex();

	if(sectorNumber == -1) {
		aprintf("Create Dir: Not enough space for dir.\n");
		return -1;
	}

	unsigned char* currentDirectory = diskContents[currentProcess()->currentDirectorySector];

	int parentInode = currentDirectory[0];

	//we make the file header.

	//put in inode.
	fileBuffer[0] = currentInode;
	++currentInode;

	//put in name from directoryName
	insertName(fileName, fileBuffer);

	//put time in the header.
	long currTime = getTimeOfDay();

	fileBuffer[10] = currTime & 0xFF;
	fileBuffer[9] = (currTime >> 8) & 0xFF;
	fileBuffer[8] = (currTime >> 16) & 0xFF;

	int fileDescription = 0;
	fileDescription += (3 << 1);
	fileDescription += (parentInode << 3);

	fileBuffer[11] = fileDescription;

	//find the index sector for the new dir, and put its
	//msb and lsb in the buffer.
	diskContentsLock();
	long newFileIndex = findOpenSector();
	fileBuffer[13] = (newFileIndex >> 8) & 0xFF;
	fileBuffer[12] = newFileIndex & 0xFF;

	fileBuffer[14] = '\0';
	fileBuffer[15] = '\0';

	//write file header
	bufferCopy(fileBuffer, diskContents[sectorNumber]);

	for(int i = 0; i<PGSIZE; i=i+2) {

		long indexLocation = findOpenSector();
		fileBuffer[i+1] = (indexLocation >> 8) & 0xFF;
		fileBuffer[i] = indexLocation & 0xFF;

	}

	//write file index.
	bufferCopy(fileBuffer, diskContents[newFileIndex]);
	diskContentsUnlock();
	free(tempBuffer);
	free(fileBuffer);
	return 0;

}

/**
 * Opens a file with a given name
 * in the current directory.
 * Parameters:
 * fileName: the name of the file to be opened.
 * Returns the file's inode if successful
 * Returns -1 if an error occurs.
 */
int openFile(char* fileName) {

	unsigned char* fileBuffer = 0;

	int currentSector = currentProcess()->currentDirectorySector;
	unsigned char* tempBuffer = diskContents[currentSector];

	int indexMsb = tempBuffer[13];
	int indexLsb = tempBuffer[12];

	int indexSector = (indexMsb << 8) + indexLsb;

	tempBuffer = diskContents[indexSector];

	int cursor = 0;

	do {

		indexLsb = tempBuffer[cursor];
		indexMsb = tempBuffer[cursor+1];

		indexSector = (indexMsb << 8) + indexLsb;

		fileBuffer = diskContents[indexSector];

		if(isUnwritten(fileBuffer)) {
			//we didn't find the file.
			int createResult = createFile(fileName);

			if(createResult == 0) return openFile(fileName);

			else return -1;

		}


		if(cursor == 14) {

			aprintf("Ran out of space for files.\n");
			exit(0);

		}
		//the file has the name we're looking for
		//and is not a directory.
		else if(hasName(fileBuffer, fileName) && !isDir(fileBuffer)) {

			//we put this file in the open
			//files queue. its QOrder is the
			//inode number.
			OpenFile* file = malloc(sizeof(OpenFile));
			file->inode = fileBuffer[0];
			file->sector = indexSector;

			openFilesLock();
			QInsert(openFilesQueueId, fileBuffer[0], file);
			openFilesUnlock();


			return fileBuffer[0];

		}
		 else {
			//we haven't reached the end yet.
			cursor+=2;
		}

	} while(1);

}

/**
 * At a currently unwritten buffer which should be
 * an index sector, allocate a set of indices and
 * turn the buffer into an index sector.
 * Returns 0 if successful, returns -1 if an error occurred.
 */
int allocateIndices(unsigned char* buffer) {

	for(int i = 0; i<PGSIZE; i = i+2) {

		int indexLocation = findOpenSector();

		if(indexLocation == -1) return -1;

		buffer[i+1] = (indexLocation >> 8) & 0xFF;
		buffer[i] = indexLocation & 0xFF;

	}
	return 0;
}

/**
 * Finds the sector that a given datablock should be
 * found for a file's given top-level index sector.
 * Files have an index level of 3.
 * Parameters:
 * logicalBlock: the logical number of the datablock to find.
 * topIndexSector: the sector of the file's top-level index.
 * Returns the sector the datablock should be found at.
 */
int findDataBlockSector(int logicalBlock, int topIndexSector) {

	//we start at the top index.
	unsigned char* tempBuffer = diskContents[topIndexSector];

	//this will be used to calculate where to put the data block.
	int num = logicalBlock;

	int subtractionsBy64 = 0;

	while(num >= 64) {
		num -= 64;
		++subtractionsBy64;
	}

	int midIndexMsb = tempBuffer[2*subtractionsBy64 + 1];
	int midIndexLsb = tempBuffer[2*subtractionsBy64];

	int midIndexSector = (midIndexMsb << 8) + midIndexLsb;

	if(isUnwritten(diskContents[midIndexSector])) {
		int result = allocateIndices(diskContents[midIndexSector]);

		if(result == -1) {
			aprintf("Not enough space to allocate for datablock.\n");
			exit(0);
		}
	}

	tempBuffer = diskContents[midIndexSector];

	int subtractionsBy8 = 0;
	int lowIndex = num % 8; //which index will we put the data block in

	while(num >= 8) {
		num -= 8;
		++subtractionsBy8;
	}

	int lowIndexMsb = tempBuffer[2*subtractionsBy8 + 1];
	int lowIndexLsb = tempBuffer[2*subtractionsBy8];

	int lowIndexSector = (lowIndexMsb << 8) + lowIndexLsb;

	if(isUnwritten(diskContents[lowIndexSector])) {
		int result = allocateIndices(diskContents[lowIndexSector]);

		if(result == -1) {
			aprintf("Not enough space to allocate for datablock.\n");
			exit(0);
		}
	}

	tempBuffer = diskContents[lowIndexSector];

	int dataBlockIndexMsb = tempBuffer[2*lowIndex + 1];
	int dataBlockIndexLsb = tempBuffer[2*lowIndex];

	int dataBlockSector = (dataBlockIndexMsb << 8) + dataBlockIndexLsb;

	return dataBlockSector;
}

/**
 * Writes a given data block to a given file.
 * Parameters:
 * inode: the inode of the file to be written to.
 * logicalBlock: the block to be written.
 * writeBuffer: the data to be written to the block.
 * Returns 0 if successful. Returns -1 if an error occurs.
 */
int writeFile(long inode, int logicalBlock, char* writeBuffer) {

	//means this inode isn't open.
	if((int)isOpen(inode) == -1) {
		return -1;
	}

	//logical block outside of range.
	if(logicalBlock < 0 || logicalBlock >= 512) {
		return -1;
	}

	OpenFile* file = isOpen(inode);
	unsigned char* fileHeader = diskContents[file->sector];

	int topIndexMsb = fileHeader[13];
	int topIndexLsb = fileHeader[12];

	int topIndexSector = (topIndexMsb << 8) + topIndexLsb;

	diskContentsLock();
	int dataBlockSector = findDataBlockSector(logicalBlock, topIndexSector);
	diskContentsUnlock();

	//increase the file size.
	int fileSizeMsb = fileHeader[15];
	int fileSizeLsb = fileHeader[14];
	int fileSize = (fileSizeMsb << 8) + fileSizeLsb;

	++fileSize;

	fileHeader[15] = (fileSize >> 8) & 0xFF;
	fileHeader[14] = fileSize & 0xFF;

	bufferCopy((unsigned char*)writeBuffer, diskContents[dataBlockSector]);
	writeToDisk(currentProcess()->currentDisk, dataBlockSector, writeBuffer);

	return 0;

}

/**
 * Reads a given data block from a given file.
 * Parameters:
 * inode: the inode of the file to be written to.
 * logicalBlock: the block to be written.
 * readBuffer: the buffer which will contain data read.
 * Returns 0 if successful and -1 if an error occurred.
 */
int readFile(long inode, int logicalBlock, char* readBuffer) {

	//means this inode isn't open.
	if((int)isOpen(inode) == -1) {
		return -1;
	}

	//logical block outside of range.
	if(logicalBlock < 0 || logicalBlock >= 512) {
		return -1;
	}

	OpenFile* file = isOpen(inode);
	unsigned char* fileHeader = diskContents[file->sector];

	int topIndexMsb = fileHeader[13];
	int topIndexLsb = fileHeader[12];

	int topIndexSector = (topIndexMsb << 8) + topIndexLsb;

	diskContentsLock();
	int dataBlockSector = findDataBlockSector(logicalBlock, topIndexSector);
	diskContentsUnlock();

	readFromDisk(currentProcess()->currentDisk, dataBlockSector, readBuffer);
	return 0;
}

/**
 * Checks if a file with a given inode is open.
 * Parameters:
 * inode: the inode of the file to check.
 * Returns the address if the file is open, -1 otherwise.
 */
OpenFile* isOpen(int inode) {

	openFilesLock();
	int i = 0;
	OpenFile* curr = QWalk(openFilesQueueId, i);

	while((int)curr != -1) {

		if(curr->inode == inode) {
			openFilesUnlock();
			return curr;
		}

		++i;
		curr = QWalk(openFilesQueueId, i);
	}

	openFilesUnlock();
	return (OpenFile*)-1;

}

/**
 * Closes a file.
 * Parameters:
 * inode: the inode of the file to close.
 * Returns 0 if successful or -1 if an error occurred.
 */
int closeFile(long inode) {

	OpenFile* file = isOpen(inode);

	if((int)file == -1) {
		return -1;
	} else {
		openFilesLock();
		QRemoveItem(openFilesQueueId, file);
		openFilesUnlock();
		flushDiskContents();
		return 0;
	}

}

/**
 * Retrieves the name of a file from a file header.
 * Parameters:
 * buffer: a character buffer representing a file header
 * Returns the name of the file
 */
char* getName(unsigned char* buffer) {

	char* result = malloc(sizeof(char) * 8);
	int cursor = 0;

	for(int i = 1; i<8; i++) {
		result[cursor] = buffer[i];
		++cursor;
	}

	//ensure there is a terminator.
	result[cursor] = '\0';
	return result;
}

/**
 * Prints the contents of the
 * current directory and information
 * about those files.
 */
void dirContents() {

	int currentSector = currentProcess()->currentDirectorySector;
	unsigned char* tempBuffer = diskContents[currentSector];

	char* currentDirectoryName = getName(tempBuffer);

	int indexMsb = tempBuffer[13];
	int indexLsb = tempBuffer[12];

	int indexSector = (indexMsb << 8) + indexLsb;

	tempBuffer = diskContents[indexSector];

	aprintf("Contents of Directory %s:\n", currentDirectoryName);
	aprintf("Inode,\tFilename,\tD/F,\tCreation Time,\tFile Size\n");

	for(int i = 0; i<PGSIZE; i = i+2) {

		indexMsb = tempBuffer[i+1];
		indexLsb = tempBuffer[i];

		int fileSector = (indexMsb << 8) + indexLsb;

		if(isUnwritten(diskContents[fileSector])) {
			break;
		}

		unsigned char* fileHeader = diskContents[fileSector];
		char* name = getName(fileHeader);

		int inode = fileHeader[0];
		long creationTime = fileHeader[10] + (fileHeader[9] << 8) + (fileHeader[8] << 16);
		int fileSize = fileHeader[14] + (fileHeader[15] << 8);

		aprintf("%d\t", inode);
		aprintf("%s\t\t", name);

		if(isDir(fileHeader)) {
			aprintf("D\t");
		} else {
			aprintf("F\t");
		}

		aprintf("%ld\t\t", creationTime);

		if(isDir(fileHeader)) {
			aprintf("--");
		} else {
			aprintf("%d", fileSize);
		}

		aprintf("\n");

	}


}

/**
 * Writes a given frame's data to swap space.
 * Parameters:
 * frameData: the frame to write.
 */
void writeToSwapSpace(FrameData* frameData) {

	UINT16* pageTable = (UINT16*)getProcess(frameData->pid)->pageTable;
	char* pageData = calloc(PGSIZE, sizeof(char));

	Z502ReadPhysicalMemory(pageTable[frameData->pageNumber] & PTBL_PHYS_PG_NO, pageData);

	swapLock();
	int swapSpaceBlock = getSwapSpaceBlockFromFrame(frameData);
	swapUnlock();

	int diskID = getFormattedDisk();

	writeToDisk(diskID, swapSpaceBlock, pageData);

	memcpy(diskContents[swapSpaceBlock], pageData, PGSIZE);

	free(pageData);

}

/**
 * Reads a given page's data from swap space for this process.
 * Parameters:
 * pageNumber: the page to read.
 * Returns a pointer to the page's data.
 */
char* readFromSwapSpace(int pageNumber) {

	char* pageData = calloc(PGSIZE, sizeof(char));

	swapLock();
	int swapSpaceBlock = getSwapSpaceBlock(pageNumber);
	swapUnlock();

	int diskID = getFormattedDisk();

	if(!isUnwritten(diskContents[swapSpaceBlock])) {
		readFromDisk(diskID, swapSpaceBlock, pageData);
	}

	return pageData;

}

/**
 * Finds a disk that is formatted for this process to use.
 * Returns a formatted diskID.
 */
int getFormattedDisk() {

	if(formattedDisk != -1) {
		return formattedDisk;
	}

	formatDisk(0);
	return formattedDisk;

}
