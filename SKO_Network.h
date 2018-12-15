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
#include "Global.h"
#include "hasher.h"

class SKO_Network
{

public:

	SKO_Network(OPI_MYSQL * database, int port, unsigned long int saveRateSeconds);
	std::string Startup();
	void Cleanup();

	// Handle all network functions of a client
	void SKO_Network::HandleClient(unsigned char userId);

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

	// Profile functions for the database
	void saveProfile(unsigned int userId);
	void saveAllProfiles();
	int loadProfile(std::string Username, std::string Password); //todo return std::String helpful return message
	int SKO_Network::load_data(int userId);

	// Client socket functions
	void SKO_Network::RecvPacket(GE_Socket* socket);
	void SKO_Network::DisconnectClient(unsigned int userId);

	// Helper functions
	void despawnTarget(int target, int current_map);
	void spawnTarget(int target, int current_map);
	void Respawn(int current_map, int i);
	void Warp(int i, SKO_Portal portal);
	void DivideLoot(int enemy, int party);
	void KillEnemy(int current_map, int enemy);
	void SpawnLoot(int current_map, SKO_ItemObject loot);
	void GiveLoot(int enemy, int player);
	void EnemyAttack(int i, int current_map);
	void SendAttack(int CurrSock, float x, float y);
	void SendJump(int CurrSock, float x, float y);
	void SendLeft(int CurrSock, float x, float y);
	void SendRight(int CurrSock, float x, float y);
	void SendStop(int CurrSock, float x, float y);
	void GiveXP(int CurrSock, int xp);
	void quitParty(int CurrSock);
	int createPlayer(std::string Username, std::string Password, std::string IP);
	int banIP(int Mod_i, std::string IP, std::string Reason);
	int banPlayer(int Mod_i, std::string Username, std::string Reason, int flag);
	int kickPlayer(int Mod_i, std::string Username);
	int mutePlayer(int Mod_i, std::string Username, int flag);

    // Socket send (variadic arguments)
	template<typename First, typename ... Rest>
	void send(GE_Socket* Socket, First const& first, Rest const& ... rest);
};

#endif
