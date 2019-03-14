#include "../SKO_Game/SKO_Player.h"
#include "../SKO_Repository/SKO_Repository.h"
#include "../SKO_Utilities/SKO_Utilities.h"
#include "SKO_Network.h"
#include "SKO_PacketTypes.h"
#include "SKO_PacketFactory.h"
#include "SKO_PacketParser.h"
#include "GE_Socket.h"

//TODO remove this when possible
#include "../Global.h"

SKO_Network::SKO_Network(SKO_Repository *repository, int port, unsigned long int saveRateSeconds)
{
	// Bind this port to accept connections
	this->port = port;

	// Database calls are wrapped in repository class
	this->repository = repository;

	// Properties passed into threads
	this->saveRateSeconds = saveRateSeconds;
	this->lastSaveTime = OPI_Clock::milliseconds();
	this->listenSocket = new GE_Socket();
	this->packetHandler = new SKO_PacketHandler(this);
}

// Initialize threads and Start listening for connections
std::string SKO_Network::startup()
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
	this->connectionThread = std::thread(&SKO_Network::ConnectionLoop, this);
	this->saveThread = std::thread(&SKO_Network::SaveLoop, this);

	return "success";
}

void SKO_Network::forceCloseClient(unsigned char userId)
{
	this->send(User[userId].socket, EXIT);
	disconnectClient(userId);
}

void SKO_Network::cleanup()
{
	//Stop listening for new connection
	this->listenSocket->Close();

	//Disconnect each user and clean up their socket
	for (unsigned char userId = 0; userId < MAX_CLIENTS; userId++)
	{
		this->forceCloseClient(userId);
	}
}

void SKO_Network::saveAllProfiles()
{
	std::lock_guard<std::mutex> lock(this->saveMutex);
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

	unsigned int numPlayers = (playersLinux + playersWindows + playersMac);

	printf("number of players: %i, average ping: %i\n", numPlayers, averagePing);

	if (!this->pauseSavingStatus)
	{
		if (numPlayers > 0)
			averagePing = averagePing / numPlayers;

		repository->saveServerStatus(playersLinux, playersWindows, playersMac, averagePing);
	}

	if (numPlayers)
		this->pauseSavingStatus = false;
	else
		this->pauseSavingStatus = true;

	this->lastSaveTime = OPI_Clock::milliseconds();
	printf("lastSaveTime is: %llu\n", this->lastSaveTime);
}

void SKO_Network::SaveLoop()
{
	while (!SERVER_QUIT)
	{
		// throttle this call
		// other functions may have already saved recently
		while (OPI_Clock::milliseconds() - this->lastSaveTime < 1000*this->saveRateSeconds)
		{
			//Sleep for one second
			OPI_Sleep::seconds(1);
		}

		printf("OPI_Clock::milliseconds() is: %llu\n", OPI_Clock::milliseconds());
		printf("and lastSaveTime is: %llu\n", this->lastSaveTime);
		printf("this->saveRateSeconds is: %lu\n", this->saveRateSeconds);

		printf("so: (%llu > %lu)\n\n", (OPI_Clock::milliseconds() - this->lastSaveTime), this->saveRateSeconds);
		printf("Auto Save...\n");
		saveAllProfiles();
	}
}

void SKO_Network::sendVersionSuccess(unsigned char userId)
{
	send(User[userId].socket, VERSION_SUCCESS);
}

void SKO_Network::sendVersionFail(unsigned char userId)
{
	send(User[userId].socket, VERSION_FAIL);
	User[userId].Queue = false;
	User[userId].socket->Close();
	User[userId] = SKO_Player();
}

void SKO_Network::verifyVersionLoop()
{
	// Wait for users to authenticate
	for (unsigned char userId = 0; userId < MAX_CLIENTS; userId++)
	{
		// check Queue
		if (!User[userId].Queue)
			continue;

		printf("User[%i] is waiting in Queue...\n", userId);

		//receive
		if (User[userId].socket->Recv() & GE_Socket_OK)
		{
			std::string packet = User[userId].socket->data;
			printf("A client is trying to connect...\n");
			printf("data.length() = [%i]\n", (int)packet.length());

			//if you got anything
			if (packet.length() >= 6)
			{
				SKO_PacketParser *parser = new SKO_PacketParser(packet);

				// [VERSION_CHECK][(unsigned char)version_major][(unsigned char)version_minor][(unsigned char)version_patch][(unsigned char)version_os]
				if (parser->getPacketType() == VERSION_CHECK)
				{
					unsigned char versionMajor = parser->nextByte();
					unsigned char versionMinor = parser->nextByte();
					unsigned char versionPatch = parser->nextByte();
					unsigned char versionOS    = parser->nextByte();

					if (versionMajor == VERSION_MAJOR && versionMinor == VERSION_MINOR && versionPatch == VERSION_PATCH)
					{
						printf("Correct version!\n");
						User[userId].socket->data = "";
						User[userId].Status = true;
						User[userId].Queue = false;
						sendVersionSuccess(userId);
						printf("Queue Time: \t%llu\n", User[userId].QueueTime);
						printf("Current Time: \t%llu\n", OPI_Clock::milliseconds());

						//operating system statistics
						User[userId].OS = versionOS;
					}
					else //not correct version
					{
						printf("error, packet code failed on VERSION_CHECK see look:\n");
						printf(">>>[read values] VERSION_MAJOR: %i VERSION_MINOR: %i VERSION_PATCH: %i VERSION_OS: %i\n",
								versionMajor, versionMinor, versionPatch, versionOS);
						printf(">>>[expected values] VERSION_MAJOR: %i VERSION_MINOR: %i VERSION_PATCH: %i\n",
								VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
						
						sendVersionFail(userId);
					}
				}
				else //not correct packet type...
				{
					printf(kRed "[FATAL] Client sent wrong packet type during version check!\n" kNormal);
					for (unsigned int i = 0; i < User[userId].socket->data.length(); i++)
					{
						printf("[%i]", User[userId].socket->data[i]);
					}
					printf("\n");

					printf("error, packet code failed on VERSION_CHECK (2)\n");
					sendVersionFail(userId);
				}
			}
		}
		else // Recv returned error!
		{
			forceCloseClient(userId);
			printf("*\n**\n*\nQUE FAIL! (Recv returned error) IP IS %s*\n**\n*\n\n", User[userId].socket->IP.c_str());
		}

		//didn't recv anything, don't kill unless it's too long
		if (OPI_Clock::milliseconds() - User[userId].QueueTime >= this->queueLimitHealthyMs)
		{
			forceCloseClient(userId);
			printf("Closing socket %i for timeout.\n", userId);
			printf("*\n**\n*\nQUE FAIL! IP IS %s*\n**\n*\n\n", User[userId].socket->IP.c_str());
		}
	} //end for loop
}

void SKO_Network::handleAuthenticatedUsersLoop()
{
	for (unsigned char userId = 0; userId < MAX_CLIENTS; userId++)
	{
		// Ignore socket if it is not connected
		if (!User[userId].Status)
			continue;

		handleClient(userId);

		// Ping check authenticated users
		if (!User[userId].Ident)
			continue;

		if (User[userId].pingWaiting)
		{
			unsigned long long int ping = OPI_Clock::milliseconds() - User[userId].pingTicker;
			if (ping > this->pingLimitHealthyMs)
			{
				printf("\e[31;0mClosing socket based on ping greater than one minute.\e[m\n");
				disconnectClient(userId);
			}
		}
		else
		{
			if (OPI_Clock::milliseconds() - User[userId].pingTicker > this->pingCheckIntervalMs)
			{
				send(User[userId].socket, PING);
				User[userId].pingWaiting = true;
				User[userId].pingTicker = OPI_Clock::milliseconds();
			}
		}
	}
}

void SKO_Network::handleIncomingConnections()
{
	// If there is someone trying to connect
	if (listenSocket->GetStatus2() & GE_Socket_Read)
	{
		printf(kGreen "Incoming socket connection...\n" kNormal);
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
			return;
		}

		printf("incoming socket is: %i\n", incomingSocket);
		printf("> User[incomingSocket].socket->Connected == %s\n", User[incomingSocket].socket->Connected ? "True" : "False");
		printf("> User[incomingSocket].Save == %i\n", (int)User[incomingSocket].Save);
		printf("> User[incomingSocket].Status == %i\n", (int)User[incomingSocket].Status);
		printf("> User[incomingSocket].Ident == %i\n", (int)User[incomingSocket].Ident);

		//make them mute and such
		User[incomingSocket] = SKO_Player();
		User[incomingSocket].socket = listenSocket->Accept();

		//error reporting
		if (User[incomingSocket].socket->Socket == 0)
		{
			printf("socket[%i] INVALID_SOCKET\n", incomingSocket);
		}
		else
		{
			// Set the status of that socket to taken 
			User[incomingSocket].socket->Connected = true;

			//set the data counting clock
			User[incomingSocket].socket->stream_ticker = OPI_Clock::milliseconds();

			//set the data_stream to null
			User[incomingSocket].socket->byte_counter = 0;

			//set bandwidth to null
			User[incomingSocket].socket->bandwidth = 0;
		}
		// Output that a client connected
		printf("[ !!! ] Client %i Connected\n", incomingSocket);

		std::string their_ip = User[incomingSocket].socket->Get_IP();

		if (repository->IsAddressBanned(their_ip))
		{
			//cut them off!
			printf("SOMEONE BANNED TRIED TO CONNECT FROM [%s]\7\n", their_ip.c_str());
			forceCloseClient(incomingSocket);
		}

		//put in Queue
		User[incomingSocket].Queue = true;
		User[incomingSocket].QueueTime = OPI_Clock::milliseconds();

		printf("put socket %i in Queue\n", incomingSocket);
	} //if connection incoming
}
//TODO refactor into different functions, handle authentication in the socket class.
void SKO_Network::ConnectionLoop()
{
	while (!SERVER_QUIT)
	{
		// Handle all clients that need to authenticate
		verifyVersionLoop();

		// Handle all authenticated clients
		handleAuthenticatedUsersLoop();

		// Handle new incoming connections
		handleIncomingConnections();

		// Sleep in between checking for new connections
		OPI_Sleep::milliseconds(1);
	} //end while

	printf(kMagenta "ConnectionLoop has finished.\n" kNormal);
}

void SKO_Network::recvPacket(GE_Socket *socket)
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

	// printf(kCyan "[send(...)]: ");
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

void SKO_Network::disconnectClient(unsigned char userId)
{
	User[userId].Ident = false;
	User[userId].socket->Close();
	User[userId].socket->Connected = false;

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
			send(User[i].socket, EXIT, userId);
		}
	}

	printf("resetting trade statuses...\n");

	//were they trading? notify the other player.
	if (User[userId].tradeStatus > 0 && User[userId].tradePlayer >= 0)
	{
		int playerB = User[userId].tradePlayer;
		send(User[playerB].socket, TRADE, CANCEL);

		//set playerB to not trading at all
		User[playerB].tradeStatus = 0;
		User[playerB].tradePlayer = 0;
	}

	quitParty(userId);

	printf("Clearing User #%i named %s\n", userId, User[userId].Nick.c_str());
	if (User[userId].socket)
	{
		delete User[userId].socket;
	}
	User[userId] = SKO_Player();

	printf("[DISCONNECT] User[%i].Clear() called.\n", userId);
}

void SKO_Network::attemptRegister(unsigned char userId, std::string username, std::string password)
{
    //try to create new player account
    int result = repository->createPlayer(username, password, User[userId].socket->IP);

    if (result == 1) // user already exists
    {
        sendRegisterResponse_AlreadyRegistered(userId);
        printf("%s tried to double-register!\n", username.c_str());
    }
    else if (result == 0) // user created successfully
    {
        sendRegisterResponse_Success(userId);
        printf("%s has been registered!\n", username.c_str());
    }
    else if (result == 2) // user is ip banned
    {
        forceCloseClient(userId);
    }
    else if (result == 3) // fatal error occurred
    {
        printf(kRed "[FATAL] Unable to create player.\n" kNormal);
        disconnectClient(userId);
    }
}
void SKO_Network::attemptLogin(unsigned char userId, std::string username, std::string password)
{

    printf("\n::LOGIN::\nUsername[%s]\nPassword[%s]\n", username.c_str(), password.c_str());

    //go through and see if you are logged in already
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        //tell the client if you are logged in already
        if (SKO_Utilities::lowerString(User[i].Nick).compare(SKO_Utilities::lowerString(username)) == 0)
        {
            printf("User that is you already is: %s x is %i i is %i\n", User[i].Nick.c_str(), (int)User[i].x, i);
            sendLoginResponse_AlreadyOnline(userId);
            return;
        }
    }

    //Login and load some status such as is mute, is banned
    int result = repository->loginPlayer(username, password);

	switch (result)
	{
	case 1:
		//TODO kick and client from server after several failed login attempts
		printf("%s has entered the wrong password!\n", username.c_str());
		sendLoginResponse_PasswordFailed(userId);
		return;
	case 2:
		SERVER_QUIT = true;
		return; 
	case 3:
		sendLoginResponse_PlayerBanned(userId);
        printf("%s is banned and tried to login!\n", username.c_str());
		return;
	case 4:
		sendLoginResponse_PasswordFailed(userId);
        printf("%s tried to login but doesn't exist!\n", username.c_str());
		return;
	case 5:
		User[userId].Mute = true;
		break;
	default:break;
	}

	printf("(login success) User %i %s socket status: %i\n", userId, username.c_str(), User[userId].socket->GetStatus());

	//successful login
	sendLoginResponse_Success(userId);

	//set display name
	User[userId].Nick = username;

	std::string clanTag = repository->GetClanTag(username, "(noob)");

	printf("Clan: %s\n", clanTag.c_str());
	User[userId].Clan = clanTag;

	printf(">>>about to load data.\n");
	if (repository->loadPlayerData(userId) != 0)
	{
		printf("couldn't load data. KILL!\n");
		User[userId].Save = false;
		disconnectClient(userId);
		return;
	}

	//set identified
	User[userId].Ident = true;

	//the current map of this user
	unsigned char mapId = User[userId].mapId;

	printf("going to tell client stats\n");

	printf(kGreen "hp: %i\n" kNormal, (int)User[userId].hp);
	printf(kGreen "max hp: %i\n" kNormal, (int)User[userId].max_hp);
	printf(kGreen "xp: %i\n" kNormal, (int)User[userId].xp);
	printf(kGreen "max xp: %i\n" kNormal, (int)User[userId].max_xp);

	// HP
	sendStatHp(userId, User[userId].hp);
	sendStatHpMax(userId, User[userId].max_hp);
	sendStatRegen(userId, User[userId].regen);

	// XP
	sendStatXp(userId, User[userId].xp);
	sendStatXpMax(userId, User[userId].max_xp);

	//STATS
	sendStatLevel(userId, User[userId].level);
	sendStatStr(userId, User[userId].strength);
	sendStatDef(userId, User[userId].defense);
	sendStatPoints(userId, User[userId].stat_points);

	//equipment
	sendEquip(userId, userId, (char)0, Item[User[userId].equip[0]].equipID, User[userId].equip[0]);
	sendEquip(userId, userId, (char)1, Item[User[userId].equip[1]].equipID, User[userId].equip[1]);
	sendEquip(userId, userId, (char)2, Item[User[userId].equip[2]].equipID, User[userId].equip[2]);

	//cosmetic inventory order
	sendInventoryOrder(userId, User[userId].getInventoryOrder());

	//inventory
	for (unsigned char i = 0; i < NUM_ITEMS; i++)
	{
		//if they own this item, tell them how many they own.
		unsigned int amount = User[userId].inventory[i];
		//prevents them from holding more than 24 items
		if (amount > 0)
		{
			User[userId].inventory_index++;
			printf("Loading Character: User has inventory_index of: %i\n", User[userId].inventory_index);
			sendPocketItem(userId, i, amount);
		}

		amount = User[userId].bank[i];
		//printf("this player owns [%i] of item %i\n", amt, i);
		if (amount > 0)
		{
			//TODO: remove this bank index or change how the bank works!
			User[userId].bank_index++;
			sendBankItem(userId, i, amount);
		}
	} //end loop 256 for items in inventory and map

	//bank
	for (int i = 0; i < 256; i++)
	{
		//go through all the ItemObjects since we're already looping
		if (map[mapId].ItemObj[i].status)
		{
			unsigned char itemId = map[mapId].ItemObj[i].itemID;
			float numx = map[mapId].ItemObj[i].x;
			float numy = map[mapId].ItemObj[i].y;
			float numxs = map[mapId].ItemObj[i].x_speed;
			float numys = map[mapId].ItemObj[i].y_speed;
			sendSpawnItem(userId, i, mapId, itemId, numx, numy, numxs, numys);
		}
	}

	//targets
	for (int i = 0; i < map[mapId].num_targets; i++)
	{
		if (map[mapId].Target[i].active)
			sendSpawnTarget(i, mapId);
	}

	//npcs
	for (int i = 0; i < map[mapId].num_npcs; i++)
	{
		unsigned char action = NPC_MOVE_STOP;

		if (map[mapId].NPC[i]->x_speed < 0)
			action = NPC_MOVE_LEFT;
		if (map[mapId].NPC[i]->x_speed > 0)
			action = NPC_MOVE_RIGHT;

		sendNpcAction(map[mapId].NPC[i], action, i, mapId);
	}

	// load all enemies
	for (int i = 0; i < map[mapId].num_enemies; i++)
	{
		unsigned char action = ENEMY_MOVE_STOP;
		if (map[mapId].Enemy[i]->x_speed < 0)
			action = ENEMY_MOVE_LEFT;
		if (map[mapId].Enemy[i]->x_speed > 0)
			action = ENEMY_MOVE_RIGHT;

		sendEnemyAction(map[mapId].Enemy[i], action, i, mapId);

		//enemy health bars
		int hp = (unsigned char)((float)map[mapId].Enemy[i]->hp / map[mapId].Enemy[i]->hp_max * hpBar);
		sendEnemyHp(userId, i, mapId, hp);
	}

	// inform all players
	for (unsigned char i = 0; i < MAX_CLIENTS; i++)
	{
		// Tell the new user about existing players
		if (i == userId && User[i].Nick.compare("Paladin") != 0)
		{
			sendPlayerJoin(userId, i);
		}
		// Tell existing players about new user
		else if (User[i].Ident)
		{
			sendPlayerJoin(i, userId);
		}
	}

	// Trigger loading complete on client.
	sendPlayerLoaded(userId);

	//mark this client to save when they disconnect..since Send/Recv change Ident!!
	User[userId].Save = true;
	saveAllProfiles();
}
void SKO_Network::createClan(unsigned char userId, std::string clanTag)
{
    CreateClan(userId, clanTag); 
}
void SKO_Network::acceptClanInvite(unsigned char userId)
{
	AcceptClanInvite(userId);
}
void SKO_Network::clanInvite(unsigned char userId, unsigned char playerId)
{
   clanInvite(userId, playerId);
}
void SKO_Network::sendSpawnTarget(unsigned char targetId, unsigned char mapId)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident)
			send(User[i].socket, SPAWN_TARGET, targetId, mapId);
	}
}
void SKO_Network::sendDespawnTarget(unsigned char targetId, unsigned char mapId)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{  
		if (User[i].Ident)
			send(User[i].socket, DESPAWN_TARGET, targetId, mapId);
	}
}
void SKO_Network::sendPlayerHit(unsigned char userId, unsigned char hitUserId) 
{
	send(User[userId].socket, TARGET_HIT, (char)1, hitUserId);		
}
void SKO_Network::sendEnemyHit(unsigned char userId, unsigned char enemyId)
{
	send(User[userId].socket, TARGET_HIT, (char)0, enemyId);
}
bool SKO_Network::verifyAdmin(unsigned char userId)
{
	if (User[userId].Moderator)
		return true;
	
	sendChat(userId, "You are not authorized to perform this action.");
	return false;
}
void SKO_Network::warpPlayer(unsigned char userId, std::string username, int x, int y, unsigned char mapId)
{ 
	if (!verifyAdmin(userId))
		return;

	printf("Warp %s to (%i,%i) on map [%i]\n", username.c_str(), x, y, mapId);

    //find user
    for (int wu = 0; mapId <= NUM_MAPS && wu < MAX_CLIENTS; wu++)
    {
        if (User[wu].Ident && (SKO_Utilities::lowerString(User[wu].Nick).compare(SKO_Utilities::lowerString(username)) == 0))
        {
            SKO_Portal *warp_p = new SKO_Portal();
            warp_p->spawn_x = x;
            warp_p->spawn_y = y;
            warp_p->mapId = mapId;

            Warp(wu, warp_p);
            break;
        }
    }
}
void SKO_Network::unbanPlayer(unsigned char userId, std::string username)
{
	if (!verifyAdmin(userId))
		return;

    int result = repository->banPlayer(userId, username, "unban", 0);
	switch (result)
	{
	case 0:
		sendChat(userId, username + " has been unbanned.");
		return;
	case 1: 
		sendChat(userId, username + " does not exist.");
		return;
	case 2:
		sendChat(userId, username + " cannot be unbanned.");
		return;
	case 3:
        printf("The user [%s] tried to unban [%s] but they are not moderator!\n", User[userId].Nick.c_str(), username.c_str());
        sendChat(userId, "You are not authorized to unban a player.");
		return;
	default:break;
	}
}
void SKO_Network::getIp(unsigned char userId, std::string username)
{
    if (!verifyAdmin(userId))
		return;

	std::string playerIP = repository->getIP(username);
	sendChat(userId, username + " has an IP of [" + playerIP + "]");
}
void SKO_Network::banAddress(unsigned char userId, std::string ip, std::string reason)
{
	if (!verifyAdmin(userId))
        return;

    int result = repository->banIP(User[userId].Nick, ip, reason);
    switch (result)
	{
	case 0:
		sendChat(userId, "[" + ip + "] has been banned (" + reason + ")");
		return;
	default:
		sendChat(userId, "Could not ban IP [" + ip + "] for unknown error.");
		return;
	}
}
void SKO_Network::banPlayer(unsigned char userId, std::string username, std::string reason)
{
	if (!verifyAdmin(userId))
		return;

	int result = repository->banPlayer(userId, username, reason, 1);
	switch (result)
	{
	case 1:
        sendChat(userId, username + " does not exist.");
		return;
    case 2:
        sendChat(userId, username + " cannot be banned.");
		return;
    case 3:
        printf("The user [%s] tried to ban [%s] but they are not moderator!\n", User[userId].Nick.c_str(), username.c_str());
        sendChat(userId, "You re not authorized to ban a player.");
		return;
	default:break;
	}

	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident)
		{
			sendChat(i, username + " has been banned. (" + reason + ")");

			if (SKO_Utilities::lowerString(User[i].Nick).compare(SKO_Utilities::lowerString(username)) == 0)
				kickPlayer(userId, username, reason);
		}
	}
}
void SKO_Network::unmutePlayer(unsigned char userId, std::string username)
{ 
	if (!verifyAdmin(userId))
		return;

	int result = repository->mutePlayer(userId, username, "unmute this player", 0);
	switch (result)
	{
	case 1:
		sendChat(userId, username + " does not exist.");
		return;
	case 2:
		sendChat(userId, username + " cannot be muted.");
		return;
	
	case 3:
        printf("The user [%s] tried to mute [%s] but they are not moderator!\n", User[userId].Nick.c_str(), username.c_str());
        sendChat(userId, "You are not authorized to mute players.");
		return;
	default:break;
	}
}
void SKO_Network::mutePlayer(unsigned char userId, std::string username, std::string reason)
{
	if (!verifyAdmin(userId))
		return;

	int result =  repository->mutePlayer(userId, username, reason, 1);
	switch (result)
	{
	case 1:
		sendChat(userId, username + " does not exist.");
		return;
	case 2:
		sendChat(userId, username + " cannot be muted.");
		return;
	default:break;
	}

	//find the sock of the username
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		// well, unmute the person
		if (SKO_Utilities::lowerString(User[i].Nick).compare(SKO_Utilities::lowerString(username)) == 0)
			User[i].Mute = false;

		// well, tell everyone
		// If this socket is taken
		if (User[i].Ident)
			sendChat(userId, username + " has been unmuted.");
	}
}
bool SKO_Network::isPlayerOnline(std::string username)
{
	//check if they are online
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (SKO_Utilities::lowerString(User[i].Nick).compare(SKO_Utilities::lowerString(username)) == 0)
			return true;
	}
	return false;
}

void SKO_Network::showPlayerPing(unsigned char userId, std::string username)
{
	// Restrict this command to moderator
	if (!verifyAdmin(userId))
		return;

	int playerId;
	bool result = false;

	for (playerId = 0; playerId < MAX_CLIENTS; playerId++)
	{
		if (User[playerId].Ident && User[playerId].Nick.compare(username) == 0)
		{
			printf("Moderator inquiry of %s\n", username.c_str());
			result = true;
			break;
		}
	}

	//find user
	std::string pingDumpPacket = "0";
	pingDumpPacket += CHAT;

	if (result)
	{
		std::stringstream ss;
		ss << "User[" << playerId << "] " << User[playerId].Nick << " has a ping of " << User[playerId].ping;
		sendChat(userId, ss.str());
	}
	else
	{
		sendChat(userId, "username " + username + " was not found.");
	}
}
void SKO_Network::kickPlayer(unsigned char userId, std::string username, std::string reason)
{
	// Restrict this command to moderator
	if (!verifyAdmin(userId))
		return;

	if (!isPlayerOnline(username))
    {
		sendChat(userId, "Sorry, " + username + " is not online.");
		return;
	}

	//okay, now send
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		//if socket is taken, send message that a user was kicked from the game
		if (User[i].Ident)
			sendChat(i, username + " has been kicked. (" + reason + ")");

		//kick the user
		if (SKO_Utilities::lowerString(username).compare(SKO_Utilities::lowerString(User[i].Nick)) == 0)
		{
			//Send kick packet
			forceCloseClient(i);
		}
	}
}
void SKO_Network::handleClient(unsigned char userId)
{
	// Ensure socket health
	if (!(User[userId].socket->GetStatus() & GE_Socket_OK))
	{
		disconnectClient(userId);
		return;
	}

	// Get incoming data from the socket
	// If anything was received
	recvPacket(User[userId].socket);

	// If the data holds a complete data
	unsigned int data_len = User[userId].socket->data.length();

	// Do not process if there is no packet!
	if (data_len == 0)
		return;

	unsigned int pack_len = User[userId].socket->data[0];

	// Do not proceed if the packet is not fully captured!
	if (data_len < pack_len)
		return;

	// If more than one packet was received, only process one.
	std::string newPacket = "";
	if (data_len > pack_len)
		newPacket = User[userId].socket->data.substr(pack_len, data_len - pack_len);

	User[userId].socket->data = User[userId].socket->data.substr(0, pack_len);
	packetHandler->parsePacket(userId, User[userId].socket->data);

	//put the extra data (if any) into data
	User[userId].socket->data = newPacket;
};
// Other public functions called from main
void SKO_Network::sendSpawnEnemy(SKO_Enemy *enemy, unsigned char enemyId, unsigned char mapId)
{
	//enemy health bars
	unsigned char hp = (int)((float)enemy->hp / enemy->hp_max * hpBar);

	//tell everyone they spawned
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident)
		{
			send(User[c].socket, ENEMY_MOVE_STOP, enemyId, mapId, enemy->x, enemy->y);
			send(User[c].socket, ENEMY_HP, enemyId, mapId, hp);
		}
	}
}
void SKO_Network::sendEnemyHp(unsigned char userId, unsigned char enemyId, unsigned char mapId, unsigned char displayHp)
{ 
	send(User[userId].socket, ENEMY_HP, enemyId, mapId, displayHp);
}
void SKO_Network::sendEnemyAction(SKO_Enemy* enemy, unsigned char action, unsigned char enemyId, unsigned char mapId)
{
	//tell everyone they spawned
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && User[c].mapId == mapId)
			send(User[c].socket, action, enemyId, mapId, enemy->x, enemy->y);
	}
}
void SKO_Network::sendNpcAction(SKO_NPC* npc, unsigned char action, unsigned char npcId, unsigned char mapId)
{
	//tell everyone they spawned
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && User[c].mapId == mapId)
			send(User[c].socket, action, npcId, mapId, npc->x, npc->y);
	}
}
void SKO_Network::sendPlayerAction(bool isCorrection, unsigned char action, unsigned char userId, float x, float y)
{
	// Do not send action to the client who is performing a player action,
	// due to client predictive physics.
	for (int c = 0; c < MAX_CLIENTS; c++)
		if (c != userId && User[c].Ident && User[c].mapId == User[userId].mapId)
			send(User[c].socket, action, userId, x, y);

	// However, when the client needs correcting due to lag, 
	// send the correction packet to the given userId.
	if (isCorrection)
		send(User[userId].socket, action, userId, x, y);
}
void SKO_Network::sendStatLevel(unsigned char userId, unsigned char level)
{	
	send(User[userId].socket, STAT_LEVEL, level);
}
void SKO_Network::sendStatRegen(unsigned char userId, unsigned char regen)
{
	send(User[userId].socket, STAT_REGEN, regen);
}
void SKO_Network::sendStatHp(unsigned char userId, unsigned char hp)
{
	send(User[userId].socket, STAT_HP, hp);
}
void SKO_Network::sendStatHpMax(unsigned char userId, unsigned char hp_max)
{
	send(User[userId].socket, STATMAX_HP, hp_max);
}
void SKO_Network::sendStatStr(unsigned char userId, unsigned char str)
{
	send(User[userId].socket, STAT_STR, str);
}
void SKO_Network::sendStatDef(unsigned char userId, unsigned char def)
{
	send(User[userId].socket, STAT_DEF, def);
}
void SKO_Network::sendStatPoints(unsigned char userId, unsigned char points)
{
	send(User[userId].socket, STAT_POINTS, points);
}
void SKO_Network::sendStatXp(unsigned char userId, unsigned int xp)
{
	send(User[userId].socket, STAT_XP, xp);
}
void SKO_Network::sendStatXpMax(unsigned char userId, unsigned int xp_max)
{
	send(User[userId].socket, STATMAX_XP, xp_max);

}
void SKO_Network::sendBuddyStatXp(unsigned char userId, unsigned char partyMemberId, unsigned char displayXp)
{
	send(User[userId].socket, BUDDY_XP, partyMemberId, displayXp);
}
void SKO_Network::sendBuddyStatHp(unsigned char userId, unsigned char partyMemberId, unsigned char displayHp)
{
	send(User[userId].socket, BUDDY_HP, partyMemberId, displayHp);
}
void SKO_Network::sendBuddyStatLevel(unsigned char userId, unsigned char partyMemberId, unsigned char displayLevel)
{
	send(User[userId].socket, BUDDY_LEVEL, partyMemberId, displayLevel);
}
void SKO_Network::sendSpawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId, unsigned char itemType, float x, float y, float x_speed, float y_speed)
{
	send(User[userId].socket, SPAWN_ITEM, itemObjId, mapId, itemType, x, y, x_speed, y_speed);  
}
void SKO_Network::sendDespawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId)
{
	send(User[userId].socket, DESPAWN_ITEM, itemObjId, mapId);
}
void SKO_Network::sendPocketItem(unsigned char userId, unsigned char itemId, unsigned int amount)
{
	send(User[userId].socket, POCKET_ITEM, itemId, amount);
}
void SKO_Network::sendBankItem(unsigned char userId, unsigned char itemId, unsigned int amount)
{
	send(User[userId].socket, BANK_ITEM, itemId, amount);
}
void SKO_Network::sendWarpPlayer(unsigned char userId, unsigned char warpUserId, unsigned char mapId, float x, float y)
{ 
	send(User[userId].socket, WARP, warpUserId, mapId, x, y);
}
void SKO_Network::sendPlayerRespawn(unsigned char userId, unsigned char deadUserId, float x, float y)
{
	send(User[userId].socket, RESPAWN, deadUserId, x, y);
}
void SKO_Network::sendPong(unsigned char userId)
{
	send(User[userId].socket, PONG);
}
void SKO_Network::receivedPong(unsigned char userId)
{
	User[userId].ping = OPI_Clock::milliseconds() - User[userId].pingTicker;
    User[userId].pingTicker = OPI_Clock::milliseconds();
    User[userId].pingWaiting = false; 
}
void SKO_Network::sendRegisterResponse_Success(unsigned char userId)
{
	send(User[userId].socket, REGISTER_SUCCESS);
}
void SKO_Network::sendRegisterResponse_AlreadyRegistered(unsigned char userId)
{
	send(User[userId].socket, REGISTER_FAIL_DOUBLE);
}
void SKO_Network::sendLoginResponse_Success(unsigned char userId)
{
	send(User[userId].socket, LOGIN_SUCCESS, userId);
}
void SKO_Network::sendLoginResponse_AlreadyOnline(unsigned char userId) 
{
	send(User[userId].socket, LOGIN_FAIL_DOUBLE);
}
void SKO_Network::sendLoginResponse_PlayerBanned(unsigned char userId)
{
	send(User[userId].socket, LOGIN_FAIL_BANNED); 
}
void SKO_Network::sendLoginResponse_PasswordFailed(unsigned char userId)
{
	send(User[userId].socket, LOGIN_FAIL_NONE);
}
void SKO_Network::sendEquip(unsigned char userId, unsigned char playerId, unsigned char equipSlot, unsigned char equipId, unsigned char itemId)
{ 
	send(User[userId].socket, EQUIP, playerId, equipSlot, equipId, itemId);
}
void SKO_Network::sendInventoryOrder(unsigned char userId, std::string inventoryOrder)
{
	send(User[userId].socket, INVENTORY, inventoryOrder);
}
void SKO_Network::sendPlayerJoin(unsigned char userId, unsigned char playerId)
{
	float x = User[playerId].x;
	float y = User[playerId].y;
	float x_speed = User[playerId].x_speed;
	float y_speed = User[playerId].y_speed;
	unsigned char facing_right = User[playerId].facing_right ? 1 : 0;
	unsigned char mapId = User[playerId].mapId;
	std::string userTag = User[playerId].Nick + "|" + User[playerId].Clan;

	// notify useRId that playerId joined
	send(User[userId].socket, JOIN, playerId, x, y, x_speed, y_speed, facing_right, mapId, userTag);
	
	// Always send player equipment after player join
	sendEquip(userId, playerId, (char)0, Item[User[playerId].equip[0]].equipID, User[playerId].equip[0]);
	sendEquip(userId, playerId, (char)1, Item[User[playerId].equip[1]].equipID, User[playerId].equip[1]);
	sendEquip(userId, playerId, (char)2, Item[User[playerId].equip[2]].equipID, User[playerId].equip[2]);                       
}
// Help the user experience a smooth load-in
// This packet will stop the "Loading..." screen on the client and let them playe the game.
void SKO_Network::sendPlayerLoaded(unsigned char userId)
{
	send(User[userId].socket, LOADED);
}
void SKO_Network::sendWho(unsigned char userId)
{
    //find how many players are online
    int players = 0;
    bool flag = false;
    std::string strNicks = "";

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        //if socket is taken
        //TODO remove "Paladin" and use flag: SKO_Player::hidden
        if (User[i].Ident && User[i].Nick.compare("Paladin") != 0)
        {
            players++;

            //formatting
            if (flag)
                strNicks += ", ";
            else if (User[i].Ident)
                flag = true;

            //add the nickname to the list
            if (User[i].Ident)
                strNicks += User[i].Nick;
        }
    }

    //TODO change this to use multiple messages 
    char strPlayers[252];
    if (players == 1)
        sprintf(strPlayers, "There is %i player online: ", players);
    else
        sprintf(strPlayers, "There are %i players online: ", players);

    if (players > 0)
        sendChat(userId, strPlayers);
}
void SKO_Network::sendChat(unsigned char userId, std::string message)
{
	send(User[userId].socket, CHAT, message);
}
void SKO_Network::sendClanInvite(unsigned char userId, unsigned char playerId)
{
	send(User[userId].socket, CLAN, INVITE, playerId);
}
void SKO_Network::sendTradeInvite(unsigned char userId, unsigned char playerId)
{
	send(User[userId].socket, TRADE, INVITE, playerId);
}
void SKO_Network::sendTradeReady(unsigned char userId, char tradeWindow)
{ 
	send(User[userId].socket, TRADE, READY);
} 
void SKO_Network::sendTradeAccept(unsigned char userId, unsigned char playerId)
{ 
	send(User[userId].socket, TRADE, ACCEPT, playerId);
}
void SKO_Network::sendTradeOffer(unsigned char userId, char tradeWindow, unsigned char itemId, unsigned int amount)
{ 
	send(User[userId].socket, TRADE, OFFER, tradeWindow, itemId, amount); 
}
void SKO_Network::sendTradeCancel(unsigned char userId)
{ 
	send(User[userId].socket, TRADE, CANCEL);
}
void SKO_Network::sendBankOpen(unsigned char userId)
{
	send(User[userId].socket, BANK, INVITE);
}
void SKO_Network::sendShopOpen(unsigned char userId, unsigned char shopId)
{
	send(User[userId].socket, SHOP, shopId);
}
void SKO_Network::sendPartyAccept(unsigned char userId, unsigned char playerId, char party)
{
	send(User[userId].socket, PARTY, ACCEPT, playerId, party);
}
void SKO_Network::sendPartyCancel(unsigned char userId, unsigned char quitUserId)
{
	send(User[userId].socket, PARTY, CANCEL, quitUserId);
}
void SKO_Network::sendPartyLeave(unsigned char userId, unsigned char quitUserId)
{
	send(User[userId].socket, PARTY, ACCEPT, quitUserId, (char)-1);
}
