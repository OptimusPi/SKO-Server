#ifndef __SKO_NETWORK_H_
#define __SKO_NETWORK_H_


//operating system
#define WINDOWS_OS  1
#define LINUX_OS 	2
#define MAC_OS 		3

#include <semaphore.h>
#include <string>
#include <thread>

#include "SKO_Repository.h"
#include "SKO_PacketHandler.h" 
#include "SKO_item_defs.h"
#include "OPI_MYSQL.h"
#include "GE_Socket.h"
#include "base64.h"
#include "Global.h"

class SKO_PacketHandler;

class SKO_Network
{
 
public:

	SKO_Network(SKO_Repository * repository, int port, unsigned long int saveRateSeconds);
	std::string Startup();
	void Cleanup();

	// Handle all network functions of a client
	void HandleClient(unsigned char userId);
	void SendPlayerLoaded(unsigned char userId);
	void SendPlayerJoin(unsigned char userId, unsigned char playerId);
	void SendSpawnEnemy(SKO_Enemy *enemy, unsigned char enemyId, unsigned char mapId);
	void SendEnemyAction(SKO_Enemy* enemy, unsigned char action, unsigned char enemyId, unsigned char mapId);
	void SendNpcAction(SKO_NPC* npc, unsigned char action, unsigned char npcId, unsigned char mapId);
	void SendPlayerAction(bool isCorrection, unsigned char action, unsigned char userId, float x, float y);
	void SendPlayerRespawn(unsigned char userId, unsigned char deadUserId, float x, float y);
	void SendSpawnTarget(unsigned char targetId, unsigned char mapId);
	void SendDespawnTarget(unsigned char targetId, unsigned char mapId);
	void SendPlayerHit(unsigned char userId, unsigned char hitUserId);
	void SendEnemyHit(unsigned char userId, unsigned char enemyId);
	void SendEnemyHp(unsigned char userId, unsigned char enemyId, unsigned char mapId, unsigned char displayHp);
	void SendWarpPlayer(unsigned char userId, unsigned char warpUserId, unsigned char mapId, float x, float y);

	// Item related functions
	void SendSpawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId, unsigned char itemType, float x, float y, float x_speed, float y_speed);
	void SendDespawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId);
	void SendPocketItem(unsigned char userId, unsigned char itemId, unsigned int amount);
	void SendBankItem(unsigned char userId, unsigned char itemId, unsigned int amount);

	//TODO- equipId is a cosmetic information for the client. This could be done on the client side. It is only used to choose which image to draw.
	void SendEquip(unsigned char userId, unsigned char playerId, unsigned char equipSlot, unsigned char equipId, unsigned char itemId);
	void SendInventoryOrder(unsigned char userId, std::string inventoryOrder);

	// Update client player's own stat points 
	void SendStatRegen(unsigned char userId, unsigned char regen);
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

	//When PING is received, reply immediately with PONG
	void SendPong(unsigned char userId);
	void SendRegisterResponse_Success(unsigned char userId);
	void SendRegisterResponse_AlreadyRegistered(unsigned char userId);
	void SendLoginResponse_Success(unsigned char userId); 
	void SendLoginResponse_AlreadyOnline(unsigned char userId);
	void SendLoginResponse_PlayerBanned(unsigned char userId); 
	void SendLoginResponse_PasswordFailed(unsigned char userId); 

	//Save all profiles
	void saveAllProfiles();
	
	// Player chat and server messages
	void sendChat(unsigned char userId, std::string message);

	// Trade actions
	void sendTradeInvite(unsigned char userId, unsigned char playerId);
	void sendTradeOffer(unsigned char userId, char tradeWindow, unsigned char itemId, unsigned int amount);
	void sendTradeReady(unsigned char userId, char tradeWindow);
	void sendTradeAccept(unsigned char userId, unsigned char playerId);
	void sendTradeCancel(unsigned char userId);

	// Party actions
	void sendPartyAccept(unsigned char userId, unsigned char playerId, char party);
	void sendPartyCancel(unsigned char userId, unsigned char quitUserId);
	void sendPartyLeave(unsigned char userId, unsigned char quitUserId);

	// Clan actions
	void sendClanInvite(unsigned char userId, unsigned char playerId);

	// Bank actions
	void sendBankOpen(unsigned char userId);

	// Shop actions
	void sendShopOpen(unsigned char userId, unsigned char shopId);


	// Admin commands
	int kickPlayer(int Mod_i, std::string username);
	int mutePlayer(int Mod_i, std::string username, int flag);
	int banPlayer(int Mod_i, std::string username, std::string reason, int flag);
 private:
	
	// Bind to this port and accept incoming connections.
	unsigned int port = 0;

	// Allow only one save call at a time.
	sem_t saveMutex;

	// How often to save all valid players.
	unsigned long int saveRateSeconds;

	// Do not spam the database status table if there have been 0 players online for some time.
	bool pauseSavingStatus = false;

	// Separate packet parsing and handling into this class
	SKO_PacketHandler *packetHandler; 

	// All database calls go here
	SKO_Repository *repository;

	//Listen for incoming connections
	GE_Socket*	listenSocket;		

	// threads for running the below functions.
	std::thread queueThread, connectThread, saveThread; 
	
	// Endless loop functions that are to be assigned into threads.
	void QueueLoop();
	void ConnectLoop();
	void SaveLoop();

	// Client socket functions
	void RecvPacket(GE_Socket* socket);
	void DisconnectClient(unsigned int userId);

	// Helper functions
	void despawnTarget(int target, unsigned char mapId);
	void Respawn(unsigned char mapId, int i);
	void DivideLoot(int enemy, int party); 
	void KillEnemy(unsigned char mapId, int enemy);
	void GiveLoot(int enemy, int player);
	void GiveXP(unsigned char userId, int xp);

    // Socket send (variadic arguments)
	template<typename First, typename ... Rest>
	void send(GE_Socket* Socket, First const& first, Rest const& ... rest);
};

#endif
