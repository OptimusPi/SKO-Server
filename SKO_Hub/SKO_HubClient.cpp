#include "SKO_HubClient.h"
#include "../OPI_Utilities/OPI_Crypto.h"
#include <sstream>

SKO_HubClient::SKO_HubClient(std::string clientId, std::string apiUrl, std::string apiPort, std::string apiKey)
{
    // Connection details
    this->clientId = clientId;
    this->apiUrl = apiUrl;
    this->apiPort = apiPort;
    this->apiKey = apiKey;
    
    // Initialize receive data
    this->data = "";
}

SKO_HubClient::~SKO_HubClient()
{
    if (Socket) 
    {
        close(Socket);	
    }
};

std::thread SKO_HubClient::Start()
{
    // Initialize connection to the SKO Hub Api
    struct hostent *host;
    auto hostname = this->apiUrl.c_str();
    
    if ((host = gethostbyname(hostname)) == nullptr){
		perror("gethostbyname");
		exit(1);
	}

    struct addrinfo hints = {0}, *addrs;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    const int status = getaddrinfo(this->apiUrl.c_str(), this->apiPort.c_str(), &hints, &addrs);
    if (status != 0)
    {
        perror("getaddrinfo");
        exit(EXIT_FAILURE);
    }

    this->Socket = socket(addrs->ai_family, addrs->ai_socktype, addrs->ai_protocol);
    if (this->Socket == -1)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if (connect(this->Socket, addrs->ai_addr, addrs->ai_addrlen) == -1)
    {
		perror("connect");
		exit(EXIT_FAILURE);
	}
	memset(this->recvbuf, 0x15, sizeof(recvbuf));
	
    if (this->Send("+register"))
    {
        perror("Send(connect)");
        exit(EXIT_FAILURE);
    }

    // Process the live connection forever until exit
    std::thread processThread(&SKO_HubClient::Process, this);
    return processThread;
}

void SKO_HubClient::Process()
{   
    unsigned long long pingTime = OPI_Clock::milliseconds();
    unsigned long long  ping = 0;
    unsigned long long  nextPingTime = 0;
    bool registered = false;

    while (!this->stop)
    {
        if (this->Receive())
        {
            perror("Receive()");
            this->stop = true;
        }

        // SKO Hub is sending a query 
        if (this->data[0] == '?')
        {
            if (this->data.substr(0, 9) == "?clientId")
            {
                std::stringstream ss;
                ss << "+clientId ";
                ss << this->clientId;
                this->Send(ss.str());
                this->Chop(9);
            } 
            else if (this->data.substr(0, 7) == "?apiKey")
            {
                std::stringstream ss;
                ss << "+apiKey ";
                ss << this->apiKey;
                this->Send(ss.str());
                this->Chop(7);
            }
            else if (this->data.substr(0, 5) == "?ping")
            {
                this->Send("+pong");
                this->Chop(5);
            }
        }

        // SKO Hub is sending a response 
        if (this->data[0] == '+')
        {
            if (this->data.substr(0, 11) == "+registered")
            {
                registered = true;
                printf("+++++ registered!\r\n");
                this->Chop(11);
            }
            else if (this->data.substr(0, 5) == "+pong")
            {
                ping = OPI_Clock::milliseconds() - pingTime;
                printf("ping: %lld\r\n", ping);
                this->Chop(5);
            }
        }

        // Send ping every 10 seconds
        if (registered && OPI_Clock::milliseconds() >= nextPingTime)
        {
            printf("sending ?ping\r\n");
            this->Send("?ping");
            pingTime = OPI_Clock::milliseconds();
            nextPingTime = pingTime + 5000;
        } 
        
        OPI_Sleep::milliseconds(1);
    }

    printf("!!! STOPPING !!!\r\n");
}

bool SKO_HubClient::Send(std::string in_Data)
{
	if (send(this->Socket, in_Data.c_str(), in_Data.length(), 0) == 0 ) 
	{ 
		perror("SKO_HubClient::Send\n"); 
		return true; 
	}     
 
	return false; 
}

void SKO_HubClient::Chop(int size)
{
    this->data = this->data.substr(size);
}

bool SKO_HubClient::Receive()
{
    // TODO cut packets when multiple if necessary,
    // TODO right now it's set to only do one packet at a time.
    int recvbytes = 0;
	struct timeval timeout;
	timeout.tv_sec = 0; 
    timeout.tv_usec = 0;  
    fd_set sset;  
    FD_ZERO(&sset);  
    FD_SET((unsigned int)Socket,&sset);  

    // Only wait 0 seconds to receive
    select(FD_SETSIZE, &sset, NULL, NULL, (struct timeval*)&timeout);
 
    // Do not block on recv(), only read it if it's ready to go.
    if (FD_ISSET(Socket,&sset)) 
    { 
        if ((recvbytes = recv(this->Socket, this->recvbuf, MAX_BUFFER, 0)) == -1)
        {
            perror("recv");
            close(this->Socket);
            return true;
        }
    }

    //Save data that we received
    // TODO - packet splitting, command splitting
    this->data.append(this->recvbuf, recvbytes);

    if (recvbytes >= MAX_BUFFER) {
        return true;
    }
 
	return false; 
}