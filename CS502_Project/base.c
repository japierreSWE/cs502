/************************************************************************

 This code forms the base of the operating system you will
 build.  It has only the barest rudiments of what you will
 eventually construct; yet it contains the interfaces that
 allow test.c and z502.c to be successfully built together.

 Revision History:
 1.0 August 1990
 1.1 December 1990: Portability attempted.
 1.3 July     1992: More Portability enhancements.
 Add call to SampleCode.
 1.4 December 1992: Limit (temporarily) printout in
 interrupt handler.  More portability.
 2.0 January  2000: A number of small changes.
 2.1 May      2001: Bug fixes and clear STAT_VECTOR
 2.2 July     2002: Make code appropriate for undergrads.
 Default program start is in test0.
 3.0 August   2004: Modified to support memory mapped IO
 3.1 August   2004: hardware interrupt runs on separate thread
 3.11 August  2004: Support for OS level locking
 4.0  July    2013: Major portions rewritten to support multiple threads
 4.20 Jan     2015: Thread safe code - prepare for multiprocessors
 4.51 August  2018: Minor bug fixes
 4.60 February2019: Test for the ability to alloc large amounts of memory
 ************************************************************************/

#include             "global.h"
#include             "syscalls.h"
#include             "protos.h"
#include             "string.h"
#include             <stdlib.h>
#include             <ctype.h>
#include             <limits.h>
#include			 "diskManager.h"
#include			 "dispatcher.h"
#include			 "processManager.h"
#include 			 "moreGlobals.h"
#include			 "fileSystem.h"
#include			 "memoryManager.h"


//  This is a mapping of system call nmemonics with definitions

char *call_names[] = {"MemRead  ", "MemWrite ", "ReadMod  ", "GetTime  ",
        "Sleep    ", "GetPid   ", "Create   ", "TermProc ", "Suspend  ",
        "Resume   ", "ChPrior  ", "Send     ", "Receive  ", "PhyDskRd ",
        "PhyDskWrt", "DefShArea", "Format   ", "CheckDisk", "OpenDir  ",
        "OpenFile ", "CreaDir  ", "CreaFile ", "ReadFile ", "WriteFile",
        "CloseFile", "DirContnt", "DelDirect", "DelFile  " };


/************************************************************************
 INTERRUPT_HANDLER
 When the Z502 gets a hardware interrupt, it transfers control to
 this routine in the Operating System.
 NOTE WELL:  Just because the timer or the disk has interrupted, and
         therefore this code is executing, it does NOT mean the 
         action you requested was successful.
         For instance, if you give the timer a NEGATIVE time - it 
         doesn't know what to do.  It can only cause an interrupt
         here with an error.
         If you try to read a sector from a disk but that sector
         hasn't been written to, that disk will interrupt - the
         data isn't valid and it's telling you it was confused.
         YOU MUST READ THE ERROR STATUS ON THE INTERRUPT
 ************************************************************************/
void InterruptHandler(void) {
	INT32 DeviceID;
    INT32 Status;
    int interruptsFound = 0;

    MEMORY_MAPPED_IO mmio;       // Enables communication with hardware

    //static BOOL  remove_this_from_your_interrupt_code = TRUE; /** TEMP **/
    //static INT32 how_many_interrupt_entries = 0;              /** TEMP **/

    // Get cause of interrupt
    mmio.Mode = Z502GetInterruptInfo;
    mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
    MEM_READ(Z502InterruptDevice, &mmio);

    //this loop keeps obtaining interrupts until there
    //aren't any more.
    while(mmio.Field4 == ERR_SUCCESS) {

    	DeviceID = mmio.Field1;
    	Status = mmio.Field2;

    	if(DeviceID == TIMER_INTERRUPT) {
    		interruptPrint("InterruptHandler: Timer interrupt found.\n");
    		timerLock();
    		TimerRequest* req = (TimerRequest*)QRemoveHead(timerQueueID);
    		timerUnlock();

    		if((int)req == -1) {
    			return;
    		}

    		if(interruptPrints < INTERRUPT_PRINTS_LIMIT) {

    			aprintf("InterruptHandler: Process gotten from timer queue, PID %d\n", req->process->pid);

    		}

    		//this timer request has been fulfilled.
    		//make its process ready.
    		addToReadyQueue(req->process);

    		timerLock();
    		TimerRequest* next = (TimerRequest*)QNextItemInfo(timerQueueID);
    		timerUnlock();

    		//we now manage the other requests in the queue.
    		//ones that have already ocurred go in the ready queue.
    		//when we find one that hasn't, start the timer and then stop.
    		while((int)next != -1) {

    			//already occurred. put it in ready queue.
    			if(getTimeOfDay() >= next->sleepUntil) {

    				timerLock();
    				QRemoveHead(timerQueueID);
    				timerUnlock();

    				readyLock();
    				addToReadyQueue(next->process);
    				readyUnlock();

    				if(interruptPrints < INTERRUPT_PRINTS_LIMIT) {

						aprintf("InterruptHandler: Process gotten from timer queue, PID %d\n", next->process->pid);

					}

    				timerLock();
    				next = (TimerRequest*)QNextItemInfo(timerQueueID);
    				timerUnlock();

    			} else {
    				//means its sleepUntil is in the future.
    				//start the timer, then break.

    				//start the hardware timer.
					MEMORY_MAPPED_IO mmio;
					mmio.Mode = Z502Start;
					mmio.Field1 = next->sleepUntil - getTimeOfDay();
					mmio.Field2 = 0;
					mmio.Field3 = 0;
					mmio.Field4 = 0;
					MEM_WRITE(Z502Timer, &mmio);

					//error handling for timer
					if(mmio.Field4 != ERR_SUCCESS) {
						aprintf("Error when starting timer, %ld\n", mmio.Field4);
						exit(0);
					}

					break;

    			}

    		}
    	//manages disk interrupts
    	} else if(DeviceID == DISK_INTERRUPT_DISK0 || DeviceID == DISK_INTERRUPT_DISK1 || DeviceID == 7
    			|| DeviceID == 8 || DeviceID == 9 || DeviceID == 10 || DeviceID == 11 || DeviceID == 12) {

    		int diskID = DeviceID - 5;

    		diskLock();
    		//aprintf("Disk interrupt starting.\n");

    		//get the 1st currently using one.
    		Process* proc = removeFromDiskQueue(diskID, 0);

    		//get the other ones, ignoring ones currently using disk.
    		while((int)proc != -1) {
    			//aprintf("INTERRUPT: Making process %d ready.\n", proc->pid);
    			addToReadyQueue(proc);
    			proc = removeFromDiskQueue(diskID, 1);
    		}
    		diskUnlock();

    		if(Status != ERR_SUCCESS) {


    			switch(Status) {

    				case ERR_BAD_PARAM: {

    					interruptPrint("Interrupt Handler: DISK ERROR Bad Parameter\n");
    					break;

    				}

    				case ERR_NO_PREVIOUS_WRITE: {

						interruptPrint("Interrupt Handler: Disk read attempted on sector not written to\n");
						break;

					}

    				case ERR_DISK_IN_USE: {

						interruptPrint("Interrupt Handler: DISK ERROR Disk in use\n");
						break;

					}

    			}

    			exit(0);

    		} else {
    			interruptPrint("Interrupt Handler: Disk interrupt found\n");
    		}

    	}

    	/** REMOVE THESE SIX LINES **/
		/*how_many_interrupt_entries++; TEMP **/
		/*if (remove_this_from_your_interrupt_code && (how_many_interrupt_entries < 10)) {
			aprintf("InterruptHandler: Found device ID %d with status %d\n",
					DeviceID, Status);
		}*/
    	mmio.Mode = Z502GetInterruptInfo;
    	mmio.Field1 = mmio.Field2 = mmio.Field3 = mmio.Field4 = 0;
    	MEM_READ(Z502InterruptDevice, &mmio);

    	++interruptsFound;
    }

    if (mmio.Field4 != ERR_SUCCESS && interruptsFound == 0) {
    	interruptPrint("InterruptHandler: Could not receive interrupt info. InterruptHandler has failed to receive the interrupt.\n");
    }

}           // End of InterruptHandler

/************************************************************************
 FAULT_HANDLER
 The beginning of the OS502.  Used to receive hardware faults.
 ************************************************************************/

void FaultHandler(void) {
    INT32 DeviceID;
    INT32 Status;
    MEMORY_MAPPED_IO mmio;       // Enables communication with hardware

    // Get cause of fault
    mmio.Field1 = mmio.Field2 = mmio.Field3 = 0;
    mmio.Mode = Z502GetInterruptInfo;
    MEM_READ(Z502InterruptDevice, &mmio);
    DeviceID = mmio.Field1;
    Status   = mmio.Field2;


    int pageNumber = Status % 1024;
    UINT16* pageTable = (UINT16*)currentProcess()->pageTable;

    //this block should run for illegal address request.
    //we got here and the bit is valid, and the page entry is initialized.
    if((pageTable[pageNumber] & PTBL_VALID_BIT) != 0) {

    	if(numProcessors == 1) {
    		aprintf("Illegal memory address requested. Terminating process.\n");
    		terminateProcess(-1);
    	} else {
    		addToReadyQueue(currentProcess());
    		dispatch();
    	}

    } else {
        handlePageFault(pageNumber);
    }

} // End of FaultHandler

/************************************************************************
 SVC
 The beginning of the OS502.  Used to receive software interrupts.
 All system calls come to this point in the code and are to be
 handled by the student written code here.
 The variable do_print is designed to print out the data for the
 incoming calls, but does so only for the first ten calls.  This
 allows the user to see what's happening, but doesn't overwhelm
 with the amount of data.
 ************************************************************************/

void svc(SYSTEM_CALL_DATA *SystemCallData) {
    short call_type;
    static short do_print = 10;
    short i;

    call_type = (short) SystemCallData->SystemCallNumber;
    if (do_print > 0 && SystemCallData->SystemCallNumber != SYSNUM_MULTIDISPATCH) {
        aprintf("SVC handler: %s\n", call_names[call_type]);
        for (i = 0; i < SystemCallData->NumberOfArguments - 1; i++) {
            //Value = (long)*SystemCallData->Argument[i];
            aprintf("Arg %d: Contents = (Decimal) %8ld,  (Hex) %8lX\n", i,
                    (unsigned long) SystemCallData->Argument[i],
                    (unsigned long) SystemCallData->Argument[i]);
        }
        do_print--;
    }
    switch(SystemCallData->SystemCallNumber) {
    	case SYSNUM_TERMINATE_PROCESS: {
    		long pid = (long)SystemCallData->Argument[0];
    		long result = terminateProcess(pid);

    		//if we did this successfully, change error.
    		if(result != -1) {
    			long* errorReturned = (long*)SystemCallData->Argument[1];
    			*errorReturned = ERR_SUCCESS;
    		}

    		break;

    	}

    	case SYSNUM_GET_TIME_OF_DAY: {
    		//get the hardware time from the hardware.
    		//then send it back to the system call data.

    		long time = getTimeOfDay();
    		*SystemCallData->Argument[0] = time;

    		break;

    	}

    	case SYSNUM_GET_PROCESS_ID: {

    		//get pid from process manager.
    		//then send it to system call data.

    		char* arg = (char *)SystemCallData->Argument[0];
    		long pid = getPid(arg);

    		*SystemCallData->Argument[1] = pid;

    		if(pid != -1) {
    			*SystemCallData->Argument[2] = ERR_SUCCESS;
    		} else {
    			*SystemCallData->Argument[2] = -1;
    		}

    		break;
    	}

    	case SYSNUM_SLEEP: {

    		long time = (long)SystemCallData->Argument[0];
    		startTimer(time);
    		//idle();
    		dispatch();
    		break;
    	}

    	case SYSNUM_PHYSICAL_DISK_WRITE: {

    		long diskID = (long)SystemCallData->Argument[0];
    		long sector = (long)SystemCallData->Argument[1];
    		char* writeBuffer = (char*)SystemCallData->Argument[2];
    		writeToDisk(diskID, sector, writeBuffer);
    		break;
    	}

    	case SYSNUM_PHYSICAL_DISK_READ: {

    		long diskID = (long)SystemCallData->Argument[0];
    		long sector = (long)SystemCallData->Argument[1];
    		char* readBuffer = (char*)SystemCallData->Argument[2];
    		readFromDisk(diskID, sector, readBuffer);
    		break;
    	}

    	case SYSNUM_CHECK_DISK: {
    		long diskID = (long)SystemCallData->Argument[0];
    		long* errorReturned = (long*)SystemCallData->Argument[1];
    		checkDisk(diskID);
    		*errorReturned = ERR_SUCCESS;
    		break;
    	}

    	case SYSNUM_CREATE_PROCESS: {
    		char* processName = (char*)SystemCallData->Argument[0];
    		void* startingAddress = (void*)SystemCallData->Argument[1];
    		long initialPriority = (long)SystemCallData->Argument[2];
    		long* pid = (long*)SystemCallData->Argument[3];
    		long* errorReturned = (long*)SystemCallData->Argument[4];
    		int result = createProcess(processName,startingAddress,initialPriority,pid);

    		//if we ran successfully, make errorReturned = ERR_SUCCESS.
    		if(result != -1) {
    			*pid = result;
    			*errorReturned = ERR_SUCCESS;
    		} else {
    			//otherwise, make errorReturned = error value.
    			*errorReturned = result;
    		}

    		break;
    	}

    	case SYSNUM_SUSPEND_PROCESS: {

    		long pid = (long)SystemCallData->Argument[0];
    		long result = suspendProcess(pid);
    		long* errorReturned = (long*)SystemCallData->Argument[1];

    		//if successful, ERR_SUCCESS.
    		if(result == 0) {
    			*errorReturned = ERR_SUCCESS;
    		} else {
    			*errorReturned = result;
    		}

    		break;

    	}

    	case SYSNUM_RESUME_PROCESS: {

    		long pid = (long)SystemCallData->Argument[0];
    		long result = resumeProcess(pid);
    		long* errorReturned = (long*)SystemCallData->Argument[1];

    		if(result == 0) {
    			*errorReturned = ERR_SUCCESS;
    		} else {
    			*errorReturned = result;
    		}

    		break;
    	}

    	case SYSNUM_CHANGE_PRIORITY: {

    		long pid = (long)SystemCallData->Argument[0];
    		long newPriority = (long)SystemCallData->Argument[1];
    		long* errorReturned = (long*)SystemCallData->Argument[2];

    		long result = changePriority(pid, newPriority);

    		if(result == 0) {
				*errorReturned = ERR_SUCCESS;
			} else {
				*errorReturned = result;
			}

    		break;

    	}

    	case SYSNUM_SEND_MESSAGE: {

    		long targetPID = (long)SystemCallData->Argument[0];
    		char* messageBuffer = (char*)SystemCallData->Argument[1];
    		long msgSendLength = (long)SystemCallData->Argument[2];
    		long* errorReturned = (long*)SystemCallData->Argument[3];

    		long result = sendMessage(targetPID,messageBuffer,msgSendLength);

    		if(result == 0) {
				*errorReturned = ERR_SUCCESS;
			} else {
				*errorReturned = result;
			}

    		break;
    	}

    	case SYSNUM_RECEIVE_MESSAGE: {

    		long sourcePID = (long)SystemCallData->Argument[0];
    		char* messageBuffer = (char*)SystemCallData->Argument[1];
    		long msgReceiveLength = (long)SystemCallData->Argument[2];
    		long* msgSendLength = (long*)SystemCallData->Argument[3];
    		long* msgSenderPid = (long*)SystemCallData->Argument[4];
    		long* errorReturned = (long*)SystemCallData->Argument[5];

    		long result = receiveMessage(sourcePID, messageBuffer, msgReceiveLength, msgSendLength, msgSenderPid);

    		if(result == 0) {
				*errorReturned = ERR_SUCCESS;
			} else {
				*errorReturned = result;
			}

    		break;
    	}

    	case SYSNUM_FORMAT: {

    		long diskID = (long)SystemCallData->Argument[0];
    		long* errorReturned = (long*)SystemCallData->Argument[1];

    		long result = formatDisk(diskID);

    		if(result == 0) {
				*errorReturned = ERR_SUCCESS;
			} else {
				*errorReturned = result;
			}

    		break;
    	}

    	case SYSNUM_OPEN_DIR: {

    		long diskID = (long)SystemCallData->Argument[0];
    		char* dirName = (char*)SystemCallData->Argument[1];
    		long* errorReturned = (long*)SystemCallData->Argument[2];

    		long result = openDir(diskID, dirName);

    		if(result == 0) {
    			*errorReturned = ERR_SUCCESS;
    		} else {
    			*errorReturned = result;
    		}

    		break;
    	}

    	case SYSNUM_CREATE_DIR: {

    		char* directoryName = (char*)SystemCallData->Argument[0];
    		long* errorReturned = (long*)SystemCallData->Argument[1];

    		long result = createDir(directoryName);


    		if(result == 0) {
    			*errorReturned = ERR_SUCCESS;
    		} else {
    			*errorReturned = result;
    		}

    		break;
    	}

    	case SYSNUM_CREATE_FILE: {

    		char* fileName = (char*)SystemCallData->Argument[0];
    		long* errorReturned = (long*)SystemCallData->Argument[1];

    		long result = createFile(fileName);

    		if(result == 0) {
				*errorReturned = ERR_SUCCESS;
			} else {
				*errorReturned = result;
			}

			break;

    	}

    	case SYSNUM_OPEN_FILE: {

    		char* fileName = (char*)SystemCallData->Argument[0];
    		long* inode = (long*)SystemCallData->Argument[1];
    		long* errorReturned = (long*)SystemCallData->Argument[2];

    		long result = openFile(fileName);

    		if(result == -1) {
    			*errorReturned = result;
    		} else {
    			*errorReturned = ERR_SUCCESS;
    			*inode = result;
    		}

    		break;
    	}

    	case SYSNUM_WRITE_FILE: {

    		long inode = (int)SystemCallData->Argument[0];
    		int logicalBlock = (int)SystemCallData->Argument[1];
    		char* writeBuffer = (char*)SystemCallData->Argument[2];
    		long* errorReturned = (long*)SystemCallData->Argument[3];

    		int result = writeFile(inode, logicalBlock, writeBuffer);

    		if(result == 0) {
				*errorReturned = ERR_SUCCESS;
			} else {
				*errorReturned = result;
			}

    		break;

    	}

    	case SYSNUM_CLOSE_FILE: {

    		long inode = (int)SystemCallData->Argument[0];
    		long* errorReturned = (long*)SystemCallData->Argument[1];

    		int result = closeFile(inode);

    		if(result == 0) {
    			*errorReturned = ERR_SUCCESS;
    		} else {
    			*errorReturned = result;
    		}

    		break;
    	}

    	case SYSNUM_READ_FILE: {

    		long inode = (int)SystemCallData->Argument[0];
			int logicalBlock = (int)SystemCallData->Argument[1];
			char* readBuffer = (char*)SystemCallData->Argument[2];
			long* errorReturned = (long*)SystemCallData->Argument[3];

			int result = readFile(inode, logicalBlock, readBuffer);

			if(result == 0) {
				*errorReturned = ERR_SUCCESS;
			} else {
				*errorReturned = result;
			}

			break;

    	}

    	case SYSNUM_DIR_CONTENTS: {
    		long* errorReturned = (long*)SystemCallData->Argument[0];
    		dirContents();

    		*errorReturned = ERR_SUCCESS;
    		break;
    	}

    	case SYSNUM_MULTIDISPATCH: {
    		multidispatcher();
    		break;
    	}

    }

}                                               // End of svc

/************************************************************************
 osInit
 This is the first routine called after the simulation begins.  This
 is equivalent to boot code.  All the initial OS components can be
 defined and initialized here.
 ************************************************************************/

void osInit(int argc, char *argv[]) {
    // Every process will have a page table.  This will be used in
    // the second half of the project.  
    void *PageTable = (void *) calloc(2, NUMBER_VIRTUAL_PAGES);
    INT32 i;
    MEMORY_MAPPED_IO mmio;

    // Demonstrates how calling arguments are passed thru to here

    aprintf("Program called with %d arguments:", argc);
    for (i = 0; i < argc; i++)
        aprintf(" %s", argv[i]);
    aprintf("\n");
    aprintf("Calling with argument 'sample' executes the sample program.\n");

    // Here we check if a second argument is present on the command line.
    // If so, run in multiprocessor mode.  Note - sometimes people change
    // around where the "M" should go.  Allow for both possibilities
    if (argc > 2) {
        if ((strcmp(argv[1], "M") ==0) || (strcmp(argv[1], "m")==0)) {
            strcpy(argv[1], argv[2]);
            strcpy(argv[2],"M\0");
        }
        if ((strcmp(argv[2], "M") ==0) || (strcmp(argv[2], "m")==0)) {
            aprintf("Simulation is running as a MultProcessor\n\n");
            mmio.Mode = Z502SetProcessorNumber;
            mmio.Field1 = MAX_NUMBER_OF_PROCESSORS;
            mmio.Field2 = (long) 0;
            mmio.Field3 = (long) 0;
            mmio.Field4 = (long) 0;
            MEM_WRITE(Z502Processor, &mmio);   // Set the number of processors
        }
    } else {
        aprintf("Simulation is running as a UniProcessor\n");
        aprintf("Add an 'M' to the command line to invoke multiprocessor operation.\n\n");
    }

    //  Some students have complained that their code is unable to allocate
    //  memory.  Who knows what's going on, other than the compiler has some
    //  wacky switch being used.  We try to allocate memory here and stop
    //  dead if we're unable to do so.
    //  We're allocating and freeing 8 Meg - that should be more than
    //  enough to see if it works.
    void *Temporary = (void *) calloc( 8, 1024 * 1024);
    if ( Temporary == NULL )  {  // Not allocated
	    printf( "Unable to allocate memory in osInit.  Terminating simulation\n");
	    exit(0);
    }
    free(Temporary);
    //  Determine if the switch was set, and if so go to demo routine.
    //  We do this by Initializing and Starting a context that contains
    //     the address where the new test is run.
    //  Look at this carefully - this is an example of how you will start
    //     all of the other tests.

    if ((argc > 1) && (strcmp(argv[1], "sample") == 0)) {

    	long address = (long)SampleCode;
    	pcbInit(address, (long)PageTable);

    	//need to do pcb init to get all things set up
    	//for 1st process. above is code to do that.

        mmio.Mode = Z502InitializeContext;
        mmio.Field1 = 0;
        mmio.Field2 = (long) SampleCode;
        mmio.Field3 = (long) PageTable;

        MEM_WRITE(Z502Context, &mmio);   // Start of Make Context Sequence
        mmio.Mode = Z502StartContext;
        // Field1 contains the value of the context returned in the last call
        mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
        MEM_WRITE(Z502Context, &mmio);     // Start up the context

    }

    //don't schedule print for memory tests
    if(strncmp(argv[1], "test4", 5) == 0) {
    	schedulePrintLimit = 0;
    }

    //control blocks for each test.
    //if we find a test, make the PCB for that test.
    if((argc > 1) && (strcmp(argv[1], "test0") == 0)) {
    	long address = (long)test0;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test1") == 0)) {

    	long address = (long)test1;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test2") == 0)) {

    	long address = (long)test2;
		pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test3") == 0)) {

    	long address = (long)test3;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test4") == 0)) {

    	long address = (long)test4;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test5") == 0)) {

    	long address = (long)test5;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test6") == 0)) {

    	long address = (long)test6;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test7") == 0)) {

    	long address = (long)test7;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test8") == 0)) {

    	long address = (long)test8;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test9") == 0)) {

    	long address = (long)test9;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test10") == 0)) {

    	long address = (long)test10;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test11") == 0)) {

    	long address = (long)test11;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test12") == 0)) {

    	long address = (long)test12;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test13") == 0)) {

    	long address = (long)test13;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test14") == 0)) {

    	long address = (long)test14;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test21") == 0)) {

    	long address = (long)test21;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test22") == 0)) {

    	long address = (long)test22;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test23") == 0)) {

    	long address = (long)test23;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test24") == 0)) {

    	long address = (long)test24;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test25") == 0)) {

    	long address = (long)test25;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test26") == 0)) {

    	long address = (long)test26;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test41") == 0)) {

    	long address = (long)test41;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test42") == 0)) {

    	long address = (long)test42;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test43") == 0)) {

    	long address = (long)test43;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test44") == 0)) {

    	long address = (long)test44;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test45") == 0)) {

    	long address = (long)test45;
    	pcbInit(address, (long)PageTable);

    } else if((argc > 1) && (strcmp(argv[1], "test46") == 0)) {

    	long address = (long)test46;
    	pcbInit(address, (long)PageTable);

    }

    //otherwise, we do the default: running test0.

    // End of handler for sample code - This routine should never return here
    //  By default test0 runs if no arguments are given on the command line
    //  Creation and Switching of contexts should be done in a separate routine.
    //  This should be done by a "OsMakeProcess" routine, so that
    //  test0 runs on a process recognized by the operating system.

    long address = (long)test0;
    pcbInit(address, (long)PageTable);

    //below is old code. above is how test0 would be run
    //with my code.

    mmio.Mode = Z502InitializeContext;
    mmio.Field1 = 0;
    mmio.Field2 = (long) test0;
    mmio.Field3 = (long) PageTable;

    MEM_WRITE(Z502Context, &mmio);   // Start this new Context Sequence
    mmio.Mode = Z502StartContext;
    // Field1 contains the value of the context returned in the last call
    // Suspends this current thread
    mmio.Field2 = START_NEW_CONTEXT_AND_SUSPEND;
    MEM_WRITE(Z502Context, &mmio);     // Start up the context

}                                               // End of osInit

