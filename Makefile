
PATH_MALLOC = custom-malloc
PATH_LL = linked-list

INC_MALLOC = $(PATH_MALLOC)
INC_LL = $(PATH_LL)/include

ALL_INC = -I$(INC_MALLOC) -I$(INC_LL) ${ADDITIONAL_INC}
ALL_OBJ := obj/malloc.o obj/ll.o obj/list.o obj/socket.o obj/util.o obj/event.o
CFLAGS := -fPIC


# Hier kommen die Konfigurationsvariablen

# wenn man einen eigenen CC angeben will
ifdef CC
GCC := ${CC}	
else
GCC := gcc
endif

# wenn man einen eigenen output path angeben will
ifdef OUT
OUTDIR := ${OUT}
else
OUTDIR := lib
endif

# wenn man ein paar weitere cflags geben will
ifdef ADDITIONAL_CFLAGS
ADDITIONAL_CFLAGS := ${ADDITIONAL_CFLAGS}
else
ADDITIONAL_CFLAGS :=
endif

# wenn kein TLS verwenden will, weil es nicht funktioniert in enklaven
ifdef NO_TLS
ADDITIONAL_CFLAGS := $(ADDITIONAL_CFLAGS) -DNO_TLS
endif

# Das gesamte API wird nur von einem einzigen Thread aufgerufen
# wird gebraucht, wenn NO_TLS angegeben ist (Momentan)
ifdef SINGLE_THREAD
ADDITIONAL_CFLAGS := $(ADDITIONAL_CFLAGS) -DSINGLE_THREAD
endif

# Anstatt sleep(2) aufzufrufen einfach spinnen und CPU wasten
ifdef SPIN_SLEEP
ADDITIONAL_CFLAGS := $(ADDITIONAL_CFLAGS) -DSPIN_SLEEP
endif


# man schreibt in eine Datei die loadAddress der region
ifdef FIXED_REGION
ADDITIONAL_CFLAGS := $(ADDITIONAL_CFLAGS) -DMODE_FIXED_REGION
endif


.PHONY := enclave-server
.PHONY := enclave-client
.PHONY := test-server
.PHONY := test-client
.PHONY := static-server
.PHONY := static-client
.PHONY := shared-server
.PHONY := shared-client

.PHONY := clean-lib
.PHONY := clean-obj
.PHONY := clean


# alle moeglichen flags und Kombinationen
enclave-server: info
	export ADD_CLFAGS="$(ADDITIONAL_CFLAGS) -DENCLAVE -DMODE_SERVER" && make libshmsock.a
	mv libshmsock.a $(OUTDIR)/libshmsock-$@.a	
	
enclave-client: info
	export ADD_CLFAGS="$(ADDITIONAL_CFLAGS) -DENCLAVE" && make libshmsock.a
	mv libshmsock.a $(OUTDIR)/libshmsock-$@.a

test-server: info
	export ADD_CLFAGS="$(ADDITIONAL_CFLAGS) -DENCLAVE_TEST -DMODE_SERVER" && make libshmsock.a
	mv libshmsock.a $(OUTDIR)/libshmsock-$@.a
	
test-client: info
	export ADD_CLFAGS="$(ADDITIONAL_CFLAGS) -DENCLAVE_TEST" && make libshmsock.a
	mv libshmsock.a $(OUTDIR)/libshmsock-$@.a
	
static-server: info
	export ADD_CLFAGS="$(ADDITIONAL_CFLAGS) -DMODE_SERVER" && make libshmsock.a
	mv libshmsock.a $(OUTDIR)/libshmsock-$@.a
	
static-client: info
	export ADD_CLFAGS="$(ADDITIONAL_CFLAGS) " && make libshmsock.a
	mv libshmsock.a $(OUTDIR)/libshmsock-$@.a
	

shared-server: info
	export ADD_CLFAGS="$(ADDITIONAL_CFLAGS) -DMODE_SERVER -DMODE_PRELOAD" && make libshmsock.so
	mv libshmsock.so $(OUTDIR)/libshmsock-server.so
	
shared-client: info
	export ADD_CLFAGS="$(ADDITIONAL_CFLAGS) -DMODE_PRELOAD" && make libshmsock.so
	mv libshmsock.so $(OUTDIR)/libshmsock-client.so
	

# Die malloc implementation
obj/malloc.o : $(PATH_MALLOC)/malloc.c $(PATH_MALLOC)/_malloc.h
	$(GCC) -g -c -o $@  $< -lpthread $(ALL_INC) $(CFLAGS) ${ADD_CLFAGS}

# die ll implementation, die von list gebraucht wird
obj/ll.o : $(PATH_LL)/src/ll.c $(PATH_LL)/include/ll.h
	$(GCC) -g -c -o $@  $< -lpthread $(ALL_INC) $(CFLAGS) ${ADD_CLFAGS}

obj/%.o : ./%.c ./%.h | obj
	$(GCC) -g -c -o $@  $< -lpthread $(ALL_INC) $(CFLAGS) ${ADD_CLFAGS}

# wichtig: Zuerst muessen die Objekdateien gecleart werden, weil Makefile checkt nicht, 
# dass er neu compilen muss, wenn sich die flags geandert haben

libshmsock.a : clean-obj $(ALL_OBJ)
	ar rcs libshmsock.a $(ALL_OBJ)	
	
libshmsock.so : clean-obj $(ALL_OBJ)
	$(GCC) -o libshmsock.so $(ALL_OBJ) -shared -g $(CFLAGS)  ${ADD_CLFAGS}
	
	
clean-obj:
	rm -f obj/*
	
clean-lib:
	rm -r -f lib/*.a
	
clean: clean-obj clean-lib

info: 
	@echo "builde target ..."
	@echo "outdir = $(OUTDIR)"
	@echo "gcc = $(GCC)"
	@echo "additional flags = $(ADDITIONAL_CFLAGS)"
	
	
	
	
	
