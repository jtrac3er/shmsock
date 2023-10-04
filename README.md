# shmsock
Implementation of the unix socket API and event API using shared memory 

This library implements the socket API and the event API with shared memory. One must provide a shared region between two processes and call an initialization function for the library to register the interface 
between the processes. It can be used for socket communication between two processes without invoking the kernel

## Limitations
* only a connection between two processes is possible by now
* not very efficient, custom malloc function is used and overall it is not performance oriented
* not all functions are implemented and the functions themselves are not completely implemented, but it should be enough for simple socket communication
* the code itself would need refactoring and the Makefile contains many options which are not useful, but this project is used in another project and there those are needed, so just ignore most of them
* it lacks documentation and usage can be a bit complicated, best if you try to understand the source code

## Usage
Use the Makefile to compile the library. Many options are unuseful.
When compiled, one can simply link against this library and the libc functions will be shadowed by this library

