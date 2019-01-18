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
    

    // Health check packet types
    void parsePing(unsigned char userId);
    void parsePong(unsigned char userId);

    // Login and Register player account
    void parseLogin(unsigned char userId, SKO_PacketParser *parser);
    void parseRegister(unsigned char userId, SKO_PacketParser *parser);    
    
    // Player actions
    void parseAttack(unsigned char userId, SKO_PacketParser *parser);    
    void parseMoveLeft(unsigned char userId, SKO_PacketParser *parser);
    void parseMoveRight(unsigned char userId, SKO_PacketParser *parser);
    void parseMoveJump(unsigned char userId, SKO_PacketParser *parser);
    void parseMoveStop(unsigned char userId, SKO_PacketParser *parser);
    void parseCastSpell(unsigned char userId);

    // Stat point spending
    void parseStatHp(unsigned char userId, SKO_PacketParser* parser);
    void parseStatStr(unsigned char userId, SKO_PacketParser* parser);
    void parseStatDef(unsigned char userId, SKO_PacketParser* parser);
    
    // Items and equipment
    void parseUnequip(unsigned int userId, SKO_PacketParser *parser);
    void parseUseItem(unsigned char userId, SKO_PacketParser *parser);
    void parseDropItem(unsigned char userId, SKO_PacketParser *parser);

    // Chat and slash commands
    void parseChat(unsigned char userId, std::string message);
    std::string nextParameter(std::string &parameters);
    void parseSlashCommand(unsigned char userId, std::string parameters);
    void parseSlashBan(unsigned char userId, std::string parameters);
    void parseSlashUnban(unsigned char userId, std::string parameters);
    void parseSlashKick(unsigned char userId, std::string parameters);
    void parseSlashMute(unsigned char userId, std::string parameters);
    void parseSlashUnmute(unsigned char userId, std::string parameters);
    void parseSlashIpban(unsigned char userId, std::string parameters);
    void parseSlashGetip(unsigned char userId, std::string parameters); 
    void parseSlashWarp(unsigned char userId, std::string parameters);
    void parseSlashPing(unsigned char userId, std::string parameters);
    void parseSlashWho(unsigned char userId);
};
    
#endif