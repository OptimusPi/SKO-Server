#include "SKO_HubClient.h"

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
    // Run for 15 seconds to test
    // TODO - the rest of the fucking owl
    int i = 0;
    while (!this->stop)
    {
        printf("running...%i\r\n", i);
        OPI_Sleep::milliseconds(1);

        if (this->Receive())
        {
            perror("Receive()");
            this->stop = true;
        }

        // SKO Hub is sending a query 
        if (this->data[0] == "?")
        {
            if (this->data.substring(0, 8) == "+?apiKey")
            {

            }
        }

        

        if (++i > 15)
        {
            this->stop = true;
        }
    }
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

bool SKO_HubClient::Receive()
{
    int recvbytes = 0;
	if ((recvbytes = recv(this->Socket, this->recvbuf, MAX_BUFFER, 0)) == -1)
	{
		perror("recv");
		close(this->Socket);
		return true;
	}

    //Save data that we received
    // TODO - packet splitting, command splitting
    this->data.append(this->recvbuf, recvbytes);

    if (recvbytes >= MAX_BUFFER) {
        return true;
    }
 
	return false; 
}