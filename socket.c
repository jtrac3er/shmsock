
#include "list.h"
#include "socket.h"
#include "_malloc.h"
#include <string.h>
#include <sys/syscall.h>
#include "debug.h"

#ifdef ENCLAVE
#include "errno_enclave.h"
#else
#include <errno.h>
#endif


#define assertFd(fd) \
	if (!fd) \
	{ \
		errno = EBADF; \
		return(1); \
	}

#define assertFdState(fd, _state) \
	if (!fd || fd->state != _state) \
	{ \
		errno = EBADF; \
		return(-1); \
	}
		
#define assertFdState2(fd, s1, s2) \
	if (!fd || (fd->state != s1 && fd->state != s2)) \
	{ \
		errno = EBADF; \
		return(-1); \
	}

#define assertNewFdAllocation(fd) \
		if (!fd) { \
		errno = ENOMEM; \
		return(-1); }
	
#define PORT(addr) (((struct sockaddr_in*)addr)->sin_port)
#define SOCKADDR(addr) (sock_address)(((struct sockaddr_in*)addr)->sin_addr.s_addr)

#define TO_PORT(port) port
#define TO_SOCKADDR(addr) (struct in_addr){(unsigned long)addr}

#define NOT_IMPLEMENTED() \
	debug_printf("function %s not implemented\n", __FUNCTION__)
	
#define NOT_IMPLEMENTED_COMPLETE() \
	debug_printf("function %s operation not supported\n", __FUNCTION__)



// So nun kommt es zum eigentlichen API

struct fd fds[MAX_FDS];
int currentMaxFd = FD_START;
int randomPort = 10000;

struct fd* getFdByNum(int num)
{
	for (int i = 0; i < MAX_FDS; i++)
	{
		if (fds[i].active && fds[i].num == num)
			return fds +i;
	}
	return NULL;
}

struct fd* getEmptyFd()
{
	for (int i = 0; i < MAX_FDS; i++)
	{
		if (!fds[i].active)
		{
			fds[i].active = 1;
			return fds +i;
		}
	}
	return NULL;
}

void deleteFd(struct fd* fd)
{
	fd->active = 0;
}

void handleCloseWhileIO(struct fd* fd)
{
	fd->state = s_close;
}

// achtung: Fuehrt zu problemen, wenn ein thread den fd schliesst, den
// ein anderer thread gerade benuetzt. Und zwar, weil man den fd komplett loescht
// also zur Wiederverwendung freigibt. wenn dann ein anderer FD allocated wird an derselben
// stelle dann kann das zu Problemem fuehren

// Genug grosse anzahl FDs nehmen um es zu verhindern, MAX_SOCK am besten, dann hat man keine
// probleme mehr, aber dafuer ist es langsamer

// 100% loesung: alle alten FD-structs aufbewahren und erst loeschen, wenn keiner mehr referenziert

int handleSockfdClose(int sockfd)
{
	struct fd* fd = getFdByNum(sockfd);
	assertFd(fd);
	
	if (fd->pConn){
		closeConnection(fd->pConn);
	}
	// bringt vielleicht etwas: Wenn ein anderer thread gerade noch den FD nutzt
	fd->state = s_close;
	deleteFd(fd);
	return 0;
}

// lock implementieren, sodass alle Funktionen MT sicher sind. Das ist wichtig
// wenn ich ausserhalb von Enklaven teste
#define NOLOCK

#include "api-tools.h"

// Ab hier Socket API


int socket(int domain, int type, int protocol)
{
	FUNC_LOCK();
	
	debug_printf("socket(int domain=%d, int type=%d, int protocol=%d)\n",
		domain, type, protocol);
		
	// eigentlich man alle ignorieren, es gibt nur eine Art der Kommunikation
	// error wenn was anderes genommen wird
	//if (domain != AF_INET || type != SOCK_STREAM || protocol != 0)
	if (domain != AF_INET)
	{
		debug_printf("Requested socket AF not supported\n");
		errno = EAFNOSUPPORT; 
		return(-1);
	}
	// type kann mit 2 flags geORt sein, wegnehmen, cloexec wird ignoriert
	if ((type & ~(SOCK_NONBLOCK | SOCK_CLOEXEC)) != SOCK_STREAM /*|| protocol != 0*/)
	{
		debug_printf("Requested socket type or PF not supported\n");
		errno = EPROTONOSUPPORT;
		
		return(-1);
	}
	
	if (currentMaxFd >= POLLFD_START)
	{
		errno = ENFILE;
		return(-1);
	}
	
	struct fd* fd = getEmptyFd();
	assertNewFdAllocation(fd);

	fd->num = currentMaxFd++;
	fd->state = s_created;
	fd->pConn = NULL;
	fd->flagNonblock = type & SOCK_NONBLOCK;
	
	return(fd->num);
}


// es wird angenommen, dass man ueberall (alle ifaces) dieselbe Addresse hat und auf die Addresse
// wird auch nicht geschaut beim akzeptieren, nur auf den Port
int bind(int sockfd, struct sockaddr *addr, socklen_t addrlen)
{
	FUNC_LOCK();
	
	debug_printf("bind(int sockfd=%d, struct sockaddr *addr=%p, socklen_t addrlen=%d)\n",
		sockfd, addr, addrlen);

	struct fd* fd = getFdByNum(sockfd);
	assertFdState(fd, s_created);
	
	// erstelle eine Listener Connection
	fd->listenPort = PORT(addr);
	fd->listenAddress = SOCKADDR(addr);
	fd->state = s_bound;

	return(0);
	
}

int listen(int sockfd, int backlog)
{
	FUNC_LOCK();
	
	debug_printf("listen(int sockfd=%d, int backlog=%d)\n",
		sockfd, backlog);
	
	struct fd* fd = getFdByNum(sockfd);
	assertFdState(fd, s_bound);
	
	// erstelle neu auch eine connection die nur auf create ist
	// So koennen dennoch events gesetzt werden fuer die connection
	fd->pConn = createNewListenerConnection();
	
	fd->state = s_listen;
	return(0);
}


int accept(int sockfd, struct sockaddr *_Null _out addr, socklen_t *_Null _out addrlen)
{
	FUNC_LOCK();
	return(accept4(sockfd, addr, addrlen, 0));
}


int accept4(int sockfd, struct sockaddr *_Null _out addr, socklen_t *_Null _out addrlen, int flags)
{
	FUNC_LOCK();
	
	debug_printf("accept4(int sockfd=%d, struct sockaddr *addr=%p, socklen_t *addrlen=%p, int flags=%d)\n",
		sockfd, addr, addrlen, flags);
	
	struct fd* fd = getFdByNum(sockfd);
	assertFdState(fd, s_listen);
	
	// so hier wird es nun etwas komplizierter. Und zwar sollen alle Interfaces
	// abgegengen werden und es soll geschaut werden, ob irgend eine Verbindung verfuegabar ist
	
	struct connection* pConn;
	while (!(pConn = acceptNewConnectionOnAnyIface(fd->listenAddress, fd->listenPort)))
	{
		// es gibt gerade keine verbindung die angenommen werden kann
		if (fd->flagNonblock) 
		{
			errno = EAGAIN;
			return(-1);
		}
		else if (fd->state == s_close)
		{
			// man hat geschlossen waehrend man am accepten war von anderem thread
			errno = EBADF;
			return(-1);
		}
		else
		{
			SLEEP_LOCKED(1000);
		}
	}
	
	// nun hat man eine Connection die angenommen wurde
	// es kann nun ein neuer FD erstellt werden
	struct fd* newFd = getEmptyFd();
	assertNewFdAllocation(fd);
	
	newFd->num = currentMaxFd++;
	newFd->state = s_open;
	newFd->pConn = pConn;
	newFd->flagNonblock = flags & SOCK_NONBLOCK;
	
	// Addresse ausfuellen im userspace von neuer verbindung
	// addr ausfuellen
	getpeername(newFd->num, addr, addrlen);
	
	return(newFd->num);
}

// dasselbe wie connect, nur dass man es mehrere male aufrufen kann um zu checken
// ob der socket nun akzeptiert wurde oder nicht
int connectEx(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	FUNC_LOCK();
	struct fd* fd = getFdByNum(sockfd);
	assertFdState2(fd, s_created, s_connecting);
	
	if (fd->state == s_connecting)
		// in diesem Falle direkt zum checken gehen
		goto wait_for_accept;
	
	// zuerst ein Interface suchen, welches die Addresse unterstuetzt
	struct interface* pIface = getInterfaceBySockaddr(SOCKADDR(addr));
	if (!pIface)
	{
		// die Addresse ist nicht erreichbar
		errno = ENETUNREACH;
		return(-1);		
	}
	
	// in diesem Falle muss man die Connection noch starten
	fd->pConn = createNewConnection(pIface, randomPort++, PORT(addr));
	
	// und die verbindung zum akzeptieren freigeben
	initiateNewConnection(fd->pConn);
	fd->state = s_connecting;
	
wait_for_accept:
	
	// warten bis akzeptiert, variiert je nach nonblock
	// zudem gibt 'checkIfAcceptedConnection' auch true, wenn geschlossen
	while (!checkIfAcceptedConnection(fd->pConn))
	{
		if (checkIfClosedConnection(fd->pConn))
		{
			// die Verbinung wurde geschlossen bevor man verbinden konnte
			errno = ECONNREFUSED;
			return(-1);
		}
		else if (fd->flagNonblock) 
		{
			errno = EINPROGRESS;
			return(-1);
		}
		else
		{
			SLEEP_LOCKED(1000);
		}
	}
	
	// so die Verbindung wurde akzeptiert
	fd->state = s_open;
	return(0);
}



int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
	FUNC_LOCK();
	
	debug_printf("connect(int sockfd=%d, struct sockaddr *addr=%p, socklen_t addrlen=%d)\n",
		sockfd, addr, addrlen);
	
	struct fd* fd = getFdByNum(sockfd);
	assertFdState2(fd, s_created, s_connecting);
	
	// hier auch nochmals kompliziert. Man muss die Verbindung erstellen
	// dann warten bis angenommen
	
	if (fd->state == s_connecting)
	{
		// man hat schon connected, muss warten
		errno = EALREADY;
		return(-1);
	}
	
	// ex aufrufen
	return(connectEx(sockfd, addr, addrlen));
}



ssize_t send(int sockfd, void* buf, size_t len, int flags)
{
	FUNC_LOCK();
	
	debug_printf("send(int sockfd=%d, void* buf=%p, size_t len=%d, int flags=%d)\n",
		sockfd, buf, len, flags);
	
	struct fd* fd = getFdByNum(sockfd);
	assertFdState(fd, s_open);
	
	if (fd->pConn->connState == c_close)
	{
		// nicht verbunden, spezieller errorcode
		handleCloseWhileIO(fd);
		errno = ENOTCONN;
		return(-1);
	}
	
	return(sendConnection(fd->pConn, buf, len));
}
 
 
ssize_t recv(int sockfd, void* buf, size_t len, int flags)
{
	FUNC_LOCK();
	
	debug_printf("recv(int sockfd=%d, void* buf=%p, size_t len=%d, int flags=%d)\n",
		sockfd, buf, len, flags);
	
	struct fd* fd = getFdByNum(sockfd);
	assertFdState(fd, s_open);
	int ret;
	
	while ((ret = recvConnection(fd->pConn, buf, len)) == 0)
	{
		// es gibt gerade keine Daten zum lesen
		
		if (fd->pConn->connState == c_close)
		{
			// socket wurde waehrenddessen geschlossen
			handleCloseWhileIO(fd);
			return(0);	// EOF
		}
		else if (fd->flagNonblock || flags & MSG_DONTWAIT)
		{
			// es gibt keine Daten zum lesen
			errno = EAGAIN;
			return(-1);
		}
		else
		{
			// warten bis Daten da sind, timeout?
			// da sollte man nicht zu lange warten, hoechstens 100us
			// sonst hat man moeglicherweise lange latenzen
			SLEEP_LOCKED(10);
		}
	}
	// Man hat Daten gelesen in den Buffer
	return(ret);
}

// shutdown ist etwas komisch.. aber libmemcached und ich denke alle anderen Programme
// verwenden shutdown() immer vor einem close() und es ist mehr oder weniger ein NOP
// ich behandle es hier deshalb auch wie ein NOP
int shutdown(int sockfd, int how)
{
	FUNC_LOCK();
	
	debug_printf("shutdown(int sockfd=%d, int how=%d)\n",
		sockfd, how);
		
	struct fd* fd = getFdByNum(sockfd);
	assertFdState2(fd, s_open, s_shutdown);
	
	switch (how)
	{
	case SHUT_RD:
		// in diesem falle will man nichts mehr empfangen ueber diesen Socket
		return(0);
		
	case SHUT_WR:
		// in diesem falle will man nichts mehr senden ueber diesen Socket
		return(0);
	
	case SHUT_RDWR:
		// in diesem falle will man entweder senden oder empfangen
		// also man hat eigentlich geschlossen aber will wiederverwenden, fd nicht loeschen
		return(0);
	
	default:
		errno = EINVAL;
		return(-1);
	}
	
}


int close(int sockfd)
{
	FUNC_LOCK();
	// hier muss man zuerst schauen, ob man wirklich einen sockfd hat, sonst weiterleiten
	// es koennte theoretisch auch zu kollisionen kommen
	
	if (sockfd < FD_START)
	{
		// in diesem falle exisitiert der FD nicht, weiterleiten
		return(syscall(SYS_close, sockfd));
	}
	else if (sockfd < POLLFD_START)
	{	
		// in diesem falle ist es ein Socket
		// Achtung: Die Verbindung kann in mehreren Zustaenden sein
		return(handleSockfdClose(sockfd));
	}
	else
	{
		// in diesem falle ist es ein Pollfd
		return(handleEpfdClose(sockfd));
	}
}


// sogar ioctl wird von sockets gebraucht
int ioctl(int sockfd, unsigned long request, void* argp)
{
	FUNC_LOCK();
	if (sockfd < FD_START){
		return(syscall(SYS_ioctl, sockfd, request, argp));
	}
	
	// sonst... tja was macht man sonst? einfach ignorieren
	// ist gar nicht so einfach, denn ioctls koennen eigene return
	// werte erzeugen, wo nicht klar definiert ist, was jetzt success ist (nicht immer 0 vlt)
	// hier wird einfach 0 angenommen
	NOT_IMPLEMENTED();
	return(0);			
}

// read write auch noch uebernehmen
ssize_t read(int sockfd, void* buf, size_t count)
{
	FUNC_LOCK();
	if (sockfd < FD_START){
		return(syscall(SYS_read, sockfd, buf, count));
	}
	else{
		return(recv(sockfd, buf, count, 0));
	}
}



ssize_t write(int sockfd, const void* buf, size_t count)
{
	FUNC_LOCK();
	if (sockfd < FD_START){
		return(syscall(SYS_write, sockfd, buf, count));
	}
	else{
		return(send(sockfd, buf, count, 0));
	}
}


// anderer name wie es scheint
int fcntl64(int sockfd, int cmd, uintptr_t arg)
{
	FUNC_LOCK();
	return(fcntl(sockfd, cmd, arg));
}



int fcntl(int sockfd, int cmd, uintptr_t arg)
{
	FUNC_LOCK();
	if (sockfd < FD_START){
		return(syscall(SYS_fcntl, sockfd, cmd, arg));
	}
	
	// so wie es aussieht muss es implementiert sein... memached braucht es
	// zudem macht es einen unterschied was der fd fuer einen zustand hat
	
	struct fd* fd = getFdByNum(sockfd);
	assertFd(fd);
	
	switch (cmd)
	{
		case (F_GETFL):
			// lese die flags die der socket hat
			// man schaut nur auf die nonblock flag
			return((fd->flagNonblock ? O_NONBLOCK : 0));
		
		case (F_SETFL):
			fd->flagNonblock = arg & O_NONBLOCK;
			return(0);
			
		// anscheinend kann man O_NONBLOCK auch ueber F_GETFD und F_SETFD machen
		// man unterscheidet zwischen file status flags und file descriptor flags
		// ich nehme an, man nimmt beides mal dasselbe O_NONBLOCK
		
		// oder vielleicht ist auch der nonblock-code falsch, keine Ahnung
		
		case (F_GETFD):
			return(fd->flagNonblock ? O_NONBLOCK : 0);
		
		case (F_SETFD):
			fd->flagNonblock = arg & O_NONBLOCK;
			return(0);
		
		default:
			NOT_IMPLEMENTED_COMPLETE();
			errno = EINVAL;
			return(-1);
	}
}

// vereinfachte implementation: Es wird einfach als scattered read genommen
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	FUNC_LOCK();
	struct fd* fd = getFdByNum(sockfd);
	assertFdState(fd, s_open);
	int bytesSent = 0;

	if (fd->pConn->connState == c_close)
	{
		// nicht verbunden, spezieller errorcode
		handleCloseWhileIO(fd);
		errno = ENOTCONN;
		return(-1);
	}
	
	// msg name ignorieren, anc data ingorieren, nur message iov betrachten
	for (int i = 0; i < msg->msg_iovlen; i++)
	{
		// jedes einzelne stueck schicken
		int ret = send(sockfd, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len, flags);
		if (ret < 0)
		{
			return(-1);
		}
		else
			bytesSent += ret;
	}
	return(bytesSent);
}

ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	FUNC_LOCK();
	NOT_IMPLEMENTED();
	return(-1);
}


int getsockopt(int sockfd, int level, int optname, 
	void* optval, socklen_t *restrict optlen)
{
	FUNC_LOCK();
	NOT_IMPLEMENTED();
	return(0);			// ignorieren
}	

int setsockopt(int sockfd, int level, int optname,
	void* optval, socklen_t optlen)
{
	FUNC_LOCK();
	NOT_IMPLEMENTED();	// ignorieren, kein fehler wird erzeugt!
	return(0);
}


// hier die eigene addresse
int getsockname(int sockfd, struct sockaddr * addr, socklen_t * addrlen)
{
	FUNC_LOCK();
	
	debug_printf("getsockname(int sockfd=%d, struct sockaddr *addr=%p, socklen_t *addrlen=%d)\n",
		sockfd, addr, addrlen);
	
	struct fd* fd = getFdByNum(sockfd);
	assertFd(fd);
	struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
	
	if (fd->state == s_listen)
	{
		// in diesem falle kann man den listener port zureckgeben
		addr_in->sin_port = TO_PORT(fd->listenPort);
		addr_in->sin_addr = TO_SOCKADDR(fd->listenAddress);
	}
	else
	{
		// in diesem falle muss man den random Port der connection nehmen
		if (!fd->pConn || fd->pConn->connState != c_open)
		{
			errno = ENOTCONN;
			return(-1);
		}
		addr_in->sin_port = TO_PORT(fd->pConn->own_port);
		addr_in->sin_addr = TO_SOCKADDR(
			fd->pConn->pAssociatedIface->own_address);
	}
	return(0);
}
                       

// hier der verbundene, wenn verbunden
int getpeername(int sockfd, struct sockaddr * addr, socklen_t * addrlen)
{
	FUNC_LOCK();
	
	debug_printf("getpeername(int sockfd=%d, struct sockaddr *addr=%p, socklen_t *addrlen=%d)\n",
		sockfd, addr, addrlen);
	
	struct fd* fd = getFdByNum(sockfd);
	assertFd(fd);
	
	// sollte eigentlich nicht vorkommen
	if (!fd->pConn || fd->pConn->connState != c_open)
	{
		errno = ENOTCONN;
		return(-1);
	}
	
	struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
	addr_in->sin_family = AF_INET;
	addr_in->sin_port = TO_PORT(fd->pConn->remote_port);
	addr_in->sin_addr = TO_SOCKADDR(
		fd->pConn->pAssociatedIface->remote_address);
	
	return(0);
}




