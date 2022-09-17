#include "SKO_PacketHandler.h"
#include "SKO_ChatHandler.h"
#include "SKO_Network.h"
#include "../OPI_Utilities/OPI_Utilities.h"
#include "../Global.h"

//constructor
SKO_PacketHandler::SKO_PacketHandler(SKO_Network *network)
{ 
    this->network = network;
    this->chatHandler = new SKO_ChatHandler(network);
}

// When server receives PING,
// It immediately replies to the client with PONG
// [PING]
void SKO_PacketHandler::parsePing(unsigned char userId)
{
    network->sendPong(userId);
}

// [PONG]
void SKO_PacketHandler::parsePong(unsigned char userId)
{
    network->receivedPong(userId);
}

// [LOGIN][<username>][" "][<password>]
void SKO_PacketHandler::parseLogin(unsigned char userId, SKO_PacketParser *parser)
{
    // Declare message string
    std::string loginRequest = parser->getPacketBody();
    std::string username = OPI_Utilities::nextParameter(loginRequest);
    std::string password = loginRequest;
    network->attemptLogin(userId, username, password);
}

// [REGISTER][<username>][" "][<password>]
void SKO_PacketHandler::parseRegister(unsigned char userId, std::string parameters)
{
    std::string username = OPI_Utilities::nextParameter(parameters);
    std::string password = parameters;//do not use nextParameter to get password, just use remaining
    network->attemptRegister(userId, username, password);      
}

// [ATTACK][(float)x][(float)y]
void SKO_PacketHandler::parseAttack(unsigned char userId, SKO_PacketParser *parser)
{
    float x = parser->nextFloat();
    float y = parser->nextFloat();
    Attack(userId, x, y);
}

// [MOVE_LEFT][(float)x][(float)y]
void SKO_PacketHandler::parseMoveLeft(unsigned char userId, SKO_PacketParser *parser)
{
    float x = parser->nextFloat();
    float y = parser->nextFloat();
    Left(userId, x, y);
}

// [MOVE_RIGHT][(float)x][(float)y]
void SKO_PacketHandler::parseMoveRight(unsigned char userId, SKO_PacketParser *parser)
{
    float x = parser->nextFloat();
    float y = parser->nextFloat();
    Right(userId, x, y);
}

// [MOVE_JUMP][(float)x][(float)y]
void SKO_PacketHandler::parseMoveJump(unsigned char userId, SKO_PacketParser *parser)
{
    float x = parser->nextFloat();
    float y = parser->nextFloat();
    Jump(userId, x, y);
}

// [MOVE_STOP][(float)x][(float)y]
void SKO_PacketHandler::parseMoveStop(unsigned char userId, SKO_PacketParser *parser)
{
    float x = parser->nextFloat();
    float y = parser->nextFloat();
    Stop(userId, x, y);
}

// [CAST_SPELL]
void SKO_PacketHandler::parseCastSpell(unsigned char userId)
{
    CastSpell(userId);
}

// [STAT_HP]
void SKO_PacketHandler::parseStatHp(unsigned char userId)
{
    SpendStatPoint(userId, STAT_HP);
} 

// [STAT_STR]
void SKO_PacketHandler::parseStatStr(unsigned char userId)
{
    SpendStatPoint(userId, STAT_STR);
}

// [STAT_DEF]
void SKO_PacketHandler::parseStatDef(unsigned char userId)
{
    SpendStatPoint(userId, STAT_DEF);
}

// [EQUIP][(unsigned char)equipSlot]
void SKO_PacketHandler::parseUnequip(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char slot = parser->nextByte();
    UnequipItem(userId, slot);
}

// TODO - move logic to game class
// [USE_ITEM][(unsigned char)item]
void SKO_PacketHandler::parseUseItem(unsigned char userId, SKO_PacketParser *parser)
{
    int itemId = parser->nextByte();
    UseItem(userId, itemId); 
}

// [DROP_ITEM][(unsigned char)item][(unsigned int)amount]
void SKO_PacketHandler::parseDropItem(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char itemId = parser->nextByte();
    unsigned int amount = parser->nextInt();
    DropItem(userId, itemId, amount);
}

// [TRADE][INVITE][(unsigned char)playerB]
void SKO_PacketHandler::parseTradeInvite(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char playerB = parser->nextByte();
    InvitePlayerToTrade(userId, playerB);
}

// [TRADE][ACCEPT][(unsigned char)playerB]
void SKO_PacketHandler::parseTradeAccept(unsigned char userId, SKO_PacketParser *parser)
{
    AcceptTrade(userId);
}

// [TRADE][CANCEL]
void SKO_PacketHandler::parseTradeCancel(unsigned char userId, SKO_PacketParser *parser)
{
    CancelTrade(userId);
}

// [TRADE][OFFER]
void SKO_PacketHandler::parseTradeOffer(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char itemId = parser->nextByte();
    unsigned int amount = parser->nextInt();
    OfferTradeItem(userId, itemId, amount);
}

// [TRADE][CONFIRM]
void SKO_PacketHandler::parseTradeConfirm(unsigned char userId, SKO_PacketParser *parser)
{
    ConfirmTrade(userId); 
}

// [TRADE][(unsigned char)tradeAction][...]
void SKO_PacketHandler::parseTrade(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char tradeAction = parser->nextByte();
    switch (tradeAction)
    {
    case INVITE:
        parseTradeInvite(userId, parser);
        break; //end INVITE

    case ACCEPT:
        parseTradeAccept(userId, parser);
        break;

    case CANCEL:
        parseTradeCancel(userId, parser);

    case OFFER:
        parseTradeOffer(userId, parser);
        break;

    case CONFIRM:
        parseTradeConfirm(userId, parser);
        break;

    default:
        break;
    } //end trade action switch
}


// [PARTY][INVITE][(unsigned char)playerId]
void SKO_PacketHandler::parsePartyInvite(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char playerB = parser->nextByte();
    InvitePlayerToParty(userId, playerB);
}

// [PARTY][ACCEPT]
void SKO_PacketHandler::parsePartyAccept(unsigned char userId)
{
    AcceptPartyInvite(userId);
} 

// [PARTY][(unsigned char)partyAction]
void SKO_PacketHandler::parseParty(unsigned char userId, SKO_PacketParser *parser)
{
    int partyAction = parser->nextByte();
    switch (partyAction)
    {
    case INVITE:
        parsePartyInvite(userId, parser);
        break;

    case ACCEPT:
        parsePartyAccept(userId);
        break;

    case CANCEL:
        //quit them out of this party
        quitParty(userId);
        break;

    default:
        break;
    } //end switch mode
}

// [MAKE_CLAN]["<clanTag>"]
void SKO_PacketHandler::parseClanCreate(unsigned char userId, SKO_PacketParser *parser)
{
    std::string clanTag = parser->getPacketBody();
    network->createClan(userId, clanTag);
}

// [CLAN][ACCEPT]
void SKO_PacketHandler::parseClanAccept(unsigned char userId)
{
    //nothing to parse here
    network->acceptClanInvite(userId);
}

// [CLAN][INVITE][(unsigned char)playerId]
void SKO_PacketHandler::parseClanInvite(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char playerB = parser->nextByte();
    network->clanInvite(userId, playerB);
}

// [CLAN][(unsigned char)clanAction][...]
void SKO_PacketHandler::parseClan(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char clanAction = parser->nextByte();

    switch (clanAction)
    {

    case INVITE:
        parseClanInvite(userId, parser);
        break;

    case ACCEPT:
        parseClanAccept(userId);
        break;

    case CANCEL:
        DeclineClanInvite(userId);
        break;

    default:
        break;
    }
}

// [INVENTORY]["<base64 encoded inventory order>"]
void SKO_PacketHandler::parseInventory(unsigned char userId, SKO_PacketParser *parser)
{
    std::string inventory_order = parser->getPacketBody();
    User[userId].inventory_order = inventory_order;
}

// [SHOP][INVITE][(unsigned char)itemId)][(unsigned int)amount]
void SKO_PacketHandler::parseShopInvite(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char stallId = parser->nextByte();
    OpenStall(userId, stallId);
} 

// [SHOP][BUY][(unsigned char)itemId)][(unsigned int)amount]
void SKO_PacketHandler::parseShopBuy(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char shopItem = parser->nextByte();
    unsigned int amount = parser->nextInt();
    ShopBuy(userId, shopItem, amount);
}

// [SHOP][SELL][(unsigned char)itemId)][(unsigned int)amount]
void SKO_PacketHandler::parseShopSell(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char itemId = parser->nextByte();
    unsigned int amount = parser->nextInt();
    ShopSell(userId, itemId, amount);
}

// [SHOP][(unsigned char)shopAction)][...]
void SKO_PacketHandler::parseShop(unsigned char userId, SKO_PacketParser *parser)
{
    int shopAction = parser->nextByte();

    switch (shopAction)
    {
    case INVITE:
        parseShopInvite(userId, parser);
        break;

    case BUY:
        parseShopBuy(userId, parser);
        break;

    case SELL:
        parseShopSell(userId, parser);
        break;      
      
    case CANCEL:
        User[userId].tradeStatus = 0;
        User[userId].tradePlayer = 0;
        break;

    default:
        break;
    }
}

// [BANK][BANK_ITEM][(unsigned char)itemId)][(unsigned int)amount]
void SKO_PacketHandler::parseBankDeposit(unsigned char userId, SKO_PacketParser *parser)
{
    //get item type and amount
    unsigned char itemId = parser->nextByte();
    unsigned int amount = parser->nextInt();
    DepositBankItem(userId, itemId, amount);      
}

// [BANK][DEBANK_ITEM][(unsigned char)itemId)][(unsigned int)amount]
void SKO_PacketHandler::parseBankWithdrawal(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char itemId = parser->nextByte();
    unsigned int amount = parser->nextInt();
    WithdrawalBankItem(userId, itemId, amount);
}

// [BANK][(unsigned char)bankAction]
void SKO_PacketHandler::parseBank(unsigned char userId, SKO_PacketParser *parser)
{
    // Only proceed if player is using the bank
    if (User[userId].tradeStatus != BANK)
        return;

    unsigned char bankAction = parser->nextByte();
    switch (bankAction)
    {
    case CANCEL:
        User[userId].tradeStatus = 0;
        break;

    case BANK_ITEM:
        parseBankDeposit(userId, parser);
        break;

    case DEBANK_ITEM:
        break;

    default:
        break;
    }
}

void SKO_PacketHandler::parseChat(unsigned char userId, std::string message)
{
    this->chatHandler->parseChat(userId, message);
}

// [(unsigned char)packetLength][(unsigned char)packetType][...]
void SKO_PacketHandler::parsePacket(unsigned char userId, std::string packet)
{
    // Construct a packet parser from this packet
    SKO_PacketParser *parser = new SKO_PacketParser(packet);
    
    // Ensure packet has length and types
    if (packet.length() < 2)
    {
        printf(kRed "[FATAL] SKO_PacketHandler::parsePacket() called with incomplete packet!\n" kNormal);
        printf(kRed "[       SKO_PacketHandler::parsePacket(%i, %s)] \n" kNormal, userId, parser->toString().c_str());
        return;
    }

    // Ensure packet is complete
    if (packet.length() < parser->getPacketLength())
    {
        printf(kRed "[FATAL] SKO_PacketHandler::parsePacket() called with incomplete packet!\n" kNormal);
        printf(kRed "[       SKO_PacketHandler::parsePacket(%i, %s)] \n" kNormal, userId, parser->toString().c_str());
        return;
    }

    // Ensure this was not calle dwith multiple packets
    if (packet.length() > parser->getPacketLength())
    {
        printf(kRed "[FATAL] SKO_PacketHandler::parsePacket() called with too many packets!\n" kNormal);
        printf(kRed "[       SKO_PacketHandler::parsePacket(%i, %s)] \n" kNormal, userId, parser->toString().c_str());
        return;
    }

    switch (parser->getPacketType())
    {
    case PING:
        parsePing(userId);
        break;

    case PONG:
        parsePong(userId);
        break;

    case LOGIN:
        parseLogin(userId, parser);
        break;

    case REGISTER:
        parseRegister(userId, parser->getPacketBody());
        break;

    case ATTACK:
        parseAttack(userId, parser);
        break;

    case MOVE_RIGHT:
        parseMoveRight(userId, parser);
        break;

    case MOVE_LEFT:
        parseMoveLeft(userId, parser);
        break;
    
    case MOVE_JUMP:
        parseMoveJump(userId, parser);
        break;

    case MOVE_STOP:
        parseMoveStop(userId, parser);
        break;

    case USE_ITEM:
        parseUseItem(userId, parser);
        break;

    case DROP_ITEM:
        parseDropItem(userId, parser);
        break;

    case CHAT:
        parseChat(userId, parser->getPacketBody());
        break;

    case TRADE:
        parseTrade(userId, parser);
        break;

    case SHOP:
        parseShop(userId, parser);
        break;

    case BANK:
        parseBank(userId, parser);
        break;

    case INVENTORY:
        parseInventory(userId, parser);
        break;

    case PARTY:
        parseParty(userId, parser);
        break;

    case CLAN:
        parseClan(userId, parser);
        break;

    case MAKE_CLAN:
        parseClanCreate(userId, parser);
        break;
 
    case STAT_HP:
        parseStatHp(userId);
        break;

    case STAT_DEF:
        parseStatDef(userId);
        break;

    case STAT_STR:
        parseStatStr(userId);
        break;
    
    case CAST_SPELL:
        parseCastSpell(userId);
        break;

    case EQUIP:
        parseUnequip(userId, parser);
        break;
    default:
        // Disconnect clients sending nonsense packets
        printf(kRed "[FATAL] SKO_PacketHandler::parsePacket() called with unknown packet type!\n" kNormal);
        printf(kRed "[       SKO_PacketHandler::parsePacket(%i, %s)] \n" kNormal, userId, parser->toString().c_str());
        network->forceCloseClient(userId);
        break;
    }

    //Clean up memory used by parser
    delete parser;
}