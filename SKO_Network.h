#ifndef __SKO_NETWORK_H_
#define __SKO_NETWORK_H_


//operating system
#define WINDOWS_OS  1
#define LINUX_OS 	2
#define MAC_OS 		3

#include <string>
#include <thread>
#include "SKO_Repository.h"
#include "SKO_PacketHandler.h" 
#include "SKO_item_defs.h"
#include "OPI_MYSQL.h"
#include "GE_Socket.h"
#include "base64.h"
#include "Global.h"
#include <mutex>

class SKO_PacketHandler;

class SKO_Network
{
 
public:

	SKO_Network(SKO_Repository * repository, int port, unsigned long int saveRateSeconds);
	std::string startup();
	void cleanup();

	// Handle all network functions of a client
	void handleClient(unsigned char userId);

	//Login and loading helper functions
	void sendVersionSuccess(unsigned char userId);
	void sendPlayerLoaded(unsigned char userId);
	void sendPlayerJoin(unsigned char userId, unsigned char playerId);
	void sendSpawnEnemy(SKO_Enemy *enemy, unsigned char enemyId, unsigned char mapId);
	void sendEnemyAction(SKO_Enemy* enemy, unsigned char action, unsigned char enemyId, unsigned char mapId);
	void sendNpcAction(SKO_NPC* npc, unsigned char action, unsigned char npcId, unsigned char mapId);
	void sendPlayerAction(bool isCorrection, unsigned char action, unsigned char userId, float x, float y);
	void sendPlayerRespawn(unsigned char userId, unsigned char deadUserId, float x, float y);
	void sendSpawnTarget(unsigned char targetId, unsigned char mapId);
	void sendDespawnTarget(unsigned char targetId, unsigned char mapId);
	void sendPlayerHit(unsigned char userId, unsigned char hitUserId);
	void sendEnemyHit(unsigned char userId, unsigned char enemyId);
	void sendEnemyHp(unsigned char userId, unsigned char enemyId, unsigned char mapId, unsigned char displayHp);
	void sendWarpPlayer(unsigned char userId, unsigned char warpUserId, unsigned char mapId, float x, float y);

	// Item related functions
	void sendSpawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId, unsigned char itemType, float x, float y, float x_speed, float y_speed);
	void sendDespawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId);
	void sendPocketItem(unsigned char userId, unsigned char itemId, unsigned int amount);
	void sendBankItem(unsigned char userId, unsigned char itemId, unsigned int amount);

	//TODO- equipId is a cosmetic information for the client. This could be done on the client side. It is only used to choose which image to draw.
	void sendEquip(unsigned char userId, unsigned char playerId, unsigned char equipSlot, unsigned char equipId, unsigned char itemId);
	void sendInventoryOrder(unsigned char userId, std::string inventoryOrder);

	// Update client player's own stat points 
	void sendStatRegen(unsigned char userId, unsigned char regen);
	void sendStatHp(unsigned char userId, unsigned char hp);
	void sendStatHpMax(unsigned char userId, unsigned char hp_max);
	void sendStatStr(unsigned char userId, unsigned char str);
	void sendStatDef(unsigned char userId, unsigned char def);
	void sendStatPoints(unsigned char userId, unsigned char points);
	void sendStatXp(unsigned char userId, unsigned int xp);
	void sendStatXpMax(unsigned char userId, unsigned int xp_max);
	void sendStatLevel(unsigned char userId, unsigned char level);

	// Update party stats for client party player list
	void sendBuddyStatXp(unsigned char userId, unsigned char partyMemberId, unsigned char displayXp); 
	void sendBuddyStatHp(unsigned char userId, unsigned char partyMemberId, unsigned char displayHp); 
	void sendBuddyStatLevel(unsigned char userId, unsigned char partyMemberId, unsigned char displayLevel); 

	//When PING is received, reply immediately with PONG
	void sendPong(unsigned char userId);
	void sendRegisterResponse_Success(unsigned char userId);
	void sendRegisterResponse_AlreadyRegistered(unsigned char userId);
	void sendLoginResponse_Success(unsigned char userId); 
	void sendLoginResponse_AlreadyOnline(unsigned char userId);
	void sendLoginResponse_PlayerBanned(unsigned char userId); 
	void sendLoginResponse_PasswordFailed(unsigned char userId); 

	//Save all profiles
	void saveAllProfiles();
	
	// Player account actions
	void attemptLogin(unsigned char userId, std::string username, std::string password);
	void attemptRegister(unsigned char userId, std::string username, std::string password);
	void createClan(unsigned char userId, std::string clanTag);
	void acceptClanInvite(unsigned char userId);
	void clanInvite(unsigned char userId, unsigned char playerId);

	// Player chat and server messages
	void sendChat(unsigned char userId, std::string message);
	void sendWho(unsigned char userId);

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
	bool isPlayerOnline(std::string username);
	bool verifyAdmin(unsigned char userId);
	void getIp(unsigned char userId, std::string username);
	void kickPlayer(unsigned char userId, std::string username, std::string reason);
	void mutePlayer(unsigned char userId, std::string username, std::string reason);
	void unmutePlayer(unsigned char userId, std::string username);
	void banAddress(unsigned char userId, std::string ip, std::string reason);
	void banPlayer(unsigned char userId, std::string username, std::string reason);
	void unbanPlayer(unsigned char userId, std::string username);
	void warpPlayer(unsigned char userId, std::string usernamne, int x, int y, unsigned char mapId);
 private:
	
	// Bind to this port and accept incoming connections.
	unsigned int port = 0;

	// Allow only one save call at a time.
	std::mutex saveMutex;

	// How often to save all valid players.
	unsigned long int saveRateSeconds;
	unsigned long int lastSaveTime;
	
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
	void recvPacket(GE_Socket* socket);
	void disconnectClient(unsigned char userId);

	// Helper functions
	void attemptLogin(std::string username, std::string password);
	void despawnTarget(int target, unsigned char mapId);
	void respawn(unsigned char mapId, int i);
	void divideLoot(int enemy, int party); 
	void killEnemy(unsigned char mapId, int enemy);
	void giveLoot(int enemy, int player);
	void giveXP(unsigned char userId, int xp);

    // Socket send (variadic arguments)
	template<typename First, typename ... Rest>
	void send(GE_Socket* Socket, First const& first, Rest const& ... rest);
};

#endif
