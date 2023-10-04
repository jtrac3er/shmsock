
#client 1
set environment SHMSOCK_OWN_ADDRESS_0=0
set environment SHMSOCK_REMOTE_ADDRESS_0=1

#client 2
set environment SHMSOCK_OWN_ADDRESS_1=0
set environment SHMSOCK_REMOTE_ADDRESS_1=2

set environment SHMSOCK_IFACE_COUNT=2

set environment LD_PRELOAD=../lib/libshmsock-server.so

# r ./server.py 0.0.0.0 9898

# server auf IP 0

#b socket
#b bind
#b listen
#b accept
#b accept4
#b connect
#b send
#b recv


#b sendmsg 
#b recvmsg 
#b getsockopt 
#b setsockopt 

#b fcntl

