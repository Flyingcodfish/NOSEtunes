/*
 * server.c
 * Cody LaFlamme
 * Design 2, Spring 2019
 *
 * A music streaming service made to be used with embedded processors.
 * The command interface is deliberately barebones (ie no JSON).
 * Everything is designed with an 8-bit wifi module in mind.
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
#include <dirent.h>

#include "playlists.h"

typedef struct SoundPlayer {
	char playlistID;
	FILE* sound_file;
	DIR * playlist_dir;
	struct dirent* dir_entry;
} SoundPlayer_t;


int process_command(char* cmd, int N);
int process_request(char command, char* params, int pN, char* outBuffer, int* outN, SoundPlayer_t* player_ptr);
int getSound(SoundPlayer_t* player_ptr, int numSamples, char* outBuffer);
int setPlaylist(SoundPlayer_t* player_ptr, char id);
int setNextSongFile(SoundPlayer_t* player_ptr);


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
	

	//make port re-usable
	int yes=1; 
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

	int SHOULD_EXIT = 0;
	while(1){
		//poll stdin and our server socket for events; no timeout
		poll(fds, 2, -1);
		
		//handle each socket with events
		if (fds[0].revents & POLLIN){
			//there is something to read from stdin
			printf("Received command...\n");
			int N = read(STDIN_FILENO, msg, 256);
			int cmdRet = process_command(msg, N);
			if (cmdRet == 2){
				SHOULD_EXIT = 1;
				break;
			}
		}
		if (fds[1].revents & POLLIN){
			//server socket has someone trying to connect to it
			printf("Detected a waiting client...\n");
			do{
				clt_sock = accept(srv_sock, (struct sockaddr*)&clt_addr, &client_size); 
				if (clt_sock == -1){
					printf("Error: Could not connect to client. errno: %d. Retrying...\n", errno);
				}
			} while (clt_sock == -1);
			printf("Established connection.\n");
			pid = fork();
		}

		if (pid == 0) break; 	//child thread breaks and handles client
		//else continue		//parent thread loops and accepts more clients
	}
	if (SHOULD_EXIT){
		close(srv_sock);
		//TODO kill all child threads?
		exit(0);
	}
	//============================== HANDLING EXISTING CONNECTIONS ===========
	//prepare to poll client socket
	struct pollfd child_fds = { .fd = clt_sock, .events = POLLIN };
	int pollResult;
	
	//we are a child thread spawned to handle a client
	char* inByte;
	int COMMAND_STARTED = 0;
	int param_length = -1;
	int param_i = 0;
	char command_code = 0;
	char paramBuffer [256];

	char outBuffer[259]; //256 bytes for data, one for sync, data length, and event ID
	int outN;
	
	#define ACK_CMD 3
	#define ACK_STS 4
	char ackString [5] = {0xBB, 0x02, 0xAA, 0x00, 0x01}; 

	SoundPlayer_t player;
	player.playlistID = 0;
	player.sound_file = NULL;
	player.playlist_dir = NULL;

	while(1){	
		//poll message from client socket
		pollResult = poll(&child_fds, 1, 8000); //timeout after 8 seconds
		if (pollResult == 0){
			printf("Client timed out.\n");
			SHOULD_EXIT = 1;
			break;
		}		
		
		//read and process message: note that the network cannot preserve packet boundaries.
		//we may receive half of a command here, or two commands, or half of one and half of another.
		int N = read(clt_sock, inByte, 1);
		if (!COMMAND_STARTED && *inByte == 0xBB){ //we are expecting a sync character
			COMMAND_STARTED = 1;
		}
		else if (param_length < 0){ //we are expecting param length byte
			param_length = *inByte;
			param_i = 0;
		}
		else if (command_code == 0){ //we are expecting a command opcode
			command_code = *inByte;
		}
		else if (param_i < param_length){ //we are expecting some param bytes
			paramBuffer[param_i++] = *inByte;
		}
		else{ //we are expecting nothing, we are done receiving this command
			int result = process_request(command_code, paramBuffer, param_length, outBuffer, &outN, &player);
			//send basic ack with value depending on result of process_command
			ackString[ACK_CMD] = command_code;
			ackString[ACK_STS] = result;
			write(clt_sock, ackString, 5);

			//send additional reply if deemed neccessary (if outN > 0)
			if (outN > 0){
				write(clt_sock, outBuffer, outN);
			}

			//prepare to receive the next command
			COMMAND_STARTED = 0;
			param_length = -1;
			param_i = 0;
			command_code = 0;
		}
	}
	//only way to get here is by setting SHOULD_EXIT	
	printf("Closing connection to client...\n");	
	close(clt_sock);
	close(srv_sock);
	exit(0);
}

//handles commands received from clients. bad names, I know.
int process_request(char command, char* params, int pN, char* outBuffer, int* outN, SoundPlayer_t* player_ptr){
	*outN = 0;
	switch(command){
	case 0x01: //ping
		printf("Pong.");
		return 0;
	case 0x02: //Query Playlist List, NYI
		return 1;
	case 0x03: //Select Playlist
		if (pN < 1) return 1;
		printf("Selecting playlist with ID %#04X.", params[0]);
		return setPlaylist(player_ptr, params[0]);
	case 0x04: //Skip song
		printf("Skipping song...");
		return setNextSongFile(player_ptr);
	case 0x05: //Request sound data
		//read data from the current sound file
		//don't print anything here, it'll spam the console
		if (pN < 1) return 1;
		int numBundles = params[0];
		if (numBundles > 63) return 1;
		outBuffer[0] = 0xBB; //sync char
		outBuffer[1] = numBundles * 4; //data length in bytes, 4 bytes per bundle (2 channels, each gets a 2-byte sample)
		outBuffer[2] = 0x15; //event code
		int getSoundResult = getSound(player_ptr, params[0], outBuffer+3); //add 3 to outbuffer so sound is placed after sync, length and command code
		*outN = numBundles*4+3;
		return getSoundResult;
	case 0x06: //Query playlist name, NYI
		return 1;
	default:
		printf("Received unknown command: %#04X\n", command);
		return 1;
	}
}


int getSound(SoundPlayer_t* player_ptr, int numSamples, char* outBuffer){
	if (player_ptr == NULL) return 1;
	//make sure a song and playlist are being read from
	int result;
	if (player_ptr->sound_file == NULL) { //no song has been chosen
		if (player_ptr->playlist_dir == NULL) { //no playlist has been chosen
			result = setPlaylist(player_ptr, DEFAULT_PLAYLIST_ID); //set to default playlist
			if (result == 1) return 1;
		}
		result = setNextSongFile(player_ptr);
		if (result == 1) return 1;
	}
	
	//now a song and playlist have definitely been chosen
	int read_result = fread(outBuffer, 4, numSamples, player_ptr->sound_file);
	//if we couldn't read enough sound data to satisfy the request, read some from the next file
	while (read_result < numSamples){
		if (setNextSongFile(player_ptr) != 0) return 1;
		read_result += fread(outBuffer+4*read_result, 4, numSamples-read_result, player_ptr->sound_file);
	}
	return 0;
}

int setPlaylist(SoundPlayer_t* player_ptr, char id){
	//sets the player's playlist directory based on a predetermined playlist ID
	playlist_t* plist_ptr = getPlaylistWithId(id);
	if (plist_ptr == NULL) return 1;

	player_ptr->playlist_dir = opendir(plist_ptr->path);
	player_ptr->playlistID = id;
	if (player_ptr->playlist_dir == NULL) return 1;
	return 0;
}

int setNextSongFile(SoundPlayer_t* player_ptr){
	//opens next sound file in the directory and closes the current one
	if (player_ptr->playlist_dir == NULL) return 1;
	struct dirent* entry = readdir(player_ptr->playlist_dir);
	if (entry == NULL){
		//either there's nothing in the directory, or we just need to cycle back to the beginning.
		rewinddir(player_ptr->playlist_dir);
		entry = readdir(player_ptr->playlist_dir);
		if (entry == NULL) return 1; //there really is nothing in here after all	
	}
	fclose(player_ptr->sound_file);
	player_ptr->sound_file = fopen(entry->d_name, "rb");
	if (player_ptr->sound_file == NULL) return 1;
	return 0;	
}


//process strings given by the command line via stdin on the host machine
//return values:
//1: something weird happened
//2: we should exit/stop accepting clients
int process_command(char* cmd, int N){
	if (N <= 0) return 1;
	
	//we've received a command from stdin. 
	cmd[N-1] = 0x00; //change newline at end to a null character
	printf("Received command: %s, length: %d\n", cmd, N);
	if (strcmp(cmd, "ping") == 0){
		printf("pong!\n");
		return 0;
	}
	if (strcmp(cmd, "exit") == 0){
		printf("No longer acepting new clients.\n");
		printf("Will exit once all connections cease.\n");
		return 2;
	}
	printf("Unrecognized command.\n");
	return 1;
} 














