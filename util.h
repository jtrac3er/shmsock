
#ifndef __UTIL_CUS_H__
#define __UTIL_CUS_H__

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#include "list.h"
#include "_malloc.h"
#include "event.h"
#include "socket.h"


typedef int sock_address;
typedef int connection_state;

#define MAX_INTERFACES 10

struct interface
{
	int magic; // = IFACE_MAGIC;
	
	struct llist connectionList;				// <struct connection> als Elemente
	struct malloc_context mctx;

#ifdef MODE_SERVER
	sock_address own_address, remote_address;
#else
	sock_address remote_address, own_address;
#endif

	// shared memory Informationen
	int shmemSize;
	void *shmemStart, *shmemEnd;
	
	bool active;
};

enum connectionState
{
	c_create, c_open, c_close, c_listen
};

struct connection
{
	
	enum connectionState connState;
	//connection_state connState;
	struct interface* pAssociatedIface;

#ifdef MODE_SERVER

	struct llist recvBuffer, sendBuffer;		// <struct bufferEntry> als Elemente
	eventmask own_events, remote_events;		// edge triggered, werden von connection.c geschrieben
	eventmask own_lt_events, remote_lt_events;	// level triggered, werden von event.c herausgefunden
	int own_port, remote_port;
	
#else

	struct llist sendBuffer, recvBuffer;	
	eventmask remote_events, own_events;
	eventmask remote_lt_events, own_lt_events;
	int remote_port, own_port;
	
#endif

};



struct bufferEntry
{
	int datasize;
	int offset;
	char data[];
};



// helpers

struct interface** getFreeInterface();

struct interface* getInterfaceBySockaddr(sock_address address);

void* createSharedRegion(char* name, int size, void* addr);



#ifndef ENCLAVE

int registerInterface(char* name, int size, 
		sock_address remoteAddress, sock_address ownAddress);

#else
#endif



struct connection* createNewConnection(
	struct interface* pIface, int own_port, int remote_port);
	
struct connection* createNewListenerConnection();

bool initiateNewConnection(struct connection* pConn);

bool checkIfAcceptedConnection(struct connection* pConn);

struct connection* acceptNewConnectionOnIface(
	struct interface* pIface, int own_port);

struct connection* acceptNewConnectionOnAnyIface(sock_address own_addr, int own_port);

bool removeConnection(struct connection* pConn);

void closeConnection(struct connection* pConn);

int sendConnection(struct connection* pConn, char* data, int len);

int recvConnection(struct connection* pConn, char* data, int len);



#endif
