
# hier muss man nun aufpassen, denn man muss die Preloads richtig setzen


# hier muss man nun aufpassen, denn man muss die Preloads richtig setzen

export LD_PRELOAD=../lib/libshmsock-server.so
export SHMSOCK_OWN_ADDRESS=1
export SHMSOCK_REMOTE_ADDRESS=0

python3 client.py


