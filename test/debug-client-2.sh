set environment SHMSOCK_OWN_ADDRESS_0=2
set environment SHMSOCK_REMOTE_ADDRESS_0=0
set environment SHMSOCK_IFACE_COUNT=1
set environment LD_PRELOAD=../lib/libshmsock-client.so

# client2 auf IP 2

# r ./client.py 0.0.0.0 9898
