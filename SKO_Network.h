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
	void HandleClient(unsigned char userId);
	void SendSpawnEnemy(SKO_Enemy *enemy, unsigned char enemyId, unsigned char current_map);
	void SendEnemyAction(SKO_Enemy* enemy, unsigned char action, unsigned char enemyId, unsigned char current_map);
	void SendNpcAction(SKO_NPC* npc, unsigned char action, unsigned char npcId, unsigned char current_map);
	void SendPlayerAction(bool isCorrection, unsigned char action, unsigned char userId, float x, float y);
	void SendPlayerRespawn(unsigned char userId, unsigned char deadUserId, float x, float y);
	void SendSpawnTarget(unsigned char targetId, unsigned char current_map);
	void SendDespawnTarget(unsigned char targetId, unsigned char current_map);
	void SendPlayerHit(unsigned char userId, unsigned char hitUserId);
	void SendEnemyHit(unsigned char userId, unsigned char enemyId);
	void SendEnemyHp(unsigned char userId, unsigned char enemyId, unsigned char current_map, unsigned char displayHp);
	void SendWarpPlayer(unsigned char userId, unsigned char warpUserId, unsigned char current_map, float x, float y);
	void SendQuitParty(unsigned char userId, unsigned char quitUserId);

	// Item related functions
	void SendSpawnItem(unsigned char userId, unsigned char itemObjId, unsigned char current_map, unsigned char itemType, float x, float y, float x_speed, float y_speed);
	void SendDespawnItem(unsigned char userId, unsigned char itemObjId, unsigned char current_map);
	void SendPocketItem(unsigned char userId, unsigned char itemId, unsigned int amount);


	// Update client player's own stat points 
	void SendStatHp(unsigned char userId, unsigned char hp);
	void SendStatHpMax(unsigned char userId, unsigned char hp_max);
	void SendStatStr(unsigned char userId, unsigned char str);
	void SendStatDef(unsigned char userId, unsigned char def);
	void SendStatPoints(unsigned char userId, unsigned char points);
	void SendStatXp(unsigned char userId, unsigned int xp);
	void SendStatXpMax(unsigned char userId, unsigned int xp_max);
	void SendStatLevel(unsigned char userId, unsigned char level);

	// Update party stats for client party player list
	void SendBuddyStatXp(unsigned char userId, unsigned char partyMemberId, unsigned char displayXp); 
	void SendBuddyStatHp(unsigned char userId, unsigned char partyMemberId, unsigned char displayHp); 
	void SendBuddyStatLevel(unsigned char userId, unsigned char partyMemberId, unsigned char displayLevel); 

 private:
	
	// Bind to this port and accept incoming connections.
	unsigned int port = 0;

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
	int load_data(int userId);

	// Client socket functions
	void RecvPacket(GE_Socket* socket);
	void DisconnectClient(unsigned int userId);

	// Helper functions
	void despawnTarget(int target, int current_map);
	void Respawn(int current_map, int i);
	void DivideLoot(int enemy, int party); 
	void KillEnemy(int current_map, int enemy);
	void GiveLoot(int enemy, int player);
	void GiveXP(unsigned char userId, int xp);
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
