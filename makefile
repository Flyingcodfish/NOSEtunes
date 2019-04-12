all: server.out client.out

server.out: server.c playlists.h
	cc server.c -o server.out -std=c99

client.out: client.c
	cc client.c -o client.out -std=c99

clean:
	rm *.out
