#include "SKO_Network.h"
#include "SKO_Player.h"
#include "SKO_Repository.h"
#include "SKO_PacketTypes.h"
#include "SKO_PacketFactory.h"
#include "SKO_PacketParser.h"
#include "Global.h"
#include "OPI_Clock.h"
#include "GE_Socket.h"

SKO_Network::SKO_Network(SKO_Repository *repository, int port, unsigned long int saveRateSeconds)
{
	// Bind this port to accept connections
	this->port = port;

	// Database calls are wrapped in repository class
	this->repository = repository;

	// Properties passed into threads
	this->saveRateSeconds = saveRateSeconds;
	this->lastSaveTime = Clock();
	this->listenSocket = new GE_Socket();
	this->packetHandler = new SKO_PacketHandler(this);
}

//Initialize threads and Start listening for connections
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
	this->queueThread = std::thread(&SKO_Network::QueueLoop, this);
	this->connectThread = std::thread(&SKO_Network::ConnectLoop, this);
	this->saveThread = std::thread(&SKO_Network::SaveLoop, this);

	return "success";
}

void SKO_Network::cleanup()
{
	//TODO delete sockets etc
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

	this->lastSaveTime = Clock();
	printf("lastSaveTime is: %lu\n", this->lastSaveTime);
}

void SKO_Network::SaveLoop()
{
	while (!SERVER_QUIT)
	{
		// throttle this call
		// other functions may have already saved recently
		while (Clock() - this->lastSaveTime < 1000*this->saveRateSeconds)
		{
			//Sleep for one second
			Sleep(1000);
		}

		printf("Clock() is: %lu\n", Clock());
		printf("and lastSaveTime is: %lu\n", this->lastSaveTime);
		printf("this->saveRateSeconds is: %lu\n", this->saveRateSeconds);

		printf("so: (%lu > %lu)\n\n", (Clock() - this->lastSaveTime), this->saveRateSeconds);
		printf("Auto Save...\n");
		saveAllProfiles();
	}
}

void SKO_Network::sendVersionSuccess(unsigned char userId)
{
	send(User[userId].Sock, VERSION_SUCCESS);
}

void SKO_Network::QueueLoop()
{
	while (!SERVER_QUIT)
	{
		// Cycle through all connections
		for (unsigned char userId = 0; userId < MAX_CLIENTS; userId++)
		{
			//check Que
			if (!User[userId].Que)
				continue;

			//receive
			if (User[userId].Sock->Recv() & GE_Socket_OK)
			{
				std::string packet = User[userId].Sock->Data;
				printf("A client is trying to connect...\n");
				printf("Data.length() = [%i]\n", (int)packet.length());

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
							User[userId].Sock->Data = "";
							User[userId].Status = true;
							User[userId].Que = false;
							sendVersionSuccess(userId);
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
		User[playerB].tradeStatus = 0;
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
void SKO_Network::attemptRegister(unsigned char userId, std::string username, std::string password)
{
    //try to create new player account
    int result = repository->createPlayer(username, password, User[userId].Sock->IP);

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
        User[userId].Sock->Close();
    }
    else if (result == 3) // fatal error occurred
    {
        printf(kRed "[FATAL] Unable to create player.\n" kNormal);
        User[userId].Sock->Close();
    }
}
void SKO_Network::attemptLogin(unsigned char userId, std::string username, std::string password)
{

    printf("\n::LOGIN::\nUsername[%s]\nPassword[%s]\n", username.c_str(), password.c_str());

    //go through and see if you are logged in already
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        //tell the client if you are logged in already
        if (lower(User[i].Nick).compare(lower(username)) == 0)
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

	printf("(login success) User %i %s socket status: %i\n", userId, username.c_str(), User[userId].Sock->GetStatus());

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
		User[userId].Sock->Close();
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
	sendStatDef(userId, User[userId].defence);
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
    //check if the player has enough money.
    if (User[userId].inventory[ITEM_GOLD] < 100000) // 100000 TODO make a const for this
    {
        sendChat(userId, "Sorry, you cannot afford to establish a clan. It costs 100K gold.");
        return;
    }

    int result = repository->createClan(User[userId].Nick, clanTag);
    if (result == 0)
    {
        //send to all players so everyone knows a new clan was formed!
        for (int cu1 = 0; cu1 < MAX_CLIENTS; cu1++)
        {
            if (User[cu1].Ident)
                sendChat(cu1, "Clan (" + clanTag + ") has been established by owner " + User[userId].Nick + ".");
        } //end loop all clients

        //TODO - when below fix is complete, notify player their gold has decreased by 100K
        User[userId].inventory[ITEM_GOLD] -= 100000;

        //TODO do not rely on client reconnect when forming or joining a clan
        User[userId].Sock->Close();
        User[userId].Sock->Connected = false;
    }
    else if (result == 1)
    {
        //Clan already exists
        sendChat(userId, "You cannot establish [" + clanTag + "] because it already exists.");
    }
}
void SKO_Network::acceptClanInvite(unsigned char userId)
{
	if (User[userId].clanStatus != INVITE)
		return;
    
	repository->setClanId(User[userId].Nick, User[userId].tempClanId);

	// TODO - make new packet type to update clan instead of booting player
	// This relies on client to automatically reconnect
	User[userId].Sock->Close();
	User[userId].Sock->Connected = false;
}
void SKO_Network::clanInvite(unsigned char userId, unsigned char playerId)
{
    std::string clanId = repository->getOwnerClanId(User[userId].Nick);

    if (User[userId].Clan[0] == '(')
    {
        sendChat(userId, "You are not in a clan.");
        return;
    }

    if (User[playerId].Clan[0] != '(')
    {
        sendChat(userId, "Sorry, " + User[playerId].Nick + " Is already in a clan.");
        return;
    }

    if (!clanId.length())
    {
        sendChat(userId, "Sorry, only the clan owner can invite new members.");
        return;
    }

    //set the clan status of the invited person
    User[playerId].clanStatus = INVITE;
    User[playerId].tempClanId = clanId;

    //ask the user to clan
    sendClanInvite(playerId, userId);
}
void SKO_Network::sendSpawnTarget(unsigned char targetId, unsigned char mapId)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident)
			send(User[i].Sock, SPAWN_TARGET, targetId, mapId);
	}
}
void SKO_Network::sendDespawnTarget(unsigned char targetId, unsigned char mapId)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{  
		if (User[i].Ident)
			send(User[i].Sock, DESPAWN_TARGET, targetId, mapId);
	}
}
void SKO_Network::sendPlayerHit(unsigned char userId, unsigned char hitUserId) 
{
	send(User[userId].Sock, TARGET_HIT, (char)1, hitUserId);		
}
void SKO_Network::sendEnemyHit(unsigned char userId, unsigned char enemyId)
{
	send(User[userId].Sock, TARGET_HIT, (char)0, enemyId);
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
        if (User[wu].Ident && (lower(User[wu].Nick).compare(lower(username)) == 0))
        {
            SKO_Portal warp_p = SKO_Portal();
            warp_p.spawn_x = x;
            warp_p.spawn_y = y;
            warp_p.mapId = mapId;

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

			if (lower(User[i].Nick).compare(lower(username)) == 0)
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
		if (lower(User[i].Nick).compare(lower(username)) == 0)
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
		if (lower(User[i].Nick).compare(lower(username)) == 0)
			return true;
	}
	return false;
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
		//if socket is taken
		if (User[i].Ident)
			sendChat(userId, username + " has been kicked. (" + reason + ")");

		//kick the user
		if (lower(username).compare(lower(User[i].Nick)) == 0)
		{
			//Send kick packet
			User[i].Sock->Close();
			User[i].Sock->Connected = false;
		}
	}
}
void SKO_Network::handleClient(unsigned char userId)
{
	// Ensure socket health
	if (!(User[userId].Sock->GetStatus() & GE_Socket_OK))
	{
		disconnectClient(userId);
		return;
	}

	// Get incoming data from the socket
	// If anything was received
	recvPacket(User[userId].Sock);

	// If the data holds a complete data
	unsigned int data_len = User[userId].Sock->Data.length();

	// Do not process if there is no packet!
	if (data_len == 0)
		return;

	unsigned int pack_len = User[userId].Sock->Data[0];

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
void SKO_Network::sendSpawnEnemy(SKO_Enemy *enemy, unsigned char enemyId, unsigned char mapId)
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
void SKO_Network::sendEnemyHp(unsigned char userId, unsigned char enemyId, unsigned char mapId, unsigned char displayHp)
{ 
	send(User[userId].Sock, ENEMY_HP, enemyId, mapId, displayHp);
}
void SKO_Network::sendEnemyAction(SKO_Enemy* enemy, unsigned char action, unsigned char enemyId, unsigned char mapId)
{
	//tell everyone they spawned
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && User[c].mapId == mapId)
			send(User[c].Sock, action, enemyId, mapId, enemy->x, enemy->y);
	}
}
void SKO_Network::sendNpcAction(SKO_NPC* npc, unsigned char action, unsigned char npcId, unsigned char mapId)
{
	//tell everyone they spawned
	for (int c = 0; c < MAX_CLIENTS; c++)
	{
		if (User[c].Ident && User[c].mapId == mapId)
			send(User[c].Sock, action, npcId, mapId, npc->x, npc->y);
	}
}
void SKO_Network::sendPlayerAction(bool isCorrection, unsigned char action, unsigned char userId, float x, float y)
{
	// Do not send action to the client who is performing a player action,
	// due to client predictive physics.
	for (int c = 0; c < MAX_CLIENTS; c++)
		if (c != userId && User[c].Ident && User[c].mapId == User[userId].mapId)
			send(User[c].Sock, action, userId, x, y);

	// However, when the client needs correcting due to lag, 
	// send the correction packet to the given userId.
	if (isCorrection)
		send(User[userId].Sock, action, userId, x, y);
}
void SKO_Network::sendStatLevel(unsigned char userId, unsigned char level)
{	
	send(User[userId].Sock, STAT_LEVEL, level);
}
void SKO_Network::sendStatRegen(unsigned char userId, unsigned char regen)
{
	send(User[userId].Sock, STAT_REGEN, regen);
}
void SKO_Network::sendStatHp(unsigned char userId, unsigned char hp)
{
	send(User[userId].Sock, STAT_HP, hp);
}
void SKO_Network::sendStatHpMax(unsigned char userId, unsigned char hp_max)
{
	send(User[userId].Sock, STATMAX_HP, hp_max);
}
void SKO_Network::sendStatStr(unsigned char userId, unsigned char str)
{
	send(User[userId].Sock, STAT_STR, str);
}
void SKO_Network::sendStatDef(unsigned char userId, unsigned char def)
{
	send(User[userId].Sock, STAT_DEF, def);
}
void SKO_Network::sendStatPoints(unsigned char userId, unsigned char points)
{
	send(User[userId].Sock, STAT_POINTS, points);
}
void SKO_Network::sendStatXp(unsigned char userId, unsigned int xp)
{
	send(User[userId].Sock, STAT_XP, xp);
}
void SKO_Network::sendStatXpMax(unsigned char userId, unsigned int xp_max)
{
	send(User[userId].Sock, STATMAX_XP, xp_max);

}
void SKO_Network::sendBuddyStatXp(unsigned char userId, unsigned char partyMemberId, unsigned char displayXp)
{
	send(User[userId].Sock, BUDDY_XP, partyMemberId, displayXp);
}
void SKO_Network::sendBuddyStatHp(unsigned char userId, unsigned char partyMemberId, unsigned char displayHp)
{
	send(User[userId].Sock, BUDDY_HP, partyMemberId, displayHp);
}
void SKO_Network::sendBuddyStatLevel(unsigned char userId, unsigned char partyMemberId, unsigned char displayLevel)
{
	send(User[userId].Sock, BUDDY_LEVEL, partyMemberId, displayLevel);
}
void SKO_Network::sendSpawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId, unsigned char itemType, float x, float y, float x_speed, float y_speed)
{
	send(User[userId].Sock, SPAWN_ITEM, itemObjId, mapId, itemType, x, y, x_speed, y_speed);  
}
void SKO_Network::sendDespawnItem(unsigned char userId, unsigned char itemObjId, unsigned char mapId)
{
	send(User[userId].Sock, DESPAWN_ITEM, itemObjId, mapId);
}
void SKO_Network::sendPocketItem(unsigned char userId, unsigned char itemId, unsigned int amount)
{
	send(User[userId].Sock, POCKET_ITEM, itemId, amount);
}
void SKO_Network::sendBankItem(unsigned char userId, unsigned char itemId, unsigned int amount)
{
	send(User[userId].Sock, BANK_ITEM, itemId, amount);
}
void SKO_Network::sendWarpPlayer(unsigned char userId, unsigned char warpUserId, unsigned char mapId, float x, float y)
{ 
	send(User[userId].Sock, WARP, warpUserId, mapId, x, y);
}
void SKO_Network::sendPlayerRespawn(unsigned char userId, unsigned char deadUserId, float x, float y)
{
	send(User[userId].Sock, RESPAWN, deadUserId, x, y);
}
void SKO_Network::sendPong(unsigned char userId)
{
	send(User[userId].Sock, PONG);
}
void SKO_Network::sendRegisterResponse_Success(unsigned char userId)
{
	send(User[userId].Sock, REGISTER_SUCCESS);
}
void SKO_Network::sendRegisterResponse_AlreadyRegistered(unsigned char userId)
{
	send(User[userId].Sock, REGISTER_FAIL_DOUBLE);
}
void SKO_Network::sendLoginResponse_Success(unsigned char userId)
{
	send(User[userId].Sock, LOGIN_SUCCESS, userId);
}
void SKO_Network::sendLoginResponse_AlreadyOnline(unsigned char userId) 
{
	send(User[userId].Sock, LOGIN_FAIL_DOUBLE);
}
void SKO_Network::sendLoginResponse_PlayerBanned(unsigned char userId)
{
	send(User[userId].Sock, LOGIN_FAIL_BANNED); 
}
void SKO_Network::sendLoginResponse_PasswordFailed(unsigned char userId)
{
	send(User[userId].Sock, LOGIN_FAIL_NONE);
}
void SKO_Network::sendEquip(unsigned char userId, unsigned char playerId, unsigned char equipSlot, unsigned char equipId, unsigned char itemId)
{ 
	send(User[userId].Sock, EQUIP, playerId, equipSlot, equipId, itemId);
}
void SKO_Network::sendInventoryOrder(unsigned char userId, std::string inventoryOrder)
{
	send(User[userId].Sock, INVENTORY, inventoryOrder);
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
	send(User[userId].Sock, JOIN, playerId, x, y, x_speed, y_speed, facing_right, mapId, userTag);
	
	// Always send player equipment after player join
	sendEquip(userId, playerId, (char)0, Item[User[playerId].equip[0]].equipID, User[playerId].equip[0]);
	sendEquip(userId, playerId, (char)1, Item[User[playerId].equip[1]].equipID, User[playerId].equip[1]);
	sendEquip(userId, playerId, (char)2, Item[User[playerId].equip[2]].equipID, User[playerId].equip[2]);                       
}
// Help the user experience a smooth load-in
// This packet will stop the "Loading..." screen on the client and let them playe the game.
void SKO_Network::sendPlayerLoaded(unsigned char userId)
{
	send(User[userId].Sock, LOADED);
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
        //TODO remove "Paladin" and use a "SKO_Player::hidden flag
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
