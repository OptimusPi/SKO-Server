#ifndef __SKO_PACKET_HANDLER_H_
#define __SKO_PACKET_HANDLER_H_

#include "Global.h"

#include <cstring>
#include "SKO_Network.h"
#include "SKO_PacketParser.h"
#include "SKO_PacketTypes.h"

// Forward-declare SKO_Network so I can call its functions.
class SKO_Network;

// Class to handle packets
class SKO_PacketHandler
{

public:
    //SKO_PacketHandler(SKO_GameState gameState);
    SKO_PacketHandler(SKO_Network *network);
    void parsePacket(unsigned char userId, std::string packet);
    
private:
    //SKO_GameState gameState;
    SKO_Network *network;

    // Health check packet types
    void parsePing(unsigned char userId);
    void parsePong(unsigned char userId);

    // Login and Register player account
    void parseLogin(unsigned char userId, SKO_PacketParser *parser);
    void parseRegister(unsigned char userId, std::string parameters);    
    
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
    void parseUnequip(unsigned char userId, SKO_PacketParser *parser);
    void parseUseItem(unsigned char userId, SKO_PacketParser *parser);
    void parseDropItem(unsigned char userId, SKO_PacketParser *parser);

    //Trading
    void parseTrade(unsigned char userId, SKO_PacketParser *parser);
    void parseTradeInvite(unsigned char userId, SKO_PacketParser *parser);
    void parseTradeAccept(unsigned char userId, SKO_PacketParser *parser);
    void parseTradeCancel(unsigned char userId, SKO_PacketParser *parser);
    void parseTradeOffer(unsigned char userId, SKO_PacketParser *parser);
    void parseTradeConfirm(unsigned char userId, SKO_PacketParser *parser);

    //Inventory
    void parseInventory(unsigned char userId, SKO_PacketParser *parser);

    //Shop 
    void parseShop(unsigned char userId, SKO_PacketParser *parser);
    void parseShopInvite(unsigned char userId, SKO_PacketParser *parser);
    void parseShopBuy(unsigned char userId, SKO_PacketParser *parser);
    void parseShopSell(unsigned char userId, SKO_PacketParser *parser);
    void parseShopCancel(unsigned char userId, SKO_PacketParser *parser);

    //Bank
    void parseBank(unsigned char userId, SKO_PacketParser *parser);
    void parseBankDeposit(unsigned char userId, SKO_PacketParser *parser);
    void parseBankWithdrawal(unsigned char userId, SKO_PacketParser *parser);
    
    //Party
    void parseParty(unsigned char userId, SKO_PacketParser *parser);
    void parsePartyInvite(unsigned char userId, SKO_PacketParser *parser);
    void parsePartyAccept(unsigned char userId);

    //Clan
    void parseClan(unsigned char userId, SKO_PacketParser *parser);
    void parseClanCreate(unsigned char userId, SKO_PacketParser *parser);
    void parseClanInvite(unsigned char userId, SKO_PacketParser *parser);
    void parseClanAccept(unsigned char userId);


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