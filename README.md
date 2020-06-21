# cs502

This is a project I did for a graduate Operating Systems course from August to December 2019. The project was an operating system running on a simulated machine. It can be built and executed by downloading the source code, running "make", then executing the z502 executable.

The operating system is written in C and includes a memory manager, a disk manager, a file system, a process dispatcher, and an inter-process communication system. It also includes more than 20 system calls, which can be found in the base.c file. All components were written from scratch, except for the system that distinguishes system calls, which was provided as starter code. The operating system was built to pass a set of tests, which can be found in test.c. To run the operating system with a test, execute it like this: "z502 <testId>". The operating system passes all tests except for test48.
  
The operating system runs on a uniprocessor machine by default, but can also be executed in multiprocessed mode, which uses multithreading to execute processes simultaneously. You can execute the operating system in multiprocessed mode by adding "M" to the command line arguments.
