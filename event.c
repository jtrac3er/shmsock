#include "event.h"
#include "util.h"
#include "socket.h"
#include <sys/syscall.h>
#include "list.h"
#include "debug.h"

#ifdef ENCLAVE
#include "errno_enclave.h"
#else
#include <errno.h>
#endif



#define validatePfd(pfd) \
	if (!pfd) \
	{ \
		errno = EBADF; \
		return(-1); \
	}
	
#define assertFd(fd) \
	if (!fd) \
	{ \
		errno = EBADF; \
		return(-1); \
	}

#define assertNewFdAllocation(fd) \
		if (!fd) { \
		errno = ENOMEM; \
		return(-1); }

// wenn ein event gesezt wird, welches nicht unterstuetzt wird
#define assertEventFlags(eventmask) \
	if (!validateEventFlags(eventmask)) { \
		/*errno = EINVAL; return(-1); */ \
		debug_printf("some of the epoll-flags are not supported, ignoring\n"); \
	}
		
		

struct epollfd pollfds[MAX_POLLFDS];
int randomPollFd = POLLFD_START;			
// pollfd verwendet eigene fds, kollision mit socket moeglich


struct epollfd* getPollfdByNum(int num)
{
	for (int i = 0; i < MAX_POLLFDS; i++)
	{
		if (pollfds[i].active && pollfds[i].num == num)
			return pollfds +i;
	}
	return NULL;
}

struct epollfd* getEmptyPollfd()
{
	for (int i = 0; i < MAX_POLLFDS; i++)
	{
		if (!pollfds[i].active)
		{
			pollfds[i].active = 1;
			return pollfds +i;
		}
	}
	return NULL;
}
void deletePollfd(struct epollfd* pfd)
{
	// zudem sollen auch alle zugehoerigen lfd geschlossen werden
	for (int i = 0; i < MAX_FDS_LISTEN; i++)
		deleteListener(pfd->lfds +i);
	
	pfd->active = 0;
}



// suche nach einem fd in einer pollfd struktur
// achtung: es kann sein, dass der FD geschlossen wurde, deshalb mit fd nummer
// suchen und nicht mit fd Pointer
struct eventListenFd* getListenerForFd(struct epollfd* pfd, int fd)
{
	for (int i = 0; i < MAX_FDS_LISTEN; i++)
	{
		if (pfd->lfds[i].active && pfd->lfds[i].fd->num == fd)
			if (pfd->lfds[i].fd->active)
				return pfd->lfds + i;
			else
			{
				// in diesem Falle wurde der FD geschlossen
				handleFdCloseInEvents(pfd, pfd->lfds +i);
				return NULL;
			}
	}
	return NULL;
}

struct eventListenFd* getEmptyListener(struct epollfd* pfd)
{
	for (int i = 0; i < MAX_FDS_LISTEN; i++)
		if (!pfd->lfds[i].active)
		{
			pfd->lfds[i].active = 1;
			return pfd->lfds + i;
		}
	return NULL;
}

void deleteListener(struct eventListenFd* lfd)
{
	lfd->active = 0;
}

// wenn ein gesamter Epfd geschlossen wird
// hier wird noch dazu angenommen, dass ein epfd nie geschlossen werden kann von einem thread, wahrend
// ein anderer thread den epfd gerade nutzt (betrifft vorallem epoll_Wait)

int handleEpfdClose(int epfd)
{
	struct epollfd* pfd = getPollfdByNum(epfd);
	if (!pfd) 
	{
		errno = EBADF;
		return -1;
	}
	
	// pfd inaktivieren
	pfd->active = 0;
	
	// alle lfd loeschen
	for (int i = 0; i < MAX_FDS_LISTEN; i++)
		deleteListener(pfd->lfds +i);
	
	return 0;
}


// wenn ein listener der mit einem pfd assoziiert war geschlossen wurde
// weil der fd des listeners geschlossen wurde
void handleFdCloseInEvents(struct epollfd* pfd, struct eventListenFd* lfd)
{
	// in diesem Falle muss man ihn deregistrieren
	deleteListener(lfd);
}

// schaut ob alle angefragten Events unterstuetzt werden
bool validateEventFlags(eventmask events)
{
	if (IS_EVENT_SET(events, ~(EPOLLIN | EPOLLOUT | EPOLLHUP)))
	{
		// irgend ein anderes event als die genannten ist gesetzt
		return 0;
	}
}

// die events in der Connection selber sind nur edge triggered, hier braucht
// es eine Funktion, die level triggered events setzt
void setLevelTriggeredEvents(struct connection* pConn)
{
	pConn->own_lt_events = 0x0;		// clearen am anfang
	
	if (hasDataList(&pConn->recvBuffer))
	{
		// man hat Daten die empfangen werden koennen
		SET_EVENT(pConn->own_lt_events, EPOLLIN);
	}
	if (1 /* Man hat genuegend freien speicher*/ )
	{
		// schreiben kann man immer
		SET_EVENT(pConn->own_lt_events, EPOLLOUT);
	}
	if (pConn->connState == c_close)
	{
		// wenn sie geschlossen ist, bleibt sie geschlosen
		SET_EVENT(pConn->own_lt_events, EPOLLHUP);
	}
}


#define pollfdExists(pfd) (getPollfdByNum(pfd) != NULL)
#define listenerExists(pfd, fd) (getListenerForFd(pfd, fd) != NULL)


// wenn ich ausserhalb von Enklaven teste - lock macht nur probleme
#define NOLOCK

#include "api-tools.h"


// API Funktionen

// macht eigentlich gar nichts, einfach fd erstellen, flags ingorieren
// achtung: Es wird angenommen, dass epoll NUR fuer sockets verwendet wird
// wenn epoll nun mit normalen Dateien ausgefuehrt wird, gibt es einen error

int epoll_create1(int flags)
{
	FUNC_LOCK();
	
	debug_printf("epoll_create1(int flags=%d)\n", flags);
	
	// test ob man noch genug FDs hat
	if (randomPollFd >= SYSTEM_MAX_FD)
	{
		errno = ENFILE;
		return(-1);
	}
	
	struct epollfd* pfd = getEmptyPollfd();
	assertNewFdAllocation(pfd);
	
	
	pfd->num = randomPollFd++;
	pfd->allFlags = flags;
	
	return(pfd->num);
}


// zeigt an, auf welche Events man warten soll
int epoll_ctl(int epfd, int op, int sockfd, struct epoll_event *_Null event)
{	
	FUNC_LOCK();
	
	debug_printf("epoll_ctl(int epfd=%d, int op=%d, int sockfd=%d, struct epoll_event* event=%p)\n",
		 epfd, op, sockfd, event);
	
	// zuerst alle fd erhalten
	struct epollfd* pfd = getPollfdByNum(epfd);
	struct fd* fd = getFdByNum(sockfd);
	validatePfd(pfd);
	
	if (sockfd < FD_START)
	{
		// man hat einen richtigen fd
		if (!pfd->isMixed)
		{
			// das ist der erste der gemixt ist
			pfd->realPollfd = syscall(SYS_epoll_create1, pfd->allFlags);
			pfd->isMixed = 1;
		}
		
		// weiterleiten an epollctl
		return(syscall(SYS_epoll_ctl, pfd->realPollfd, op, sockfd, event));
	}
	
	// schauen dass der fd auch existiert
	assertFd(fd);
	
	// so nun kann man die Eventmask setzen fuer den fd
	struct eventListenFd* lfd;
	switch (op)
	{
	case (EPOLL_CTL_ADD):
		// ich glaube hier ist es so, dass man keinen fd hat und neu events zuweist -> ja
		if (listenerExists(pfd, sockfd))
		{
			// es gibt schon einen listener fuer diesen FD
			errno = EEXIST;
			return(-1);
		}
		lfd = getEmptyListener(pfd);
		assertNewFdAllocation(lfd);		// returnt ENOMEM wenn gescheitert, geht auch fuer listener
		lfd->fd = fd;
		
		// achtung: hier kopiert man das ganze struct
		lfd->event = *event;
		assertEventFlags(lfd->event.events);
		
		lfd->isLevelTriggered = ! IS_EVENT_SET(event->events, EPOLLET);
		return(0);
		
	case (EPOLL_CTL_MOD):
		// dann hier ist es so, dass man einen bestehenden modifiziert
		lfd = getListenerForFd(pfd, sockfd);
		if (!lfd)
		{
			// es gibt keinen listener den man modidizieren kann
			errno = ENOENT;
			return(-1);
		}
		
		// achtung: hier kopiert man das ganze struct
		lfd->event = *event;
		assertEventFlags(lfd->event.events);
		
		lfd->isLevelTriggered = ! IS_EVENT_SET(event->events, EPOLLET);
		return(0);
		
	case (EPOLL_CTL_DEL):
		// in diesem Falle loescht man einen raus
		lfd = getListenerForFd(pfd, sockfd);
		if (!lfd)
		{
			errno = ENOENT;
			return(-1);
		}
		deleteListener(lfd);
		return(0);
		
	default:
		errno = EINVAL;
		return(-1);
	}	
}



// signale werden sowieso ignoriert
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
	FUNC_LOCK();
	return(epoll_pwait(epfd, events, maxevents, timeout, NULL));
}
                      


// hier wartet man auf die events oder darauf, dass das timeout abgelaufen ist
int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout,
                      const sigset_t *_Null sigmask)
{
	FUNC_LOCK();
	
	debug_printf("epoll_pwait(int epfd=%d, struct epoll_event *events=%p, int maxevents=%d, int timeout=%d, ...)\n",
		epfd, events, maxevents, timeout);
	
	// also man fuellt den events array aus mit events, die stattegfunden haben 
	// oder sonst blockiert man, wenn es keins gibt oder timeout
	
	struct epollfd* pfd = getPollfdByNum(epfd);
	int eventsReturned = 0;
	int elapsedSleep = 0;
	validatePfd(pfd);
	
	//#define eventsListening lfd->event.events
	#define eventsConnection 		lfd->fd->pConn->own_events
	#define eventsConnection_lt 	lfd->fd->pConn->own_lt_events
	
	#define checkAndSetEvents(eventsListening) \
		if (lfd->isLevelTriggered) { \
			if (!(lfd->fd->state == s_listen || lfd->fd->state == s_connecting)){ \
				setLevelTriggeredEvents(lfd->fd->pConn); \
			} \
			if (IS_EVENT_SET(eventsConnection_lt, eventsListening)) { \
				events[eventsReturned++] = (struct epoll_event){ \
					.data=lfd->event.data ,.events=(eventsConnection_lt & eventsListening)}; \
			} \
		} \
		else{ \
			if (IS_EVENT_SET(eventsConnection, eventsListening)){ \
				events[eventsReturned++] = (struct epoll_event){ \
					.data=lfd->event.data ,.events=(eventsConnection & eventsListening)}; \
				CLEAR_EVENT(eventsConnection, eventsListening); \
			} \
		}
	
	while (1)
	{
		// wenn man ein komma nimmt in der Bedinung dann nimmt man nur das letzte
		for (int j = 0; j < MAX_FDS_LISTEN && eventsReturned < maxevents; j++)
		{		
			struct eventListenFd* lfd = pfd->lfds + j;
			
			// weitermachen, wenn der listener nicht aktiv ist
			if (!lfd->active) continue;
			
			// zudem check machen, ob der fd noch aktiv ist fuer den lfd
			if (!lfd->fd->active)
			{
				handleFdCloseInEvents(pfd, lfd);
				continue;
			}
			
			// spezialbehandlung fuer neue Verbindungen die reinkommmen
			if (lfd->fd->state == s_listen)
			{
				// man hat einen listener socket
				// wenn neue verbindung vorhanden, read signalisieren
				// https://linux.die.net/man/2/accept4
				// A readable event will be delivered when a new connection is attempted and 
				// you may then call accept() to get a socket for that connection.
				
				// schauen ob man eine neue Verbindung hat
				if (searchNewConnectionOnAnyIface(lfd->fd->listenAddress, lfd->fd->listenPort))
				{
					// es sind neue verbindungen verfuegbar
					// nur returnen, wenn man auch auf POLLIN listened, sonst nicht
					// setze lt und normale events auf input
					SET_EVENT(eventsConnection, EPOLLIN);
					SET_EVENT(eventsConnection_lt, EPOLLIN);
					
					checkAndSetEvents(EPOLLIN);
				}			
				else
				{
					// wichtig: Wenn das event nicht gesetzt, dann loeschen bei lt
					CLEAR_EVENT(eventsConnection_lt, EPOLLIN);
				}
				continue;
			}
			
			// spezialbehandlung fuer connect non-block
			if (lfd->fd->state == s_connecting)
			{
				if (checkIfAcceptedConnection(lfd->fd->pConn))
				{
					// der Socket wurde akzeptiert, die Frage ist, ob man den FD nun automatisch
					// auf s_open stellen soll oder einfach nur das Event zurueckgeben...
					// ich gehe davon aus, nur das Event zurueckgeben - falsch! Beides 
					
					// EPOLLIN oder EPOLLOUT?
					SET_EVENT(eventsConnection, EPOLLIN);
					SET_EVENT(eventsConnection_lt, EPOLLIN);
					checkAndSetEvents(EPOLLIN);
					
					// also nun auch noch den Socket connecten, einfach call simulieren
					// das war ein krasser bug, es muss connectEx erwartet die fd nummer nicht 
					// struct fd* !!!
					connectEx(lfd->fd->num, NULL, 0);
					
				}
				else
				{
					CLEAR_EVENT(eventsConnection_lt, EPOLLIN);
				}
				continue;
			}
			
			// sonst schauen, ob es neue events gegeben hat, auf die man hoert
			checkAndSetEvents(lfd->event.events);
		}
		
		// so zudem muss man noch checken, ob es im falle vom einem mixed fd
		// sonst noch events gegeben hat
		if (pfd->isMixed)
		{
			// also man versucht den restlichen platz noch mit real events aufzufuellen
			// falls es sonst keine gegeben hat
			
#if defined(SYS_epoll_wait)

			int ret = syscall(SYS_epoll_wait, 
					pfd->realPollfd, 					// richtigen pfd nehmen!
					events + eventsReturned, 			// offset verandern
					maxevents - eventsReturned, 		// laenge anpassen
					0									// sofort returnen
				);
				
#elif defined(SYS_epoll_pwait)

			int ret = syscall(SYS_epoll_pwait, 
					pfd->realPollfd, 					// richtigen pfd nehmen!
					events + eventsReturned, 			// offset verandern
					maxevents - eventsReturned, 		// laenge anpassen
					0,
					NULL
				);
#else
#error "epoll API not supported"
#endif

			
			if (ret > 0)
				eventsReturned += ret;
			//else
				// ignorieren, es hat einen epoll fehler gegeben
		}
		
		// man hat events gefunden, aufhoeren
		if (eventsReturned > 0)
			break;
		
		// in diesem Falle hat man keine events gefunden, also geht man schlafen
		// wenn timeout erreicht, dann fehler
		#define SLEEP_DURATION 100
		
		// wenn timeout -1 ist, dann heisst das unendlich lange warten
		// timeout ist in millisekunden, sleep ist in mikrosekunden
		if (timeout != -1 && (elapsedSleep++)*SLEEP_DURATION > timeout*1000)
		{
			return(0);	// kein errno
		}
		else 
		{
			// wichtig. waehrend dem schlafen den lock abgeben, denn sonst koennen
			// andere threads gar nichts mehr machen...
			SLEEP_LOCKED(SLEEP_DURATION);
		}
	}
	
	return(eventsReturned);
}


// poll ist einfacher zu implementieren als epoll denke ich
// annahme: EPOLL.. Konstanten sind gleich wie POLL... Konstanten (EPOLLIN=POLLIN,...)
// ein bisschen verschwenderisch mit epollfds (jedes mal ein neuer) aber egal
// ich frage mich sowieso, ist es notwendig errno = SUCESS zu setzen nach erfolgreichem API-call?
// theoretisch schon, aber wenn man am return schon erkennt, ob man success hat oder nicht schaut
// man sich ernno auch nie an

// Problem das auftritt: poll wird sehr oft aufgerufen und wenn es immer einen neuen fd nimmt, dann 
// hat man ziemlich schnell keine FDs mehr. Loesung: einen einzigen FD nutzen fuer poll und dann halt
// immer wieder loeschen

int ppoll(struct pollfd *fds, nfds_t nfds, struct timespec *_Null tmo_p,sigset_t *_Null sigmask)
{
	FUNC_LOCK();
	int timeout = (tmo_p == NULL) ? -1 : (tmo_p->tv_sec * 1000 + tmo_p->tv_nsec / 1000000);
	return(poll(fds, nfds, timeout));
}

int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
	FUNC_LOCK();
	
	debug_printf("poll(struct epollfd *fds=%p, nfds_t nfds=%d, int timeout=%d)\n",
		fds, nfds, timeout);
	
	// einen einzigen pollfd nehmen
	static int epollfd = -1;
	
	if (epollfd == -1)
	{
		epollfd = epoll_create1(0);
		if (epollfd == -1){
			return(-1);
		}
	}
	
	struct epoll_event epollEvents[nfds];	// vla
	
	for (int i=0; i < nfds; i++)
	{
		epollEvents[i].events = fds[i].events;
		epollEvents[i].data.u32 = i;	// zeigt auf index 
		
		if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fds[i].fd, epollEvents+i) == -1){
			return(-1);	// ernno propagieren
		}
	}
	
	// dann wenn man alle aufgesetzt hat, kann man beginnen zu warten
	int ret = epoll_wait(epollfd, epollEvents, nfds, timeout);
	
	// alle return events default auf NULL setzen, falls nicht passiert
	for (int i=0; i < nfds; i++)
		fds[i].revents  = 0x0;
		
	// dann die resultate durchgehen
	for (int i=0; i < ret; i++)
	{
		fds[epollEvents[i].data.u32].revents = epollEvents[i].events;
	}
	
	// dann am schluss noch alle EPOLLs wieder loeschen sodass man epollfd wieder neu
	// verwenden kann am Ende
	for (int i=0; i < nfds; i++)
	{
		if (epoll_ctl(epollfd, EPOLL_CTL_DEL, fds[i].fd, NULL) == -1){
			return(-1);
		}
	}
	
	return(ret);
}




