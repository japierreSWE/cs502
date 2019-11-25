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
#include "frameHandler.h"
#include "processManager.h"

int* frameTable;

/**
 * Sets up the frame table so that the frame
 * handler can be used.
 */
void initFrameHandler() {

	frameTable = calloc(NUMBER_PHYSICAL_PAGES, sizeof(int));

	for(int i = 0; i<NUMBER_PHYSICAL_PAGES; i++) {
		//frameTable[i] == -1 means the ith frame is free.
		frameTable[i] = -1;
	}

	MPData = (MP_INPUT_DATA*)malloc(sizeof(MP_INPUT_DATA));
	memoryPrints = 0;
}

/**
 * Finds a free frame for the current process.
 * Returns the frame number corresponding to the 1st
 * free frame found, or -1 if there are no free frames.
 * Also sets the frame found as used in frame table.
 */
int getFreeFrame() {

	int freeFrame = -1;

	for(int i = 0; i<NUMBER_PHYSICAL_PAGES; i++) {

		if(frameTable[i] == -1) {
			freeFrame = i;
			break;
		}

	}

	if(freeFrame != -1) {

		frameTable[freeFrame] = currentProcess()->pid;

	}

	return freeFrame;

}
