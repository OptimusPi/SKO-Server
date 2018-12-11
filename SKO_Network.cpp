#include "SKO_Network.h"
#include "SKO_Player.h"
#include "SKO_PacketTypes.h"
#include "Global.h"
#include "OPI_Clock.h"
#include "GE_Socket.h"

SKO_Network::SKO_Network(OPI_MYSQL * database, int port, unsigned long int saveRateSeconds)
{
    // Bind this port to accept connections
    this->port = port;

	// Lock save to one call at a time
	sem_init(&this->saveMutex, 0, 1);

	// Properties passed into threads
	this->threadDTO.saveRateSeconds = saveRateSeconds;
    this->threadDTO.database = database;
    this->threadDTO.listenSocket = new GE_Socket();
}

//Initialize threads and Start listening for connections
std::string SKO_Network::Startup()
{
    // Bind the given port
	if (!this->threadDTO.listenSocket->Create(port)) 
        return "Failed to bind to port " + std::to_string(port);
	
	// Start listening for incoming connections
	if (!this->threadDTO.listenSocket->Listen())
        return "Failed to listen on port " + std::to_string(port);

    //start thread that listens for correct version packet
    if (pthread_create(&queueThread, NULL, QueueLoop, (void *)&this->threadDTO))
        return "Could not create thread for QueueLoop...";

    //start thread that listens for new connections
    if (pthread_create(&connectThread, NULL, ConnectLoop, (void *)&this->threadDTO))
        return "Could not create thread for ConnectLoop...";

	//start thread that periodically saves all players to the database
	if (pthread_create(&autoSaveThread, NULL, SaveLoop, (void *)&this->threadDTO))
        return "Could not create thread for SaveLoop...";
      
	printf(kGreen "[+] listening for connections on port [%i]\n" kNormal, port);
    this->threadDTO.listenSocket->Connected = true;
    return "success";
}

void SKO_Network::Cleanup()
{
    //TODO delete sockets etc
}

void SKO_Network::saveProfile(OPI_MYSQL *database, unsigned int CurrSock)
{   
	if (User[CurrSock].Nick.length() == 0)
	{
		printf("I'm not saving someone without a username.\n");
		printf("Please investigate why this happened!\n");
		printf("User[%i].ID is: %i\n", CurrSock, User[CurrSock].ID);
		return;
	}

	std::string player_id = User[CurrSock].ID;
	//save inventory
	std::ostringstream sql;
	sql << "UPDATE inventory SET";
	for (int itm = 0; itm < NUM_ITEMS; itm++)
	{
		sql << " ITEM_";
		sql << itm;
		sql << "=";
		sql << (unsigned int)User[CurrSock].inventory[itm];
		
		if (itm+1 < NUM_ITEMS)
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
		sql << (unsigned int)User[CurrSock].bank[itm];
		if (itm+1 < NUM_ITEMS)
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
    sql << (int)User[CurrSock].level;
    sql << ", x=";
    sql << User[CurrSock].x;
    sql << ", y=";
    sql << User[CurrSock].y;
    sql << ", xp=";
    sql << User[CurrSock].xp;
    sql << ", hp=";
    sql << (int)User[CurrSock].hp;  
    sql << ", str=";
    sql << (int)User[CurrSock].strength; 
    sql << ", def=";
    sql << (int)User[CurrSock].defence;
    sql << ", xp_max=";
    sql << User[CurrSock].max_xp;
    sql << ", hp_max="; 
    sql << (int)User[CurrSock].max_hp;
    sql << ", y_speed=";
    sql << User[CurrSock].y_speed;
    sql << ", x_speed=";
    sql << User[CurrSock].x_speed;
    sql << ", stat_points=";
    sql << (int)User[CurrSock].stat_points;
    sql << ", regen=";
    sql << (int)User[CurrSock].regen;
    sql << ", facing_right=";
    sql << (int)User[CurrSock].facing_right;
    sql << ", EQUIP_0=";
    sql << (int)User[CurrSock].equip[0];
    sql << ", EQUIP_1=";
    sql << (int)User[CurrSock].equip[1];
	sql << ", EQUIP_2=";
    sql << (int)User[CurrSock].equip[2];
    
    
    //operating system
    sql << ", VERSION_OS=";
    sql << (int)User[CurrSock].OS;
    
    //time played
    unsigned long int total_minutes_played = User[CurrSock].minutesPlayed;
    double this_session_milli = (Clock() - User[CurrSock].loginTime);
    //add the milliseconds to total time
    total_minutes_played += (unsigned long int)(this_session_milli/1000.0/60.0);
    
    sql << ", minutes_played=";
    sql << (int)total_minutes_played;
   
     
    //what map are they on?
    sql << ", current_map=";
    sql << (int)User[CurrSock].current_map;

    sql << ", inventory_order='";
    sql << database->clean(base64_encode(User[CurrSock].getInventoryOrder()));

    sql << "' WHERE id=";
    sql << database->clean(player_id);
    sql << ";";
    
    database->query(sql.str());       
    sql.str("");
	sql.clear();

    printf(database->getError().c_str());
}


void SKO_Network::saveAllProfiles(OPI_MYSQL *database)
{
    printf("SAVE ALL PROFILES \n");
    sem_wait(&this->saveMutex);

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
            saveProfile(database, i);
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
    	statusQuery <<  "');";

    	database->query(statusQuery.str(), false);
    }

    if (numPlayers)
		this->pauseSavingStatus = false;
    else
		this->pauseSavingStatus = true;
     
    sem_post(&this->saveMutex);
}



void* SKO_Network::SaveLoop(void *threadDTO)
{
    // Thread data transfer object is a singler function parameter
    // This holds more than one thing we can use
    ThreadDTO *dto = (ThreadDTO *)threadDTO;
    OPI_MYSQL *database = dto->database;
	unsigned long int saveRateSeconds = dto->saveRateSeconds;


}

void* SKO_Network::QueueLoop(void *threadDTO)
{
    // Thread data transfer object is a singler function parameter
    // This holds more than one thing we can use
    ThreadDTO *dto = (ThreadDTO *)threadDTO;
    GE_Socket *socket = (GE_Socket*)dto->listenSocket;
    OPI_MYSQL *database = (OPI_MYSQL*)dto->database;

     while (!SERVER_QUIT)
     {
        // Cycle through all connections
		for( int CurrSock = 0; CurrSock < MAX_CLIENTS; CurrSock++ )
		{
            //check Que
            if (!User[CurrSock].Que)
				continue;

			//receive
			if (User[CurrSock].Sock->Recv() & GE_Socket_OK)
			{
				printf("A client is trying to connect...\n");
				printf("Data.length() = [%i]\n", (int)User[CurrSock].Sock->Data.length());

				//if you got anything                            
				if (User[CurrSock].Sock->Data.length() >= 6)
				{
					//if the packet code was VERSION_CHECK
					if (User[CurrSock].Sock->Data[1] == VERSION_CHECK)
					{
						printf("User[CurrSock].Sock->Data[1] == VERSION_CHECK\n");                      
						if (User[CurrSock].Sock->Data[2] == VERSION_MAJOR && User[CurrSock].Sock->Data[3] == VERSION_MINOR && User[CurrSock].Sock->Data[4] == VERSION_PATCH)                      
						{
							printf("Correct version!\n");
							std::string packet = "0";
							packet += VERSION_SUCCESS;
							packet[0] = packet.length();
							User[CurrSock].Sock->Data = "";
							User[CurrSock].Status = true;
							User[CurrSock].Que = false;
							User[CurrSock].SendPacket(packet);
							printf("Que Time: \t%ul\n", User[CurrSock].QueTime);
							printf("Current Time: \t%ul\n", Clock());
							
							//operating system statistics
							User[CurrSock].OS = User[CurrSock].Sock->Data[5];
							}
							else //not correct version
							{ 
								std::string packet = "0";
								packet += VERSION_FAIL;
								packet[0] = packet.length();
								User[CurrSock].SendPacket(packet);
								printf ("error, packet code failed on VERSION_CHECK see look:\n");
				printf(">>>[read values] VERSION_MAJOR: %i VERSION_MINOR: %i VERSION_PATCH: %i\n", 
					User[CurrSock].Sock->Data[2], User[CurrSock].Sock->Data[3], User[CurrSock].Sock->Data[4]);
				printf(">>>[expected values] VERSION_MAJOR: %i VERSION_MINOR: %i VERSION_PATCH: %i\n",	
					VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
								User[CurrSock].Que = false;
								User[CurrSock].Sock->Close();
							}
					}
					else //not correct packet type...
					{
								printf("Here is the back packet! ");
								
								for (int i = 0; i < User[CurrSock].Sock->Data.length(); i++)
								{
									printf("[%i]", User[CurrSock].Sock->Data[i]);
								}
								printf("\n");
								
								std::string packet = "0";
								packet += VERSION_FAIL;
								packet[0] = packet.length();
								User[CurrSock].SendPacket(packet);
								printf ("error, packet code failed on VERSION_CHECK (2)\n");
								User[CurrSock].Que = false;
								User[CurrSock].Sock->Close();
					}
				}
					
			}
			else  // Recv returned error!
			{ 
					User[CurrSock].Que = false;
					User[CurrSock].Status = false;
					User[CurrSock].Ident = false;
					User[CurrSock].Sock->Close();
					User[CurrSock] = SKO_Player();
					printf("*\n**\n*\nQUE FAIL! (Recv returned error) IP IS %s*\n**\n*\n\n", User[CurrSock].Sock->IP.c_str());
			}
			
			//didn't recv anything, don't kill unless it's too long
			if (Clock() - User[CurrSock].QueTime >= 500)
			{
				User[CurrSock].Que = false;
				User[CurrSock].Sock->Close();
				printf("Closing socket %i for timeout.\n", CurrSock);
				printf("*\n**\n*\nQUE FAIL! IP IS %s*\n**\n*\n\n", User[CurrSock].Sock->IP.c_str());
			} 
        } //end for loop
         
        //checking que loop 2 times per second is plenty fast
        Sleep(500);
    } //end while loop
}//end QueLoop

void* SKO_Network::ConnectLoop(void *threadDTO)
{
    // Thread data transfer object is a singler function parameter
    // This holds more than one thing we can use
     ThreadDTO *dto = (ThreadDTO *)threadDTO;
     GE_Socket *socket = (GE_Socket*)dto->listenSocket;
     OPI_MYSQL *database = (OPI_MYSQL*)dto->database;

     while (!SERVER_QUIT)
     {     
		//check for disconnects by too high of ping.
		for (int i = 0 ; i < MAX_CLIENTS; i++)
		{
			if (!User[i].Ident)
                continue;

            if (User[i].pingWaiting)
            {
                int tempPing = Clock() - User[i].pingTicker;
            
                if (tempPing > 60000)
                {
                    printf("\e[31;0mClosing socket based on ping greater than one minute.\e[m\n");
                    User[i].Sock->Close();
                }
            }//end pingWaiting
            else
            {
                //send ping waiting again if its been more than a second
                if (Clock() - User[i].pingTicker > 1000)
                {
                    //time to ping them.
                    std::string pingPacket = "0";
                    pingPacket += PING;
                    pingPacket[0] = pingPacket.length();
                    User[i].SendPacket(pingPacket);
                    User[i].pingWaiting = true;
                    User[i].pingTicker = Clock();
                    
                }
            }
		}//end max clients for ping

		// If there is someone trying to connect
		if ( socket->GetStatus2() & (int)GE_Socket_Read )
		{


			// Declare a temp int
			int incomingSocket;
			
			// Increase i until it finds an empty socket
			for ( incomingSocket = 0;  incomingSocket < MAX_CLIENTS; incomingSocket++ ) 
			{
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
		if (incomingSocket >= MAX_CLIENTS)
		{
			GE_Socket * tempSock = socket->Accept();
			std::string fullPacket = "0"; 
			fullPacket += SERVER_FULL;
			fullPacket[0] = fullPacket.length(); 
			tempSock->Send(fullPacket);
			tempSock->Close();	
		}
		else
		{
			printf("incoming socket is: %i\n", incomingSocket);
			printf("> User[incomingSocket].Sock->Connected == %s\n", User[incomingSocket].Sock->Connected ? "True" : "False");
			printf("> User[incomingSocket].Save == %i\n", (int)User[incomingSocket].Save);
			printf("> User[incomingSocket].Status == %i\n", (int)User[incomingSocket].Status);
			printf("> User[incomingSocket].Ident == %i\n", (int)User[incomingSocket].Ident);

			//make them mute and such
			User[incomingSocket] = SKO_Player();
			User[incomingSocket].Sock = socket->Accept();
					
				//error reporting
				if ( User[incomingSocket].Sock->Socket == 0 )
					printf("Sock[%i] INVALID_SOCKET\n", incomingSocket);
				else
				// Set the status of that socket to taken
					User[incomingSocket].Sock->Connected = true;
			   
					//set the data counting clock
					User[incomingSocket].Sock->stream_ticker = Clock(); 
					
					//set the data_stream to null
					User[incomingSocket].Sock->byte_counter = 0;
					
					//set bandwidth to null
					User[incomingSocket].Sock->bandwidth = 0;
					
				// Output that a client connected
				printf("[ !!! ] Client %i Connected\n", incomingSocket);
				
				//display their i.p.
				/***/
					std::string their_ip = User[incomingSocket].Sock->Get_IP();
					/***/
					
					
					std::string sql = "SELECT * FROM ip_ban WHERE ip like '";
			printf("database->clean(%s)\n", their_ip.c_str());
					sql += database->clean(their_ip);
					sql += "'";
			printf("database->query(%s)\n", sql.c_str());
					database->query(sql);
			
			printf("if database->count()\n");
					
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
			}//server not full
		}//if connection incoming
      
		//checking for incoming connections 3 times per second is plenty fast.
		Sleep(300);
    }//end while
}
