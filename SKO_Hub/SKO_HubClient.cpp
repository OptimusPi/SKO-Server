#include "SKO_HubClient.h"
#include <sstream>
#include "../OPI_Utilities/OPI_Crypto.h"
#include "../OPI_Utilities/base64.h"

SKO_HubClient::SKO_HubClient(std::string clientId, std::string apiUrl, std::string apiPort, std::string apiKey, std::string aesKeyHex)
{
    // Connection details
    this->clientId = clientId;
    this->apiUrl = apiUrl;
    this->apiPort = apiPort;
    this->apiKey = apiKey;
    this->crypto = new OPI_Crypto(aesKeyHex);
    aesKeyHex.clear();
    
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

// "{nonce64:data64}"
bool SKO_HubClient::IsValidInnerMessage(std::string packet)
{
    // don't bother seaarching if packet is too short.
    if (packet.length() < 3)
        return false;

    int bracket1 = packet.find("{");
    if (bracket1 < 0)
        return false;

    int colon = packet.find(":");
    if (colon < bracket1)
        return false;

    int bracket2 = packet.find("}");
    if (bracket2 < colon)
        return false;

    return true;
}

// "+actions:{nonce64:data64}"
// "?queries:{nonce64:data64}"
bool SKO_HubClient::IsValidPacket(std::string packet)
{
    // don't bother searching if packet is too short.
    if (packet.length() < 8)
    {
        return false;
    }

    // every packet needs to start with '?' or '+'
    if (packet[0] != '?' && packet[0] != '+')
    {
        perror("not a command or query!");
        return false;
    }

    // every packet needs this separator after the key
    int colon = packet.find(":");
    if (!colon)
    {
        perror("no separator found!");
        return false;
    }

    int bracket1 = packet.find("{");
    if (bracket1 < colon)
    {
        perror("no left bracket found!");
        return false;
    }

    int bracket2 = packet.find("}");
    if (bracket2 < bracket1)
    {
        perror("no right bracket found!");
        return false;
    }

    return true;
}

void SKO_HubClient::Process()
{   
    unsigned long long pingTime = OPI_Clock::milliseconds();
    unsigned long long ping = 0;
    unsigned long long nextPingTime = 0;
    bool registered = false;

    while (!this->stop)
    {
        if (this->Receive())
        {
            perror("Receive()");
            this->stop = true;
        }

        if (!this->IsValidPacket(this->data))
        {
            if (!this->data.empty())
            {
                std::string error = "WARNING: continuing due to incomplete packet: ["
                + this->data + "]";
                perror(error.c_str());
            }
            continue;
        }
 
        std::string packet = this->data.substr(0, this->data.find("}") + 1);
        this->Chop(packet.length());

        std::string tag = packet.substr(0, packet.find(":"));
        auto start_index = packet.find("{") + 1;
        auto length = packet.find("}") - start_index;

        std::string encrypted_blob = packet.substr(start_index, length);
        std::string message = this->crypto->Decrypt(encrypted_blob);

        auto nonce_index = message.find(":");
        std::string nonce64 = message.substr(0, nonce_index);
        std::string content64 = message.substr(nonce_index + 1);
        std::string content = base64_decode(content64);
        

        // SKO Hub is sending a query 
        if (tag[0] == '?')
        {
            if (tag == "?clientId")
            {
                this->Send_Secure("+clientId", this->clientId, nonce64);
            }
            else if (tag == "?apiKey")
            {
                this->Send_Secure("+apiKey", this->apiKey, nonce64);
            }
            else if (tag == "?ping")
            {
                this->Send("+pong");
            }
            else 
            {
                printf("Unknown message! = [%s]", packet.c_str());
            }
        }

        // SKO Hub is sending a response 
        if (packet[0] == '+')
        {
            if (packet.substr(0, 11) == "+registered")
            {
                registered = true;
                printf("+++++ registered!\r\n");
            }
            else if (packet.substr(0, 5) == "+pong")
            {
                ping = OPI_Clock::milliseconds() - pingTime;
                printf("ping: %lld\r\n", ping);
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


bool SKO_HubClient::Send_Secure(std::string tag, std::string data, std::string auth)
{
    std::string message_pair = "{" + auth + ":" + data + "}";
	return this->Send_Secure(tag, message_pair); 
}

bool SKO_HubClient::Send_Secure(std::string tag, std::string data)
{
    std::string encrypted_data = this->crypto->Encrypt(data);
    std::string full_packet = tag + ":{" + encrypted_data + "}";

	return this->Send(full_packet); 
}

bool SKO_HubClient::Send(std::string in_Data)
{
	if (send(this->Socket, in_Data.c_str(), in_Data.length(), 0) == 0 ) 
	{ 
		perror("SKO_HubClient::Send\n"); 
		return true; 
	}  

    printf("SKO_HubClient::Send(%s)\r\n", in_Data.c_str());   

	return false; 
}

void SKO_HubClient::Chop(int size)
{
    this->data = this->data.substr(size);
}

bool SKO_HubClient::Receive()
{
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