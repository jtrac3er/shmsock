#include "list.h"
#include "util.h"
#include "_malloc.h"
#include <string.h>
#include "debug.h"

#ifdef ENCLAVE
#include "errno_enclave.h"
#else
#include <errno.h>
#endif


int server_mode = 0;

struct interface* interfaces[MAX_INTERFACES];

struct interface** getFreeInterface()
{
	for (int i = 0; i < MAX_INTERFACES; i++)
		if (!interfaces[i]) 
			return interfaces +i;
			
	return NULL;
}

struct interface* getInterfaceBySockaddr(sock_address address)
{
	for (int i = 0; i < MAX_INTERFACES; i++)
		if (interfaces[i] && interfaces[i]->remote_address == address) 
			return interfaces[i];
			
	return NULL;
}

// dasselbe wie oben, einfach man sucht seine eigene Adressse
struct interface* getOwnInterfaceBySockaddr(sock_address address)
{
	for (int i = 0; i < MAX_INTERFACES; i++)
		if (interfaces[i] && interfaces[i]->own_address == address) 
			return interfaces[i];
			
	return NULL;
}

void* getAddressFromFile(char* fname)
{
	void* ret;
	FILE* f = fopen(fname, "r");
	fscanf(f, "%llu", &ret);
	fclose(f);
	return ret;
	
}

void setAddressToFile(char* fname, void* addr)
{
	FILE* f = fopen(fname, "w");
	fprintf(f, "%llu", (uintptr_t)addr);
	fclose(f);
	
}

// sehr leaky aber egal

char* straddstr(char* str1, char* str2)
{
	char* ret = (char*)malloc(0x100);
	strcpy(ret, str1);
	strcat(ret, str2);
	return ret;
}

char* straddnum(char* str, int num)
{
	char lit[0x20];
	sprintf(lit, "%d", num);
	return straddstr(str, lit);
}
	 


#define getInterfaceByIndex(index) (interfaces +i)

#define PTRADD(ptr, val) (void*)(((char*)ptr) + val)

#define MALLOC_IFACE() setMallocContext(&pIface->mctx)
#define MALLOC_CONN() setMallocContext(&pConn->pAssociatedIface->mctx)
#define MALLOC_NORM() setMallocContext(NULL);

#define new(type) (type*)malloc(sizeof(type))


#if defined(ENCLAVE) || defined(ENCLAVE_TEST)

// so hier kommt nun das ganze rein, wenn es als Enklave laufen soll. Enklaven erfodern ja
// eine spezialbehandlung fuer die erstellung von shmem
// ich hoffe es gibt keine Probleme wegen bool (struct in c++ definiert)

// neue variante, regions.h wird per -include gcc flag gesteuert, das macht es einfacher
//#include "regions.h"

int registerInterface2(void* addr, int size, int asServer, 
	sock_address remoteAddress, sock_address ownAddress)
{
	struct interface** ppIface = getFreeInterface();
	if (!ppIface) return 0;

	void* region = addr;

	struct interface* pIface = (struct interface*)region;
	if (!pIface) return 0;
	*ppIface = pIface;
	
	if (asServer)
	{
		// zuerst das shmem loeschen, denn es ist persistent
		memset(region, 0x0 , size);

		// daten ausfuellen
		pIface->magic = IFACE_MAGIC;
		pIface->remote_address = remoteAddress;
		pIface->own_address = ownAddress;
		pIface->shmemSize = size;
		pIface->shmemStart = region;
		pIface->shmemEnd = PTRADD(region, size);
		
		// malloc context erstellen
		pIface->mctx = createMallocContext(
			pIface + 1, size - sizeof(struct interface));
		
		// lists erstellen
		pIface->connectionList = llist_new(&pIface->mctx);
	}

	return 1;
}


// nicht sicher ob constructor in enklaven funktionieren, ggfs manuell aufrufen in eapp
// nein, wenn man statisch linkt dann werden Konsruktoren ignoriert
void /*__attribute__ ((constructor))*/ setupIface()
{
	static int ifaceAlreadySetup = 0;
	if (ifaceAlreadySetup) return;
	ifaceAlreadySetup = 1;
	
	// wenn test genommen wird, dann muss dieses einfach anders definiert werden
	setupSharedMemory();
	
	for (int i = 0; i < noRegions; i++)
	{
		struct region* pRegion = allRegions +i;
		int ret = registerInterface2(pRegion->loadVA, pRegion->regionSize, pRegion->isServer, 
			pRegion->isServer ? pRegion->clientAddress : pRegion->serverAddress,	// remote address
			pRegion->isServer ? pRegion->serverAddress : pRegion->clientAddress		// own address
		);
		
		if (ret == 0)
		{
			printf("[shmsock] could not setup shared region no.%d\n", i);
			exit(1);
		}
	}
}


#else

#define ADDRFILE_SUFFIX "addr"

void* createSharedRegion(char* name, int size, void* addr)
{
    key_t key;
    int shmid;
    char *data;
    int mode;

    /* make the key: */
    if ((key = ftok(name, 'r')) == -1) /*Here the file must exist */ 
    	return NULL;

    /*  create the segment: */
    if ((shmid = shmget(key, size, 0644 | IPC_CREAT)) == -1) 
    	return NULL;

    /* attach to the segment to get a pointer to it: */\
    // achtung: man muss die Addresse angeben wo man es hinwill
    if ((data = shmat(shmid, addr, SHM_RND)) == (void *)-1)
    	return NULL;

    return data;
}

#if defined(MODE_SERVER) && !defined(MODE_FIXED_REGION)

// wenn man im server mode ist und fixed region nicht gegeben ist, 
// dann einfach ein random teil nehmen, sonst break es legacy apps
#define MODE_RANDOM_REGION

#endif

// TODO: Thread safety beim erstellen
int registerInterface(char* name, int size, 
		sock_address remoteAddress, sock_address ownAddress)
{
	struct interface** ppIface = getFreeInterface();
	if (!ppIface) return 0;
	
	char addrFilename[0x100];
	strcpy(addrFilename, name);
	strcat(addrFilename, ".");
	strcat(addrFilename, ADDRFILE_SUFFIX);
	
#ifdef MODE_RANDOM_REGION

	// region irgendwo erstellen
	void* region = createSharedRegion(name, size, NULL);
	setAddressToFile(addrFilename, region);
	
#else
	
	// lesen bei welcher addresse die region steht und an derselben stelle mounten
	// wenn nicht, dann hat man pech
	void* addr = getAddressFromFile(addrFilename);	// muss zuerst gelesen werden
	void* region = createSharedRegion(name, size, addr);
	
#endif

	struct interface* pIface = (struct interface*)region;
	if (!pIface) return 0;
	
	*ppIface = pIface;
	
// nur der Server muss die Daten ausfuellen, der client kriegt sie
// schon wie shared memory
#ifdef MODE_SERVER

	// zuerst das shmem loeschen, denn es ist persistent
	memset(region, 0x0 , size);

	// daten ausfuellen
	pIface->magic = IFACE_MAGIC;
	pIface->remote_address = remoteAddress;
	pIface->own_address = ownAddress;
	pIface->shmemSize = size;
	pIface->shmemStart = region;
	pIface->shmemEnd = PTRADD(region, size);
	
	// malloc context erstellen
	pIface->mctx = createMallocContext(
		pIface + 1, size - sizeof(struct interface));
	
	// lists erstellen
	pIface->connectionList = llist_new(&pIface->mctx);

#endif

	return 1;
}



#ifdef MODE_PRELOAD

#define ENV_OWN_ADDRESS "SHMSOCK_OWN_ADDRESS"
#define ENV_REMOTE_ADDRESS "SHMSOCK_REMOTE_ADDRESS"

#define ENV_OWN_ADDRESS_N ENV_OWN_ADDRESS "_"
#define ENV_REMOTE_ADDRESS_N ENV_REMOTE_ADDRESS "_"
#define ENV_IFACE_COUNT "SHMSOCK_IFACE_COUNT" 
#define ENV_IFACE_START "SHMSOCK_IFACE_START" 

#define ENV_DIR "SHMSOCK_DIR" 


void __attribute__ ((constructor)) setupIface()
{
	// in diesem Falle wurde das library durch preloading erstellt und
	// die IP-Addressen muesen ueber environment variablen genommen werden
	
	// ich brauche noch support um die Region an eine bestimmte Stelle zu laden wo es
	// sicherlich auch platz hat bei beiden Prozessen
	
	// vielleicht in Zukunft noch mehrere Ifaces ermoeglichen
	
	// schutz vor mehrfachem aufrufen
	static int alreadyInitialized = 0;
	if (alreadyInitialized) return;
	alreadyInitialized = 1;
	
	int ifacecount; int ifacestart;
	char* env_ifacecount = getenv(ENV_IFACE_COUNT);
	char* env_ifacestart = getenv(ENV_IFACE_START);
	
	if (!env_ifacecount || !env_ifacestart)
	{
		printf("[shmsock] provide iface count and start variable\n");
		exit(1);
	}
	
	sscanf(env_ifacecount, "%d", &ifacecount);
	sscanf(env_ifacestart, "%d", &ifacestart);
	
	for (int j = 0, i = ifacestart; j < ifacecount; j++, i++)
	{
	
		char* env_own_address = getenv(straddnum(ENV_OWN_ADDRESS_N, i));
		char* env_remote_address = getenv(straddnum(ENV_REMOTE_ADDRESS_N, i));
		
		// entweder diesen Ordner nehmen oder den angegeebenn
		char* env_dir = getenv(ENV_DIR) ? straddstr(getenv(ENV_DIR), "/") : "./";
		
		if (!env_own_address || !env_remote_address)
		{
			printf("[shmsock] provide environment with addresses for iface '%d' setup\n",
				i);
			exit(1);
		}
		
		int own_addr, remote_addr;
		sscanf(env_own_address, "%d", &own_addr);
		sscanf(env_remote_address, "%d", &remote_addr);
		
		if (!registerInterface(straddstr(env_dir, straddnum("iface.", i)), 0x10000, remote_addr, own_addr))
		{
			printf("[shmsock] couldn't create iface '%d'\n", i);
			exit(1);
		}
	}
		
    printf("[shmsock] Iface setup finished\n");
}

#endif


#endif



// erstellt eine Verbindung zu den Port auf einem Interface
struct connection* createNewConnection(
	struct interface* pIface, int own_port, int remote_port)
{
	MALLOC_IFACE();
	struct connection* pConn = new(struct connection);
	MALLOC_NORM();
	
	pConn->own_port = own_port;
	pConn->remote_port = remote_port;
	pConn->connState = c_create;
	pConn->pAssociatedIface = pIface;
	
	// wichtig: Auch alle anderen Felder initialisieren, also auch events und so
	pConn->own_events = pConn->remote_events = 
		pConn->own_lt_events = pConn->remote_lt_events = 0x0;
	
	// listen
	pConn->recvBuffer = llist_new(&pIface->mctx);
	pConn->sendBuffer = llist_new(&pIface->mctx);
	
	return pConn;
}


// erstellt eine Verbindung zu den Port auf einem Interface
struct connection* createNewListenerConnection()
{
	// man kann die connection sogar im normalen Speicher erstellen - malloc norm
	struct connection* pConn = new(struct connection);
	
	pConn->own_port = -1;
	pConn->remote_port = -1;
	pConn->connState = c_listen;
	pConn->pAssociatedIface = NULL;
	
	pConn->own_events = pConn->remote_events = 
		pConn->own_lt_events = pConn->remote_lt_events = 0x0;
	
	return pConn;
}


// sendet eine fertig erstelle connection zu dem server
// non blocking, es ist egal ob sie angenommen wird oder nicht, es ist wie ein syn
bool initiateNewConnection(struct connection* pConn)
{	
	// nun noch in die Liste einfuegen
	llist_insert_first(&pConn->pAssociatedIface->connectionList, pConn);
	return 1;
}

// so kann man pruefen, ob die verbindung akzeptiert wurde [neu auch: geschlossen]
bool checkIfAcceptedConnection(struct connection* pConn)
{
	return pConn->connState == c_open;
}

bool checkIfClosedConnection(struct connection* pConn)
{
	return pConn->connState == c_close;
}


struct connection* searchNewConnectionOnIface(
	struct interface* pIface, int own_port)
{
	FOREACH_LIST(pIface->connectionList)
	{
		struct connection* pConn = (struct connection*)entry;
		
		if (pConn->connState != c_create)
			continue;
		
		// nur returnen, nicht akzeptieren
		if (pConn->own_port == own_port)
			return pConn;		

	}
	return NULL;
}

struct connection* searchNewConnectionOnAnyIface(sock_address own_addr, int own_port)
{
	for (int i = 0; i < MAX_INTERFACES; i++)
	{
		struct interface* pIface = interfaces[i];
		if (!pIface || pIface->own_address != own_addr) continue;
		
		struct connection* pConn = searchNewConnectionOnIface(
			pIface, own_port);
		if (pConn) return pConn;
	}
	return NULL;
}


// akzeptiert eine angefragte Verbindung wenn eine vorhanden ist
// man muss schauen dass man nicht seine eigenen akzeptiert
struct connection* acceptNewConnectionOnIface(
	struct interface* pIface, int own_port)
{
	struct connection* pConn = searchNewConnectionOnIface(
		pIface, own_port);
	
	if (pConn)
	{
		// man hat eine gefunden und hat sie akzeptiert
		pConn->connState = c_open;
		return pConn;
	}
	return NULL;
}


// dasselbe einfach auf irgend einem Interface
struct connection* acceptNewConnectionOnAnyIface(sock_address own_addr, int own_port)
{
	for (int i = 0; i < MAX_INTERFACES; i++)
	{
		struct interface* pIface = interfaces[i];
		
		// schauen ob pIface exisitiert und die richtige addresse hat
		// Kleine Aenderung, um code nicht zum breaken zu bringen: Wenn man IP=0.0.0.0
		// angibt, dann heisst das, dass man auf allen Ifaces listened. Berucksichtigen
		if (!pIface || (own_addr != 0 && pIface->own_address != own_addr)) 
			continue;
		
		struct connection* pConn = acceptNewConnectionOnIface(
			pIface, own_port);
		if (pConn) return pConn;
	}
	return NULL;
}

// loescht eine Verbindung komplett und loescht alle Buffer
// ab da an kann man nicht mehr lesen und empfangen, sonst use after free
bool removeConnection(struct connection* pConn)
{
	// und natuerlich von der Liste loeschen
	llist_remove(&pConn->pAssociatedIface->connectionList, pConn);
	
	// also alle Listen loeschen
	llist_delete(&pConn->recvBuffer);
	llist_delete(&pConn->sendBuffer);
	
	// dann noch zusaetzlich das Connection Objekt selber loeschen
	MALLOC_CONN();
	free(pConn);
	MALLOC_NORM();
	
	return 1;
}


// schliesse eine Verbindung
void closeConnection(struct connection* pConn)
{
	// schliessen heisst, dass man einfach den Zustand auf geschlossen setzt
	// man loescht noch nichts, der andere kann immer noch empfangen
	if (pConn->connState == c_close)
	{
		// wurde schon von der anderen Seite geschlossen, removen
		removeConnection(pConn);
	}
	else if (pConn->connState == c_create)
	{
		// die Verbindung wurde gar nie erst verbunden sondern direkt geloescht
		// kann vorkommen, oder bei listener mit shadow Connections
		removeConnection(pConn);
	}
	else
	{
		// man ist der erste der die Verbindung schliesst
		// es kann theoretisch zu races kommen, wenn beide gleichzeitig schliessen
		// dann gibt es einfach ein memory leak nicht so schlimm
		
		pConn->connState = c_close;
		SET_EVENT(pConn->remote_events, EPOLLHUP);	// das richtige event?
	}
}


// schreibe daten in eine Connection
int sendConnection(struct connection* pConn, char* data, int len)
{
	// erstelle das Paket im shmem
	MALLOC_CONN();
	struct bufferEntry* pBuffer = (struct bufferEntry*)malloc(
			sizeof(struct bufferEntry) + len);
	MALLOC_NORM();
	
	// daten ausfuellen
	pBuffer->datasize = len;
	pBuffer->offset = 0;
	memcpy(pBuffer->data, data, len);
	
	// das Paket in die Liste einfuegen hinten
	llist_insert_last(&pConn->sendBuffer, pBuffer);
	
	// remote signalisieren, dass gelesen werden kann
	SET_EVENT(pConn->remote_events, EPOLLIN);
	
	return len;
	
}


// empfange Daten von einer Connection
int recvConnection(struct connection* pConn, char* data, int len)
{
	// hier kann es schwieriger werden und zu fragmentierung kommen falls buffer zu klein
	// zudem muss Element vlt geloescht werden
	if (!hasDataList(&pConn->recvBuffer))
		return 0;
	
	struct bufferEntry* pBuffer = (struct bufferEntry*)llist_get_first(
			&pConn->recvBuffer);
	
	if (len >= pBuffer->datasize)
	{
		// In diesem Falle gibt es keine Fragmentierung, alles passt
		memcpy(data, pBuffer->data + pBuffer->offset, pBuffer->datasize);
		int ret = pBuffer->datasize;
		
		// das Paket aus der Liste entfernen
		llist_remove_first(&pConn->recvBuffer);
		
		// das Paket kann geloescht werden
		MALLOC_CONN();
		free(pBuffer);
		MALLOC_NORM();
		
		return ret;
	}
	else
	{
		// in diesem Falle gibt es fragmentierung
		memcpy(data, pBuffer->data + pBuffer->offset, len);
		pBuffer->offset += len;
		pBuffer->datasize -= len;
		
		return len;
	}	
}




