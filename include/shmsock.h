
/* 

das ist die einzige Datei die man includen muss, wenn man shmsock verwenden will 
Der rest kommt ja alleine durch das linken mit libshmock

*/

int registerInterface(char* name, int size, 
		sock_address remoteAddress, sock_address ownAddress);


