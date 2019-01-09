#ifndef __SKO_PACKET_HANDLER_H_
#define __SKO_PACKET_HANDLER_H_

#include "Global.h"

#include <cstring>
#include "SKO_Network.h"
#include "SKO_PacketParser.h"
#include "SKO_PacketTypes.h"

class SKO_Network;

class SKO_PacketHandler
{

public:
    //SKO_PacketHandler(SKO_GameState gameState);
    SKO_PacketHandler(SKO_Network *network, SKO_Repository *repository);
    void parsePacket(unsigned char userId, std::string packet);
    
private:
    //SKO_GameState gameState;
    SKO_Network *network;
    SKO_Repository *repository;
    SKO_PacketParser *parser;

    // Health check packet types
    void parsePing(unsigned char userId);
    void parsePong(unsigned char userId);

    // Login and Register player account
    void parseLogin(unsigned char userId, std::string packetData);
    void parseRegister(unsigned char userId, std::string packetData);    
    
    // Player actions
    void parseAttack(unsigned char userId, std::string packetData);    




};
    
#endif