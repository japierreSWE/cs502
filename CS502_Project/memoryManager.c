/*
 * FrameHandler.c
 *
 *  Created on: Nov 23, 2019
 *      Author: jean-philippe
 */

#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "syscalls.h"
#include "protos.h"
#include "moreGlobals.h"
#include "memoryManager.h"
#include "processManager.h"
#include "fileSystem.h"
#include "dispatcher.h"
#include "diskManager.h"

int* frameTable; //frameTable[i] is the page number for the ith frame
int* swapTable; //swapTable[i] is the swap space block of the ith page
int* swapBlockAvailability; //if swapBlockAvailability[i] == 1, that swap space block is available

void writeToSwapSpace(int pageNumber);
char* readFromSwapSpace(int pageNumber);
int getSwapSpaceBlock(int pageNumber);
int getFormattedDisk();

/**
 * Sets up the memory manager by initializing
 * swap space and frame table.
 */
void initMemoryManager() {

	formattedDisk = -1;
	frameTable = calloc(NUMBER_PHYSICAL_PAGES, sizeof(int));
	swapTable = calloc(1024, sizeof(int));
	swapBlockAvailability = calloc(SWAP_SIZE, sizeof(int));

	for(int i = 0; i<NUMBER_PHYSICAL_PAGES; i++) {
		//frameTable[i] == -1 means the ith frame is free.
		frameTable[i] = -1;
	}

	for(int i = 0; i<1024; i++) {
		swapTable[i] = -1;
	}

	for(int i = 0; i<SWAP_SIZE; i++) {
		swapBlockAvailability[i] = 1;
	}

	MPData = (MP_INPUT_DATA*)malloc(sizeof(MP_INPUT_DATA));
	memoryPrints = 0;
}

/**
 * Finds a free frame for a given page.
 * Returns the frame number corresponding to the 1st
 * free frame found, or -1 if there are no free frames.
 * Also sets the frame found as used in frame table.
 * Parameters:
 * pageNumber: the page we're getting a frame for.
 * Returns the frame number that's been allocated, or -1
 * if no frame was found.
 */
int getFreeFrame(int pageNumber) {

	int freeFrame = -1;

	for(int i = 0; i<NUMBER_PHYSICAL_PAGES; i++) {

		if(frameTable[i] == -1) {
			freeFrame = i;
			break;
		}

	}

	if(freeFrame != -1) {

		frameTable[freeFrame] = pageNumber;

	}

	return freeFrame;

}

/**
 * Finds a victim frame for page replacement.
 * Does this by finding a frame number whose page has no referenced bit.
 * Returns the frame or -1 if none have been found
 */
int getVictimFrame() {

	UINT16* pageTable = (UINT16*)currentProcess()->pageTable;

	for(int i = 0; i < NUMBER_PHYSICAL_PAGES; i++) {

		int pageNumber = frameTable[i];

		if( (pageTable[pageNumber] & PTBL_REFERENCED_BIT) == 0) {

			return i;

		}

	}

	return -1;

}

/**
 * Code that handles page faults. Updates the page table,
 * does page replacement, etc.
 */
void handlePageFault(int pageNumber) {

	//TODO: put locks on getting frames.

	int freeFrame = getFreeFrame(pageNumber);
	UINT16* pageTable = (UINT16*)currentProcess()->pageTable;

	if(freeFrame == -1) {
		aprintf("Page replacement needed!\n");

		/*
		 * 1. Find a victim frame (a frame whose page has no reference bit)
		 * 2. If the page has been modified(check modify bit), write the victim frame
		 * to swap space.
		 * 3. Read desired page from swap space and put it into victim frame
		 * 4. Update page table and frame table
		 * 5. Set all reference bits to 0
		 */

		int victimFrameNumber = getVictimFrame();
		int pageOfVictimFrame = frameTable[victimFrameNumber];

		if( (pageTable[pageOfVictimFrame] & PTBL_MODIFIED_BIT) != 0) {

			writeToSwapSpace(pageOfVictimFrame);

		}

		char* currentPageData = readFromSwapSpace(pageNumber);
		Z502WritePhysicalMemory(victimFrameNumber, currentPageData);

		//the current page should have the victim frame.
		pageTable[pageNumber] = victimFrameNumber | PTBL_VALID_BIT;

		//the page that was using the victim frame can't use it anymore.
		//not valid.
		pageTable[pageOfVictimFrame] = pageTable[pageOfVictimFrame] & (~PTBL_VALID_BIT);

		//the victim frame is being used by this page now.
		frameTable[victimFrameNumber] = pageNumber;

		//clear all reference bits.
		for(int i = 0; i<1024; i++) {

			pageTable[i] = pageTable[i] & (~PTBL_REFERENCED_BIT);

		}

		//TODO: memory printing needed here

		dispatch();
		return;
	}

	pageTable[pageNumber] = freeFrame;
	pageTable[pageNumber] = pageTable[pageNumber] | PTBL_VALID_BIT;

	MPData->frames[freeFrame].InUse = 1;
	MPData->frames[freeFrame].LogicalPage = pageNumber;
	MPData->frames[freeFrame].Pid = currentProcess()->pid;

	memoryPrint();

	addToReadyQueue(currentProcess());
	dispatch();

}

/**
 * Writes a given page's data to swap space.
 * Parameters:
 * pageNumber: the page to write.
 */
void writeToSwapSpace(int pageNumber) {

	UINT16* pageTable = (UINT16*)currentProcess()->pageTable;
	char* pageData = calloc(PGSIZE, sizeof(char));

	Z502ReadPhysicalMemory(pageTable[pageNumber] & PTBL_PHYS_PG_NO, pageData);

	int swapSpaceBlock = getSwapSpaceBlock(pageNumber);
	int diskID = getFormattedDisk();

	writeToDisk(diskID, swapSpaceBlock, pageData);
	free(pageData);

}

/**
 * Reads a given page's data from swap space.
 * Parameters:
 * pageNumber: the page to read.
 * Returns a pointer to the page's data.
 */
char* readFromSwapSpace(int pageNumber) {

	char* pageData = calloc(PGSIZE, sizeof(char));

	int swapSpaceBlock = getSwapSpaceBlock(pageNumber);
	int diskID = getFormattedDisk();

	readFromDisk(diskID, swapSpaceBlock, pageData);
	return pageData;

}

/**
 * Finds a block in swap space for a given
 * page to use, allocating one if necessary.
 * Parameters:
 * pageNumber: the page to find a swap block for.
 * Returns the sector corresponding to the swap block.
 */
int getSwapSpaceBlock(int pageNumber) {

	if(swapTable[pageNumber] != -1) {
		return swapTable[pageNumber];
	}

	int blockUsed = -1;

	for(int i = 0; i<SWAP_SIZE; i++) {
		if(swapBlockAvailability[i] == 1) {
			swapBlockAvailability[i] = 0;
			blockUsed = i;
			break;
		}
	}

	if(blockUsed == -1) {
		aprintf("Need more swap space.\n");
		exit(0);
	}

	swapTable[pageNumber] = SWAP_LOCATION + blockUsed;
	return swapTable[pageNumber];

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
