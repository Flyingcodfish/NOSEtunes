/*
 * server.c
 * Cody LaFlamme
 * COP4600, Fall 2017
 *
 * A simple server that will listen for a message
 * on a port (fed via command line), and print that message
 */

#include <sys/types.h>
#include <sys/socket.h> //for sockets
#include <netinet/in.h> //for INET4 standards
#include <arpa/inet.h>  //for htons: port byte order conversion

#include <string.h>
#include <stdio.h>
#include <unistd.h> //unix standard, for read system call
#include <stdlib.h> //c standard: for string parsing


int main(int argc, char** argv){
	//parse port
	if (argc != 2){
		printf("Error: Please provide only a listening port.\n");
		exit(1);
	}
	
	int port = atoi(argv[1]);
	if (port==0){
		printf("Error: Please provide a valid port number.\n");
	}
	
	printf("Port: %d\n", port);

	//create inet address (NOT socket address)
	struct in_addr srv_in_addr;
	srv_in_addr.s_addr = INADDR_ANY; //one can only listen from oneself

	//we have our port and address; open socket	
	int srv_sock = socket(AF_INET, SOCK_STREAM, 0);
	
	//create socket address
	struct sockaddr_in srv_addr;
	int s_addr_size = sizeof(srv_addr);
	
	memset(&srv_addr, 0, s_addr_size); //initialize address struct to 0
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = port;
	srv_addr.sin_addr = srv_in_addr;
	
	//bind server socket address
	if (0!=bind(srv_sock,(struct sockaddr*) &srv_addr, s_addr_size)){
		printf("Error: could not bind to port.\n");
	}
	
	//listen for connection
	listen(srv_sock, 1);

	//wait for and accept client connection
	struct sockaddr_in clt_addr;
	socklen_t client_size;
	int clt_sock = accept(srv_sock, (struct sockaddr*)&clt_addr,&client_size); 
	
	printf("Established connection.\n");

	//read message from client socket
	char msg[100];
	read(clt_sock, msg, 100);

	//print message
	printf("Server received: %s\n", msg);
	write(clt_sock, "Welcome to the server running on MINIX\n",100);

	close(clt_sock);
	close(srv_sock);
	exit(0);
}

