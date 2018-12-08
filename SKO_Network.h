#ifndef __SKO_NETWORK_H_
#define __SKO_NETWORK_H_

#include <string>
#include "OPI_MYSQL.h"
#include "GE_Socket.h"

class SKO_Network
{

public:

	SKO_Network(OPI_MYSQL * database, int port);
	std::string Startup();
	void Cleanup();

 private:
	
	// Bind to this port and accept incoming connections.
	int port;

	// Threads can only take one argument.
    // Add all the things we need before starting threads.
	struct ThreadDTO {
		OPI_MYSQL* 	database; //TODO: implement SKO_REPOSITORY
		GE_Socket*	listenSocket;
	} threadDTO;

	//posix threads
	pthread_t queThread, connectThread; 
	
	//functions that are to be assigned into threads
	static void *QueLoop(void *threadDTO);
	static void *ConnectLoop(void *threadDTO);
};

#endif
