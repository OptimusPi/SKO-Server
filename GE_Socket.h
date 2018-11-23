#ifndef __GE_SOCKET_H_
#define __GE_SOCKET_H_


#include <string>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "OPI_Clock.h"
#include <unistd.h>
int close(int fd);

#define DEFAULT_BUFLEN 10000
#define MAX_CLIENTS 16

enum {
	GE_Socket_Error = 1,
	GE_Socket_OK = 2, // Binary values used for sake of AND/OR
	GE_Socket_Read = 4,
	GE_Socket_Write = 8
};

class GE_Socket{
      
private:
	int returnval;

	fd_set sset;
	struct timeval timeout;
        int temp;
        int sendTime;
	char recvbuf[DEFAULT_BUFLEN];
        int iResult, iSendResult;

    
	char TempBuf;
 public:
	int				Socket;
	std::string			Data;
	struct sockaddr_in	sin;
	bool				Connected;
	std::string         IP;
	int                 byte_counter;//counts data in bytes
	int                 bandwidth;//bytes per second
	unsigned long int   stream_ticker;

	~GE_Socket();
	GE_Socket();
	
    bool		OpenByIP(char* IP, unsigned int Port);
	bool		OpenByName(char* Name, unsigned int Port);
	bool		Create(unsigned int Port);
	int			GetStatus();
	int			GetStatus2();
	bool		Listen();
	GE_Socket*	Accept();
	void		Close();
	int			Send(std::string);
	int			Recv();
	int         Recv_with_time(int timeOut);
	std::string Get_IP();
	void        Stream();
};

#endif
