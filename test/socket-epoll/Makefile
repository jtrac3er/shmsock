
.PHONY := all

normal : client-normal server-normal

all : normal client server
 
client-normal : 
	gcc -o client-normal client.c -DNORMAL -g

server-normal :
	gcc -o server-normal server.c -DNORMAL -g

client : client.c
	gcc -o client client.c -L.. -lshmsock-client -I.. -I../linked-list/include -I../custom-malloc -static -g


server : server.c
	gcc -o server server.c -L.. -lshmsock-server -I.. -I../linked-list/include -I../custom-malloc -static -g
