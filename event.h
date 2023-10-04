
#ifndef __EVENT_CUS_H__
#define __EVENT_CUS_H__


//#include "socket.h"
#include <sys/epoll.h>
#include <sys/poll.h>
#include <stdbool.h>

// um zirkulare includes zu vermeiden
struct fd;

typedef uint32_t eventmask;

/*

struct epoll_event {
   uint32_t      events; 
   epoll_data_t  data;  
};

union epoll_data {
   void     *ptr;
   int       fd;
   uint32_t  u32;
   uint64_t  u64;
};

typedef union epoll_data  epoll_data_t;

enum EPOLL_EVENTS
{
	EPOLLIN = 0x001,
	EPOLLPRI = 0x002,
	EPOLLOUT = 0x004,
	EPOLLRDNORM = 0x040,
	EPOLLRDBAND = 0x080,
	EPOLLWRNORM = 0x100,
	EPOLLWRBAND = 0x200,
	EPOLLMSG = 0x400,
	EPOLLERR = 0x008,
	EPOLLHUP = 0x010,
	EPOLLRDHUP = 0x2000,
	EPOLLEXCLUSIVE = 1u << 28,
	EPOLLWAKEUP = 1u << 29,
	EPOLLONESHOT = 1u << 30,
	EPOLLET = 1u << 31
};
*/

// also man hat irgendwelche Daten zusaetlich zu einer eventmask

#define MAX_POLLFDS 50
#define MAX_FDS_LISTEN 20

#define IS_EVENT_SET(eventmask, event) (eventmask & event)

// achtung: Diese macros muessen die events auch direkt setzen
#define SET_EVENT(eventmask, event) eventmask |= event
#define CLEAR_EVENT(eventmask, event) eventmask &= (~event)

struct eventListenFd
{
	bool active;
	bool isLevelTriggered;					// ob level triggered oder nicht
	struct epoll_event event;				// hat events und data zusammen
	struct fd* fd;
};


struct epollfd
{
	int active;
	int num;
	
	int allFlags;		// flags bei creation
	bool isMixed;			// wenn 1, dann gibt es auch richtige events
	int realPollfd;		// der assoziierte richtige fd
	
	// also fuer jeden FD wo man listened hat man ein solches struct
	// welches die events anzeigt fuer den FD
	struct eventListenFd lfds[MAX_FDS_LISTEN];
	
};


struct epollfd* getPollfdByNum(int num);

struct epollfd* getEmptyPollfd();

void deletePollfd(struct epollfd* pfd);

struct eventListenFd* getListenerForFd(struct epollfd* pfd, int fd);

struct eventListenFd* getEmptyListener(struct epollfd* pfd);

void deleteListener(struct eventListenFd* lfd);

void handleFdCloseInEvents(struct epollfd* pfd, struct eventListenFd* lfd);



/*
// Event API

int epoll_create1(int flags);

int epoll_ctl(int epfd, int op, int fd, struct epoll_event *_Nullable event);

int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout,
                      const sigset_t *_Nullable sigmask);

*/

#endif
