
#ifndef __SOCKET_CUST_INC__
#define __SOCKET_CUST_INC__

// Um komplikationen zu vermeiden alle Funktionen entdefinieren
#define bind(...) bind_(__VA_ARGS__)
#define send(...) send_(__VA_ARGS__)
#define fcntl(...) fcntl_(__VA_ARGS__)
#define fcntl64(...) fcntl64_(__VA_ARGS__)
#define setsockopt(...) setsockopt_(__VA_ARGS__)

#include "list.h"
#include "util.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>

#ifndef ENCLAVE

// fuer shm ausserhalb von Enklaven
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#else
#endif

// Alle includes oberhalb von hier einsetzen
#undef fcntl64
#undef fcntl
#undef bind
#undef send
#undef setsockopt

#define _Null
#define _out
#define _in
#define _io

typedef int socket_state;
typedef int sock_address;

// alle Linked lists sind automatisch im shared memory

#define IFACE_MAGIC 0x12344321
#define MAX_FDS 50

// Linux systems limit the number of file descriptors that any one process may open to 1024 per process
// also es gibt ein Limit und ich darf nicht darueber sein. Ich sollte aber auch nicht darunter sein um 
// Kollisionen zu vermeiden
	
// 0 <= fd < FD_START:						System
// FD_START <= fd < POLLFD_START:			socket fds 
// POLLFD_START <= fd <= SYSTEM_MAX_FD:		epoll fds

#define FD_START 700
#define POLLFD_START (FD_START + 200)
#define SYSTEM_MAX_FD 1024

// fuer debugging
#define print(str, ...) printf("[shmsock] " str "\n", __VA_ARGS__)



enum socketState
{
	s_created, s_bound, s_listen, s_open, s_close,
	s_connecting,					// man versucht nonblocking zu verbinden
	s_connected,					// man ist verbunden
	s_shutdown
};

struct fd
{
	int num;
	//socket_state state;
	enum socketState state;
	int listenPort;					// falls man einen listener fd hat
	sock_address listenAddress;		// auf welcher Addresse listened man
	int flagNonblock;
	struct connection* pConn;
	//struct interface* pListenIface;		// fuer listener sockets
	bool active;
};


// globale Funktionen sind schon definiert im socket API
// natuerlich gibt es immer Ausnahmen

int accept4(int sockfd, struct sockaddr *_Null _out addr, 
	socklen_t *_Null _out addrlen, int flags);



struct fd* getFdByNum(int num);

struct fd* getEmptyFd();

void deleteFd(struct fd* fd);

void handleCloseWhileIO(struct fd* fd);

	
/*

int socket(int domain, int type, int protocol);

int bind(int sockfd, struct sockaddr *addr, socklen_t addrlen);

int listen(int sockfd, int backlog);

int accept(int sockfd, struct sockaddr *_Null _out addr, 
	socklen_t *_Null _out addrlen);

int accept4(int sockfd, struct sockaddr *_Null _out addr, 
	socklen_t *_Null _out addrlen, int flags);

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

ssize_t send(int sockfd, void* buf, size_t len, int flags);

ssize_t recv(int sockfd, void* buf, size_t len, int flags);


// not implemented yet

ssize_t sendmsg (int, const struct msghdr *, int);

ssize_t recvmsg (int, struct msghdr *, int);

int getsockopt (int, int, int, void *, socklen_t *);

int setsockopt (int, int, int, const void *, socklen_t);

*/


#endif


