#ifndef __SKO_HUBCLIENT_H_
#define __SKO_HUBCLIENT_H_

#include <stdio.h>
#include <stdlib.h>


#include <string>


#include <thread>
#include "../OPI_Utilities/OPI_Clock.h"
#include "../OPI_Utilities/OPI_Sleep.h"
#include <mutex>

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>

#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>
#include <arpa/inet.h> 



#include "../OPI_Utilities/OPI_Crypto.h"

#define MAX_BUFFER 10000

class SKO_HubClient 
{
 
public:
    SKO_HubClient(std::string clientId, std::string apiUrl, std::string apiPort, std::string apiKey, std::string aesKeyHex);
    ~SKO_HubClient();

    std::thread Start();
    bool Send(std::string in_Data);
    void Process();
    void Chop(int size);

private:
    // SKO Hub connection details
    std::string clientId;
    std::string apiUrl;
    std::string apiPort;
    std::string apiKey;

    // Thread for processing SKO Hub
    bool stop = false;

    // socket stuff
    std::string data;
    bool Receive();
    char recvbuf[MAX_BUFFER];
    int Socket;

    // Encryption provider for AES CBC mode with 256-bit HMAC
    OPI_Crypto* crypto;
};

#endif