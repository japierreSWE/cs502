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


FrameData** frameTable; //frameTable[i] is the page number for the ith frame
int* swapBlockAvailability; //if swapBlockAvailability[i] == 1, that swap space block is available
int frameCursor = 0; //used for clock algorithm

//each process should have swap table.
//frame table should contain swap block, process that put it there, and page number.
//

/**
 * Sets up the memory manager by initializing
 * swap space and frame table.
 */
void initMemoryManager() {

	formattedDisk = -1;
	frameTable = calloc(NUMBER_PHYSICAL_PAGES, sizeof(FrameData*));
	swapBlockAvailability = calloc(SWAP_SIZE, sizeof(int));

	for(int i = 0; i<NUMBER_PHYSICAL_PAGES; i++) {
		//frameTable[i] == -1 means the ith frame is free.
		frameTable[i] = malloc(sizeof(FrameData));
		frameTable[i]->free = 1;
		frameTable[i]->pid = -1; //this means not used yet.
	}

	/*for(int i = 0; i<1024; i++) {
		swapTable[i] = -1;
	}*/

	for(int i = 0; i<SWAP_SIZE; i++) {
		swapBlockAvailability[i] = 1;
	}

	MPData = (MP_INPUT_DATA*)malloc(sizeof(MP_INPUT_DATA));
	memoryPrints = 0;
}

/**
 * Finds a free frame for a given page for this process.
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

		if(frameTable[i]->free == 1) {
			freeFrame = i;
			break;
		}

	}

	if(freeFrame != -1) {

		frameTable[freeFrame]->free = 0;
		//frameTable[freeFrame]->pageNumber = pageNumber;
		//frameTable[freeFrame]->pid = currentProcess()->pid;

	}

	return freeFrame;

}

/**
 * Finds a victim frame for page replacement.
 * Does this by finding a frame number whose page has no referenced bit.
 * Returns the victim frame
 */
int getVictimFrame() {

	while(1) {
		//circle back to start.
		if(frameCursor >= 64) frameCursor = 0;

		int pageNumber = frameTable[frameCursor]->pageNumber;
		Process* userOfFrame = getProcess(frameTable[frameCursor]->pid);

		if((int)userOfFrame != -1) {

			UINT16* pageTable = userOfFrame->pageTable;

			if( (pageTable[pageNumber] & PTBL_REFERENCED_BIT) == 0) {

				break;

			} else {
				pageTable[pageNumber] &= (~PTBL_REFERENCED_BIT);
			}

		} else {
			break;
		}


		++frameCursor;
	}

	return frameCursor;

}

/**
 * Code that handles page faults. Updates the page table,
 * does page replacement, etc.
 */
void handlePageFault(int pageNumber) {

	int currentPid = currentProcess()->pid;
	Process* curr = currentProcess();

	memLock();

	int freeFrame = getFreeFrame(pageNumber);
	//memUnlock();
	UINT16* thisPageTable = (UINT16*)currentProcess()->pageTable;

	while(freeFrame == -1) {
		/*
		 * 1. Find a victim frame (a frame whose page has no reference bit)
		 * 2. If the page has been modified(check modify bit), write the victim frame
		 * to swap space. (find its swap space block and but it there)
		 * 3. Read desired page from swap space and put it into victim frame
		 * 4. Update page table and frame table
		 */

		int victimFrameNumber = getVictimFrame();
		++frameCursor;
		frameTable[victimFrameNumber]->free = 1;
		freeFrame = getFreeFrame(pageNumber);

	}

	//if someone has used this before, we do some
	//necessary page replacement stuff
	Process* frameUser = getProcess(frameTable[freeFrame]->pid);

	if((int)frameUser != -1) {
		//write the frame to swap space.
		//then read in our page.

		UINT16 pageOfFrame = frameUser->pageTable[frameTable[freeFrame]->pageNumber];

		if( (pageOfFrame & PTBL_MODIFIED_BIT) != 0) {

			writeToSwapSpace(frameTable[freeFrame]);

		}

		pageOfFrame = frameUser->pageTable[frameTable[freeFrame]->pageNumber];
		//the page that was using the victim frame can't use it anymore.
		//not valid.
		frameUser->pageTable[frameTable[freeFrame]->pageNumber] = pageOfFrame & (~PTBL_VALID_BIT);

		//aprintf("PID %d replaced at frame %d. \nReplaced: pid-%d pageNumber-%d\n",
				//currentProcess()->pid, freeFrame, frameTable[freeFrame]->pid, frameTable[freeFrame]->pageNumber);
	}

	char* currentPageData = readFromSwapSpace(pageNumber);

	if((int)currentPageData != -1) {
		Z502WritePhysicalMemory(freeFrame, currentPageData);
	}

	thisPageTable[pageNumber] = freeFrame;
	thisPageTable[pageNumber] = thisPageTable[pageNumber] | PTBL_VALID_BIT;


	frameTable[freeFrame]->pid = currentPid;
	frameTable[freeFrame]->pageNumber = pageNumber;

	MPData->frames[freeFrame].InUse = 1;
	MPData->frames[freeFrame].LogicalPage = pageNumber;
	MPData->frames[freeFrame].Pid = currentPid;

	memoryPrint();

	//aprintf("PID %d found frame %d for its page %d\n", currentProcess()->pid, freeFrame, pageNumber);
	addToReadyQueue(curr);
	memUnlock();
	dispatch();

}


/**
 * Finds a block in swap space for a given
 * frame to use, allocating one if necessary.
 * Parameters:
 * frameData: the frame to find a swap block for.
 * Returns the sector corresponding to the swap block.
 */
int getSwapSpaceBlockFromFrame(FrameData* frameData) {

	Process* process = getProcess(frameData->pid);
	int pageNumber = frameData->pageNumber;
	int* swapTable = process->swapTable;

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
 * Finds a block in swap space for the current process's given
 * page to use, allocating one if necessary.
 * Parameters:
 * pageNumber: the page to find a swap block for.
 * Returns the sector corresponding to the swap block.
 */
int getSwapSpaceBlock(int pageNumber) {

	Process* process = currentProcess();
	int* swapTable = process->swapTable;

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
