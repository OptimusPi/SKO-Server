#include "SKO_Network.h"
#include "SKO_Player.h"
#include "SKO_PacketTypes.h"
#include "SKO_PacketFactory.h"
#include "Global.h"
#include "OPI_Clock.h"
#include "GE_Socket.h"

SKO_Network::SKO_Network(OPI_MYSQL *database, int port, unsigned long int saveRateSeconds)
{
	// Bind this port to accept connections
	this->port = port;

	// Lock saveAllProfiles to one call at a time
	sem_init(&this->saveMutex, 0, 1);

	// Properties passed into threads
	this->saveRateSeconds = saveRateSeconds;
	this->database = database;
	this->listenSocket = new GE_Socket();
}

//Initialize threads and Start listening for connections
std::string SKO_Network::Startup()
{
	// Test password hasher
	std::string hashTestResult = Hash("password", "2616e26e9c5a4decb08353c1bcb2cf7e");
	if (hashTestResult != "Quq6He1Ku8vXTw4hd0cXeEZAw0nqbpwPxZn50NcOVbk=")
		return "The password hasher does not seem to be working properly. Check argon2 version.\n";

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

void SKO_Network::saveProfile(unsigned int userId)
{
	if (User[userId].Nick.length() == 0)
	{
		printf("I'm not saving someone without a username.\n");
		printf("Please investigate why this happened!\n");
		printf("User[%i].ID is: %i\n", userId, User[userId].ID);
		return;
	}

	std::string player_id = User[userId].ID;
	//save inventory
	std::ostringstream sql;
	sql << "UPDATE inventory SET";
	for (int itm = 0; itm < NUM_ITEMS; itm++)
	{
		sql << " ITEM_";
		sql << itm;
		sql << "=";
		sql << (unsigned int)User[userId].inventory[itm];

		if (itm + 1 < NUM_ITEMS)
			sql << ", ";
	}
	sql << " WHERE player_id LIKE '";
	sql << database->clean(player_id);
	sql << "'";

	database->query(sql.str());
	sql.str("");
	sql.clear();

	//save bank
	sql << "UPDATE bank SET";

	for (int itm = 0; itm < NUM_ITEMS; itm++)
	{
		sql << " ITEM_";
		sql << itm;
		sql << "=";
		sql << (unsigned int)User[userId].bank[itm];
		if (itm + 1 < NUM_ITEMS)
			sql << ", ";
	}

	sql << " WHERE player_id LIKE '";
	sql << database->clean(player_id);
	sql << "'";

	database->query(sql.str());
	sql.str("");
	sql.clear();

	sql << "UPDATE player SET";
	sql << " level=";
	sql << (int)User[userId].level;
	sql << ", x=";
	sql << User[userId].x;
	sql << ", y=";
	sql << User[userId].y;
	sql << ", xp=";
	sql << User[userId].xp;
	sql << ", hp=";
	sql << (int)User[userId].hp;
	sql << ", str=";
	sql << (int)User[userId].strength;
	sql << ", def=";
	sql << (int)User[userId].defence;
	sql << ", xp_max=";
	sql << User[userId].max_xp;
	sql << ", hp_max=";
	sql << (int)User[userId].max_hp;
	sql << ", y_speed=";
	sql << User[userId].y_speed;
	sql << ", x_speed=";
	sql << User[userId].x_speed;
	sql << ", stat_points=";
	sql << (int)User[userId].stat_points;
	sql << ", regen=";
	sql << (int)User[userId].regen;
	sql << ", facing_right=";
	sql << (int)User[userId].facing_right;
	sql << ", EQUIP_0=";
	sql << (int)User[userId].equip[0];
	sql << ", EQUIP_1=";
	sql << (int)User[userId].equip[1];
	sql << ", EQUIP_2=";
	sql << (int)User[userId].equip[2];

	//operating system
	sql << ", VERSION_OS=";
	sql << (int)User[userId].OS;

	//time played
	unsigned long int total_minutes_played = User[userId].minutesPlayed;
	double this_session_milli = (Clock() - User[userId].loginTime);
	//add the milliseconds to total time
	total_minutes_played += (unsigned long int)(this_session_milli / 1000.0 / 60.0);

	sql << ", minutes_played=";
	sql << (int)total_minutes_played;

	//what map are they on?
	sql << ", current_map=";
	sql << (int)User[userId].current_map;

	sql << ", inventory_order='";
	sql << database->clean(base64_encode(User[userId].getInventoryOrder()));

	sql << "' WHERE id=";
	sql << database->clean(player_id);
	sql << ";";

	database->query(sql.str());
	sql.str("");
	sql.clear();

	printf(database->getError().c_str());
}

void SKO_Network::saveAllProfiles()
{
	sem_wait(&this->saveMutex);
	printf("SAVE ALL PROFILES \n");

	int numSaved = 0;
	int playersLinux = 0;
	int playersWindows = 0;
	int playersMac = 0;
	float averagePing = 0;

	//loop all players
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		//save each player who is marked to save
		if (User[i].Save)
		{
			saveProfile(i);
			printf("\e[35;0m[Saved %s]\e[m\n", User[i].Nick.c_str());
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

		std::stringstream statusQuery;
		statusQuery << "INSERT INTO status ";
		statusQuery << "(playersLinux, playersWindows, playersMac, averagePing) ";
		statusQuery << "VALUES ('";
		statusQuery << playersLinux;
		statusQuery << "', '";
		statusQuery << playersWindows;
		statusQuery << "', '";
		statusQuery << playersMac;
		statusQuery << "', '";
		statusQuery << averagePing;
		statusQuery << "');";

		database->query(statusQuery.str(), false);
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
					//if the packet code was VERSION_CHECK
					if (User[userId].Sock->Data[1] == VERSION_CHECK)
					{
						printf("User[userId].Sock->Data[1] == VERSION_CHECK\n");
						if (User[userId].Sock->Data[2] == VERSION_MAJOR && User[userId].Sock->Data[3] == VERSION_MINOR && User[userId].Sock->Data[4] == VERSION_PATCH)
						{
							printf("Correct version!\n");
							User[userId].Sock->Data = "";
							User[userId].Status = true;
							User[userId].Que = false;
							send(User[userId].Sock, VERSION_SUCCESS);
							printf("Que Time: \t%ul\n", User[userId].QueTime);
							printf("Current Time: \t%ul\n", Clock());

							//operating system statistics
							User[userId].OS = User[userId].Sock->Data[5];
						}
						else //not correct version
						{
							send(User[userId].Sock, VERSION_FAIL);
							printf("error, packet code failed on VERSION_CHECK see look:\n");
							printf(">>>[read values] VERSION_MAJOR: %i VERSION_MINOR: %i VERSION_PATCH: %i\n",
								   User[userId].Sock->Data[2], User[userId].Sock->Data[3], User[userId].Sock->Data[4]);
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
			std::string sql = "SELECT * FROM ip_ban WHERE ip like '";
			sql += database->clean(their_ip);
			sql += "'";
			database->query(sql);

			if (database->count())
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
	// Check socket health.
	// If it's not healthy, the receive loop will clean it up.
	if (!socket->Connected || (socket->GetStatus() & GE_Socket_Error)
	{
		printf("I can't send that packet. My socket is not connected!\n");
		return;
	}

	// Construct packet to send to the given socket.
	std::string packet = SKO_PacketFactory::getPacket(first, rest...);

	// Send to the given socket but log errors.
	if (socket->Send(Packet) == GE_Socket_Error)
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
	printf("[ !!! ] Client %i Disconnected (%s)\n", User[userId].Nick.c_str());

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

// TODO SQL Refactoring
int SKO_Network::load_data(int userId)
{
	int returnVals = 0;

	std::string sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(User[userId].Nick);
	sql += "'";

	for (int i = 0; i < 256; i++)
	{
		User[userId].inventory[i] = 0;
		User[userId].bank[i] = 0;
	}
	User[userId].inventory_index = 0;
	User[userId].bank_index = 0;

	database->query(sql);

	if (database->count())
	{
		database->nextRow();
		std::string player_id = database->getString(0);
		User[userId].ID = player_id;
		User[userId].level = database->getInt(3);
		User[userId].x = database->getFloat(4);
		User[userId].y = database->getFloat(5);
		User[userId].xp = database->getInt(6);
		User[userId].hp = database->getInt(7);
		User[userId].strength = database->getInt(8);
		User[userId].defence = database->getInt(9);
		User[userId].max_xp = database->getInt(10);
		User[userId].max_hp = database->getInt(11);
		User[userId].y_speed = database->getFloat(12);
		User[userId].x_speed = 0;
		User[userId].stat_points = database->getInt(14);
		User[userId].regen = database->getInt(15);
		User[userId].facing_right = (bool)(0 + database->getInt(16));
		User[userId].equip[0] = database->getInt(17);
		User[userId].equip[1] = database->getInt(18);
		User[userId].equip[2] = database->getInt(19);

		//20 = Operating System, we don't care

		//playtime stats
		User[userId].minutesPlayed = database->getInt(21);
		//what map is the current user on
		User[userId].current_map = database->getInt(22);

		User[userId].inventory_order = base64_decode(database->getString(23));

		//playtime stats
		User[userId].loginTime = Clock();

		sql = "SELECT * FROM inventory WHERE player_id LIKE '";
		sql += database->clean(player_id);
		sql += "'";

		returnVals += database->query(sql);

		if (database->count())
		{
			database->nextRow();

			for (int i = 0; i < NUM_ITEMS; i++)
			{
				//grab an item from the row
				User[userId].inventory[i] = database->getInt(i + 1);

				//you have to up the index and stop at 24 items
				if (User[userId].inventory[i] > 0)
					User[userId].inventory_index++;

				if (User[userId].inventory_index > 23)
					break;
			}
		}
		else
		{
			//fuck, it didn't load the data. KILL THIS SHIT NOW BEFORE IT DELETES THEIR DATA
			return 1;
		}

		sql = "SELECT * FROM bank WHERE player_id LIKE '";
		sql += database->clean(player_id);
		sql += "'";

		returnVals += database->query(sql);

		if (database->count())
		{
			database->nextRow();
			printf("loading bank...\n");

			for (int i = 0; i < NUM_ITEMS; i++)
			{
				//grab an item from the row
				User[userId].bank[i] = database->getInt(i + 1);
			}
		}
		else
		{
			//fuck, it didn't load the data. KILL THIS SHIT NOW BEFORE IT DELETES THEIR DATA
			return 1;
		}
	}
	else
	{
		//fuck, it didn't load the data. KILL THIS SHIT NOW BEFORE IT DELETES THEIR DATA
		return 1;
	}

	sql = "SELECT * FROM moderator WHERE player_id LIKE '";
	sql += database->clean(User[userId].ID);
	sql += "'";

	returnVals += database->query(sql);

	if (database->count())
	{
		User[userId].Moderator = true;
	}
	else
	{
		User[userId].Moderator = false;
	}

	return returnVals;
}

// TODO - SQL refactoring
int SKO_Network::loadProfile(std::string Username, std::string Password)
{
	bool mute = false;
	std::string player_id = "", player_salt = "";

	//go through and see if you are logged in already
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		//tell the client if you are logged in already
		if (lower(User[i].Nick).compare(lower(Username)) == 0)
		{
			printf("User that is you already is: %s x is %i i is %i\n", User[i].Nick.c_str(), (int)User[i].x, i);
			return 2;
		}
	}

	std::string sql = "SELECT * FROM player WHERE username like '";
	sql += database->clean(Username);
	sql += "'";
	database->query(sql);

	if (database->count())
	{
		database->nextRow();
		//get the id for loading purposes
		player_id = database->getString(0);
		//get the salt for login purposes
		player_salt = database->getString(26);
	}
	else //could not open file!
	{
		//user does not exist
		return 4;
	}

	sql = "SELECT * FROM ban WHERE player_id like '";
	sql += player_id;
	sql += "'";
	database->query(sql);

	if (database->count())
	{
		//user is banned
		return 3;
	}

	sql = "SELECT * FROM mute WHERE player_id like '";
	sql += player_id;
	sql += "'";
	database->query(sql);

	if (database->count())
	{
		//user is mute
		mute = true;
	}

	sql = "SELECT * FROM player WHERE username like '";
	sql += database->clean(Username);
	sql += "' AND password like '";
	sql += database->clean(Hash(Password, player_salt));
	sql += "'";
	database->query(sql);

	if (!database->count()) //&& Password != "389663e912c6f954c00aeb2343aee4e2")
	{
		printf("[%s] had no result.\n", sql.c_str());
		//wrong credentials
		return 1;
	}

	//server shutdown command
	if (Username == "SHUTDOWN")
		SERVER_QUIT = true;

	//mute or not
	if (mute)
		return 5;

	//not mute, logged in
	return 0;
}

void SKO_Network::spawnTarget(int target, int current_map)
{
	for (int i = 0; i < MAX_CLIENTS; i++)
	{
		if (User[i].Ident)
			send(User[i].Sock, SPAWN_TARGET, target, current_map);
	}
}

int SKO_Network::banIP(int Mod_i, std::string IP, std::string Reason)
{
	//are you a moderator
	if (User[Mod_i].Moderator)
	{
		//get noob id
		database->nextRow();
		std::string player_id = database->getString(0);

		std::string sql = "INSERT INTO ip_ban (ip, banned_by, ban_reason) VALUES('";
		sql += database->clean(IP);
		sql += "', '";
		sql += database->clean(User[Mod_i].Nick);
		sql += "', '";
		sql += database->clean(Reason);
		sql += "')";

		printf(sql.c_str());
		database->query(sql);

		printf(database->getError().c_str());
	}
	else
	{
		//not moderator
		return 1;
	}

	return 0;
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

int SKO_Network::mutePlayer(int Mod_i, std::string Username, int flag)
{
	std::string sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(Username);
	sql += "'";
	printf(sql.c_str());
	database->query(sql);

	//see if the noob exists
	if (database->count())
	{

		//get noob id
		database->nextRow();
		std::string player_id = database->getString(0);

		if (!User[Mod_i].Moderator)
		{
			//fool, you aren't even a moderator.
			return 3;
		}

		//see if the noob is a mod
		sql = "SELECT * FROM moderator WHERE player_id like '";
		sql += database->clean(player_id);
		sql += "'";
		printf(sql.c_str());
		database->query(sql);

		if (database->count())
		{

			//printf("\n::DEBUG::\n::ERROR::\nSomeone tried to mute [%s]!!\7\n\n", Username.c_str());
			return 2;
		}
		else //the person is not a mod, mute/unmute them
		{

			//mute
			if (flag == 1)
			{

				sql = "INSERT INTO mute (player_id, muted_by) VALUES('";
				sql += database->clean(player_id);
				sql += "', '";
				sql += database->clean(User[Mod_i].Nick);
				sql += "')";

				printf(sql.c_str());
				database->query(sql);

				return 0;
			}
			//unmute
			if (flag == 0)
			{

				sql = "DELETE FROM mute WHERE player_id LIKE '";
				sql += database->clean(player_id);
				sql += "'";
				database->query(sql);

				return 0;
			}
		}

		printf(database->getError().c_str());
	}
	else //the account does not exist
	{
		return 1;
	}

	return 0;
}

int SKO_Network::banPlayer(int Mod_i, std::string Username, std::string Reason, int flag)
{
	std::string sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(Username);
	sql += "'";
	printf(sql.c_str());
	database->query(sql);

	//see if the noob exists
	if (database->count())
	{

		//get noob id
		database->nextRow();
		std::string player_id = database->getString(0);

		if (!User[Mod_i].Moderator)
		{
			//fool, you aren't even a moderator.
			return 3;
		}

		//see if the noob is a mod
		sql = "SELECT * FROM moderator WHERE player_id like '";
		sql += database->clean(player_id);
		sql += "'";
		printf(sql.c_str());
		database->query(sql);

		if (database->count())
		{

			//printf("\n::DEBUG::\n::ERROR::\nSomeone tried to ban [%s]!!\7\n\n", Username.c_str());
			return 2;
		}
		else //the person is not a mod, ban/unban them
		{

			//BAN
			if (flag == 1)
			{

				sql = "INSERT INTO ban (player_id, banned_by, ban_reason) VALUES('";
				sql += database->clean(player_id);
				sql += "', '";
				sql += database->clean(User[Mod_i].Nick);
				sql += "', '";
				sql += database->clean(Reason);
				sql += "')";

				printf(sql.c_str());
				database->query(sql);

				return 0;
			}
			//unban
			if (flag == 0)
			{

				sql = "DELETE FROM ban WHERE player_id LIKE '";
				sql += database->clean(player_id);
				sql += "'";
				database->query(sql);

				return 0;
			}
		}

		printf(database->getError().c_str());
	}
	else //the account does not exist
	{
		return 1;
	}

	return 0;
}

int SKO_Network::createPlayer(std::string Username, std::string Password, std::string IP)
{

	printf("create_profile()\n");
	std::string sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(Username);
	sql += "'";
	printf(sql.c_str());
	database->query(sql);

	if (database->count())
	{
		printf("already exists.\n");
		//already exists
		return 1;
	}
	else
	{
		printf("doesn't already exist. good.\n");
	}

	sql = "SELECT * FROM ip_ban WHERE ip like '";
	sql += database->clean(IP);
	sql += "'";
	printf(sql.c_str());
	database->query(sql);

	if (database->count())
	{
		printf("ip banned. geez.\n");
		//you are banned
		return 2;
	}
	else
	{
		printf("not ip banned. good.\n");
	}

	//create unique salt for user password
	sql = "SELECT REPLACE(UUID(), '-', '');";
	database->query(sql);
	database->nextRow();
	std::string player_salt = database->getString(0);

	sql = "INSERT INTO player (username, password, level, facing_right, x, y, hp, str, def, xp_max, hp_max, current_map, salt) VALUES('";
	sql += database->clean(Username);
	sql += "', '";
	sql += database->clean(Hash(Password, player_salt));
	sql += "', '1', b'1', '314', '300', '10', '2', '1', '10', '10', '2', '";
	sql += database->clean(player_salt);
	sql += "')";
	printf(sql.c_str());
	database->query(sql);
	printf("inserted. Well, tried anyway.\n");

	//make sure it worked
	sql = "SELECT * FROM player WHERE username LIKE '";
	sql += database->clean(Username);
	sql += "'";
	printf(sql.c_str());
	database->query(sql);

	if (database->count())
	{
		printf("well, user exists. good.\n");
		//get results form the query. Save id.
		database->nextRow();
		std::string player_id = database->getString(0);

		//make inventory blank for them.
		sql = "INSERT INTO inventory (player_id) VALUES('";
		sql += database->clean(player_id);
		sql += "')";
		printf(sql.c_str());
		database->query(sql);

		//make inventory blank for them.
		printf(sql.c_str());
		sql = "INSERT INTO bank (player_id) VALUES('";
		sql += database->clean(player_id);
		sql += "')";
		database->query(sql);
	}
	else
	{
		printf(kRed "[FATAL] Unable to create player.\n" kNormal);
		Sleep(1000);
		return createPlayer(Username, Password, IP);
	}

	printf(database->getError().c_str());

	return 0;
}

void SKO_Network::HandleClient(unsigned char userId)
{
	std::string pongPacket = "0";
	pongPacket += PONG;
	pongPacket[0] = pongPacket.length();
	int data_len = 0;
	int pack_len = 0;
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
	data_len = 0;
	pack_len = 0;

	data_len = User[userId].Sock->Data.length();

	if (data_len > 0)
		pack_len = (int)(unsigned char)User[userId].Sock->Data[0];

	if (data_len >= pack_len && data_len > 0)
	{
		std::string newPacket = "";
		if (data_len > pack_len)
		{
			newPacket = User[userId].Sock->Data.substr(pack_len, data_len - pack_len);
		}

		User[userId].Sock->Data = User[userId].Sock->Data.substr(0, pack_len);

		//rip the command
		code = User[userId].Sock->Data[1];

		if (code == PONG)
		{
			User[userId].ping = Clock() - User[userId].pingTicker;
			User[userId].pingTicker = Clock();
			User[userId].pingWaiting = false;
		}
		else if (code == PING)
		{
			send(User[userId].Sock, pongPacket);
		} //end "ping"
		else if (code == LOGIN)
		{
			printf("LOGIN\n");

			// Declare message string
			std::string Message = "";
			std::string Username = "";
			std::string Password = "";

			// Declare temp string
			std::string Temp;

			//fill message with username and password
			Message += User[userId].Sock->Data.substr(2, pack_len - 2);

			//strip the appropriate data
			Username += Message.substr(0, Message.find_first_of(" "));
			Password += Message.substr(Message.find_first_of(" ") + 1, pack_len - Message.find_first_of(" ") + 1);

			//printf("DATA:%s\n", User[userId].Sock->Data.c_str());
			printf("\n::LOGIN::\nUsername[%s]\nPassword[%s]\n", Username.c_str(), Password.c_str());

			//try to load account
			int result = loadProfile(Username, Password);

			printf("load_profile() Result: %i\n", result);

			if (result == 1) //wrong password
			{
				send(User[userId].Sock, LOGIN_FAIL_NONE);

				//TODO kick server after several failed login attempts
				//warn the server, possible bruteforce hack attempt
				printf("%s has entered the wrong password!\n", Username.c_str());
			}
			else if (result == 2) //character already logged in
			{
				send(User[userId].Sock, LOGIN_FAIL_DOUBLE);
				printf("%s tried to double-log!\n", Username.c_str());
			}
			else if (result == 3) //character is banned
			{
				send(User[userId].Sock, LOGIN_FAIL_BANNED);
				printf("%s is banned and tried to login!\n", Username.c_str());
			}
			else if (result == 4)
			{
				send(User[userId].Sock, LOGIN_FAIL_NONE);
				printf("%s tried to login but doesn't exist!\n", Username.c_str());
			}

			if (result == 0 || result == 5) //login with no problems or with 1 problem: user is mute
			{								//login success

				printf("(login success) User %i %s socket status: %i\n", userId, Username.c_str(), User[userId].Sock->GetStatus());

				if (result == 0)
					User[userId].Mute = false;

				//successful login
				// Send data
				send(User[userId].Sock, LOGIN_SUCCESS, userId), User[userId].current_map;

				//set display name
				User[userId].Nick = Username;

				std::string str_SQL = "SELECT clan.name FROM player INNER JOIN clan ON clan.id = player.clan_id WHERE player.username LIKE '";
				str_SQL += database->clean(Username);
				str_SQL += "'";

				database->query(str_SQL);

				std::string clanTag = "(noob)";
				if (database->count())
				{
					database->nextRow();
					clanTag = "[";
					clanTag += database->getString(0);
					clanTag += "]";
				}

				printf("Clan: %s\n", clanTag.c_str());
				User[userId].Clan = clanTag;

				printf(">>>about to load data.\n");
				if (load_data(userId) == 0)
				{
					//set identified
					User[userId].Ident = true;

					/* */
					//log ip
					printf("i.p. logging...\n");
					std::string player_ip = User[userId].Sock->Get_IP();
					std::string player_id = User[userId].ID;

					//log the i.p.
					std::string sql = "INSERT INTO ip_log (player_id, ip) VALUES('";
					sql += database->clean(player_id);
					sql += "', '";
					sql += database->clean(player_ip);
					sql += "')";

					printf(sql.c_str());
					database->query(sql, true);

					printf(database->getError().c_str());

					//the current map of this user
					int current_map = User[userId].current_map;

					printf("Logged i.p. [%s]\n", player_ip.c_str());
					printf("going to tell client stats\n");

					// HP
					send(User[userId].Sock, STAT_HP, User[userId].hp);
					send(User[userId].Sock, STATMAX_HP, User[userId].hp);
					send(User[userId].Sock, STAT_REGEN, User[userId].regen);

					// XP
					send(User[userId].Sock, STAT_XP, User[userId].xp);
					send(User[userId].Sock, STATMAX_XP, User[userId].max_xp);

					//STATS
					send(User[userId].Sock, STAT_LEVEL, User[userId].level);
					send(User[userId].Sock, STAT_STR, User[userId].strength);
					send(User[userId].Sock, STAT_DEF, User[userId].defence);
					send(User[userId].Sock, STAT_POINTS, User[userId].stat_points);

					//equipment
					send(User[userId].Sock, EQUIP, userId, (char)0, Item[User[userId].equip[0]].equipID, User[userId].equip[0]);
					send(User[userId].Sock, EQUIP, userId, (char)1, Item[User[userId].equip[1]].equipID, User[userId].equip[1]);
					send(User[userId].Sock, EQUIP, userId, (char)1, Item[User[userId].equip[1]].equipID, User[userId].equip[1]);

					//cosmetic inventory order
					send(User[userId].Sock, INVENTORY, User[userId].getInventoryOrder());

					//inventory
					for (unsigned char i = 0; i < NUM_ITEMS; i++)
					{
						//if they own this item, tell them how many they own.
						unsigned int amount = User[userId].inventory[i];
						//prevents them from holding more than 24 items
						if (amount > 0)
						{
							User[userId].inventory_index++;
							send(User[userId].Sock, POCKET_ITEM, i, amount);
						}

						amount = User[userId].bank[i];
						//printf("this player owns [%i] of item %i\n", amt, i);
						if (amount > 0)
						{
							//TODO: remove this bank index or change how the bank works!
							User[userId].bank_index++;
							send(User[userId].Sock, BANK_ITEM, i, amount);
						}
					} //end loop 256 for items in inventory and map

					//bank
					for (int i = 0; i < 256; i++)
					{
						//go through all the ItemObjects since we're already looping
						if (map[current_map].ItemObj[i].status)
						{
							unsigned char itemId = map[current_map].ItemObj[i].itemID;
							float numx = map[current_map].ItemObj[i].x;
							float numy = map[current_map].ItemObj[i].y;
							float numxs = map[current_map].ItemObj[i].x_speed;
							float numys = map[current_map].ItemObj[i].y_speed;

							send(User[userId].Sock, SPAWN_ITEM, i, current_map, itemId, numx, numy, numxs, numys);
						}
					}

					//targets
					for (int i = 0; i < map[current_map].num_targets; i++)
					{
						if (map[current_map].Target[i].active)
							send(User[userId].Sock, SPAWN_TARGET, i, current_map);
					}

					//npcs
					for (int i = 0; i < map[current_map].num_npcs; i++)
					{
						unsigned char action = NPC_MOVE_STOP;

						if (map[current_map].NPC[i]->x_speed < 0)
							action = NPC_MOVE_LEFT;
						if (map[current_map].NPC[i]->x_speed > 0)
							action = NPC_MOVE_RIGHT;

						//break up the int as 4 bytes
						float x = map[current_map].NPC[i]->x;
						float y = map[current_map].NPC[i]->y;

						send(User[userId].Sock, action, i, current_map, x, y);
					}

					// load all enemies
					for (int i = 0; i < map[current_map].num_enemies; i++)
					{
						unsigned char action = ENEMY_MOVE_STOP;
						if (map[current_map].Enemy[i]->x_speed < 0)
							action = ENEMY_MOVE_LEFT;
						if (map[current_map].Enemy[i]->x_speed > 0)
							action = ENEMY_MOVE_RIGHT;

						//break up the int as 4 bytes
						float x = map[current_map].Enemy[i]->x;
						float y = map[current_map].Enemy[i]->y;

						send(User[userId].Sock, action, i, current_map, x, y);

						//enemy health bars
						int hp = (unsigned char)((float)map[current_map].Enemy[i]->hp / map[current_map].Enemy[i]->hp_max * hpBar);
						send(User[userId].Sock, ENEMY_HP, i, current_map, hp);
					}

					// inform all players
					for (int i = 0; i < MAX_CLIENTS; i++)
					{
						//tell everyone new has joined
						if (User[i].Ident || i == userId)
						{
							if (User[i].Nick.compare("Paladin") != 0)
							{
								//tell newbie about everyone
								float x = User[i].x;
								float y = User[i].y;
								float x_speed = User[i].x_speed;
								float y_speed = User[i].y_speed;
								unsigned char facing_right = User[i].facing_right ? 1 : 0;
								unsigned char current_map = User[i].current_map;
								std::string user = User[i].Nick + "|" + User[i].Clan;

								send(User[userId].Sock, JOIN, i, x, y, x_speed, y_speed, facing_right, current_map, user);

								//equipment
								send(User[userId].Sock, EQUIP, i, (char)0, Item[User[i].equip[0]].equipID, User[i].equip[0]);
								send(User[userId].Sock, EQUIP, i, (char)1, Item[User[i].equip[1]].equipID, User[i].equip[1]);
								send(User[userId].Sock, EQUIP, i, (char)2, Item[User[i].equip[2]].equipID, User[i].equip[2]);
							}

							//tell everyone about newbie
							if (i != userId && User[userId].Nick.compare("Paladin") != 0)
							{

								float x = User[userId].x;
								float y = User[userId].y;
								float x_speed = User[userId].x_speed;
								float y_speed = User[userId].y_speed;
								unsigned char facing_right = User[userId].facing_right ? 1 : 0;
								unsigned char current_map = User[userId].current_map;
								std::string user = User[userId].Nick + "|" + User[userId].Clan;

								// Send data
								send(User[i].Sock, JOIN, userId, x, y, x_speed, y_speed, facing_right, current_map, user);

								//equipment
								send(User[i].Sock, EQUIP, userId, (char)0, Item[User[i].equip[0]].equipID, User[i].equip[0]);
								send(User[i].Sock, EQUIP, userId, (char)1, Item[User[i].equip[1]].equipID, User[i].equip[1]);
								send(User[i].Sock, EQUIP, userId, (char)2, Item[User[i].equip[2]].equipID, User[i].equip[2]);
							} //if user != curr
						}	 //ident
					}		  //for users

					// Trigger loading complete on client.
					send(User[userId].Sock, LOADED);

					//mark this client to save when they disconnect..since Send/Recv change Ident!!
					User[userId].Save = true;
					saveAllProfiles();
				}
				else
				{
					printf("couldn't load data. KILL!\n");
					User[userId].Save = false;
					User[userId].Sock->Close();
				}
			} //end login success
		}	 //end "login"
		else if (code == REGISTER)
		{
			// Declare message string
			std::string Message = "";
			std::string Username = "";
			std::string Password = "";

			//fill message with username and password
			Message += User[userId].Sock->Data.substr(2, pack_len - 2);

			//strip the appropriate data
			Username += Message.substr(0, Message.find_first_of(" "));
			Password += Message.substr(Message.find_first_of(" ") + 1, pack_len - Message.find_first_of(" ") + 1);

			//try to create new player account
			int result = createPlayer(Username, Password, User[userId].Sock->IP);

			if (result == 1) // user already exists
			{
				send(User[userId].Sock, REGISTER_FAIL_DOUBLE);
				printf("%s tried to double-register!\n", Username.c_str());
			}
			else if (result == 0) // user created successfully
			{
				send(User[userId].Sock, REGISTER_SUCCESS);
				printf("%s has been registered!\n", Username.c_str());
			}
			else if (result == 2 || result == 3) //user is banned
			{
				User[userId].Sock->Close();
			}
		} //end "register"
		else if (code == ATTACK)
		{
			float numx, numy;
			((char *)&numx)[0] = User[userId].Sock->Data[2];
			((char *)&numx)[1] = User[userId].Sock->Data[3];
			((char *)&numx)[2] = User[userId].Sock->Data[4];
			((char *)&numx)[3] = User[userId].Sock->Data[5];
			((char *)&numy)[0] = User[userId].Sock->Data[6];
			((char *)&numy)[1] = User[userId].Sock->Data[7];
			((char *)&numy)[2] = User[userId].Sock->Data[8];
			((char *)&numy)[3] = User[userId].Sock->Data[9];
			Attack(userId, numx, numy);
		} //end enemy loop
		else if (code == MOVE_RIGHT)
		{

			float numx, numy;
			((char *)&numx)[0] = User[userId].Sock->Data[2];
			((char *)&numx)[1] = User[userId].Sock->Data[3];
			((char *)&numx)[2] = User[userId].Sock->Data[4];
			((char *)&numx)[3] = User[userId].Sock->Data[5];
			((char *)&numy)[0] = User[userId].Sock->Data[6];
			((char *)&numy)[1] = User[userId].Sock->Data[7];
			((char *)&numy)[2] = User[userId].Sock->Data[8];
			((char *)&numy)[3] = User[userId].Sock->Data[9];
			Right(userId, numx, numy);
		}
		else if (code == MOVE_LEFT)
		{
			float numx, numy;
			((char *)&numx)[0] = User[userId].Sock->Data[2];
			((char *)&numx)[1] = User[userId].Sock->Data[3];
			((char *)&numx)[2] = User[userId].Sock->Data[4];
			((char *)&numx)[3] = User[userId].Sock->Data[5];
			((char *)&numy)[0] = User[userId].Sock->Data[6];
			((char *)&numy)[1] = User[userId].Sock->Data[7];
			((char *)&numy)[2] = User[userId].Sock->Data[8];
			((char *)&numy)[3] = User[userId].Sock->Data[9];
			Left(userId, numx, numy);
		}
		else if (code == MOVE_STOP)
		{
			float numx, numy;
			((char *)&numx)[0] = User[userId].Sock->Data[2];
			((char *)&numx)[1] = User[userId].Sock->Data[3];
			((char *)&numx)[2] = User[userId].Sock->Data[4];
			((char *)&numx)[3] = User[userId].Sock->Data[5];
			((char *)&numy)[0] = User[userId].Sock->Data[6];
			((char *)&numy)[1] = User[userId].Sock->Data[7];
			((char *)&numy)[2] = User[userId].Sock->Data[8];
			((char *)&numy)[3] = User[userId].Sock->Data[9];
			Stop(userId, numx, numy);

		} //end MOVE_STOP
		else if (code == STAT_STR)
		{
			if (User[userId].stat_points > 0)
			{
				User[userId].stat_points--;
				User[userId].strength++;

				send(User[userId].Sock, STAT_STR, User[userId].strength);
				send(User[userId].Sock, STAT_POINTS, User[userId].stat_points);
			}
		}
		else if (code == STAT_HP)
		{
			if (User[userId].stat_points > 0)
			{
				User[userId].stat_points--;
				User[userId].regen += 1;

				send(User[userId].Sock, STAT_REGEN, User[userId].regen);
				send(User[userId].Sock, STAT_POINTS, User[userId].stat_points);
			}
		}
		else if (code == STAT_DEF)
		{
			if (User[userId].stat_points > 0)
			{
				User[userId].stat_points--;
				User[userId].defence++;

				send(User[userId].Sock, STAT_DEF, User[userId].defence);
				send(User[userId].Sock, STAT_POINTS, User[userId].stat_points);
			}
		}
		else if (code == MOVE_JUMP)
		{
			float numx, numy;
			((char *)&numx)[0] = User[userId].Sock->Data[2];
			((char *)&numx)[1] = User[userId].Sock->Data[3];
			((char *)&numx)[2] = User[userId].Sock->Data[4];
			((char *)&numx)[3] = User[userId].Sock->Data[5];
			((char *)&numy)[0] = User[userId].Sock->Data[6];
			((char *)&numy)[1] = User[userId].Sock->Data[7];
			((char *)&numy)[2] = User[userId].Sock->Data[8];
			((char *)&numy)[3] = User[userId].Sock->Data[9];
			Jump(userId, numx, numy);
		}						//end jump
		else if (code == EQUIP) //if user sends EQUIP it means they are unequipping that item
		{
			unsigned char slot = User[userId].Sock->Data[2];

			// TODO - sanity check on correct slot or else the server might crash
			unsigned char item = User[userId].equip[slot];

			// TODO - do not let user unequip if their inventory is full.
			// Only unequip and transfer to inventory if it actually is equipped
			if (item > 0)
			{
				//un-wear it
				User[userId].equip[slot] = 0; //TODO refactor
				//put it in the player's inventory
				User[userId].inventory[item]++; //TODO refactor

				//tell everyone
				for (int uc = 0; uc < MAX_CLIENTS; uc++)
					send(User[uc].Sock, EQUIP, userId, slot, (char)0, (char)0);

				//update the player's inventory
				int amount = User[userId].inventory[item];
				send(User[userId].Sock, POCKET_ITEM, item, amount);
			}
		} //end EQUIP
		else if (code == USE_ITEM)
		{
			printf("USE_ITEM\n");
			int item = User[userId].Sock->Data[2];
			int type = Item[item].type;
			printf("type is %i\n", type);
			int current_map = User[userId].current_map;

			//only attempt to use items if the player has them
			if (User[userId].inventory[item] > 0)
			{
				unsigned int amt = 0;
				std::string Packet = "0";
				std::string Message = "0";
				std::string hpPacket = "0";
				char *p;
				char b1, b2, b3, b4;
				float numy, numx, numys, numxs,
					rand_xs, rand_ys,
					rand_x, rand_y;
				int rand_i, rand_item;

				//TODO - refactor this into use a item handler.
				switch (type)
				{
				case 0:
					break; //cosmetic

				case 1: //food

					// Do not eat food if player is full health.
					if (User[userId].hp == User[userId].max_hp)
						break;

					//TODO refactor
					User[userId].hp += Item[item].hp;

					if (User[userId].hp > User[userId].max_hp)
						User[userId].hp = User[userId].max_hp;

					//tell this client
					send(User[userId].Sock, STAT_HP, User[userId].hp);

					//remove item
					User[userId].inventory[item]--;

					//tell them how many items they have
					unsigned int amount = User[userId].inventory[item];
					if (amount == 0)
						User[userId].inventory_index--;

					// party hp notification
					// TODO - change magic number 80 to use a config value
					unsigned char displayHp = (int)((User[userId].hp / (float)User[userId].max_hp) * 80);
					hpPacket[0] = hpPacket.length();

					for (int pl = 0; pl < MAX_CLIENTS; pl++)
					{
						if (pl != userId && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[userId].party)
							send(User[pl].Sock, BUDDY_HP, userId, displayHp);
					}

					//put in client players inventory
					send(User[userId].Sock, POCKET_ITEM, item, amount);

					break;

				case 2: //weapon
					// Does the other player have another WEAPON equipped?
					// If so, transfer it to inventory
					unsigned char otherItem = User[userId].equip[0];

					// Transfer weapon from inventory to equipment slot
					User[userId].equip[0] = item;
					User[userId].inventory[item]--;

					// Tranfer old weapon to users inventory
					if (otherItem > 0)
					{
						User[userId].inventory[otherItem]++;
						User[userId].inventory_index++;
						unsigned int amount = User[userId].inventory[otherItem];
						send(User[userId].Sock, POCKET_ITEM, otherItem, amount);
					}

					// Tell player one weapon is removed from their inventory
					unsigned int amount = User[userId].inventory[item];
					send(User[userId].Sock, POCKET_ITEM, item, amount);

					//tell everyone the player equipped their weapon
					for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
					{
						if (User[i1].Ident)
							send(User[i1].Sock, EQUIP, userId, (char)0, Item[item].equipID, item);
					}
					break;

				case 3: //hat
					// Does the other player have another HAT equipped?
					// If so, transfer it to inventory
					unsigned char otherItem = User[userId].equip[1];

					// Transfer hat from inventory to equipment slot
					User[userId].equip[1] = item;
					User[userId].inventory[item]--;

					// Tranfer old hat to users inventory
					if (otherItem > 0)
					{
						User[userId].inventory[otherItem]++;
						User[userId].inventory_index++;
						unsigned int amount = User[userId].inventory[otherItem];
						send(User[userId].Sock, POCKET_ITEM, otherItem, amount);
					}

					// Tell player one hat is removed from their inventory
					unsigned int amount = User[userId].inventory[item];
					send(User[userId].Sock, POCKET_ITEM, item, amount);

					//tell everyone the player equipped their hat
					for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
					{
						if (User[i1].Ident)
							send(User[i1].Sock, EQUIP, userId, (char)1, Item[item].equipID, item);
					}
					break;
				case 4: // mystery box
					// holiday event
					if (HOLIDAY)
					{
						////get rid of the box
						int numUsed = 0;

						if (User[userId].inventory[item] >= 10)
						{
							numUsed = 10;
							User[userId].inventory[item] -= 10;
						}
						else
						{
							numUsed = User[userId].inventory[item];
							User[userId].inventory[item] = 0;
							User[userId].inventory_index--;
						}

						//tell them how many items they have
						unsigned int amount = User[userId].inventory[item];

						//put in players inventory
						Packet = "0";
						Packet += POCKET_ITEM;
						Packet += item;

						send(User[userId].Sock, POCKET_ITEM, item, amount);

						for (int it = 0; it < numUsed; it++)
						{
							//spawn a hat or nothing
							rand_xs = 0;
							rand_ys = -5;

							// 1:100 chance of item
							rand_item = rand() % 100;
							if (rand_item != 1)
								rand_item = ITEM_GOLD;
							else
								rand_item = HOLIDAY_BOX_DROP;

							int rand_i;
							rand_x = User[userId].x + 32;
							rand_y = User[userId].y - 32;

							for (rand_i = 0; rand_i < 256; rand_i++)
							{
								if (rand_i == 255 || map[current_map].ItemObj[rand_i].status == false)
									break;
							}

							//TODO change this limit to 2 bytes (32K)
							if (rand_i == 255)
							{
								//If we traversed the whole item list already, start over.
								if (map[current_map].currentItem == 255)
									map[current_map].currentItem = 0;

								//use the currentItem to traverse the list whenever it overflows, so the "oldest" item gets deleted.
								rand_i = map[current_map].currentItem;
								map[current_map].currentItem++;
							}

							map[current_map].ItemObj[rand_i] = SKO_ItemObject(rand_item, rand_x, rand_y, rand_xs, rand_ys, 1);

							{
								int i = rand_i;
								//dont let them get stuck
								if (blocked(current_map, map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed, map[current_map].ItemObj[i].y + map[current_map].ItemObj[i].y_speed + 0.15, map[current_map].ItemObj[i].x + Item[map[current_map].ItemObj[i].itemID].w, map[current_map].ItemObj[i].y + map[current_map].ItemObj[i].y_speed + Item[map[current_map].ItemObj[i].itemID].h, false))
								{
									//move it down a bit
									rand_y = User[userId].y + 1;
									map[current_map].ItemObj[i].y = rand_y;
								}
							}
							float x = rand_x;
							float y = rand_y;
							float x_speed = rand_xs;
							float y_speed = rand_ys;

							for (int iii = 0; iii < MAX_CLIENTS; iii++)
							{
								if (User[iii].Ident && User[iii].current_map == current_map)
									send(User[iii].Sock, SPAWN_ITEM, rand_i, current_map, rand_item, x, y, x_speed, y_speed);
							}
						}
					}
					break;

				case 5: // trophy / spell
					// Does the other player have another TROPHY equipped?
					// If so, transfer it to inventory
					unsigned char otherItem = User[userId].equip[2];

					// Transfer trophy from inventory to equipment slot
					User[userId].equip[2] = item;
					User[userId].inventory[item]--;

					// Tranfer old trophy to users inventory
					if (otherItem > 0)
					{
						User[userId].inventory[otherItem]++;
						User[userId].inventory_index++;
						unsigned int amount = User[userId].inventory[otherItem];
						send(User[userId].Sock, POCKET_ITEM, otherItem, amount);
					}

					// Tell player one trophy is removed from their inventory
					unsigned int amount = User[userId].inventory[item];
					send(User[userId].Sock, POCKET_ITEM, item, amount);

					//tell everyone the player equipped their trophy
					for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
					{
						if (User[i1].Ident)
							send(User[i1].Sock, EQUIP, userId, (char)2, Item[item].equipID, item);
					}
					break;

				default:
					break;
				} //end switch
			}	 //end if you have the item
		}		  //end USE_ITEM
		else if (code == DROP_ITEM)
		{
			unsigned char item = User[userId].Sock->Data[2];
			int current_map = User[userId].current_map;

			unsigned int amount = 0;
			//build an int from 4 bytes
			((char *)&amount)[0] = User[userId].Sock->Data[3];
			((char *)&amount)[1] = User[userId].Sock->Data[4];
			((char *)&amount)[2] = User[userId].Sock->Data[5];
			((char *)&amount)[3] = User[userId].Sock->Data[6];

			//if they have enough to drop
			if (User[userId].inventory[item] >= amount && amount > 0 && User[userId].tradeStatus < 1)
			{
				//take the items away from the player
				User[userId].inventory[item] -= amount;
				int ownedAmount = User[userId].inventory[item];

				//keeping track of inventory slots
				if (User[userId].inventory[item] == 0)
				{
					//printf("the user has %i of Item[%i]", amt, i );
					//prevents them from holding more than 24 items
					User[userId].inventory_index--;
				}

				//tell the user they dropped their items.
				send(User[userId].Sock, POCKET_ITEM, item, ownedAmount);

				//TODO refactor all of this
				//next spawn a new item for all players
				int rand_i;
				float rand_x = User[userId].x + 16 + (32 - Item[item].w) / 2.0;
				float rand_y = User[userId].y - Item[item].h;

				float rand_xs = 0;
				if (User[userId].facing_right)
				{
					rand_xs = 2.2;
				}
				else
				{
					rand_xs = -2.2;
				}

				float rand_ys = -5;
				for (rand_i = 0; rand_i < 255; rand_i++)
				{
					if (map[current_map].ItemObj[rand_i].status == false)
						break;
				}

				//find item object that's free
				if (rand_i == 255)
				{
					if (map[current_map].currentItem == 255)
						map[current_map].currentItem = 0;

					rand_i = map[current_map].currentItem;

					map[current_map].currentItem++;
				}

				//tell everyone there's an item up for grabs
				map[current_map].ItemObj[rand_i] = SKO_ItemObject(item, rand_x, rand_y, rand_xs, rand_ys, amount);

				int i = rand_i;
				//dont let them get stuck
				if (blocked(current_map, map[current_map].ItemObj[i].x + map[current_map].ItemObj[i].x_speed, map[current_map].ItemObj[i].y + map[current_map].ItemObj[i].y_speed + 0.15, map[current_map].ItemObj[i].x + Item[map[current_map].ItemObj[i].itemID].w, map[current_map].ItemObj[i].y + map[current_map].ItemObj[i].y_speed + Item[map[current_map].ItemObj[i].itemID].h, false))
				{
					//move it down a bit
					rand_y = User[userId].y + 1;
					map[current_map].ItemObj[i].y = rand_y;
				}

				float x = rand_x;
				float y = rand_y;
				float x_speed = rand_xs;
				float y_speed = rand_ys;

				for (int iii = 0; iii < MAX_CLIENTS; iii++)
				{
					if (User[iii].Ident && User[iii].current_map == current_map)
						send(User[iii].Sock, SPAWN_ITEM, rand_i, current_map, item, x, y, x_speed, y_speed);
				}
			}
		} // end DROP_ITEM
		else if (code == TRADE)
		{
			//what kind of trade action, yo?
			int action = User[userId].Sock->Data[2];
			int playerB = 0;

			switch (action)
			{

			//TODO: don't let a player confirm without checking trade statuses
			case INVITE:

				playerB = User[userId].Sock->Data[3];
				printf("userId=%i playerB=%i\n", userId, playerB);
				//FFFF, don't troll me. Is playerB even real? >___>
				if (playerB < MAX_CLIENTS && playerB != userId && User[playerB].Ident)
				{

					printf("someone wants to trade...legit?\n");
					// make sure both players aren't busy!!
					if (User[userId].tradeStatus == 0 && User[playerB].tradeStatus == 0)
					{
						printf("%s trade with %s.\n", User[userId].Nick.c_str(), User[playerB].Nick.c_str());

						//Send 'trade with A' to player B
						send(User[playerB].Sock, TRADE, INVITE, userId);

						//hold status of what the players are trying to do
						// (tentative...)
						User[userId].tradeStatus = INVITE;
						User[userId].tradePlayer = playerB;
						User[playerB].tradeStatus = INVITE;
						User[playerB].tradePlayer = userId;
					} // end not busy, lets request, bro!
					else
					{
						printf("[ERR] A trade status=%i B trade status=%i\n", User[userId].tradeStatus, User[playerB].tradeStatus);
					}
				} //end invite trade

				break; //end INVITE

			case ACCEPT:
				//hold status of what the players are trying to do
				// (trading)
				playerB = User[userId].tradePlayer;

				if (playerB >= 0 && User[userId].tradeStatus == INVITE && User[playerB].tradeStatus == INVITE)
				{
					User[userId].tradeStatus = ACCEPT;
					User[playerB].tradeStatus = ACCEPT;
				}

				//tell both parties that they are now trading

				//Send 'trade with A' to player B
				send(User[playerB].Sock, TRADE, ACCEPT, userId);

				//Send 'trade with B' to player A
				send(User[userId].Sock, TRADE, ACCEPT, playerB);

				break;

			case CANCEL:

				//okay, kill the trade for this player and the sellected player
				playerB = User[userId].tradePlayer;

				if (playerB >= 0)
				{

					//set them to blank state
					User[userId].tradeStatus = 0;
					User[playerB].tradeStatus = 0;

					//set them to be trading with nobody
					User[userId].tradePlayer = -1;
					User[playerB].tradePlayer = -1;

					//tell both players cancel trade
					send(User[playerB].Sock, TRADE, CANCEL);
					send(User[userId].Sock, TRADE, CANCEL);

					//clear trade windows...
					for (int i = 0; i < 256; i++)
					{
						User[userId].localTrade[i] = 0;
						User[playerB].localTrade[i] = 0;
					}
				}
				break;

			case OFFER:
				//only do something if both parties are in accept trade mode
				if (User[userId].tradeStatus == ACCEPT && User[User[userId].tradePlayer].tradeStatus == ACCEPT)
				{
					unsigned char item = User[userId].Sock->Data[3];

					int amount = 0;
					((char *)&amount)[0] = User[userId].Sock->Data[4];
					((char *)&amount)[1] = User[userId].Sock->Data[5];
					((char *)&amount)[2] = User[userId].Sock->Data[6];
					((char *)&amount)[3] = User[userId].Sock->Data[7];

					printf("offer!\n ITEM: [%i]\nAMOUNT [%i/%i]\n...", item, amount, User[userId].inventory[item]);
					//check if you have that item
					if (User[userId].inventory[item] >= amount)
					{
						User[userId].localTrade[item] = amount;

						//send to both players!
						send(User[userId].Sock, TRADE, OFFER, (char)1, item, amount);

						//send to playerB
						send(User[User[userId].tradePlayer].Sock, TRADE, OFFER, (char)2, item, amount);
					}
				}

				break;

			case CONFIRM:
				//easy to understand if we use A & B
				unsigned char playerA = userId;
				unsigned char playerB = User[userId].tradePlayer;

				if (playerB < 0)
				{
					//tell both players cancel trade
					send(User[userId].Sock, TRADE, CANCEL);
					User[userId].tradeStatus = -1;
					User[userId].tradePlayer = 0;
					break;
				}

				//make sure playerA is in accept mode before confirming
				if (User[playerA].tradeStatus == ACCEPT)
					User[playerA].tradeStatus = CONFIRM;

				//tell both players :)
				send(User[playerA].Sock, TRADE, READY, (char)1);
				send(User[playerB].Sock, TRADE, READY, (char)2);

				//if both players are now confirm, transact and reset!
				if (User[playerA].tradeStatus == CONFIRM && User[playerB].tradeStatus == CONFIRM)
				{
					//lets make sure players have that many items
					bool error = false;
					for (int i = 0; i < 256; i++)
					{
						//compare the offer to the owned items
						if (User[playerA].inventory[i] < User[playerA].localTrade[i])
						{
							error = true;
							break;
						}
						//compare the offer to the owned items
						if (User[playerB].inventory[i] < User[playerB].localTrade[i])
						{
							error = true;
							break;
						}
					}

					//tell them the trade is over!!!
					User[playerA].tradeStatus = 0;
					User[playerB].tradeStatus = 0;

					//set them to be trading with nobody
					User[playerA].tradePlayer = -1;
					User[playerB].tradePlayer = -1;

					//tell both players cancel trade
					send(User[playerB].Sock, TRADE, CANCEL);
					send(User[playerA].Sock, TRADE, CANCEL);

					//make the transaction!
					if (!error)
					{
						printf("trade transaction!\n");

						//go through each item and add them
						for (unsigned char i = 0; i < 256; i++)
						{
							//easy to follow with these variables broski :P
							int aOffer = User[playerA].localTrade[i];
							int bOffer = User[playerB].localTrade[i];

							//give A's stuff  to B
							if (aOffer > 0)
							{
								printf(kGreen "%s gives %i of item %i to %s\n" kNormal, User[playerA].Nick.c_str(), aOffer, i, User[playerB].Nick.c_str());
								//trade the item and tell the player!
								User[playerB].inventory[i] += aOffer;
								User[playerA].inventory[i] -= aOffer;

								//put in players inventory
								unsigned int amount = User[playerB].inventory[i];
								send(User[playerB].Sock, POCKET_ITEM, i, amount);

								//take it out of A's inventory
								amount = User[playerA].inventory[i];
								send(User[playerA].Sock, POCKET_ITEM, i, amount);
							}

							//give B's stuff  to A
							if (bOffer > 0)
							{
								printf(kGreen "%s gives %i of item %i to %s\n" kNormal, User[playerB].Nick.c_str(), aOffer, i, User[playerA].Nick.c_str());
								//trade the item and tell the player!
								User[playerA].inventory[i] += bOffer;
								User[playerB].inventory[i] -= bOffer;

								//put in players inventory
								unsigned int amount = User[playerA].inventory[i];
								send(User[playerA].Sock, POCKET_ITEM, i, amount);

								//take it away from B
								amount = User[playerB].inventory[i];
								send(User[playerB].Sock, POCKET_ITEM, i, amount);
							}

							//clear the items
							User[playerA].localTrade[i] = 0;
							User[playerB].localTrade[i] = 0;
						}

						//Save all profiles after a trade
						saveAllProfiles();
					}
				}
				break;
			} //end trade action switch

		} //invite to trade
		else if (code == PARTY)
		{
			int mode = User[userId].Sock->Data[2];
			int playerB = 0;

			switch (mode)
			{

			case INVITE:

				playerB = User[userId].Sock->Data[3];

				//invite the other user
				//if the other user is not in your party
				if (User[playerB].partyStatus == 0)
				{
					int partyID = User[userId].party;

					//set their party
					if (User[userId].party == -1)
					{
						for (partyID = 0; partyID <= MAX_CLIENTS; partyID++)
						{
							//look for an open partyID
							bool taken = false;
							for (int i = 0; i < MAX_CLIENTS; i++)
							{
								if (User[i].Ident && User[i].party == partyID)
								{
									taken = true;
									break;
								}
							}
							if (!taken)
								break;
						} //find oepn party id
					}	 //end if party is null

					//make the parties equal
					User[userId].party = partyID;
					User[playerB].party = partyID;

					//set the party status of the invited person
					User[playerB].partyStatus = INVITE;
					User[userId].partyStatus = ACCEPT;

					//ask the user to party
					send(User[playerB].Sock, PARTY, INVITE, userId);
				}
				break;

			case ACCEPT:
				//only if I was invited :)
				if (User[userId].partyStatus == INVITE)
				{
					//tell everyone in the party that player has joined
					for (int pl = 0; pl < MAX_CLIENTS; pl++)

						//tell the user about all the players in the party
						if (User[pl].Ident && pl != userId && User[pl].partyStatus == PARTY && User[pl].party == User[userId].party)
						{
							//
							// Notify all members in the party that player is joining
							//
							send(User[pl].Sock, PARTY, ACCEPT, userId, User[userId].party);
							//tell existing party members the new player's stats
							unsigned char hp = (int)((User[userId].hp / (float)User[userId].max_hp) * 80);
							unsigned char xp = (int)((User[userId].xp / (float)User[userId].max_xp) * 80);
							unsigned char level = User[userId].level;
							send(User[pl].Sock, BUDDY_HP, userId, hp);
							send(User[pl].Sock, BUDDY_XP, userId, xp);
							send(User[pl].Sock, BUDDY_LEVEL, userId, level);

							//
							// Notify player of each existing party member
							//
							send(User[userId].Sock, PARTY, ACCEPT, pl, User[pl].party);
							// Send player the current party member stats
							hp = (int)((User[pl].hp / (float)User[pl].max_hp) * 80);
							xp = (int)((User[pl].xp / (float)User[pl].max_xp) * 80);
							level = User[pl].level;
							send(User[userId].Sock, BUDDY_HP, pl, hp);
							send(User[userId].Sock, BUDDY_XP, pl, xp);
							send(User[userId].Sock, BUDDY_LEVEL, pl, level);
						}
				}
				break;

			case CANCEL:
				//quit them out of this party
				quitParty(userId);
				break;

			default:
				break;
			} //end switch mode
		}	 //invite to party
		else if (code == CLAN)
		{
			int mode = User[userId].Sock->Data[2];
			int playerB = 0;

			printf("Clan mode is %i\n", mode);
			switch (mode)
			{

			case INVITE:

				playerB = User[userId].Sock->Data[3];

				//invite the other user
				//if the other user is not in your clan
				if (User[playerB].Clan[0] == '(')
				{
					//check if the inviter player is even in a clan
					if (User[userId].Clan[0] == '(')
					{
						send(User[userId].Sock, CHAT, "You are not in a clan.");
					}
					else
					{
						//make sure they are clan owner
						std::string strl_SQL = "";
						strl_SQL += "SELECT COUNT(clan.id) FROM clan INNER JOIN player ON clan.owner_id = player.id";
						strl_SQL += " WHERE player.username LIKE '";
						strl_SQL += User[userId].Nick;
						strl_SQL += "'";

						database->query(strl_SQL);

						if (database->count())
						{
							database->nextRow();
							//if you own the clan...
							if (database->getInt(0))
							{
								//get clan id
								std::string strl_SQL = "";
								strl_SQL += "SELECT clan_id FROM player";
								strl_SQL += " WHERE username LIKE '";
								strl_SQL += User[userId].Nick;
								strl_SQL += "'";

								database->query(strl_SQL);
								std::string uidl_CLAN = "";
								if (database->count())
								{
									database->nextRow();
									uidl_CLAN = database->getString(0);
								}
								else
								{
									printf(kRed "The clan failed to load for some reason. %s(%s)\n" kNormal, User[userId].Nick.c_str(), User[userId].Clan.c_str());
									break;
								}

								//set the clan status of the invited person
								User[playerB].clanStatus = INVITE;
								User[playerB].tempClanId = uidl_CLAN;

								//ask the user to clan
								send(User[playerB].Sock, CLAN, INVITE, userId);
							}
							else
							{
								send(User[userId].Sock, CHAT, "Sorry, only the clan owner can invite new members.");
							}
						}
					}
				}
				else
				{
					send(User[userId].Sock, CHAT, "Sorry, ", User[playerB].Nick, " Is already in a clan.");
				}
				break;

			case ACCEPT:
				//TODO do not disconnect the player when they accept clan invites
				//only if I was invited :)
				if (User[userId].clanStatus == INVITE)
				{
					//set the clan of the player in the DB
					std::string strl_SQL = "UPDATE player SET clan_id = ";
					strl_SQL += User[userId].tempClanId;
					strl_SQL += " WHERE username LIKE '";
					strl_SQL += User[userId].Nick;
					strl_SQL += "'";

					database->query(strl_SQL);

					// TODO - make new packet type to update clan instead of booting player
					// This relies on client to automatically reconnect
					User[userId].Sock->Close();
					User[userId].Sock->Connected = false;
				}

				break;

			case CANCEL:
				//quit them out of this clan
				User[userId].clanStatus = -1;
				break;

			default:
				break;
			} //end switch mode
		}	 //invite to clan
		else if (code == SHOP)
		{
			int action = User[userId].Sock->Data[2];
			int stallId = 0;
			int current_map = User[userId].current_map;

			switch (action)
			{
			case INVITE:
				stallId = User[userId].Sock->Data[3];
				if (stallId < map[current_map].num_stalls)
				{
					if (map[current_map].Stall[stallId].shopid == 0)
					{
						if (User[userId].tradeStatus < 1)
						{
							send(User[userId].Sock, BANK, INVITE);

							//set trade status to banking
							User[userId].tradeStatus = BANK;
						}
					}
					if (map[current_map].Stall[stallId].shopid > 0)
					{
						//tell the user "hey, shop #? opened up"
						unsigned char shopId = map[current_map].Stall[stallId].shopid;
						send(User[userId].Sock, SHOP, shopId);

						//set trade status to shopping
						User[userId].tradeStatus = SHOP;
						User[userId].tradePlayer = map[current_map].Stall[stallId].shopid;
					}
				}
				break;

			case BUY:
			{
				int sitem, amount;
				sitem = User[userId].Sock->Data[3];
				//hold the result...
				//build an int from 4 bytes
				((char *)&amount)[0] = User[userId].Sock->Data[4];
				((char *)&amount)[1] = User[userId].Sock->Data[5];
				((char *)&amount)[2] = User[userId].Sock->Data[6];
				((char *)&amount)[3] = User[userId].Sock->Data[7];

				int i = 0, item = 0, price = 0;
				for (int y = 0; y < 4; y++)
					for (int x = 0; x < 6; x++)
					{
						if (i == sitem)
						{
							item = map[current_map].Shop[User[userId].tradePlayer].item[x][y][0];
							price = map[current_map].Shop[User[userId].tradePlayer].item[x][y][1];
						}
						i++;
					}

				//printf("%s wants to buy %d of item #%d\n", User[userId].Nick.c_str(), amount, item);

				//if the player can afford it
				if (item > 0 && User[userId].inventory[ITEM_GOLD] >= price * amount)
				{
					//take away gold
					User[userId].inventory[ITEM_GOLD] -= price * amount;

					//give them the item
					User[userId].inventory[item] += amount;

					//put in client players inventory
					unsigned int amount = User[userId].inventory[item];
					send(User[userId].Sock, POCKET_ITEM, item, amount);

					//Take gold out of player's inventory
					amount = User[userId].inventory[ITEM_GOLD];
					send(User[userId].Sock, POCKET_ITEM, (char)ITEM_GOLD, amount);
				}
			}

			break;

			case SELL:
			{
				printf("SELL baby!\n");
				int item = 0, amount = 0, price = 0;
				item = User[userId].Sock->Data[3];
				price = Item[item].price;

				//hold the result...
				//build an int from 4 bytes
				((char *)&amount)[0] = User[userId].Sock->Data[4];
				((char *)&amount)[1] = User[userId].Sock->Data[5];
				((char *)&amount)[2] = User[userId].Sock->Data[6];
				((char *)&amount)[3] = User[userId].Sock->Data[7];

				//if they even have the item to sell
				if (price > 0 && User[userId].inventory[item] >= amount)
				{
					//take away gold
					User[userId].inventory[item] -= amount;

					//give them the item
					User[userId].inventory[ITEM_GOLD] += price * amount;

					//take out of client player's inventory
					unsigned int amount = User[userId].inventory[item];
					send(User[userId].Sock, POCKET_ITEM, item, amount);

					//put gold into player's inventory
					amount = User[userId].inventory[ITEM_GOLD];
					send(User[userId].Sock, POCKET_ITEM, (char)ITEM_GOLD, amount);
				}
			}

			break;

			case CANCEL:
				printf("CANCEL, BABY!\n");
				User[userId].tradeStatus = 0;
				User[userId].tradePlayer = 0;
				break;

			default:
				printf("DEFAULT BABY!\n");
				break;
			}
			printf("\n");
		} //end shop
		else if (code == BANK)
		{
			int action = User[userId].Sock->Data[2];
			std::string packet;
			switch (action)
			{
			case CANCEL:
				if (User[userId].tradeStatus == BANK)
				{
					User[userId].tradeStatus = 0;
				}
				break;

			case BANK_ITEM:
				if (User[userId].tradeStatus == BANK)
				{//TODO sanity check for packet length
					//get item type and amount
					unsigned char item = User[userId].Sock->Data[3];
					unsigned int amount = 0;
					((char *)&amount)[0] = User[userId].Sock->Data[4];
					((char *)&amount)[1] = User[userId].Sock->Data[5];
					((char *)&amount)[2] = User[userId].Sock->Data[6];
					((char *)&amount)[3] = User[userId].Sock->Data[7];

					if (item < NUM_ITEMS && User[userId].inventory[item] >= amount)
					{
						User[userId].inventory[item] -= amount;
						User[userId].bank[item] += amount;

						//send deposit notification to user
						unsigned int deposit = User[userId].bank[item];
						send(User[userId].Sock, BANK_ITEM, item, deposit);

						//update client player's inventory
						unsigned int withdrawal = User[userId].inventory[item];
						send(User[userId].Sock, POCKET_ITEM, item, withdrawal);
					}
				}
				break;

			case DEBANK_ITEM:
				if (User[userId].tradeStatus == BANK)
				{//TODO sanity check on packet length
					unsigned char item = User[userId].Sock->Data[3];
					unsigned int amount = 0;
					((char *)&amount)[0] = User[userId].Sock->Data[4];
					((char *)&amount)[1] = User[userId].Sock->Data[5];
					((char *)&amount)[2] = User[userId].Sock->Data[6];
					((char *)&amount)[3] = User[userId].Sock->Data[7];

					if (item < NUM_ITEMS && User[userId].bank[item] >= amount)
					{
						User[userId].bank[item] -= amount;
						User[userId].inventory[item] += amount;

						//send deposit notification to user
						unsigned int deposit = User[userId].bank[item];
						send(User[userId].Sock, BANK_ITEM, item, deposit);

						//update client player's inventory
						unsigned int withdrawal = User[userId].inventory[item];
						send(User[userId].Sock, POCKET_ITEM, item, withdrawal);
					}
				}
				break;

			//shouldn't happen
			default:
				break;
			}
		} //end bank
		else if (code == INVENTORY)
		{
			printf("inventory order!\n");
			std::string inventory_order = User[userId].Sock->Data.substr(2);
			User[userId].inventory_order = inventory_order;
		}
		else if (code == MAKE_CLAN)
		{
			//check if the player has enough money.
			if (User[userId].inventory[ITEM_GOLD] >= 100000) // 100000 TODO make a const for this
			{
				std::string clanTag = User[userId].Sock->Data.substr(2);
				trim(clanTag);

				//find out if it exists already...
				std::string sql = "SELECT COUNT(*) FROM clan WHERE clan.name LIKE '";
				sql += database->clean(clanTag);
				sql += "'";

				database->query(sql);
				if (database->count())
				{
					database->nextRow();
					if (database->getInt(0) > 0)
					{
						printf("clan already exists...%s\n", clanTag.c_str());
						send(User[userId].Sock, CHAT, "Sorry! Clan (", clanTag, ") already exists!");
					}
					else
					{
						std::string sql = "SELECT * FROM player WHERE username LIKE '";
						sql += database->clean(User[userId].Nick);
						sql += "'";

						database->query(sql);
						if (database->count())
						{
							database->nextRow();
							//get the id for loading purposes
							std::string player_id = database->getString(0);

							//update the clan tables!!!!
							sql = "";
							sql += "INSERT INTO clan (name, owner_id) VALUES ('";
							sql += database->clean(clanTag);
							sql += "',";

							std::string creator_id = "NULL";
							std::string sql2 = "SELECT * FROM player WHERE username LIKE '";
							sql2 += database->clean(User[userId].Nick);
							sql2 += "'";
							database->query(sql2, true);
							if (database->count())
							{
								database->nextRow();
								creator_id = database->getString(0);
							}

							sql += creator_id;
							sql += ")";

							database->query(sql, true);

							//send to all players so everyone knows a new clan was formed!
							for (int cu1 = 0; cu1 < MAX_CLIENTS; cu1++)
							{
								if (User[cu1].Ident)
									send(User[cu1].Sock, CHAT, "Clan (", clanTag, ") has been established by owner ", User[userId].Nick, ".");
							} //end loop all clients

							//set player clan id
							sql = "";
							sql += "UPDATE player SET player.clan_id = (SELECT clan.id FROM clan WHERE clan.name LIKE '";
							sql += database->clean(clanTag);
							sql += "') WHERE player.id LIKE '";
							sql += player_id;
							sql += "'";

							database->query(sql);
							
							User[userId].inventory[ITEM_GOLD] -= 100000;

							// TODO - when below fix is complete, notify player their gold has decreased by 100K
							//TODO do not rely on client reconnect when forming or joining a clan
							User[userId].Sock->Close();
							User[userId].Sock->Connected = false;
						} //end username exists
					}	 //end else clan is not a dupe
				}		  //query success
			}			  //end you have enough gold
			else
			{
				send(User[userId].Sock, CHAT, "You cannot afford to establish a clan. It costs 100K gold pieces.");
			} //end you did not have enough $$
		}	 //end make clan
		else if (code == CAST_SPELL)
		{
			//What do you have equipped?
			int spell = User[userId].equip[2];

			if (spell > 0)
			{
				if (User[userId].inventory[spell] > 0)
				{
					User[userId].inventory[spell]--;
					if (User[userId].inventory[spell] == 0)
						User[userId].inventory_index--;
					

					//notify user the item was thrown
					unsigned int amount = User[userId].inventory[spell];
					send(User[userId].Sock, POCKET_ITEM, spell, amount);
				}
				else
				{
					//Throw the item from your hand instead of inventory if that's all you have
					User[userId].equip[2] = 0;
					//send packet that says you arent holding anything!
					for (int c = 0; c < MAX_CLIENTS; c++)
						if (User[c].Ident)
							send(User[c].Sock, EQUIP,  userId, (char)2, (char)0, (char)0);
				}

				//tell all the users that an item has been thrown...
				//just use an item object with no value.
				SKO_ItemObject lootItem = SKO_ItemObject();
				lootItem.y = User[userId].y + 24;

				if (User[userId].facing_right)
				{
					lootItem.x_speed = 10;
					lootItem.x = User[userId].x + 50;
				}
				else
				{
					lootItem.x_speed = -10;
					lootItem.x = User[userId].x - 32;
				}

				lootItem.y_speed = -3.2;
				lootItem.itemID = spell;
				lootItem.owner = userId;
				lootItem.amount = 1;

				SpawnLoot(User[userId].current_map, lootItem);
			}
		}
		else if (code == CHAT)
		{
			if (!User[userId].Mute && User[userId].Ident)
			{
				std::string command = User[userId].Sock->Data.substr(2, User[userId].Sock->Data.find_first_of(" ") - 2);
				//printf("COMMAND: [%s]", command.c_str());
				if (command.compare("/ban") == 0)
				{
					// Declare message string
					std::string Message = "";
					std::string Username = "";
					std::string Reason = "";

					//fill message with username and reason
					Message += User[userId].Sock->Data.substr(User[userId].Sock->Data.find_first_of(" ") + 1, pack_len - User[userId].Sock->Data.find_first_of(" ") + 1);

					//strip the appropriate data
					Username += Message.substr(0, Message.find_first_of(" "));
					Reason += Message.substr(Message.find_first_of(" ") + 1, Message.length() - Message.find_first_of(" ") + 1);

					int result = 0;

					result = banPlayer(userId, Username, Reason, 1);

					if (result == 0)
					{
						for (int i = 0; i < MAX_CLIENTS; i++)
						{
							// If this socket is taken
							if (User[i].Status)
							{
								// Send data
								send(User[i].Sock, CHAT, Username, " has been banned. (", Reason, ")");

								//find which socket, yo
								if (lower(User[i].Nick).compare(lower(Username)) == 0)
								{
									User[i].Sock->Close();
								}
							}
						}
					}
					else if (result == 1)
					{
						send(User[userId].Sock, CHAT, Username, " does not exist.");
					}
					else if (result == 2)
					{
						send(User[userId].Sock, CHAT, Username, " cannot be banned.");
					}
					else if (result == 3)
					{
						printf("The user [%s] tried to ban [%s] but they are not moderator!\n", User[userId].Nick.c_str(), Username.c_str());
						send(User[userId].Sock, CHAT, "You re not authorized to ban a player.");
					}

				} //end "/ban"
				else if (command.compare("/unban") == 0)
				{
					// Declare message string
					std::string Username = "";
					std::string Message = "";

					//fill message with username and reason
					Message += User[userId].Sock->Data.substr(User[userId].Sock->Data.find_first_of(" ") + 1, pack_len - User[userId].Sock->Data.find_first_of(" ") + 1);

					//strip the appropriate data
					Username += Message.substr(Message.find_first_of(" ") + 1, pack_len - Message.find_first_of(" ") + 1);

					int result = 0;

					result = banPlayer(userId, Username, "null", 0);

					if (result == 0)
					{
						send(User[userId].Sock, CHAT, Username, " has been unbanned.");
					}
					else if (result == 1)
					{
						send(User[userId].Sock, CHAT, Username, " does not exist.");
					}
					else if (result == 2)
					{
						send(User[userId].Sock, CHAT, Username, " cannot be unbanned.");
					}
					else if (result == 3)
					{
						printf("The user [%s] tried to unban [%s] but they are not moderator!\n", User[userId].Nick.c_str(), Username.c_str());
						send(User[userId].Sock, CHAT, "You are not authorized to unban a player.");
					}

				} //end "/unban"
				else if (command.compare("/mute") == 0)
				{
					// Declare message string
					std::string Message = "";
					std::string Username = "";
					std::string Reason = "";

					//fill message with username and reason
					Message += User[userId].Sock->Data.substr(User[userId].Sock->Data.find_first_of(" ") + 1, pack_len - User[userId].Sock->Data.find_first_of(" ") + 1);

					//strip the appropriate data
					Username += Message.substr(0, Message.find_first_of(" "));
					Reason += Message.substr(Message.find_first_of(" ") + 1, Message.length() - Message.find_first_of(" ") + 1);

					int result = 0;

					result = mutePlayer(userId, Username, 1);

					printf("MUTE result is: %i\n", result);
					if (result == 0)
					{
						//find the sock of the username
						for (int i = 0; i < MAX_CLIENTS; i++)
						{
							//well, unmute the person
							std::string lower_username = lower(Username);
							std::string lower_nick = lower(User[i].Nick);

							if (lower_username.compare(lower_nick) == 0)
								User[i].Mute = true;

							if (User[i].Ident)
								send(User[userId].Sock, CHAT, Username, " has been muted.");
						}
					}
					else if (result == 1)
					{
						send(User[userId].Sock, CHAT, Username, " does not exist.");
					}
					else if (result == 2)
					{
						send(User[userId].Sock, CHAT, Username, " cannot be muted,");
					}
					else if (result == 3)
					{
						printf("The user [%s] tried to mute [%s] but they are not moderator!\n", User[userId].Nick.c_str(), Username.c_str());
						send(User[userId].Sock, CHAT, "You are not authorized to mute players.");
					}

				} //end "/mute"
				else if (command.compare("/unmute") == 0)
				{
					// Declare message string
					std::string Username = "";
					std::string Message = "";
					//fill message with username and reason
					Message += User[userId].Sock->Data.substr(User[userId].Sock->Data.find_first_of(" ") + 1, pack_len - User[userId].Sock->Data.find_first_of(" ") + 1);

					//strip the appropriate data
					Username += Message.substr(Message.find_first_of(" ") + 1, pack_len - Message.find_first_of(" ") + 1);

					int result = 0;

					result = mutePlayer(userId, Username, 0);

					if (result == 0)
					{
						//find the sock of the username
						for (int i = 0; i < MAX_CLIENTS; i++)
						{
							//well, unmute the person
							if (lower(User[i].Nick).compare(lower(Username)) == 0)
								User[i].Mute = false;

							//well, tell everyone
							// If this socket is taken
							if (User[i].Ident)
								send(User[userId].Sock, CHAT, Username, " has been unmuted.");
						}
					}
					else if (result == 1)
					{
						send(User[userId].Sock, CHAT, Username, " does not exist.");
					}
					else if (result == 2)
					{
						send(User[userId].Sock, CHAT, Username, " cannot be muted.");
					}
					else if (result == 3)
					{
						printf("The user [%s] tried to unmute [%s] but they are not moderator!\n", User[userId].Nick.c_str(), Username.c_str());
						send(User[userId].Sock, CHAT, "You are not authorized to mute players.");
					}

				} //end "/unmute"
				else if (command.compare("/kick") == 0)
				{
					// Declare message string
					std::string Message = "";
					std::string Username = "";
					std::string Reason = "";

					//fill message with username and reason
					Message += User[userId].Sock->Data.substr(User[userId].Sock->Data.find_first_of(" ") + 1, pack_len - User[userId].Sock->Data.find_first_of(" ") + 1);

					//strip the appropriate data
					Username += Message.substr(0, Message.find_first_of(" "));
					Reason += Message.substr(Message.find_first_of(" ") + 1, Message.length() - Message.find_first_of(" ") + 1);

					int result = 0;

					result = kickPlayer(userId, Username);

					if (result == 0)
					{
						//okay, now send
						for (int i = 0; i < MAX_CLIENTS; i++)
						{
							//if socket is taken
							if (User[i].Ident)
								send(User[userId].Sock, CHAT, Username, " has been kicked. (", Reason, ")");

							//kick the user
							if (lower(Username).compare(lower(User[i].Nick)) == 0)
							{
								User[i].Sock->Close();
								User[i].Sock->Connected = false;
							}
						}
					}
					else if (result == 1)
					{
						send(User[userId].Sock, CHAT, Username, " is not online.");
					}
					else if (result == 2)
					{
						printf("The user [%s] tried to kick [%s] but they are not moderator!\n", User[userId].Nick.c_str(), Username.c_str());
						send(User[userId].Sock, CHAT, "You are not authorized to kick players.");
					}

				} //end "/kick"
				else if (command.compare("/who") == 0)
				{
					//find how many players are online
					//see how many people are online
					int players = 0;
					bool flag = false;
					std::string strNicks = "";

					for (int i = 0; i < MAX_CLIENTS; i++)
					{
						//if socket is taken
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

					char strPlayers[127];
					if (players == 1)
						sprintf(strPlayers, "There is %i player online: ", players);
					else
						sprintf(strPlayers, "There are %i players online: ", players);
					send(User[userId].Sock, CHAT, strPlayers);

					if (players > 0)
						send(User[userId].Sock, CHAT, strNicks);

				} //end /who
				else if (command.compare("/ipban") == 0)
				{
					// Declare message string
					std::string Message = "";
					std::string IP = "";
					std::string Reason = "";

					//fill message with username and reason
					Message += User[userId].Sock->Data.substr(User[userId].Sock->Data.find_first_of(" ") + 1, pack_len - User[userId].Sock->Data.find_first_of(" ") + 1);

					//strip the appropriate data
					IP += Message.substr(0, Message.find_first_of(" "));
					Reason += Message.substr(Message.find_first_of(" ") + 1, Message.length() - Message.find_first_of(" ") + 1);

					int result = banIP(userId, IP, Reason);

					if (result == 0)
					{
						send(User[userId].Sock, CHAT, "[", IP, "] has been banned (", Reason, ")");
					}
					else if (result == 1)
					{
						send(User[userId].Sock, CHAT, "Could not ban IP [", IP, "] for unknown error.");
					}

				} //end /ipban
				else if (command.compare("/getip") == 0)
				{
					// Declare message string
					std::string Username = "";
					std::string Message = "";

					//fill message with username and reason
					Message += User[userId].Sock->Data.substr(User[userId].Sock->Data.find_first_of(" ") + 1, pack_len - User[userId].Sock->Data.find_first_of(" ") + 1);

					//strip the appropriate data
					Username += Message.substr(Message.find_first_of(" ") + 1, pack_len - Message.find_first_of(" ") + 1);

					//if they are moderator
					if (User[userId].Moderator)
					{

						//get noob id
						std::string sql = "SELECT * FROM player WHERE username like '";
						sql += database->clean(Username);
						sql += "'";
						printf(sql.c_str());
						database->query(sql);

						if (database->count())
						{
							//get noob id
							database->nextRow();
							std::string noob_id = database->getString(0);

							//get IP
							std::string IP = "";
							sql = "SELECT * FROM ip_log WHERE player_id like '";
							sql += database->clean(noob_id);
							sql += "' ORDER BY idip_log DESC"; //latest ip logged
							printf(sql.c_str());
							database->query(sql);

							if (database->count())
							{
								//get noob IP
								database->nextRow();
								IP = database->getString(2);
							}

							// Send data
							send(User[userId].Sock, CHAT, Username, "has an IP of [", IP, "]");
						}
					}
					//else not moderator :(

				} //end if /getip
				else if (command.compare("/warp") == 0)
				{
					if (User[userId].Moderator)
					{
						printf("warp...\n");
						std::string rip = User[userId].Sock->Data;

						//get parameters first
						rip = rip.substr(rip.find_first_of(" ") + 1);
						printf("rip is %s\n", rip.c_str());

						//username
						std::string warp_user = rip.substr(0, rip.find_first_of(" "));
						rip = rip.substr(rip.find_first_of(" ") + 1);
						printf("rip is %s\n", rip.c_str());

						//X
						int warp_x = atoi(rip.substr(0, rip.find_first_of(" ")).c_str());
						rip = rip.substr(rip.find_first_of(" ") + 1);
						printf("rip is %s\n", rip.c_str());

						//Y
						int warp_y = atoi(rip.substr(0, rip.find_first_of(" ")).c_str());
						rip = rip.substr(rip.find_first_of(" ") + 1);
						printf("rip is %s\n", rip.c_str());

						//MAP
						int warp_m = atoi(rip.substr(0, rip.find_first_of(" ")).c_str());

						printf("Warp %s to (%i,%i) on map [%i]\n", warp_user.c_str(), warp_x, warp_y, warp_m);

						//find user
						for (int wu = 0; warp_m >= NUM_MAPS && wu < MAX_CLIENTS; wu++)
						{
							if (User[wu].Ident && (lower(User[wu].Nick).compare(lower(warp_user)) == 0))
							{
								SKO_Portal warp_p = SKO_Portal();
								warp_p.spawn_x = warp_x;
								warp_p.spawn_y = warp_y;
								warp_p.map = warp_m;

								Warp(wu, warp_p);
								break;
							}
						}
					}
					else 
					{
						send(User[userId].Sock, "You are not authorized to warp players.");
					}
				}
				else if (command.compare("/ping") == 0)
				{
					// Declare message string
					std::string Username = "";

					//if they are moderator
					if (User[userId].Moderator)
					{
						//find username
						Username += User[userId].Sock->Data.substr(User[userId].Sock->Data.find_first_of(" ") + 1, pack_len - User[userId].Sock->Data.find_first_of(" ") + 1);

						int datUser;
						bool result = false;

						for (datUser = 0; datUser < MAX_CLIENTS; datUser++)
						{
							if (User[datUser].Ident && User[datUser].Nick.compare(Username) == 0)
							{
								printf("Moderator inquiry of %s\n", Username.c_str());
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
							ss << "User[" << datUser << "] " << User[datUser].Nick << " has a ping of " << User[datUser].ping;
							send(User[userId].Sock, CHAT, ss.str());
						}
						else
						{
							send(User[userId].Sock, CHAT, "Username ", Username, " was not found.");
						}
					}

				} //end if /ping
				else if (command[0] == '/')
				{
					printf("not a command...\n");
					send(User[userId].Sock, "Sorry, that is not a command.");
				}
				else //just chat
				{
					//wrap long text messages
					int max = 60 - User[userId].Nick.length();

					// Add nick to the send
					std::string chatMessage = "";

					if (User[userId].Nick.compare("Paladin") != 0)
					{
						chatMessage += User[userId].Nick;
						chatMessage += ": ";
					}
					else
						max = 52;

					std::string chatFull = User[userId].Sock->Data.substr(2, pack_len);
					std::string chatChunk = "";

					bool doneWrapping = false;
					if (chatFull.length() < max)
					{
						for (int i = 0; i < MAX_CLIENTS; i++)
						{
							if (User[i].Ident) 
								send(User[i].Sock, CHAT, chatMessage, chatFull);
						}
						doneWrapping = true;
					}

					int nickLength = 0;
					if (User[userId].Nick.compare("Paladin") == 0)
					{
						if (!doneWrapping && chatFull.find(": ") != std::string::npos)
						{
							nickLength += chatFull.find(": ");
							nickLength += 2; //colon and space
							max -= nickLength;
						}
					} //end paladin
					else
					{
						nickLength = User[userId].Nick.length();
					}

					while (!doneWrapping)
					{
						std::string cPacket = "";

						//add spaces
						for (int c = 0; c < nickLength; c++)
							cPacket += " ";

						//TODO remove hard-coded "Paladin" references
						if (User[userId].Nick.compare("Paladin") != 0)
							cPacket += "  ";

						if (chatChunk.length() > max)
						{
							//add message
							cPacket += chatChunk.substr(0, max);
							chatChunk = chatChunk.substr(max);
						}
						else
						{
							cPacket += chatChunk;
							doneWrapping = true;
						}
						cPacket[0] = cPacket.length();

						for (int i = 0; i < MAX_CLIENTS; i++)
						{
							if (User[i].Ident) 
								send(User[i].Sock, CHAT, cPacket);
						}						
					}
					printf("\e[0;33m[Chat]%s: %s\e[m\n", User[userId].Nick.c_str(), chatFull.c_str());
				} //end just chat
			}	 //end if not muted
		}		  //end chat
		else
		{
			//the user is hacking, or sent a bad packet
			User[userId].Sock->Close();
			User[userId].Sock->Connected = false;

			printf(kRed "(%s) ERR_BAD_PACKET_WAS: ", User[userId].Nick.c_str());
			for (int l = 0; l < User[userId].Sock->Data.length(); l++)
				printf("[%i]", (int)User[userId].Sock->Data[l]);
			
			printf("\n" kNormal);

			User[userId].Sock->Data = "";
			return;
		} //end error packet

		//   printf("X: %i\n", User[userId].Sock->GetStatus());
		//put the extra data (if any) into data
		User[userId].Sock->Data = newPacket;

	} //end execute packet
};
