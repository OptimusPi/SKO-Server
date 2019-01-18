#include "SKO_Network.h"
#include "SKO_Player.h"
#include "SKO_Repository.h"
#include "SKO_PacketTypes.h"
#include "SKO_PacketFactory.h"
#include "Global.h"
#include "OPI_Clock.h"
#include "GE_Socket.h"

SKO_Network::SKO_Network(SKO_Repository *repository, int port, unsigned long int saveRateSeconds)
{
	// Bind this port to accept connections
	this->port = port;

	// Lock saveAllProfiles to one call at a time
	sem_init(&this->saveMutex, 0, 1);

	// Database calls are wrapped in repository class
	this->repository = repository;

	// Properties passed into threads
	this->saveRateSeconds = saveRateSeconds;
	this->listenSocket = new GE_Socket();
	this->packetHandler = new SKO_PacketHandler(this, repository);
}

//Initialize threads and Start listening for connections
std::string SKO_Network::Startup()
{
	// Bind the given port
	if (!this->listenSocket->Create(port))
		return "Failed to bind to port " + std::to_string(port);

	// Start listening for incoming connections
	if (!this->listenSocket->Listen())
		return "Failed to listen on port " + std::to_string(port);

	printf(kGreen "[+] listening for connections on port [%i]\n" kNormal, port);
	this->listenSocket->Connected = true;

	//start threads
	this->queueThread = std::thread(&SKO_Network::QueueLoop, this);
	this->connectThread = std::thread(&SKO_Network::ConnectLoop, this);
	this->saveThread = std::thread(&SKO_Network::SaveLoop, this);

	return "success";
}

void SKO_Network::Cleanup()
{
	//TODO delete sockets etc
}

void SKO_Network::saveAllProfiles()
{
	sem_wait(&this->saveMutex);
	printf("SAVE ALL PROFILES \n");

	int numSaved = 0;
	int playersLinux = 0;
	int playersWindows = 0;
	int playersMac = 0;
	unsigned int averagePing = 0;

	//loop all players
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		//save each player who is marked to save
		if (User[i].Save)
		{
			if (!repository->savePlayerData(i))
				printf(kRed "[FATAL] Unable to Save %s!\n" kNormal, User[i].Nick.c_str());
			numSaved++;
		}

		//Count every player who is logged in
		if (User[i].Ident)
		{
			printf("SAVE ALL PROFILES- numSaved: %i \n", numSaved);
			this->pauseSavingStatus = false;
			printf("SAVE ALL PROFILES- Ident is true \n");
			if (User[i].OS == LINUX_OS)
				playersLinux++;
			if (User[i].OS == WINDOWS_OS)
				playersWindows++;
			if (User[i].OS == MAC_OS)
				playersMac++;

			printf("SAVE ALL PROFILES- playersWindows: %i \n", playersWindows);
			averagePing += User[i].ping;
			printf("SAVE ALL PROFILES- averagePing: %i \n", averagePing);
		}
	}

	printf("SAVE ALL PROFILES- numSaved: %i \n", numSaved);
	if (numSaved)
	{
		printf("Saved %i players.\nsavePaused is %i\n", numSaved, (int)this->pauseSavingStatus);
	}

	int numPlayers = (playersLinux + playersWindows + playersMac);

	printf("number of players: %i, average ping: %i\n", numPlayers, averagePing);

	if (!this->pauseSavingStatus)
	{
		if (numPlayers > 0)
			averagePing = (int)(averagePing / numPlayers);

		repository->saveServerStatus(playersLinux, playersWindows, playersMac, averagePing);
	}

	if (numPlayers)
		this->pauseSavingStatus = false;
	else
		this->pauseSavingStatus = true;

	sem_post(&this->saveMutex);
}

void SKO_Network::SaveLoop()
{
	while (!SERVER_QUIT)
	{
		printf("Auto Save...\n");
		saveAllProfiles();

		//Sleep in milliseconds
		Sleep(1000 * this->saveRateSeconds);
	}
}

void SKO_Network::QueueLoop()
{
	while (!SERVER_QUIT)
	{
		// Cycle through all connections
		for (int userId = 0; userId < MAX_CLIENTS; userId++)
		{
			//check Que
			if (!User[userId].Que)
				continue;

			//receive
			if (User[userId].Sock->Recv() & GE_Socket_OK)
			{
				printf("A client is trying to connect...\n");
				printf("Data.length() = [%i]\n", (int)User[userId].Sock->Data.length());

				//if you got anything
				if (User[userId].Sock->Data.length() >= 6)
				{
					unsigned char versionCheck = User[userId].Sock->Data[1];
					unsigned char versionMajor = User[userId].Sock->Data[2];
					unsigned char versionMinor = User[userId].Sock->Data[3];
					unsigned char versionPatch = User[userId].Sock->Data[4];
					unsigned char versionOS = User[userId].Sock->Data[5];

					//if the packet code was VERSION_CHECK
					if (versionCheck == VERSION_CHECK)
					{
						printf("User[userId].Sock->Data[1] == VERSION_CHECK\n");
						if (versionMajor == VERSION_MAJOR && versionMinor == VERSION_MINOR && versionPatch == VERSION_PATCH)
						{
							printf("Correct version!\n");
							User[userId].Sock->Data = "";
							User[userId].Status = true;
							User[userId].Que = false;
							send(User[userId].Sock, VERSION_SUCCESS);
							printf("Que Time: \t%lu\n", User[userId].QueTime);
							printf("Current Time: \t%lu\n", Clock());

							//operating system statistics
							User[userId].OS = versionOS;
						}
						else //not correct version
						{
							send(User[userId].Sock, VERSION_FAIL);
							printf("error, packet code failed on VERSION_CHECK see look:\n");
							printf(">>>[read values] VERSION_MAJOR: %i VERSION_MINOR: %i VERSION_PATCH: %i VERSION_OS: %i\n",
								  versionMajor, versionMinor, versionPatch, versionOS);
							printf(">>>[expected values] VERSION_MAJOR: %i VERSION_MINOR: %i VERSION_PATCH: %i\n",
								   VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
							User[userId].Que = false;
							User[userId].Sock->Close(); 
							User[userId] = SKO_Player();
						}
					}
					else //not correct packet type...
					{
						printf("Here is the back packet! ");

						for (int i = 0; i < User[userId].Sock->Data.length(); i++)
						{
							printf("[%i]", User[userId].Sock->Data[i]);
						}
						printf("\n");

						send(User[userId].Sock, VERSION_FAIL);
						printf("error, packet code failed on VERSION_CHECK (2)\n");
						User[userId].Que = false;
						User[userId].Sock->Close();
					}
				}
			}
			else // Recv returned error!
			{
				User[userId].Que = false;
				User[userId].Status = false;
				User[userId].Ident = false;
				User[userId].Sock->Close();
				User[userId] = SKO_Player();
				printf("*\n**\n*\nQUE FAIL! (Recv returned error) IP IS %s*\n**\n*\n\n", User[userId].Sock->IP.c_str());
			}

			//didn't recv anything, don't kill unless it's too long
			if (Clock() - User[userId].QueTime >= 500)
			{
				User[userId].Que = false;
				User[userId].Sock->Close();
				printf("Closing socket %i for timeout.\n", userId);
				printf("*\n**\n*\nQUE FAIL! IP IS %s*\n**\n*\n\n", User[userId].Sock->IP.c_str());
			}
		} //end for loop

		//checking que loop 2 times per second is plenty fast
		Sleep(500);
	} //end while loop
} //end QueLoop

void SKO_Network::ConnectLoop()
{
	while (!SERVER_QUIT)
	{
		//check for disconnects by too high of ping.
		for (int i = 0; i < MAX_CLIENTS; i++)
		{
			if (!User[i].Ident)
				continue;

			if (User[i].pingWaiting)
			{
				int ping = Clock() - User[i].pingTicker;

				//TODO set limit for ping
				if (ping > 60000)
				{
					printf("\e[31;0mClosing socket based on ping greater than one minute.\e[m\n");
					User[i].Sock->Close();
				}
			} //end pingWaiting
			else
			{
				//send ping waiting again if its been more than a second
				if (Clock() - User[i].pingTicker > 1000)
				{
					//time to ping them.
					send(User[i].Sock, PING);
					User[i].pingWaiting = true;
					User[i].pingTicker = Clock();
				}
			}
		} //end max clients for ping

		// If there is someone trying to connect
		if (listenSocket->GetStatus2() & (int)GE_Socket_Read)
		{
			// Declare a temp int
			int incomingSocket;

			// Increase i until it finds an empty socket
			for (incomingSocket = 0; incomingSocket <= MAX_CLIENTS; incomingSocket++)
			{
				if (incomingSocket == MAX_CLIENTS)
					break;

				if (User[incomingSocket].Save == true || User[incomingSocket].Status == true || User[incomingSocket].Ident == true)
				{
					continue;
				}
				else
				{
					break;
				}
			}

			//break
			if (incomingSocket == MAX_CLIENTS)
			{
				GE_Socket *tempSock = listenSocket->Accept();
				std::string fullPacket = "0";
				fullPacket += SERVER_FULL;
				fullPacket[0] = fullPacket.length();
				tempSock->Send(fullPacket);
				tempSock->Close();
				continue;
			}

			printf("incoming socket is: %i\n", incomingSocket);
			printf("> User[incomingSocket].Sock->Connected == %s\n", User[incomingSocket].Sock->Connected ? "True" : "False");
			printf("> User[incomingSocket].Save == %i\n", (int)User[incomingSocket].Save);
			printf("> User[incomingSocket].Status == %i\n", (int)User[incomingSocket].Status);
			printf("> User[incomingSocket].Ident == %i\n", (int)User[incomingSocket].Ident);

			//make them mute and such
			User[incomingSocket] = SKO_Player();
			User[incomingSocket].Sock = listenSocket->Accept();

			//error reporting
			if (User[incomingSocket].Sock->Socket == 0)
			{
				printf("Sock[%i] INVALID_SOCKET\n", incomingSocket);
			}
			else
			{
				// Set the status of that socket to taken
				User[incomingSocket].Sock->Connected = true;

				//set the data counting clock
				User[incomingSocket].Sock->stream_ticker = Clock();

				//set the data_stream to null
				User[incomingSocket].Sock->byte_counter = 0;

				//set bandwidth to null
				User[incomingSocket].Sock->bandwidth = 0;
			}
			// Output that a client connected
			printf("[ !!! ] Client %i Connected\n", incomingSocket);

			std::string their_ip = User[incomingSocket].Sock->Get_IP();

			if (repository->IsAddressBanned(their_ip))
			{
				//cut them off!
				printf("SOMEONE BANNED TRIED TO CONNECT FROM [%s]\7\n", their_ip.c_str());
				User[incomingSocket].Sock->Close();
			}

			//put in que
			User[incomingSocket].Que = true;
			User[incomingSocket].QueTime = Clock();

			printf("put socket %i in que\n", incomingSocket);
		} //if connection incoming

		// Sleep in between checking for new connections
		Sleep(100);
	} //end while
}

void SKO_Network::RecvPacket(GE_Socket *socket)
{
	if (socket->Recv() == GE_Socket_Error)
	{
		printf("Recv() failed and returned GE_Socket_Error!\n");
		return;
	}
}

template <typename First, typename... Rest>
void SKO_Network::send(GE_Socket *socket, First const &first, Rest const &... rest)
{
	// Construct packet to send to the given socket.
	std::string packet = SKO_PacketFactory::getPacket(first, rest...);

	// printf(kCyan "[network->send(...)]: ");
	// for (int i = 0; i < packet.length(); i++)
	// {
	// 	printf("[%i]", (int)(unsigned char)packet[i]);
	// }
	// printf("\n" kNormal);

	// Check socket health.
	// If it's not healthy, the receive loop will clean it up.
	if (!socket->Connected || (socket->GetStatus() & GE_Socket_Error))
		return;

	// Send to the given socket but log errors.
	if (socket->Send(packet) == GE_Socket_Error)
	{
		printf("Send() failed and returned GE_Socket_Error!\n");
		return;
	}
}

void SKO_Network::DisconnectClient(unsigned int userId)
{
	User[userId].Ident = false;
	User[userId].Sock->Close();
	User[userId].Sock->Connected = false;

	// Output socket status
	printf("[ ERR ] User status: %i\n", User[userId].Status);

	// Output that socket was deleted
	printf("[ !!! ] Client %i Disconnected (%s)\n", userId, User[userId].Nick.c_str());

	// save data for all connected players
	if (User[userId].Save)
		saveAllProfiles();

	// Tell all other players userId left the game
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident && i != userId)
		{
			send(User[i].Sock, EXIT, userId);
		}
	}

	printf("resetting trade statuses...\n");

	//were they trading? notify the other player.
	if (User[userId].tradeStatus > 0 && User[userId].tradePlayer >= 0)
	{
		int playerB = User[userId].tradePlayer;
		send(User[playerB].Sock, TRADE, CANCEL);

		//set playerB to not trading at all
		User[playerB].tradeStatus = -1;
		User[playerB].tradePlayer = 0;
	}

	quitParty(userId);

	printf("Clearing User #%i named %s\n", userId, User[userId].Nick.c_str());
	if (User[userId].Sock)
	{
		delete User[userId].Sock;
	}
	User[userId] = SKO_Player();

	printf("[DISCONNECT] User[%i].Clear() called.\n", userId);
}

void SKO_Network::SendSpawnTarget(unsigned char targetId, unsigned char mapId)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident)
			send(User[i].Sock, SPAWN_TARGET, targetId, mapId);
	}
}

void SKO_Network::SendDespawnTarget(unsigned char targetId, unsigned char mapId)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{  
		if (User[i].Ident)
			send(User[i].Sock, DESPAWN_TARGET, targetId, mapId);
	}
}

void SKO_Network::SendPlayerHit(unsigned char userId, unsigned char hitUserId) 
{
	send(User[userId].Sock, TARGET_HIT, (char)1, hitUserId);		
}

void SKO_Network::SendEnemyHit(unsigned char userId, unsigned char enemyId)
{
	send(User[userId].Sock, TARGET_HIT, (char)0, enemyId);
}

int SKO_Network::banPlayer(int Mod_i, std::string username, std::string reason, int flag)
{
	if (!User[Mod_i].Moderator)
	{
		//not a moderator!
		return 3;
	}

	return repository->banPlayer(Mod_i, username, reason, flag);
}

int SKO_Network::mutePlayer(int Mod_i, std::string username, int flag)
{
	if (!User[Mod_i].Moderator)
	{
		//not a moderator!
		return 3;
	}

	return repository->mutePlayer(Mod_i, username, flag);
}

int SKO_Network::kickPlayer(int Mod_i, std::string Username)
{
	if (!User[Mod_i].Moderator)
	{
		//not a moderator!
		return 2;
	}

	//check if they are online
	//find the sock of the username
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		//well are they online
		if (lower(User[i].Nick).compare(lower(Username)) == 0)
		{
			return 0;
		}
	}

	return 1;
}

void SKO_Network::HandleClient(unsigned char userId)
{
	int data_len = 0;
	unsigned char pack_len = 0;
	int code = -1;

	if (!(User[userId].Sock->GetStatus() & GE_Socket_OK))
	{
		DisconnectClient(userId);
		return;
	} //end error else

	// Get incoming data from the socket
	// If anything was received
	RecvPacket(User[userId].Sock);

	// If the data holds a complete data
	data_len = User[userId].Sock->Data.length();

	// Do not process if there is no packet!
	if (data_len == 0)
		return;

	pack_len = User[userId].Sock->Data[0];

	// Do not proceed if the packet is not fully captured!
	if (data_len < pack_len)
		return;

	// If more than one packet was received, only process one.
	std::string newPacket = "";
	if (data_len > pack_len)
		newPacket = User[userId].Sock->Data.substr(pack_len, data_len - pack_len);

	User[userId].Sock->Data = User[userId].Sock->Data.substr(0, pack_len);
	packetHandler->parsePacket(userId, User[userId].Sock->Data);

	//put the extra data (if any) into data
	User[userId].Sock->Data = newPacket;
};

// Other public functions called from main
void SKO_Network::SendSpawnEnemy(SKO_Enemy *enemy, unsigned char enemyId, unsigned char mapId)
{
	//enemy health bars
	unsigned char hp = (int)((float)enemy->hp / enemy->hp_max * hpBar);

	//tell everyone they spawned
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident)
		{
			send(User[c].Sock, ENEMY_MOVE_STOP, enemyId, mapId, enemy->x, enemy->y);
			send(User[c].Sock, ENEMY_HP, enemyId, mapId, hp);
		}
	}
}

void SKO_Network::SendEnemyHp(unsigned char userId, unsigned char enemyId, unsigned char mapId, unsigned char displayHp)
{ 
	send(User[userId].Sock, ENEMY_HP, enemyId, mapId, displayHp);
}

void SKO_Network::SendEnemyAction(SKO_Enemy* enemy, unsigned char action, unsigned char enemyId, unsigned char mapId)
{
	//tell everyone they spawned
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && User[c].mapId == mapId)
			send(User[c].Sock, action, enemyId, mapId, enemy->x, enemy->y);
	}
}

void SKO_Network::SendNpcAction(SKO_NPC* npc, unsigned char action, unsigned char npcId, unsigned char mapId)
{
	//tell everyone they spawned
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && User[c].mapId == mapId)
			send(User[c].Sock, action, npcId, mapId, npc->x, npc->y);
	}
}

void SKO_Network::SendPlayerAction(bool isCorrection, unsigned char action, unsigned char userId, float x, float y)
{
	// Do not send action to the client who is performing a player action,
	// sue to client predictive physics.
	for (int c = 0; c < MAX_CLIENTS; c++)
		if (c != userId && User[c].Ident && User[c].mapId == User[userId].mapId)
			send(User[c].Sock, action, userId, x, y);

	// However, when the client needs correcting due to lag, 
	// send the correction packet to the given userId.
	if (isCorrection)
		send(User[userId].Sock, action, userId, x, y);
}

void SKO_Network::SendStatLevel(unsigned char userId, unsigned char level)
{	
	send(User[userId].Sock, STAT_LEVEL, level);
}
void SKO_Network::SendStatRegen(unsigned char userId, unsigned char regen)
{
	send(User[userId].Sock, STAT_REGEN, regen);
}
void SKO_Network::SendStatHp(unsigned char userId, unsigned char hp)
{
	send(User[userId].Sock, STAT_HP, hp);
}
void SKO_Network::SendStatHpMax(unsigned char userId, unsigned char hp_max)
{
	send(User[userId].Sock, STATMAX_HP, hp_max);
}
void SKO_Network::SendStatStr(unsigned char userId, unsigned char str)
{
	send(User[userId].Sock, STAT_STR, str);
}
void SKO_Network::SendStatDef(unsigned char userId, unsigned char def)
{
	send(User[userId].Sock, STAT_DEF, def);
}
void SKO_Network::SendStatPoints(unsigned char userId, unsigned char points)
{
	send(User[userId].Sock, STAT_POINTS, points);
}
void SKO_Network::SendStatXp(unsigned char userId, unsigned int xp)
{
	send(User[userId].Sock, STAT_XP, xp);
}
void SKO_Network::SendStatXpMax(unsigned char userId, unsigned int xp_max)
{
	send(User[userId].Sock, STATMAX_XP, xp_max);

}
void SKO_Network::SendBuddyStatXp(unsigned char userId, unsigned char partyMemberId, unsigned char displayXp)
{
	send(User[userId].Sock, BUDDY_XP, partyMemberId, displayXp);
}
void SKO_Network::SendBuddyStatHp(unsigned char userId, unsigned char partyMemberId, unsigned char displayHp)
{
	send(User[userId].Sock, BUDDY_HP, partyMemberId, displayHp);
}
void SKO_Network::SendBuddyStatLevel(unsigned char userId, unsigned char partyMemberId, unsigned char displayLevel)
{
	send(User[userId].Sock, BUDDY_LEVEL, partyMemberId, displayLevel);
}
void SKO_Network::SendSpawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId, unsigned char itemType, float x, float y, float x_speed, float y_speed)
{
	send(User[userId].Sock, SPAWN_ITEM, itemObjId, mapId, itemType, x, y, x_speed, y_speed);  
}
void SKO_Network::SendDespawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId)
{
	send(User[userId].Sock, DESPAWN_ITEM, itemObjId, mapId);
}
void SKO_Network::SendPocketItem(unsigned char userId, unsigned char itemId, unsigned int amount)
{
	send(User[userId].Sock, POCKET_ITEM, itemId, amount);
}
void SKO_Network::SendBankItem(unsigned char userId, unsigned char itemId, unsigned int amount)
{
	send(User[userId].Sock, BANK_ITEM, itemId, amount);
}
void SKO_Network::SendWarpPlayer(unsigned char userId, unsigned char warpUserId, unsigned char mapId, float x, float y)
{ 
	send(User[userId].Sock, WARP, warpUserId, mapId, x, y);
}
void SKO_Network::SendPlayerRespawn(unsigned char userId, unsigned char deadUserId, float x, float y)
{
	send(User[userId].Sock, RESPAWN, deadUserId, x, y);
}
void SKO_Network::SendPong(unsigned char userId)
{
	send(User[userId].Sock, PONG);
}
void SKO_Network::SendRegisterResponse_Success(unsigned char userId)
{
	send(User[userId].Sock, REGISTER_SUCCESS);
}
void SKO_Network::SendRegisterResponse_AlreadyRegistered(unsigned char userId)
{
	send(User[userId].Sock, REGISTER_FAIL_DOUBLE);
}
void SKO_Network::SendLoginResponse_Success(unsigned char userId)
{
	send(User[userId].Sock, LOGIN_SUCCESS, userId);
}
void SKO_Network::SendLoginResponse_AlreadyOnline(unsigned char userId) 
{
	send(User[userId].Sock, LOGIN_FAIL_DOUBLE);
}
void SKO_Network::SendLoginResponse_PlayerBanned(unsigned char userId)
{
	send(User[userId].Sock, LOGIN_FAIL_BANNED); 
}
void SKO_Network::SendLoginResponse_PasswordFailed(unsigned char userId)
{
	send(User[userId].Sock, LOGIN_FAIL_NONE);
}
void SKO_Network::SendEquip(unsigned char userId, unsigned char playerId, unsigned char equipSlot, unsigned char equipId, unsigned char itemId)
{ 
	send(User[userId].Sock, EQUIP, playerId, equipSlot, equipId, itemId);
}
void SKO_Network::SendInventoryOrder(unsigned char userId, std::string inventoryOrder)
{
	send(User[userId].Sock, INVENTORY, inventoryOrder);
}
void SKO_Network::SendPlayerJoin(unsigned char userId, unsigned char playerId)
{
	//tell newbie about everyone
	float x = User[playerId].x;
	float y = User[playerId].y;
	float x_speed = User[playerId].x_speed;
	float y_speed = User[playerId].y_speed;
	unsigned char facing_right = User[playerId].facing_right ? 1 : 0;
	unsigned char mapId = User[playerId].mapId;
	std::string userTag = User[playerId].Nick + "|" + User[playerId].Clan;

	send(User[userId].Sock, JOIN, playerId, x, y, x_speed, y_speed, facing_right, mapId, userTag);
	
	// Always send player equipment after player join
	SendEquip(userId, playerId, (char)0, Item[User[playerId].equip[0]].equipID, User[playerId].equip[0]);
	SendEquip(userId, playerId, (char)1, Item[User[playerId].equip[1]].equipID, User[playerId].equip[1]);
	SendEquip(userId, playerId, (char)2, Item[User[playerId].equip[2]].equipID, User[playerId].equip[2]);                       
}
void SKO_Network::SendPlayerLoaded(unsigned char userId)
{
	send(User[userId].Sock, LOADED);
}
void SKO_Network::sendChat(unsigned char userId, std::string message)
{
	send(User[userId].Sock, CHAT, message);
}
void SKO_Network::sendClanInvite(unsigned char userId, unsigned char playerId)
{
	send(User[userId].Sock, CLAN, INVITE, playerId);
}
void SKO_Network::sendTradeInvite(unsigned char userId, unsigned char playerId)
{
	send(User[userId].Sock, TRADE, INVITE, playerId);
}
void SKO_Network::sendTradeReady(unsigned char userId, char tradeWindow)
{ 
	send(User[userId].Sock, TRADE, READY);
} 
void SKO_Network::sendTradeAccept(unsigned char userId, unsigned char playerId)
{ 
	send(User[userId].Sock, TRADE, ACCEPT, playerId);
}
void SKO_Network::sendTradeOffer(unsigned char userId, char tradeWindow, unsigned char itemId, unsigned int amount)
{ 
	send(User[userId].Sock, TRADE, OFFER, tradeWindow, itemId, amount); 
}
void SKO_Network::sendTradeCancel(unsigned char userId)
{ 
	send(User[userId].Sock, TRADE, CANCEL);
}
void SKO_Network::sendBankOpen(unsigned char userId)
{
	send(User[userId].Sock, BANK, INVITE);
}
void SKO_Network::sendShopOpen(unsigned char userId, unsigned char shopId)
{
	send(User[userId].Sock, SHOP, shopId);
}
void SKO_Network::sendPartyAccept(unsigned char userId, unsigned char playerId, char party)
{
	send(User[userId].Sock, PARTY, ACCEPT, playerId, party);
}
void SKO_Network::sendPartyCancel(unsigned char userId, unsigned char quitUserId)
{
	send(User[userId].Sock, PARTY, CANCEL, quitUserId);
}
void SKO_Network::sendPartyLeave(unsigned char userId, unsigned char quitUserId)
{
	send(User[userId].Sock, PARTY, ACCEPT, quitUserId, (char)-1);
}