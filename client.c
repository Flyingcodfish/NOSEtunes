/*
 * client.c
 * Cody LaFlamme
 * COP4600, Fall 2017
 *
 * A simple client program that will send a message
 * on a port (fed via command line) to a server hosted on this machine
 */

#include <sys/types.h>
#include <sys/socket.h> //for networking
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>

#include <stdio.h>
#include <string.h>
#include <unistd.h> //unix standard, for read system call
#include <stdlib.h> //c standard: for string parsing

int main(int argc, char** argv){
	//parse port
	if (argc != 2){
		printf("Error: Please provide only a conneciton port.\n");
		exit(1);
	}
	
	int port = atoi(argv[1]);
	if (port==0){
		printf("Error: Please provide a valid port number.\n");
		exit(1);
	}
	
	printf("Port: %d\n", port);

	//create address
	struct in_addr srv_in_addr;
	struct hostent *srv_host;
	srv_host = gethostbyname("localhost");

	bcopy((char*)srv_host->h_addr, (char*)&srv_in_addr, srv_host->h_length);
		
	struct sockaddr_in srv_addr;
	memset(&srv_addr, 0, sizeof(srv_addr)); //initialize address struct to 0
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = port;
	srv_addr.sin_addr = srv_in_addr;

	//create socket
	int srv_sock = socket(AF_INET, SOCK_STREAM, 0);

	//connect to foreign address
	if (0!=connect(srv_sock,(struct sockaddr*)&srv_addr, sizeof(srv_addr))){
		printf("Error: Could not connect to server.\n");
		exit(1);
	}
	
	//send message
	write(srv_sock, "Cody LaFlamme: 7064-9536\n", 100);
	char msg[100];
	read(srv_sock, msg, 100);
	printf("Received message from server: %s", msg);
	close(srv_sock);
	exit(0);
}

