//playlists.h
//

typedef struct playlist {
	char ID;
	char* path;
} playlist_t;

char* const PLAYLIST_FOLDER_PATH = "/home/cody/NOSEtunes/";

#define NUM_PLAYLISTS 6
#define DEFAULT_PLAYLIST_ID 0x05
playlist_t PLAYLISTS [NUM_PLAYLISTS] = {
	{.ID = 0x01, .path = "playlists/Dandy/"},
	{.ID = 0x02, .path = "playlists/Miku/"},
	{.ID = 0x03, .path = "playlists/Sam n Ash/"},
	{.ID = 0x04, .path = "playlists/Transistor/"},
	{.ID = 0x05, .path = "playlists/Rock/"},
	{.ID = 0xFF, .path = "playlists/test/"}
};

playlist_t* getPlaylistWithId(char ID){
	for (int i=0; i<NUM_PLAYLISTS; i++){
		if (ID == PLAYLISTS[i].ID){
			return &PLAYLISTS[i];
		}
	}
	//no matching playlist was found
	return NULL;
}


