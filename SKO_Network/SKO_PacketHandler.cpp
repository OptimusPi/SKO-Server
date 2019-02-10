#include "SKO_PacketHandler.h"

//constructor
SKO_PacketHandler::SKO_PacketHandler(SKO_Network *network)
{ 
    this->network = network;
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
    User[userId].ping = OPI_Clock::milliseconds() - User[userId].pingTicker;
    User[userId].pingTicker = OPI_Clock::milliseconds();
    User[userId].pingWaiting = false;
}

// [LOGIN][<username>][" "][<password>]
void SKO_PacketHandler::parseLogin(unsigned char userId, SKO_PacketParser *parser)
{
    // Declare message string
    std::string loginRequest = parser->getPacketBody();
    std::string username = nextParameter(loginRequest);
    std::string password = loginRequest;
    printf("attemptLogin%s, %s);\n", username.c_str(), password.c_str());
    network->attemptLogin(userId, username, password);
}

// [REGISTER][<username>][" "][<password>]
void SKO_PacketHandler::parseRegister(unsigned char userId, std::string parameters)
{
    std::string username = nextParameter(parameters);
    std::string password = parameters;//do not use nextParameter to get password, just use remaining
    printf("attemptRegister%s, %s);\n", username.c_str(), password.c_str());
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
void SKO_PacketHandler::parseStatHp(unsigned char userId, SKO_PacketParser* parser)
{
    if (User[userId].stat_points > 0)
    {
        User[userId].stat_points--;
        User[userId].regen++;

        network->sendStatRegen(userId, User[userId].regen);
        network->sendStatPoints(userId, User[userId].stat_points);
    }
}

// [STAT_STR]
void SKO_PacketHandler::parseStatStr(unsigned char userId, SKO_PacketParser* parser)
{
    if (User[userId].stat_points > 0)
    {
        User[userId].stat_points--;
        User[userId].strength++;

        network->sendStatStr(userId, User[userId].strength);
        network->sendStatPoints(userId, User[userId].stat_points);
    }
}

// [STAT_DEF]
void SKO_PacketHandler::parseStatDef(unsigned char userId, SKO_PacketParser* parser)
{
    if (User[userId].stat_points > 0)
    {
        User[userId].stat_points--;
        User[userId].defense++;

        network->sendStatDef(userId, User[userId].defense);
        network->sendStatPoints(userId, User[userId].stat_points);
    }
}


// [EQUIP][(unsigned char)equipSlot]
void SKO_PacketHandler::parseUnequip(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char slot = parser->nextByte();
    unsigned char item = User[userId].equip[slot];

    // TODO - do not let user unequip if their inventory is full.
    // Only unequip and transfer to inventory if it actually is equipped
    if (item > 0)
    {
        //un-wear it
        User[userId].equip[slot] = 0; //TODO refactor
        //put it in the player's inventory
        User[userId].inventory[item]++; //TODO refactor

        //tell everyone
        for (int uc = 0; uc < MAX_CLIENTS; uc++)
            network->sendEquip(uc, userId, slot, (char)0, (char)0);

        //update the player's inventory
        int amount = User[userId].inventory[item];
        network->sendPocketItem(userId, item, amount);
    }
}

// TODO - move logic to game class
// [USE_ITEM][(unsigned char)item]
void SKO_PacketHandler::parseUseItem(unsigned char userId, SKO_PacketParser *parser)
{
    int item = parser->nextByte();
    int type = Item[item].type;
    unsigned char mapId = User[userId].mapId;

    //only attempt to use items if the player has them
    if (User[userId].inventory[item] > 0)
    {
        float rand_xs, rand_ys,
            rand_x, rand_y;
        unsigned int rand_item;

        //TODO - refactor this into use a item handler.
        switch (type)
        {
        case 0:
            break; //cosmetic

        case 1: //food
        {
            // Do not eat food if player is full health.
            if (User[userId].hp == User[userId].max_hp)
                break;

            //TODO refactor
            User[userId].hp += Item[item].hp;

            if (User[userId].hp > User[userId].max_hp)
                User[userId].hp = User[userId].max_hp;

            //tell this client
            network->sendStatHp(userId, User[userId].hp);

            //remove item
            User[userId].inventory[item]--;

            //tell them how many items they have
            unsigned int amount = User[userId].inventory[item];
            if (amount == 0)
                User[userId].inventory_index--;

            // party hp notification
            // TODO - change magic number 80 to use a config value
            unsigned char displayHp = (int)((User[userId].hp / (float)User[userId].max_hp) * 80);

            for (int pl = 0; pl < MAX_CLIENTS; pl++)
            {
                if (pl != userId && User[pl].Ident && User[pl].partyStatus == PARTY && User[pl].party == User[userId].party)
                    network->sendBuddyStatHp(pl, userId, displayHp);
            }

            //put in client players inventory
            network->sendPocketItem(userId, item, amount);

            break;
        }
        case 2: //weapon
        {
            // Does the other player have another WEAPON equipped?
            // If so, transfer it to inventory
            unsigned char otherItem = User[userId].equip[0];

            // Transfer weapon from inventory to equipment slot
            User[userId].equip[0] = item;
            User[userId].inventory[item]--;

            // Tranfer old weapon to users inventory
            if (otherItem > 0)
            {
                User[userId].inventory[otherItem]++;
                User[userId].inventory_index++;
                unsigned int amount = User[userId].inventory[otherItem];
                network->sendPocketItem(userId, otherItem, amount);
            }

            // Tell player one weapon is removed from their inventory
            unsigned int amount = User[userId].inventory[item];
            network->sendPocketItem(userId, item, amount);

            //tell everyone the player equipped their weapon
            for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
            {
                if (User[i1].Ident)
                    network->sendEquip(i1, userId, (char)0, Item[item].equipID, item);
            }
            break;
        }
        case 3: //hat
        {
            // Does the other player have another HAT equipped?
            // If so, transfer it to inventory
            unsigned char otherItem = User[userId].equip[1];

            // Transfer hat from inventory to equipment slot
            User[userId].equip[1] = item;
            User[userId].inventory[item]--;

            // Tranfer old hat to users inventory
            if (otherItem > 0)
            {
                User[userId].inventory[otherItem]++;
                User[userId].inventory_index++;
                unsigned int amount = User[userId].inventory[otherItem];
                network->sendPocketItem(userId, otherItem, amount);
            }

            // Tell player one hat is removed from their inventory
            unsigned int amount = User[userId].inventory[item];
            network->sendPocketItem(userId, item, amount);

            //tell everyone the player equipped their hat
            for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
            {
                if (User[i1].Ident)
                    network->sendEquip(i1, userId, (char)1, Item[item].equipID, item);
            }
            break;
        }
        case 4: // mystery box
        {
            // holiday event
            if (HOLIDAY)
            {
                ////get rid of the box
                int numUsed = 0;

                if (User[userId].inventory[item] >= 10)
                {
                    numUsed = 10;
                    User[userId].inventory[item] -= 10;
                }
                else
                {
                    numUsed = User[userId].inventory[item];
                    User[userId].inventory[item] = 0;
                    User[userId].inventory_index--;
                }

                //tell them how many items they have
                unsigned int amount = User[userId].inventory[item];

                //put in players inventory
                network->sendPocketItem(userId, item, amount);

                for (int it = 0; it < numUsed; it++)
                {
                    //spawn a hat or nothing
                    rand_xs = 0;
                    rand_ys = -5;

                    // 1:100 chance of item
                    rand_item = rand() % 100;
                    if (rand_item != 1)
                        rand_item = ITEM_GOLD;
                    else
                        rand_item = HOLIDAY_BOX_DROP;

                    int rand_i;
                    rand_x = User[userId].x + 32;
                    rand_y = User[userId].y - 32;

                    for (rand_i = 0; rand_i < 256; rand_i++)
                    {
                        if (rand_i == 255 || map[mapId].ItemObj[rand_i].status == false)
                            break;
                    }

                    //TODO change this limit to 2 bytes (32K)
                    if (rand_i == 255)
                    {
                        //If we traversed the whole item list already, start over.
                        if (map[mapId].currentItem == 255)
                            map[mapId].currentItem = 0;

                        //use the currentItem to traverse the list whenever it overflows, so the "oldest" item gets deleted.
                        rand_i = map[mapId].currentItem;
                        map[mapId].currentItem++;
                    }

                    map[mapId].ItemObj[rand_i] = SKO_ItemObject(rand_item, rand_x, rand_y, rand_xs, rand_ys, 1);

                    {
                        int i = rand_i;
                        //dont let them get stuck
                        if (blocked(mapId, map[mapId].ItemObj[i].x + map[mapId].ItemObj[i].x_speed, map[mapId].ItemObj[i].y + map[mapId].ItemObj[i].y_speed + 0.15, map[mapId].ItemObj[i].x + Item[map[mapId].ItemObj[i].itemID].w, map[mapId].ItemObj[i].y + map[mapId].ItemObj[i].y_speed + Item[map[mapId].ItemObj[i].itemID].h, false))
                        {
                            //move it down a bit
                            rand_y = User[userId].y + 1;
                            map[mapId].ItemObj[i].y = rand_y;
                        }
                    }
                    float x = rand_x;
                    float y = rand_y;
                    float x_speed = rand_xs;
                    float y_speed = rand_ys;

                    for (int iii = 0; iii < MAX_CLIENTS; iii++)
                    {
                        if (User[iii].Ident && User[iii].mapId == mapId)
                            network->sendSpawnItem(iii, rand_i, mapId, rand_item, x, y, x_speed, y_speed);
                    }
                }
            }
            break;
        }
        case 5: // trophy / spell
        {
            // Does the other player have another TROPHY equipped?
            // If so, transfer it to inventory
            unsigned char otherItem = User[userId].equip[2];

            // Transfer trophy from inventory to equipment slot
            User[userId].equip[2] = item;
            User[userId].inventory[item]--;

            // Tranfer old trophy to users inventory
            if (otherItem > 0)
            {
                User[userId].inventory[otherItem]++;
                User[userId].inventory_index++;
                unsigned int amount = User[userId].inventory[otherItem];
                network->sendPocketItem(userId, otherItem, amount);
            }

            // Tell player one trophy is removed from their inventory
            unsigned int amount = User[userId].inventory[item];
            network->sendPocketItem(userId, item, amount);

            //tell everyone the player equipped their trophy
            for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
            {
                if (User[i1].Ident)
                    network->sendEquip(i1, userId, (char)2, Item[item].equipID, item);
            }
            break;
        }
        default:
            break;
        } //end switch
    }     //end if you have the item
}

// [DROP_ITEM][(unsigned char)item][(unsigned int)amount]
void SKO_PacketHandler::parseDropItem(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char item = parser->nextByte();
    unsigned char mapId = User[userId].mapId;
    unsigned int amount = parser->nextInt();

    //if they have enough to drop
    if (User[userId].inventory[item] >= amount && amount > 0 && User[userId].tradeStatus < 1)
    {
        //take the items away from the player
        User[userId].inventory[item] -= amount;
        int ownedAmount = User[userId].inventory[item];

        //keeping track of inventory slots
        if (User[userId].inventory[item] == 0)
        {
            //printf("the user has %i of Item[%i]", amt, i );
            //prevents them from holding more than 24 items
            User[userId].inventory_index--;
        }

        //tell the user they dropped their items.
        network->sendPocketItem(userId, item, ownedAmount);

        //TODO refactor all of this
        //next spawn a new item for all players
        int rand_i;
        float rand_x = User[userId].x + 16 + (32 - Item[item].w) / 2.0;
        float rand_y = User[userId].y - Item[item].h;

        float rand_xs = 0;
        if (User[userId].facing_right)
        {
            rand_xs = 2.2;
        }
        else
        {
            rand_xs = -2.2;
        }

        float rand_ys = -5;
        for (rand_i = 0; rand_i < 255; rand_i++)
        {
            if (map[mapId].ItemObj[rand_i].status == false)
                break;
        }

        //find item object that's free
        if (rand_i == 255)
        {
            if (map[mapId].currentItem == 255)
                map[mapId].currentItem = 0;

            rand_i = map[mapId].currentItem;

            map[mapId].currentItem++;
        }

        //tell everyone there's an item up for grabs
        map[mapId].ItemObj[rand_i] = SKO_ItemObject(item, rand_x, rand_y, rand_xs, rand_ys, amount);

        int i = rand_i;
        //dont let them get stuck
        if (blocked(mapId, map[mapId].ItemObj[i].x + map[mapId].ItemObj[i].x_speed, map[mapId].ItemObj[i].y + map[mapId].ItemObj[i].y_speed + 0.15, map[mapId].ItemObj[i].x + Item[map[mapId].ItemObj[i].itemID].w, map[mapId].ItemObj[i].y + map[mapId].ItemObj[i].y_speed + Item[map[mapId].ItemObj[i].itemID].h, false))
        {
            //move it down a bit
            rand_y = User[userId].y + 1;
            map[mapId].ItemObj[i].y = rand_y;
        }

        float x = rand_x;
        float y = rand_y;
        float x_speed = rand_xs;
        float y_speed = rand_ys;

        for (int iii = 0; iii < MAX_CLIENTS; iii++)
        {
            if (User[iii].Ident && User[iii].mapId == mapId)
                network->sendSpawnItem(iii, rand_i, mapId, item, x, y, x_speed, y_speed);
        }
    }
}

// Returns the next parameter from a slash command
// Also shrinks the parameter list by one
// Example:
//    slash command: "/warp pifreak 2500 100 2"
//    parameters: "pifreak 2500 100 2"
//    nextParameter(): "pifreak"
//    parameters: "2500 100 2"
//    nextParameter(): "2500"
//    parameters: "100 2"
//    nextParameter(): "100"
//    parameters: "2"
//    nextParameter(): "2"
//    parameters: ""
//    nextParameter: ""
std::string SKO_PacketHandler::nextParameter(std::string &parameters)
{
    //grab first parameter from list
    std::string next = parameters.substr(0, parameters.find_first_of(" "));

    //shrink list of parameters 
    parameters = parameters.substr(parameters.find_first_of(" ") + 1);
    
    //return a single parameter; the next in line
    return next;
}

// [CHAT]["/ban"][" "]["<username>"][" "]["<reason>"]
void SKO_PacketHandler::parseSlashBan(unsigned char userId, std::string parameters)
{
    std::string username = nextParameter(parameters);
    std::string reason = parameters;
    network->banPlayer(userId, username, reason);
}

// [CHAT]["/ban"][" "]["<username>"][" "]["<reason>"]
void SKO_PacketHandler::parseSlashUnban(unsigned char userId, std::string parameters)
{
    //strip the appropriate data
    std::string username = parameters;
    network->unbanPlayer(userId, username);
}

// [CHAT]["/mute"][" "]["<username>"][" "]["<reason>"]
void SKO_PacketHandler::parseSlashMute(unsigned char userId, std::string parameters)
{
    //strip the appropriate data
    std::string username = nextParameter(parameters);
    std::string reason = parameters;
    network->mutePlayer(userId, username, reason);
}
 
// [CHAT]["/unmute"][" "]["<username>"]
void SKO_PacketHandler::parseSlashUnmute(unsigned char userId, std::string parameters)
{
    std::string username = nextParameter(parameters);
    network->unmutePlayer(userId, username);
}

// [CHAT]["/ban"][" "]["<username>"][" "]["<reason>"]
void SKO_PacketHandler::parseSlashKick(unsigned char userId, std::string parameters)
{
    std::string username = nextParameter(parameters);
    std::string reason = parameters;
    printf("network->kickPlayer(%i, %s, %s);\n", userId, username.c_str(), reason.c_str());
    network->kickPlayer(userId, username, reason);
}

// [CHAT]["/who"]
void SKO_PacketHandler::parseSlashWho(unsigned char userId)
{
    //nothing special to parse here!
    network->sendWho(userId);
}

// [CHAT]["/ipban"][" "]["<ipv4 address>"][" "][<reason>]
void SKO_PacketHandler::parseSlashIpban(unsigned char userId, std::string parameters)
{
    std::string ip = nextParameter(parameters);
    std::string reason = parameters;
    network->banAddress(userId, ip, reason);
}

// [CHAT]["/getip"][" "]["<player>"]
void SKO_PacketHandler::parseSlashGetip(unsigned char userId, std::string parameters)
{
    std::string username = nextParameter(parameters);
    network->getIp(userId, username);
}
// [CHAT]["/warp"][" "]["<player>"][" "][(string)x][" "][(string)y][" "][(string)mapId]
void SKO_PacketHandler::parseSlashWarp(unsigned char userId, std::string parameters)
{
    std::string username = nextParameter(parameters);
    int x = atoi(nextParameter(parameters).c_str());
    int y = atoi(nextParameter(parameters).c_str());
    int mapId = atoi(nextParameter(parameters).c_str());
    network->warpPlayer(userId, username, x, y, mapId);                
}

// [CHAT]["/ping"]
void SKO_PacketHandler::parseSlashPing(unsigned char userId, std::string parameters)
{
    //if they are moderator
    if (User[userId].Moderator)
    {
        std::string username = nextParameter(parameters);

        int datUser;
        bool result = false;

        for (datUser = 0; datUser < MAX_CLIENTS; datUser++)
        {
            if (User[datUser].Ident && User[datUser].Nick.compare(username) == 0)
            {
                printf("Moderator inquiry of %s\n", username.c_str());
                result = true;
                break;
            }
        }

        //find user
        std::string pingDumpPacket = "0";
        pingDumpPacket += CHAT;

        if (result)
        {
            std::stringstream ss;
            ss << "User[" << datUser << "] " << User[datUser].Nick << " has a ping of " << User[datUser].ping;
            network->sendChat(userId, ss.str());
        }
        else
        {
            network->sendChat(userId, "username " + username + " was not found.");
        }
    }
}

// [CHAT]["/"]["<command>"][" "]["<parameters>"]
void SKO_PacketHandler::parseSlashCommand(unsigned char userId, std::string message)
{
    std::string command = message.substr(0, message.find_first_of(" "));
    std::string parameters = message.substr(message.find_first_of(" ") + 1);

    if (command.compare("/ban") == 0)
        parseSlashBan(userId, message);
    else if (command.compare("/unban") == 0)
        parseSlashUnban(userId, parameters);
    else if (command.compare("/mute") == 0)
        parseSlashMute(userId, parameters);
    else if (command.compare("/unmute") == 0)
        parseSlashUnmute(userId, parameters);
    else if (command.compare("/kick") == 0)
        parseSlashKick(userId, parameters);
    else if (command.compare("/who") == 0)
        parseSlashWho(userId);
    else if (command.compare("/ipban") == 0)
        parseSlashIpban(userId, parameters);
    else if (command.compare("/getip") == 0)
        parseSlashGetip(userId, parameters);   
    else if (command.compare("/warp") == 0)
        parseSlashWarp(userId, parameters);
    else if (command.compare("/ping") == 0)
        parseSlashPing(userId, parameters);
    else
        network->sendChat(userId, "Sorry, that is not a command.");
}

// [CHAT]["<message body, either a plain string or slash command>"]
void SKO_PacketHandler::parseChat(unsigned char userId, std::string message)
{
    // Do nothing but log message if the user is either mute or not logged in
    if (User[userId].Mute || !User[userId].Ident)
    {
        printf("User[%i](%s) tried to chat: [%s]\n", (int)userId, User[userId].Nick.c_str(), message.c_str());
        return;
    }

    // Parse slash commands in separate function
    if (message[0] == '/') 
    {
        parseSlashCommand(userId, message);
        return;
    }

    // Parse chat messages
    //wrap long text messages
    unsigned int max = 62;
    std::string userTag = "";

    // Add nick to the send
    std::string chatMessage = "";

    if (User[userId].Nick.compare("Paladin") != 0)
    {
        userTag += "<";
        userTag += User[userId].Nick;
        userTag += "> ";
    }

    chatMessage += userTag;
    chatMessage += message;
    printf(kGreen "[Chat]%s\n" kNormal, chatMessage.c_str());

    // Send a short chat that fits on one line.
    if (chatMessage.length() < max)
    {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (User[i].Ident)
                network->sendChat(i, chatMessage);
        }
        return;
    }

    // Wrap long chat
    bool doneWrapping = false;
    while (!doneWrapping)
    {
        std::string chatChunk = "";
        if (chatMessage.length() > max)
        {
            //Break message apart on spaces if they are found in the first chunk
            size_t found = chatMessage.substr(0, max).find_last_of(" ");
            if (found > userTag.length() && found > max - 6)
            {
                chatChunk = chatMessage.substr(0, found);
                chatMessage = chatMessage.substr(found + 1);
            }
            else
            {
                chatChunk = chatMessage.substr(0, max - 1) + "-";
                chatMessage = chatMessage.substr(max - 1);
            }
        }
        else
        {
            chatChunk = chatMessage;
            doneWrapping = true;
        }

        //Send to all clients who are logged in
        //Send a chunk each time it is formed
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (User[i].Ident)
                network->sendChat(i, chatChunk);
        }
    }
}

// [TRADE][INVITE][(unsigned char)playerB]
void SKO_PacketHandler::parseTradeInvite(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char playerB = parser->nextByte();

    if (playerB >= MAX_CLIENTS || playerB == userId || !User[playerB].Ident)
        return;
    
        
    // make sure both players aren't busy!!
    if (User[userId].tradeStatus != 0 && User[playerB].tradeStatus != 0)
    {
        printf("[ERR] A trade status=%i B trade status=%i\n", User[userId].tradeStatus, User[playerB].tradeStatus);
        return;
    }

    printf("%s trade with %s.\n", User[userId].Nick.c_str(), User[playerB].Nick.c_str());

    //Send 'trade with A' to player B
    network->sendTradeInvite(playerB, userId);

    //hold status of what the players are trying to do
    // (tentative...)
    User[userId].tradeStatus = INVITE;
    User[userId].tradePlayer = playerB;
    User[playerB].tradeStatus = INVITE;
    User[playerB].tradePlayer = userId;
}

// [TRADE][ACCEPT][(unsigned char)playerB]
void SKO_PacketHandler::parseTradeAccept(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char playerB = User[userId].tradePlayer;

    if (playerB >= 0 && User[userId].tradeStatus == INVITE && User[playerB].tradeStatus == INVITE)
    {
        User[userId].tradeStatus = ACCEPT;
        User[playerB].tradeStatus = ACCEPT;
    }

    //Accept trade on both ends
    network->sendTradeAccept(playerB, userId);
    network->sendTradeAccept(userId, playerB);
}

// [TRADE][CANCEL]
void SKO_PacketHandler::parseTradeCancel(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char playerB = User[userId].tradePlayer;

    if (playerB < 0)
        return;

    //set them to blank state
    User[userId].tradeStatus = 0;
    User[playerB].tradeStatus = 0;

    //set them to be trading with nobody
    User[userId].tradePlayer = -1;
    User[playerB].tradePlayer = -1;

    //tell both players cancel trade
    network->sendTradeCancel(playerB);
    network->sendTradeCancel(userId);

    //clear trade windows...
    for (int i = 0; i < 256; i++)
    {
        User[userId].localTrade[i] = 0;
        User[playerB].localTrade[i] = 0;
    }
}

// [TRADE][OFFER]
void SKO_PacketHandler::parseTradeOffer(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char playerB = 0;
    playerB = User[userId].tradePlayer;
    //only do something if both parties are in accept trade mode
    if (User[userId].tradeStatus == ACCEPT && User[User[userId].tradePlayer].tradeStatus == ACCEPT)
    {
        unsigned char item = parser->nextByte();
        unsigned int amount = parser->nextInt();

        printf("offer!\n ITEM: [%i]\nAMOUNT [%i/%i]\n...", item, amount, User[userId].inventory[item]);
        //check if you have that item
        if (User[userId].inventory[item] >= amount)
        {
            User[userId].localTrade[item] = amount;

            //send to both players!
            network->sendTradeOffer(userId, (char)1, item, amount);

            //send to playerB
            network->sendTradeOffer(playerB, (char)2, item, amount);
        }
    }
}

// [TRADE][CONFIRM]
void SKO_PacketHandler::parseTradeConfirm(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char playerA = userId;
    unsigned char playerB = User[userId].tradePlayer;

    if (playerB < 0)
    {
        network->sendTradeCancel(userId);
        User[userId].tradeStatus = 0;
        User[userId].tradePlayer = 0;
        return;
    }

    //make sure playerA is in accept mode before confirming
    if (User[playerA].tradeStatus == ACCEPT)
        User[playerA].tradeStatus = CONFIRM;

    //tell both players :)
    network->sendTradeReady(playerA, (char)1);
    network->sendTradeReady(playerB, (char)2);

    //if both players are now confirm, transact and reset!
    if (User[playerA].tradeStatus != CONFIRM || User[playerB].tradeStatus != CONFIRM)
        return;
    
    //lets make sure players have that many items
    bool error = false;
    for (int i = 0; i < 256; i++)
    {
        //compare the offer to the owned items
        if (User[playerA].inventory[i] < User[playerA].localTrade[i])
        {
            printf("Trade error: User: %s, item: %i", User[playerA].Nick.c_str(), i);
            error = true;
            break;
        }
        //compare the offer to the owned items
        if (User[playerB].inventory[i] < User[playerB].localTrade[i])
        {
            printf("Trade error: User: %s, item: %i", User[playerB].Nick.c_str(), i);
            error = true;
            break;
        }
    }

    //tell them the trade is over!!!
    User[playerA].tradeStatus = 0;
    User[playerB].tradeStatus = 0;

    //set them to be trading with nobody
    User[playerA].tradePlayer = -1;
    User[playerB].tradePlayer = -1;

    //tell both players cancel trade
    network->sendTradeCancel(playerA);
    network->sendTradeCancel(playerB);

    //make the transaction!
    if (error)
    {
        printf("Error in trade.");
        return;
    }
    
    printf("trade transaction!\n");

    //go through each item and add them
    for (int i = 0; i < 256; i++)
    {
        unsigned char itemId = (unsigned char)i;

        //easy to follow with these variables broski :P
        int aOffer = User[playerA].localTrade[itemId];
        int bOffer = User[playerB].localTrade[itemId];
        
        //give A's stuff  to B
        if (aOffer > 0)
        {
            printf(kGreen "%s gives %itemId of item %itemId to %s\n" kNormal, User[playerA].Nick.c_str(), aOffer, itemId, User[playerB].Nick.c_str());
            //trade the item and tell the player!
            User[playerB].inventory[itemId] += aOffer;
            User[playerA].inventory[itemId] -= aOffer;

            //put in players inventory
            unsigned int amount = User[playerB].inventory[itemId];
            network->sendPocketItem(playerB, itemId, amount);

            //take it out of A's inventory
            amount = User[playerA].inventory[itemId];
            network->sendPocketItem(playerA, itemId, amount);
        }

        //give B's stuff  to A
        if (bOffer > 0)
        {
            printf(kGreen "%s gives %itemId of item %itemId to %s\n" kNormal, User[playerB].Nick.c_str(), aOffer, itemId, User[playerA].Nick.c_str());
            //trade the item and tell the player!
            User[playerA].inventory[itemId] += bOffer;
            User[playerB].inventory[itemId] -= bOffer;

            //put in players inventory
            unsigned int amount = User[playerA].inventory[itemId];
            network->sendPocketItem(playerA, itemId, amount);

            //take it away from B
            amount = User[playerB].inventory[itemId];
            network->sendPocketItem(playerB, itemId, amount);
        }

        //clear the items
        User[playerA].localTrade[itemId] = 0;
        User[playerB].localTrade[itemId] = 0;
    }

    //Save all profiles after a trade
    network->saveAllProfiles();
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

    //invite the other user
    //if the other user is not in your party
    if (User[playerB].partyStatus != 0)
        return;
        
    int partyID = User[userId].party;

    //set their party
    if (User[userId].party == -1)
    {
        for (partyID = 0; partyID <= MAX_CLIENTS; partyID++)
        {
            //look for an open partyID
            bool taken = false;
            for (int i = 0; i < MAX_CLIENTS; i++)
            {
                if (User[i].Ident && User[i].party == partyID)
                {
                    taken = true;
                    break;
                }
            }
            if (!taken)
                break;
        } //find oepn party id
    }     //end if party is null

    //make the parties equal
    User[userId].party = partyID;
    User[playerB].party = partyID;

    //set the party status of the invited person
    User[playerB].partyStatus = INVITE;
    User[userId].partyStatus = ACCEPT;

    //ask the user to party
    network->sendTradeInvite(playerB, userId);   
}

// [PARTY][ACCEPT]
void SKO_PacketHandler::parsePartyAccept(unsigned char userId)
{
    //only if I was invited :)
    if (User[userId].partyStatus != INVITE)
        return;

    //tell the user about all the players in the party
    for (int pl = 0; pl < MAX_CLIENTS; pl++)
    {
        if (!User[pl].Ident || pl == userId || User[pl].partyStatus != PARTY || User[pl].party != User[userId].party)
            continue;
        //
        // Notify all members in the party that player is joining
        //
        network->sendPartyAccept(pl, userId, User[userId].party);
        //tell existing party members the new player's stats
        unsigned char hp = (int)((User[userId].hp / (float)User[userId].max_hp) * 80);
        unsigned char xp = (int)((User[userId].xp / (float)User[userId].max_xp) * 80);
        unsigned char level = User[userId].level;

        network->sendBuddyStatHp(pl, userId, hp);
        network->sendBuddyStatXp(pl, userId, xp);
        network->sendBuddyStatLevel(pl, userId, level);

        //
        // Notify player of each existing party member
        //
        network->sendPartyAccept(userId, pl, User[pl].party);
        // Send player the current party member stats
        hp = (int)((User[pl].hp / (float)User[pl].max_hp) * 80);
        xp = (int)((User[pl].xp / (float)User[pl].max_xp) * 80);
        level = User[pl].level;

        // Update party stats for client party player list
        network->sendBuddyStatHp(userId, pl, hp);
        network->sendBuddyStatXp(userId, pl, xp);
        network->sendBuddyStatLevel(userId, pl, level);
    }
        
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
        //quit them out of this clan
        User[userId].clanStatus = -1;
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
    unsigned char mapId = User[userId].mapId;
    unsigned char stallId = parser->nextByte();

    if (stallId >= map[mapId].num_stalls)
        return;

    //open bank
    if (map[mapId].Stall[stallId].shopid == 0)
    {
        if (User[userId].tradeStatus < 1)
        {
            //set trade status to banking
            User[userId].tradeStatus = BANK;
            network->sendBankOpen(userId);
        }
    }

    //open shop
    if (map[mapId].Stall[stallId].shopid > 0)
    {
        //tell the user "shop[shopId] opened up"
        unsigned char shopId = map[mapId].Stall[stallId].shopid;
        network->sendShopOpen(userId, shopId);

        //set trade status to shopping
        User[userId].tradeStatus = SHOP;
        User[userId].tradePlayer = map[mapId].Stall[stallId].shopid;
    }
}

// [SHOP][BUY][(unsigned char)itemId)][(unsigned int)amount]
void SKO_PacketHandler::parseShopBuy(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char mapId = User[userId].mapId;
    unsigned char shopItem = parser->nextByte();
    unsigned int amount = parser->nextInt();
    int i = 0, item = 0, price = 0;

    //Shops are a 6x4 grid
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 6; x++)
        {
            if (i == shopItem)
            {
                item = map[mapId].Shop[User[userId].tradePlayer].item[x][y][0];
                price = map[mapId].Shop[User[userId].tradePlayer].item[x][y][1];
            }
            i++;
        }

    //if the player can afford it
    if (item <= 0 || User[userId].inventory[ITEM_GOLD] < price * amount)
    {
        network->sendChat(userId, "Sorry, but you cannot afford this item.");
        return;
    }

    //take away gold
    User[userId].inventory[ITEM_GOLD] -= price * amount;

    //give them the item
    User[userId].inventory[item] += amount;

    //put in client players inventory
    amount = User[userId].inventory[item];
    network->sendPocketItem(userId, item, amount);

    //Take gold out of player's inventory
    amount = User[userId].inventory[ITEM_GOLD];
    network->sendPocketItem(userId, (unsigned char)ITEM_GOLD, amount);
}

// [SHOP][SELL][(unsigned char)itemId)][(unsigned int)amount]
void SKO_PacketHandler::parseShopSell(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char item = parser->nextByte();
    unsigned int price = Item[item].price;
    unsigned int amount = parser->nextInt();

    //if they even have the item to sell
    if (price <= 0 || User[userId].inventory[item] < amount)
    {
        network->sendChat(userId, "Sorry, but you cannot sell this item.");
        return;
    }

    //take away gold
    User[userId].inventory[item] -= amount;

    //give them the item
    User[userId].inventory[ITEM_GOLD] += price * amount;

    //take out of client player's inventory
    amount = User[userId].inventory[item];
    network->sendPocketItem(userId, item, amount);

    //put gold into player's inventory
    amount = User[userId].inventory[ITEM_GOLD];
    network->sendPocketItem(userId, (char)ITEM_GOLD, amount);
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
    unsigned char item = parser->nextByte();
    unsigned int amount = parser->nextInt();

    if (item >= NUM_ITEMS || User[userId].inventory[item] < amount)
    {
        network->sendChat(userId, "Sorry, but you may cannot deposit this item.");
        return;
    }

    User[userId].inventory[item] -= amount;
    User[userId].bank[item] += amount;

    //send deposit notification to user
    unsigned int deposit = User[userId].bank[item];
    network->sendBankItem(userId, item, deposit);

    //update client player's inventory
    unsigned int withdrawal = User[userId].inventory[item];
    network->sendPocketItem(userId, item, withdrawal);
}

// [BANK][DEBANK_ITEM][(unsigned char)itemId)][(unsigned int)amount]
void SKO_PacketHandler::parseBankWithdrawal(unsigned char userId, SKO_PacketParser *parser)
{
    unsigned char item = parser->nextByte();
    unsigned int amount = parser->nextInt();

    if (item >= NUM_ITEMS || User[userId].bank[item] < amount)
    {
        network->sendChat(userId, "Sorry, but you cannot withdrawal this item.");
        return;
    }
    
    User[userId].bank[item] -= amount;
    User[userId].inventory[item] += amount;

    //send deposit notification to user
    unsigned int deposit = User[userId].bank[item];
    network->sendBankItem(userId, item, deposit);

    //update client player's inventory
    unsigned int withdrawal = User[userId].inventory[item];
    network->sendPocketItem(userId, item, withdrawal);
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
 
    default:
        // Disconnect clients sending nonsense packets
        //TODO//User[userId].socket->Close();
        printf(kRed "[FATAL] SKO_PacketHandler::parsePacket() called with unknown packet type!\n" kNormal);
        printf(kRed "[       SKO_PacketHandler::parsePacket(%i, %s)] \n" kNormal, userId, parser->toString().c_str());
        break;
    }

    //Clean up memory used by parser
    delete parser;
}