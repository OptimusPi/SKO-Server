#ifndef __SKO_NETWORK_H_
#define __SKO_NETWORK_H_


//operating system
#define WINDOWS_OS  1
#define LINUX_OS 	2
#define MAC_OS 		3

#include <semaphore.h>
#include <string>
#include <thread>

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

	// Traverse list of players and save all profiles that are valid.
	void saveAllProfiles();
	
 private:
	
	// Bind to this port and accept incoming connections.
	int port = 0;

	// Allow only one save call at a time.
	sem_t saveMutex;

	// How often to save all valid players.
	unsigned long int saveRateSeconds;

	// Do not spam the database status table if there have been 0 players online for some time.
	bool pauseSavingStatus = false;

	//TODO: implement SKO_REPOSITORY
	OPI_MYSQL* 	database; 

	//Listen for incoming connections
	GE_Socket*	listenSocket;		

	// threads for running the below functions.
	std::thread queueThread, connectThread, saveThread; 
	
	// Endless loop functions that are to be assigned into threads.
	void QueueLoop();
	void ConnectLoop();
	void SaveLoop();

	//Save a single profile
	void saveProfile(unsigned int userId);

	template<typename First, typename ... Rest>
	void send(GE_Socket* Socket, First const& first, Rest const& ... rest);
};

#endif
