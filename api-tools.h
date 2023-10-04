
// statische header only klasse diverse api helfer exportiert fuer socket.c und event.c API
// achtung! return wird uebeschrieben, wirklich nur API Funktionen anwenden

// optionen:			NOLOCK			api explizit nicht locken
//						DEBUG_PRINTS	return value loggen
//						ENCLAVE			man laeuft als enklave

// schlafe ein paar mikrosekunden
#define SLEEP(micros) usleep(micros)

// da sowieso singlethreaded kann man mutext sein lassen
// oder wenn man es explizit sonst angibt kann man Lock auch sein lassen
// !defined(NOLOCK) weil sonst doppelt definert...
#if defined(ENCLAVE) && !defined(NOLOCK)
#define NOLOCK
#endif

#ifdef NOLOCK

#define FUNC_LOCK()
#define FUNC_UNLOCK() 
#define SLEEP_LOCKED(ms) (SLEEP(ms))

// achtung hier: Wenn man einen Wert 2 mal verwendet return val, print(val) und val ist eine Funktion,
// dann wird die Funktion 2 mal ausgefuehrt! Neue variable _val erzeugen. val hat typ int, wie alle API funktionen

#ifdef DEBUG_PRINTS
//#define return(expr) {int val_ = expr; debug_printf(" = %d\n", val_); return val_;}
#define return(expr) {int val_ = expr; debug_printf("%s = %d\n", __func__, val_); return val_;}
#endif

#else

#include <pthread.h>

static pthread_mutex_t Mutex;
static pthread_mutexattr_t Attr;
static void __attribute__ ((constructor)) setupLock()
{
	pthread_mutexattr_init(&Attr);
	pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&Mutex, &Attr);
}

#define FUNC_LOCK() pthread_mutex_lock(&Mutex)
#define FUNC_UNLOCK() pthread_mutex_unlock(&Mutex)

// also return ueberall hier ueberschreiben
// ergaenzung: Wenn man den lock abgibt ohne ihn zu haben dann fehler
// so sollen funktionen erkannt werden, wo FUNC_LOCK() vergessen wurde

#define SLEEP_LOCKED(ms) (FUNC_LOCK(), SLEEP(ms), FUNC_UNLOCK())

#ifdef DEBUG_PRINTS
#define __DEBUG_PRINT_CODE(val) debug_printf("%s = %d\n", __func__, val)
//#define __DEBUG_PRINT_CODE(val) debug_printf(" = %d\n", val)
#else
#define __DEBUG_PRINT_CODE(val)
#endif

#define return(expr){ \
	int val_ = expr; \
	__DEBUG_PRINT_CODE(val_); \
	if (pthread_mutex_trylock(&Mutex)){ \
		debug_printf("[shmsock] ++MT++ lock error\n");} \
	FUNC_UNLOCK(); \
	return val_; }
	

// alle API Funktionen muessen gelockt werden
// also listen und malloc ist thread safe, aber wenn mehrere threads
// auf dasselbe Element in der Liste zugreifen, dann vlt. nicht mehr...

#endif


// nochmals eine spezialbehandlung: Es gibt die Option ohne sleep call 
// sondern einfach eine while schlaufe. So bleibt die ausfuehrung immer in der app
// was debugging fuer enklaven vereinfacht

#ifdef SPIN_SLEEP

// 0.1 GHz / 100MHz ca |  f*s = i	
#define FREQ 100*1000*1000

#undef SLEEP_LOCKED
#define SLEEP_LOCKED(ms) for(volatile int i=0; i<(FREQ*(ms/1000)); i++){}

#endif





