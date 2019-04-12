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
#include <poll.h>

int process_command(char*msg, int N, int srv_sock);

int main(int argc, char** argv){
	//parse port
	if (argc != 3){
		printf("Usage: NOSEtunes [host address] [port]\n");
		exit(1);
	}
	
	int port = atoi(argv[2]);
	if (port==0){
		printf("Error: Please provide a valid port number.\n");
		exit(1);
	}
	
	printf("Host: %s\n", argv[1]);	
	printf("Port: %d\n", port);

	//create address
	struct in_addr srv_in_addr;
	struct hostent *srv_host;
	srv_host = gethostbyname(argv[1]);

	//bcopy((char*)srv_host->h_addr, (char*)&srv_in_addr, srv_host->h_length);
	memmove((char*)&srv_in_addr, (char*)srv_host->h_addr_list[0], srv_host->h_length);	
	
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
	//====================== CONNECTION MADE ====================	
	printf("Connected to server. Commands may now be entered.\n");
	
	char msg[256];
	int SHOULD_EXIT = 0;
	
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
			printf("Received console command...\n");
			int N = read(STDIN_FILENO, msg, 256);
			int cmdRet = process_command(msg, N, srv_sock);
			if (cmdRet == 2){
				SHOULD_EXIT = 1;
				break;
			}
		}
		
		if (fds[1].revents & POLLHUP){
			printf("Server closed connection. Exiting.\n");
			SHOULD_EXIT = 1;
			break;
		}		
		if (fds[1].revents & POLLIN){
			//server socket has sent us a message
			int N = read(srv_sock, msg, 256);
			if (N > 0) printf("Received data from server: ");
			for (int i=0;i<N;i++) printf("%02X ", (unsigned char)msg[i]);
			printf("\n");
		}

	}
	if (SHOULD_EXIT){
		close(srv_sock);
		exit(0);
	}

}


int process_command(char*msg, int N, int srv_sock){
	//just convert string to raw hex unless it's "exit"
	//last char is \n, make it \0 so we can use strcmp
	if (N < 2) return 1;
	msg[N-1] = 0x00;
	if (strcmp(msg, "exit") == 0)
		return 2;

	//else, convert string to hex and send to server
	char* endptr;
	char* startptr = msg;
	printf("Sending to server:");
	do{
		unsigned char num = strtol(startptr, &endptr, 16);
		printf(" %02X", num);
		int wRes = write(srv_sock, &num, 1);
		if (wRes == 0){
			printf("\nCould not write command byte.\n");
			return 1;
		}
		startptr = endptr;
	} while (*endptr != 0x00);
	printf("\n");
	return 0;
}

