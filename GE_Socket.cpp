#include "GE_Socket.h" 

GE_Socket::~GE_Socket()
{		
	printf("~GE_Socket()\n");
	if (Socket) { 
		printf (">     close (Socket);\n");
		close (Socket);	
	} else {
		printf(">     Nothing to clean up.\n");
	}
}
 
GE_Socket::GE_Socket() 
{ 
 
    Data = ""; 
    Connected = false; 
    IP = ""; 
    byte_counter = 0;//counts data in bytes 
    bandwidth = 0;//bytes per second 
    micro_seconds = 0;//counter 
    stream_ticker = 0;   
    timeout.tv_sec = 0; 
    timeout.tv_usec = 0;    
    sendTime = 0; 
    Socket = NULL;
} 
 
void GE_Socket::Stream() 
{ 
    //this function prevents DoS attacks 
    //this tracks the bandwith usage of each client 
     
    //get the time 
    float seconds = (Clock() - stream_ticker)  /  1000.0;  
     
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
       stream_ticker = Clock(); 
        
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
 
    int i = select(FD_SETSIZE,&rset,&wset,&eset,(struct timeval*)&timeout); 
     
      
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
    if( FD_ISSET(Socket,&wset) ){  
        Status |= GE_Socket_Write; 
    } 
    else if( FD_ISSET(Socket,&rset) ){ 
         Status |= GE_Socket_Read; 
    } 
     
    return Status; 
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
 
    //printf("Time a: %i\n", Clock()); 
    int i = select(FD_SETSIZE,&rset,&wset,&eset,(struct timeval*)&timeout); 
    //printf("Time b: %i\n", Clock()); 
     
    if (i == -1){ 
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
    if( FD_ISSET(Socket,&wset) ){  
        Status |= GE_Socket_Write; 
    } 
    else if( FD_ISSET(Socket,&rset) ){ 
         Status |= GE_Socket_Read; 
    } 
     
    return Status; 
} 
 
 
bool GE_Socket::Create(unsigned int Port){ 
   
    
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
 
GE_Socket* GE_Socket::Accept(){ 
    int client; 
    socklen_t length; 
 
    length = sizeof sin; 
    client = accept( Socket, (struct sockaddr*)&sin, &length ); 
     
    //disable throttling/Nagle Algorithm 
     int flag = 1; 
     int nresult = setsockopt(client,            /* socket affected */ 
                                 IPPROTO_TCP,     /* set option at TCP level */ 
                                 TCP_NODELAY,     /* name of option */ 
                                 (char *) &flag,  /* the cast is historical 
                                                         cruft */ 
                                 sizeof(int));    /* length of option value */ 
     if (nresult < 0){ 
        printf("ERROR cant set TCP_NODELAY or some shiz...\n"); 
     } 
      
    if ( client == 0 ) 
    { 
        perror("[ ERR ] invalid socket??"); 
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
    Data = "";
    Connected = false; 
    if (Socket > 0)
close(Socket);
    //crashes the server xD 
    //WSACleanup(); //Clean up Winsock 
} 
 
int GE_Socket::Send(std::string in_Data) 
{ 
        if( send(Socket, in_Data.c_str(), in_Data.length(), 0) == 0 ) 
        { 
            perror("SEND() FAILED!!!\n"); 
            printf("send_Data [%s] send_Data.length [%i]\n", in_Data.c_str(), (int)in_Data.length()); 
            printf("Packet code was %i\n", in_Data[1]); 
             
            printf("getStatus returns this: [%i]\n", GetStatus()); 
            Close(); 
             
            return (int)GE_Socket_Error; 
        }     
     
         
         
        return (int)GE_Socket_OK; 
} 
 
 
int GE_Socket::Recv(){ 
     
    int timea = Clock(); 
     
    fd_set sset; 
     
    FD_ZERO(&sset);  
    FD_SET((unsigned int)Socket,&sset);  
     
 
    select(FD_SETSIZE,&sset,NULL,NULL,(struct timeval*)&timeout); 
 
 
    if (FD_ISSET(Socket,&sset)) { 
     
        int timeb = Clock();     
        iResult = recv (Socket,recvbuf,DEFAULT_BUFLEN,0); 
        int timec = Clock(); 
        int timed = 0; 
        if (iResult > 0)  
        { 
           Data.append(recvbuf, iResult); 
           timed = Clock(); 
        } 
        else if (iResult == 0 ) 
        { 
            perror("recv failed,\n[Connection has gracefully been closed]");
            Close(); 
            return 1;   
        } 
        else 
        { 
            printf("iResult is :%i\n", iResult); 
            perror("recv failed for an unknown reason"); 
            Close(); 
            return 1; 
	    printf("iABCD DISCONNECTED?! But Attempting to continue...\n");
        } 
         
        if ((int)(Clock() - timea) > 5 ) 
        { 
                printf("\7*\n GE_Socket::Recv() took a long time.\n It took %d milliseconds.\n",(unsigned int)( Clock() - timea));           
                printf("(recvbuf) packet type is: %i\n", recvbuf[0]); 
                printf("timea: %d", timea); 
                printf("timeb: %d", timeb); 
                printf("timec: %d", timec); 
                printf("timed: %d", timed);  
                printf("clock: %d", Clock());        
        } 
    }         
         
    return (int)GE_Socket_OK; 
} 

