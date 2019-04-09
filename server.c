/*
 * server.c
 * Cody LaFlamme
 * COP4600, Fall 2017
 *
 * A simple server that will listen for a message
 * on a port (fed via command line), and print that message
 */

#include <sys/types.h>	//generally important
#include <sys/socket.h> //for sockets
#include <netinet/in.h> //for INET4 standards
#include <arpa/inet.h>  //for htons: port byte order conversion

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h> //unix standard, for read system call
#include <stdlib.h> //c standard: for string parsing
#include <poll.h>

int process_command(char* cmd, int N);

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
	
	int yes=1; 

	//make port re-usable
	if (setsockopt(srv_sock,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int)) == -1) { 
	    perror("setsockopt"); 
	    exit(1); 
	}  	


	//bind server socket address
	if (0!=bind(srv_sock,(struct sockaddr*) &srv_addr, s_addr_size)){
		printf("Error: Could not bind to port %d.\n", port);
		return 1;
	}
	
	//listen for connection
	listen(srv_sock, 1);	
	printf("Listening for connections on port %d.\n", port);	

	//=============================== FORGING CONNECTIONS  ======================
	struct sockaddr_in clt_addr;
//	memset(&clt_addr, 0, sizeof(clt_addr));

	int clt_sock;
	socklen_t client_size = sizeof(clt_addr);
	pid_t pid = -1;
	char msg [256];

	//struct of fil descriptors we will poll: stdin and our server socket
	struct pollfd fds[2] = {
		{fd: STDIN_FILENO, events: POLLIN},
		{fd: srv_sock, events : POLLIN}
	};

	while(1){
		//poll stdin and our server socket for events; no timeout
		poll(fds, 2, -1);
		
		//handle each socket with events
		if (fds[0].revents & POLLIN){
			//there is something to read from stdin
			printf("Received command...\n");
			int N = read(STDIN_FILENO, msg, 256);
			process_command(msg, N);
		}
		if (fds[1].revents & POLLIN){
			//server socket has someone trying to connect to it
			printf("Detected a waiting client...\n");
			do{
				clt_sock = accept(srv_sock, (struct sockaddr*)&clt_addr, &client_size); 
				if (clt_sock == -1){
					printf("Error: could not connect to client. errno: %d. Retrying...\n", errno);
				}
			} while (clt_sock == -1);
			printf("Established connection.\n");
			pid = fork();
		}

		if (pid == 0) break; 	//child thread breaks and handles client
		//else continue		//parent thread loops and accepts more clients
	}
	//============================== HANDLING EXISTING CONNECTIONS ===========
	char buffer [256];
	char buffer2 [256];
	//say hello to new client
	//write(clt_sock, "hello", 5);

	//read message from client socket

	read(clt_sock, buffer, 256);
	read(clt_sock, buffer2, 256);
	//print message
	printf("Server received: '%s'\n", buffer);
	printf("Server received: '%s'\n", buffer2);
	write(clt_sock, "Welcome to NOSEtunes.\n", 256);
	
	printf("Closing connection to client...\n");	
	close(clt_sock);
	close(srv_sock);
	exit(0);
}


int process_command(char* cmd, int N){
	if (N <= 0) return 1;
	
	//we've received a command from stdin. 
	cmd[N-1] = 0x00;
	printf("Received command: %s, length: %d\n", cmd, N);
	return 0;
} 














