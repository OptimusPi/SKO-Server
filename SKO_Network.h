#ifndef __SKO_NETWORK_H_
#define __SKO_NETWORK_H_


//operating system
#define WINDOWS_OS  1
#define LINUX_OS 	2
#define MAC_OS 		3

#include <semaphore.h>
#include <string>

#include "SKO_item_defs.h"
#include "OPI_MYSQL.h"
#include "GE_Socket.h"
#include "base64.h"


class SKO_Network
{

public:

	SKO_Network(OPI_MYSQL * database, int port, unsigned long int saveRateSeconds);
	std::string Startup();
	void Cleanup();
	void saveAllProfiles(OPI_MYSQL * database);
	
 private:
	
	// Bind to this port and accept incoming connections.
	int port = 0;

	// Allow only one save call at a time.
	sem_t saveMutex;

	// Traverse list of players and save all profiles that are valid.

	void saveProfile(OPI_MYSQL *database, unsigned int userId);
	// Do not spam the database status table if there have been 0 players online for some time.
	bool pauseSavingStatus = false;

	// Threads can only take one argument when starting up.
    // Add all the things we need before starting threads.
	struct ThreadDTO {
		OPI_MYSQL* 	database; //TODO: implement SKO_REPOSITORY
		GE_Socket*	listenSocket;
		unsigned long int saveRateSeconds;
	} threadDTO;

	// POSIX threads for running the below functions.
	pthread_t queueThread, connectThread, autoSaveThread; 
	
	// Endless loop functions that are to be assigned into threads.
	static void *QueueLoop(void *threadDTO);
	static void *ConnectLoop(void *threadDTO);
	static void *SaveLoop(void *threadDTO);
};

#endif
