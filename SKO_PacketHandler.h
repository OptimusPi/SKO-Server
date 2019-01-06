#ifndef __SKO_PACKET_HANDLER_H_
#define __SKO_PACKET_HANDLER_H_

#include "Global.h"

#include <cstring>
#include "SKO_Network.h"

class SKO_Network;

class SKO_PacketHandler
{

public:
    //SKO_PacketHandler(SKO_GameState gameState);
    SKO_PacketHandler(SKO_Network *network, SKO_Repository *repository);
    void parse(unsigned char userId, std::string packet);

private:
    //SKO_GameState gameState;
    SKO_Network *network;
    SKO_Repository *repository;
};
    
#endif