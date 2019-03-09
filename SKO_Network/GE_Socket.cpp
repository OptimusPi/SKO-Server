#include "GE_Socket.h" 

GE_Socket::~GE_Socket()
{		
	if (Socket)
		close (Socket);	
}
 
GE_Socket::GE_Socket() 
{ 
    data = ""; 
    Connected = false; 
    IP = ""; 
    byte_counter = 0;//counts data in bytes 
    bandwidth = 0;//bytes per second 
    stream_ticker = 0;   
    timeout.tv_sec = 0; 
    timeout.tv_usec = 0;    
    sendTime = 0; 
    Socket = 0;
} 
 
void GE_Socket::Stream() 
{ 
    //this function prevents DoS attacks 
    //this tracks the bandwith usage of each client 
     
    //get the time 
    float seconds = (OPI_Clock::milliseconds() - stream_ticker)  /  1000.0;  
     
    //update every second 
    if (seconds >= 1.0)
    {
       //if there was nothing, prevent buffer overflow 
       if (byte_counter < 1) 
           bandwidth = 0; 
                  
       //calculate and set the bandwidth usage 
       if (byte_counter > 0) 
          bandwidth = (unsigned int)((double)byte_counter / seconds); 

       //reset stream          
       byte_counter = 0; 
        
       //reset clock 
       stream_ticker = OPI_Clock::milliseconds(); 
    } 
} 
 
std::string GE_Socket::Get_IP() 
{ 
   int s = Socket; 
   struct sockaddr_in peer; 
   socklen_t peer_len; 
    
   //put the length in a variable. 
   peer_len = sizeof(peer); 
    
   //obtain the i.p. and port from the SOCKET 0=OK -1=ERROR  
   if (getpeername(s, (struct sockaddr*)&peer, &peer_len) == -1) 
      printf("getpeername() failed\n"); 
 
   IP = inet_ntoa(peer.sin_addr); 
   printf("Peer's IP address is: %s\n", IP.c_str()); 
    
   std::string ipStr = ""; 
   ipStr += IP.c_str(); 
    
   return ipStr; 
} 
 
int GE_Socket::GetStatus2() 
{ 
       
    fd_set rset; 
    fd_set wset; 
    fd_set eset; 
 
    FD_ZERO(&rset);  
    FD_ZERO(&wset);  
    FD_ZERO(&eset);  
    FD_SET((unsigned int)Socket,&rset);  
    FD_SET((unsigned int)Socket,&wset);  
    FD_SET((unsigned int)Socket,&eset); 
      
    select(FD_SETSIZE,&rset,&wset,&eset,(struct timeval*)&timeout); 

    int status = 0; 
    if( FD_ISSET(Socket,&eset) ) 
    { 
        status |= GE_Socket_Error; 
        return status; 
    } 
    else 
    { 
        status |= GE_Socket_OK; 
    } 
    
    if( FD_ISSET(Socket,&wset) )
	{  
        status |= GE_Socket_Write; 
    } 
    else if( FD_ISSET(Socket,&rset) ){
		
         status |= GE_Socket_Read; 
    } 
     
    return status; 
} 
 
int GE_Socket::GetStatus() 
{
    if( Connected == false ) { 
       printf("GetStatus() says Connected == false. Returning GE_SOCKET_ERROR\n"); 
       return GE_Socket_Error; 
    }  
    
    fd_set rset; 
    fd_set wset; 
    fd_set eset; 
 
    FD_ZERO(&rset);
    FD_ZERO(&wset);
    FD_ZERO(&eset);
    FD_SET((unsigned int)Socket,&rset);
    FD_SET((unsigned int)Socket,&wset);
    FD_SET((unsigned int)Socket,&eset);
 
    int i = select(FD_SETSIZE,&rset,&wset,&eset,(struct timeval*)&timeout); 
     
    if (i == -1)
    { 
       perror("In GetStatus(), select() returned error"); 
       Close(); 
       return GE_Socket_Error; 
    } 
      
    int Status = 0; 
    if( FD_ISSET(Socket,&eset) ) 
    { 
        Status |= GE_Socket_Error; 
        return Status; 
    } 
    else 
    { 
        Status |= GE_Socket_OK; 
    } 
    if( FD_ISSET(Socket,&wset) )
    {  
        Status |= GE_Socket_Write; 
    } 
    else if( FD_ISSET(Socket,&rset) )
    { 
         Status |= GE_Socket_Read; 
    } 
     
    return Status; 
} 
 
 
bool GE_Socket::Create(unsigned int Port)
{ 
    Socket = socket( AF_INET, SOCK_STREAM, 0 ); 
 
    sin.sin_family = AF_INET; 
    sin.sin_addr.s_addr = INADDR_ANY; 
    sin.sin_port = htons((int) Port );

	socklen_t len = sizeof sin;
 
   if (Socket < 0) {
	perror("listenSock < 0");
	return false;
   }	

    if ( bind( Socket, (struct sockaddr *)&sin, len ) < 0 ) 
    { 
        /* could not start server */ 
        perror("cant bind");
        return false; 
    } 
 
    return true; 
} 
 
bool GE_Socket::Listen() 
{ 
    if ( listen( Socket, MAX_CLIENTS ) ) 
    { 
        perror("[ ERR ] cant listen"); 
        return false; 
    } 
    return true; 
} 
 
GE_Socket* GE_Socket::Accept()
{ 
    int client; 
    socklen_t length; 
 
    length = sizeof sin; 
    client = accept( Socket, (struct sockaddr*)&sin, &length ); 
     
    //disable throttling/Nagle Algorithm 
     int flag = 1; 
     int nresult = setsockopt(client,            /* socket affected */ 
                                 IPPROTO_TCP,     /* set option at TCP level */ 
                                 TCP_NODELAY,     /* name of option */ 
                                 (char *) &flag,  /* the cast is historical cruft */ 
                                 sizeof(int));    /* length of option value */ 
    if (nresult < 0)
    { 
        printf("ERROR cant set TCP_NODELAY or other options.\n"); 
    } 
      
    if ( client == 0 ) 
    { 
        perror("[ ERR ] invalid socket when trying to accept connection."); 
        return NULL; 
    } 
	
    GE_Socket *nSock = new GE_Socket(); 
    nSock->sin = sin; 
    nSock->Socket = client; 
    return nSock; 
} 
 
void GE_Socket::Close() 
{ 
    //Close the socket if it exists 
    data = "";
    Connected = false; 
    if (Socket > 0)
		close(Socket);
} 
 
int GE_Socket::Send(std::string in_Data) 
{ 
	if( send(Socket, in_Data.c_str(), in_Data.length(), 0) == 0 ) 
	{ 
		perror("GE_Socket::Send() FAILED!\n"); 
		printf("send_Data [%s] send_Data.length [%i]\n", in_Data.c_str(), (int)in_Data.length()); 
		printf("Packet code was %i\n", in_Data[1]); 
		 
		printf("getStatus returns this: [%i]\n", GetStatus()); 
		Close(); 
		 
		return GE_Socket_Error; 
	}     
 
	return GE_Socket_OK; 
} 
 
 
int GE_Socket::Recv()
{ 
    fd_set sset; 
     
    FD_ZERO(&sset);  
    FD_SET((unsigned int)Socket,&sset);  
 
    select(FD_SETSIZE, &sset, NULL, NULL, (struct timeval*)&timeout);
 
    if (FD_ISSET(Socket,&sset)) 
    { 
        int iResult = recv (Socket, recvbuf, DEFAULT_BUFLEN, 0); 
        if (iResult > 0)  
        { 
           data.append(recvbuf, iResult); 
        } 
        else if (iResult == 0 ) 
        { 
            perror("recv failed,\n[Connection has gracefully been closed]");
            Close(); 
            return GE_Socket_Error;   
        } 
        else 
        { 
            perror("recv failed for an unexpected reason.");
            printf("recv error ID: %i\n", iResult); 
            Close(); 
            return GE_Socket_Error; 
        } 
    }         
         
    return GE_Socket_OK; 
} 

