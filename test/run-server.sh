
# hier muss man nun aufpassen, denn man muss die Preloads richtig setzen

export SHMSOCK_OWN_ADDRESS_0=0
export SHMSOCK_REMOTE_ADDRESS_0=1
touch iface.0

export SHMSOCK_IFACE_COUNT=1


export LD_PRELOAD=../lib/libshmsock-server.so
python3 server.py


