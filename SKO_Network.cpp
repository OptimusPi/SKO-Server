#include "SKO_Network.h"
#include "SKO_Player.h"
#include "SKO_PacketTypes.h"
#include "Global.h"
#include "OPI_Clock.h"
#include "GE_Socket.h"

SKO_Network::SKO_Network(OPI_MYSQL * database, int port)
{
    // Bind this port to accept connections
    this->port = port;
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
    if (pthread_create(&queThread, NULL, QueLoop, (void *)&this->threadDTO))
        return "Could not create thread for que...";

    //start thread that listens for new connections
    if (pthread_create(&connectThread, NULL, ConnectLoop, (void *)&this->threadDTO))
        return "Could not create thread for connect...";

	printf(kGreen "[+] listening for connections on port [%i]\n" kNormal, port);
    this->threadDTO.listenSocket->Connected = true;
    return "success";
}

void SKO_Network::Cleanup()
{
    //TODO delete sockets etc
}

void* SKO_Network::QueLoop(void *threadDTO)
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
