#include "SKO_PacketHandler.h"

//constructor
SKO_PacketHandler::SKO_PacketHandler(SKO_Network *network, SKO_Repository *repository)
{
    this->network = network;
    this->repository = repository;
}
// When server receives PING,
// It immediately replies to the client with PONG
// [PING]
void SKO_PacketHandler::parsePing(unsigned char userId)
{
    network->SendPong(userId);
}

// [PONG]
void SKO_PacketHandler::parsePong(unsigned char userId)
{
    User[userId].ping = Clock() - User[userId].pingTicker;
    User[userId].pingTicker = Clock();
    User[userId].pingWaiting = false;
}

// [LOGIN][<username>][" "][<password>]
void SKO_PacketHandler::parseLogin(unsigned char userId, SKO_PacketParser *parser)
{
    // Declare message string
    std::string loginRequest = "";
    std::string username = "";
    std::string password = "";

    // Declare temp string
    std::string Temp;

    //fill message with username and password
    loginRequest = parser->getPacketBody();;

    //strip the appropriate data
    username += loginRequest.substr(0, loginRequest.find_first_of(" "));
    password += loginRequest.substr(loginRequest.find_first_of(" ") + 1);

    printf("\n::LOGIN::\nUsername[%s]\nPassword[%s]\n", username.c_str(), password.c_str());

    //go through and see if you are logged in already
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        //tell the client if you are logged in already
        if (lower(User[i].Nick).compare(lower(username)) == 0)
        {
            printf("User that is you already is: %s x is %i i is %i\n", User[i].Nick.c_str(), (int)User[i].x, i);
            network->SendLoginResponse_AlreadyOnline(userId);
            return;
        }
    }

    //Login and load some status such as is mute, is banned
    int result = repository->loginPlayer(username, password);

    if (result == 1) //wrong password
    {
        network->SendLoginResponse_PasswordFailed(userId);

        //TODO kick server after several failed login attempts
        //warn the server, possible bruteforce hack attempt
        printf("%s has entered the wrong password!\n", username.c_str());
    }
    else if (result == 3) //character is banned
    {
        network->SendLoginResponse_PlayerBanned(userId);
        printf("%s is banned and tried to login!\n", username.c_str());
    }
    else if (result == 4)
    {
        network->SendLoginResponse_PasswordFailed(userId);
        printf("%s tried to login but doesn't exist!\n", username.c_str());
    }

    if (result == 0 || result == 5) //login with no problems or with 1 problem: user is mute
    {                               //login success

        printf("(login success) User %i %s socket status: %i\n", userId, username.c_str(), User[userId].Sock->GetStatus());

        if (result == 0)
            User[userId].Mute = false;

        //successful login
        network->SendLoginResponse_Success(userId);

        //set display name
        User[userId].Nick = username;

        std::string clanTag = repository->GetClanTag(username, "(noob)");

        printf("Clan: %s\n", clanTag.c_str());
        User[userId].Clan = clanTag;

        printf(">>>about to load data.\n");
        if (repository->loadPlayerData(userId) != 0)
        {
            printf("couldn't load data. KILL!\n");
            User[userId].Save = false;
            User[userId].Sock->Close();
            return;
        }

        //set identified
        User[userId].Ident = true;

        //the current map of this user
        unsigned char mapId = User[userId].mapId;

        printf("going to tell client stats\n");

        printf(kGreen "hp: %i\n" kNormal, (int)User[userId].hp);
        printf(kGreen "max hp: %i\n" kNormal, (int)User[userId].max_hp);
        printf(kGreen "xp: %i\n" kNormal, (int)User[userId].xp);
        printf(kGreen "max xp: %i\n" kNormal, (int)User[userId].max_xp);

        // HP
        network->SendStatHp(userId, User[userId].hp);
        network->SendStatHpMax(userId, User[userId].max_hp);
        network->SendStatRegen(userId, User[userId].regen);

        // XP
        network->SendStatXp(userId, User[userId].xp);
        network->SendStatXpMax(userId, User[userId].max_xp);

        //STATS
        network->SendStatLevel(userId, User[userId].level);
        network->SendStatStr(userId, User[userId].strength);
        network->SendStatDef(userId, User[userId].defence);
        network->SendStatPoints(userId, User[userId].stat_points);

        //equipment
        network->SendEquip(userId, userId, (char)0, Item[User[userId].equip[0]].equipID, User[userId].equip[0]);
        network->SendEquip(userId, userId, (char)1, Item[User[userId].equip[1]].equipID, User[userId].equip[1]);
        network->SendEquip(userId, userId, (char)2, Item[User[userId].equip[2]].equipID, User[userId].equip[2]);

        //cosmetic inventory order
        network->SendInventoryOrder(userId, User[userId].getInventoryOrder());

        //inventory
        for (unsigned char i = 0; i < NUM_ITEMS; i++)
        {
            //if they own this item, tell them how many they own.
            unsigned int amount = User[userId].inventory[i];
            //prevents them from holding more than 24 items
            if (amount > 0)
            {
                User[userId].inventory_index++;
                network->SendPocketItem(userId, i, amount);
            }

            amount = User[userId].bank[i];
            //printf("this player owns [%i] of item %i\n", amt, i);
            if (amount > 0)
            {
                //TODO: remove this bank index or change how the bank works!
                User[userId].bank_index++;
                network->SendBankItem(userId, i, amount);
            }
        } //end loop 256 for items in inventory and map

        //bank
        for (int i = 0; i < 256; i++)
        {
            //go through all the ItemObjects since we're already looping
            if (map[mapId].ItemObj[i].status)
            {
                unsigned char itemId = map[mapId].ItemObj[i].itemID;
                float numx = map[mapId].ItemObj[i].x;
                float numy = map[mapId].ItemObj[i].y;
                float numxs = map[mapId].ItemObj[i].x_speed;
                float numys = map[mapId].ItemObj[i].y_speed;
                network->SendSpawnItem(userId, i, mapId, itemId, numx, numy, numxs, numys);
            }
        }

        //targets
        for (int i = 0; i < map[mapId].num_targets; i++)
        {
            if (map[mapId].Target[i].active)
                network->SendSpawnTarget(i, mapId);
        }

        //npcs
        for (int i = 0; i < map[mapId].num_npcs; i++)
        {
            unsigned char action = NPC_MOVE_STOP;

            if (map[mapId].NPC[i]->x_speed < 0)
                action = NPC_MOVE_LEFT;
            if (map[mapId].NPC[i]->x_speed > 0)
                action = NPC_MOVE_RIGHT;

            network->SendNpcAction(map[mapId].NPC[i], action, i, mapId);
        }

        // load all enemies
        for (int i = 0; i < map[mapId].num_enemies; i++)
        {
            unsigned char action = ENEMY_MOVE_STOP;
            if (map[mapId].Enemy[i]->x_speed < 0)
                action = ENEMY_MOVE_LEFT;
            if (map[mapId].Enemy[i]->x_speed > 0)
                action = ENEMY_MOVE_RIGHT;

            network->SendEnemyAction(map[mapId].Enemy[i], action, i, mapId);

            //enemy health bars
            int hp = (unsigned char)((float)map[mapId].Enemy[i]->hp / map[mapId].Enemy[i]->hp_max * hpBar);
            network->SendEnemyHp(userId, i, mapId, hp);
        }

        // inform all players
        for (unsigned char i = 0; i < MAX_CLIENTS; i++)
        {
            // Tell the new user about existing players
            if (i == userId && User[i].Nick.compare("Paladin") != 0)
            {
                network->SendPlayerJoin(userId, i);
            }
            // Tell existing players about new user
            else if (User[i].Ident)
            {
                network->SendPlayerJoin(i, userId);
            }
        }

        // Trigger loading complete on client.
        network->SendPlayerLoaded(userId);

        //mark this client to save when they disconnect..since Send/Recv change Ident!!
        User[userId].Save = true;
        network->saveAllProfiles();
    } //end login success
}


// [REGISTER][<username>][" "][<password>]
void SKO_PacketHandler::parseRegister(unsigned char userId, SKO_PacketParser *parser)
{
    std::string username = "";
    std::string password = "";
    std::string packetData = parser->getPacketBody();

    //strip the appropriate data
    username += packetData.substr(0, packetData.find_first_of(" "));
    password += packetData.substr(packetData.find_first_of(" ") + 1);

    //try to create new player account
    int result = repository->createPlayer(username, password, User[userId].Sock->IP);

    if (result == 1) // user already exists
    {
        network->SendRegisterResponse_AlreadyRegistered(userId);
        printf("%s tried to double-register!\n", username.c_str());
    }
    else if (result == 0) // user created successfully
    {
        network->SendRegisterResponse_Success(userId);
        printf("%s has been registered!\n", username.c_str());
    }
    else if (result == 2) // user is banned
    {
        User[userId].Sock->Close();
    }
    else if (result == 3) // fatal error occurred
    {
        printf(kRed "[FATAL] Unable to create player.\n" kNormal);
        User[userId].Sock->Close();
    }
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
    //What do you have equipped?
    int spell = User[userId].equip[2];
    if (spell > 0)
    {
        if (User[userId].inventory[spell] > 0)
        {
            User[userId].inventory[spell]--;
            if (User[userId].inventory[spell] == 0)
                User[userId].inventory_index--;

            //notify user the item was thrown
            unsigned int amount = User[userId].inventory[spell];
            network->SendPocketItem(userId, spell, amount);
        }
        else
        {
            //Throw the item from your hand instead of inventory if that's all you have
            User[userId].equip[2] = 0;
            //send packet that says you arent holding anything!
            for (int c = 0; c < MAX_CLIENTS; c++)
                if (User[c].Ident)
                    network->SendEquip(c, userId, (char)2, (char)0, (char)0);
        }

        //tell all the users that an item has been thrown...
        //just use an item object with no value.
        SKO_ItemObject lootItem = SKO_ItemObject();
        lootItem.y = User[userId].y + 24;

        if (User[userId].facing_right)
        {
            lootItem.x_speed = 10;
            lootItem.x = User[userId].x + 50;
        }
        else
        {
            lootItem.x_speed = -10;
            lootItem.x = User[userId].x - 32;
        }

        lootItem.y_speed = -3.2;
        lootItem.itemID = spell;
        lootItem.owner = userId;
        lootItem.amount = 1;

        SpawnLoot(User[userId].mapId, lootItem);
    }
}

// [STAT_HP]
void SKO_PacketHandler::parseStatHp(unsigned char userId, SKO_PacketParser* parser)
{
    if (User[userId].stat_points > 0)
    {
        User[userId].stat_points--;
        User[userId].regen++;

        network->SendStatRegen(userId, User[userId].regen);
        network->SendStatPoints(userId, User[userId].stat_points);
    }
}

// [STAT_STR]
void SKO_PacketHandler::parseStatStr(unsigned char userId, SKO_PacketParser* parser)
{
    if (User[userId].stat_points > 0)
    {
        User[userId].stat_points--;
        User[userId].strength++;

        network->SendStatStr(userId, User[userId].strength);
        network->SendStatPoints(userId, User[userId].stat_points);
    }
}

// [STAT_DEF]
void SKO_PacketHandler::parseStatDef(unsigned char userId, SKO_PacketParser* parser)
{
    if (User[userId].stat_points > 0)
    {
        User[userId].stat_points--;
        User[userId].defence++;

        network->SendStatDef(userId, User[userId].defence);
        network->SendStatPoints(userId, User[userId].stat_points);
    }
}


// [EQUIP][(unsigned char)equipSlot]
void SKO_PacketHandler::parseUnequip(unsigned int userId, SKO_PacketParser *parser)
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
            network->SendEquip(uc, userId, slot, (char)0, (char)0);

        //update the player's inventory
        int amount = User[userId].inventory[item];
        network->SendPocketItem(userId, item, amount);
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
        unsigned int amt = 0;
        float numy, numx, numys, numxs,
            rand_xs, rand_ys,
            rand_x, rand_y;
        int rand_i, rand_item;

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
            network->SendStatHp(userId, User[userId].hp);

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
                    network->SendBuddyStatHp(pl, userId, displayHp);
            }

            //put in client players inventory
            network->SendPocketItem(userId, item, amount);

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
                network->SendPocketItem(userId, otherItem, amount);
            }

            // Tell player one weapon is removed from their inventory
            unsigned int amount = User[userId].inventory[item];
            network->SendPocketItem(userId, item, amount);

            //tell everyone the player equipped their weapon
            for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
            {
                if (User[i1].Ident)
                    network->SendEquip(i1, userId, (char)0, Item[item].equipID, item);
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
                network->SendPocketItem(userId, otherItem, amount);
            }

            // Tell player one hat is removed from their inventory
            unsigned int amount = User[userId].inventory[item];
            network->SendPocketItem(userId, item, amount);

            //tell everyone the player equipped their hat
            for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
            {
                if (User[i1].Ident)
                    network->SendEquip(i1, userId, (char)1, Item[item].equipID, item);
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
                network->SendPocketItem(userId, item, amount);

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
                            network->SendSpawnItem(iii, rand_i, mapId, rand_item, x, y, x_speed, y_speed);
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
                network->SendPocketItem(userId, otherItem, amount);
            }

            // Tell player one trophy is removed from their inventory
            unsigned int amount = User[userId].inventory[item];
            network->SendPocketItem(userId, item, amount);

            //tell everyone the player equipped their trophy
            for (int i1 = 0; i1 < MAX_CLIENTS; i1++)
            {
                if (User[i1].Ident)
                    network->SendEquip(i1, userId, (char)2, Item[item].equipID, item);
            }
            break;
        }
        default:
            break;
        } //end switch
    }     //end if you have the item
}

// [DROP_ITEM][(unsigned char)item][]
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
        network->SendPocketItem(userId, item, ownedAmount);

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
                network->SendSpawnItem(iii, rand_i, mapId, item, x, y, x_speed, x_speed);
        }
    }
}


std::string SKO_PacketHandler::nextParameter(std::string &parameters)
{
    //grab first parameter from list
    std::string next = parameters.substr(0, parameters.find_first_of(" ") + 1);

    //shrink list of parameters 
    parameters = parameters.substr(parameters.find_first_of(" ") + 1);
    
    //return a single parameter; the next in line
    return next;
}


void SKO_PacketHandler::parseSlashBan(unsigned char userId, std::string parameters)
{
    std::string username = nextParameter(parameters);
    std::string reason = parameters;

    int result = network->banPlayer(userId, username, reason, 1);
    if (result == 0)
    {
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (User[i].Ident)
            {
                // Send data
                network->sendChat(userId, username + " has been banned. (" + reason + ")");

                //find which socket, yo
                if (lower(User[i].Nick).compare(lower(username)) == 0)
                {
                    User[i].Sock->Close();
                }
            }
        }
    }
    else if (result == 1)
    {
        network->sendChat(userId, username + " does not exist.");
    }
    else if (result == 2)
    {
        network->sendChat(userId, username + " cannot be banned.");
    }
    else if (result == 3)
    {
        printf("The user [%s] tried to ban [%s] but they are not moderator!\n", User[userId].Nick.c_str(), username.c_str());
        network->sendChat(userId, "You re not authorized to ban a player.");
    }
}

void SKO_PacketHandler::parseSlashUnban(unsigned char userId, std::string parameters)
{
    //strip the appropriate data
    std::string username = parameters;

    int result = repository->banPlayer(userId, username, "unban the player :)", 0);

    if (result == 0)
    {
        network->sendChat(userId, username + " has been unbanned.");
    }
    else if (result == 1)
    {
        network->sendChat(userId, username + " does not exist.");
    }
    else if (result == 2)
    {
        network->sendChat(userId, username + " cannot be unbanned.");
    }
    else if (result == 3)
    {
        printf("The user [%s] tried to unban [%s] but they are not moderator!\n", User[userId].Nick.c_str(), username.c_str());
        network->sendChat(userId, "You are not authorized to unban a player.");
    }
}

void SKO_PacketHandler::parseSlashMute(unsigned char userId, std::string parameters)
{
    //strip the appropriate data
    std::string username = nextParameter(parameters);
    std::string reason = parameters;

    int result = network->mutePlayer(userId, username, 1);
    if (result == 0)
    {
        //find the sock of the username
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            //well, unmute the person
            std::string lower_username = lower(username);
            std::string lower_nick = lower(User[i].Nick);

            if (lower_username.compare(lower_nick) == 0)
                User[i].Mute = true;

            if (User[i].Ident)
                network->sendChat(userId, username + " has been muted.");
        }
    }
    else if (result == 1)
    {
        network->sendChat(userId, username + " does not exist.");
    }
    else if (result == 2)
    {
        network->sendChat(userId, username + " cannot be muted,");
    }
    else if (result == 3)
    {
        printf("The user [%s] tried to mute [%s] but they are not moderator!\n", User[userId].Nick.c_str(), username.c_str());
        network->sendChat(userId, "You are not authorized to mute players.");
    }
}


void SKO_PacketHandler::parseSlashUnmute(unsigned char userId, std::string parameters)
{
    //strip the appropriate data
    std::string username = nextParameter(parameters);
    
    int result = network->mutePlayer(userId, username, 0);
    if (result == 0)
    {
        //find the sock of the username
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            // well, unmute the person
            if (lower(User[i].Nick).compare(lower(username)) == 0)
                User[i].Mute = false;

            // well, tell everyone
            // If this socket is taken
            if (User[i].Ident)
                network->sendChat(userId, username + " has been unmuted.");
        }
    }
    else if (result == 1)
    {
        network->sendChat(userId, username + " does not exist.");
    }
    else if (result == 2)
    {
        network->sendChat(userId, username + " cannot be muted.");
    }
    else if (result == 3)
    {
        printf("The user [%s] tried to unmute [%s] but they are not moderator!\n", User[userId].Nick.c_str(), username.c_str());
        network->sendChat(userId, "You are not authorized to mute players.");
    }
}

void SKO_PacketHandler::parseSlashKick(unsigned char userId, std::string parameters)
{
    std::string username = nextParameter(parameters);
    std::string Reason = parameters;
    
    int result = 0;
    result = network->kickPlayer(userId, username);

    if (result == 0)
    {
        //okay, now send
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            //if socket is taken
            if (User[i].Ident)
                network->sendChat(userId, username + " has been kicked. (" + Reason + ")");

            //kick the user
            if (lower(username).compare(lower(User[i].Nick)) == 0)
            {
                User[i].Sock->Close();
                User[i].Sock->Connected = false;
            }
        }
    }
    else if (result == 1)
    {
        network->sendChat(userId, username + " is not online.");
    }
    else if (result == 2)
    {
        printf("The user [%s] tried to kick [%s] but they are not moderator!\n", User[userId].Nick.c_str(), username.c_str());
        network->sendChat(userId, "You are not authorized to kick players.");
    }
}

void SKO_PacketHandler::parseSlashWho(unsigned char userId)
{
    //find how many players are online
    int players = 0;
    bool flag = false;
    std::string strNicks = "";

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        //if socket is taken
        if (User[i].Ident && User[i].Nick.compare("Paladin") != 0)
        {
            players++;

            //formatting
            if (flag)
                strNicks += ", ";
            else if (User[i].Ident)
                flag = true;

            //add the nickname to the list
            if (User[i].Ident)
                strNicks += User[i].Nick;
        }
    }

    //TODO change this
    char strPlayers[127];
    if (players == 1)
        sprintf(strPlayers, "There is %i player online: ", players);
    else
        sprintf(strPlayers, "There are %i players online: ", players);
    network->sendChat(userId, strPlayers);

    if (players > 0)
        network->sendChat(userId, strNicks);
}

void SKO_PacketHandler::parseSlashIpban(unsigned char userId, std::string parameters)
{
    std::string IP = nextParameter(parameters);
    std::string Reason = parameters;

    //TODO create SKO_Network function to wrap repository call.
    if (User[userId].Moderator)
    {
        int result = repository->banIP(User[userId].Nick, IP, Reason);
        if (result == 0)
        {
            network->sendChat(userId, "[" + IP + "] has been banned (" + Reason + ")");
        }
        else
        {
            network->sendChat(userId, "Could not ban IP [" + IP + "] for unknown error.");
        }
    }
    else
    {
        network->sendChat(userId, "You are not authorized to ban IP addresses.");
    }
}

void SKO_PacketHandler::parseSlashGetip(unsigned char userId, std::string parameters)
{
    std::string username = nextParameter(parameters);

    //TODO create SKO_Network function to wrap repository call.
    if (User[userId].Moderator)
    {
        std::string playerIP = repository->getIP(username);
        network->sendChat(userId, username + " has an IP of [" + playerIP + "]");
    }
    else
    {
        network->sendChat(userId, "You are not authorized to get IP addresses.");
    }
}

void SKO_PacketHandler::parseSlashWarp(unsigned char userId, std::string parameters)
{
    //TODO create SKO_Network function to wrap repository call.
    if (User[userId].Moderator)
    {

        std::string warp_user = nextParameter(parameters);
        int warp_x = atoi(nextParameter(parameters).c_str());
        int warp_y = atoi(nextParameter(parameters).c_str());
        int warp_m = atoi(nextParameter(parameters).c_str());

        printf("Warp %s to (%i,%i) on map [%i]\n", warp_user.c_str(), warp_x, warp_y, warp_m);

        //find user
        for (int wu = 0; warp_m >= NUM_MAPS && wu < MAX_CLIENTS; wu++)
        {
            if (User[wu].Ident && (lower(User[wu].Nick).compare(lower(warp_user)) == 0))
            {
                SKO_Portal warp_p = SKO_Portal();
                warp_p.spawn_x = warp_x;
                warp_p.spawn_y = warp_y;
                warp_p.map = warp_m;

                Warp(wu, warp_p);
                break;
            }
        }
    }
    else
    {
        network->sendChat(userId, "You are not authorized to warp players.");
    }
}

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

void SKO_PacketHandler::parseSlashCommand(unsigned char userId, std::string message)
{
    std::string command = message.substr(0, message.find_first_of(" ") - 2);
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
    int max = 62;
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
            int found = chatMessage.substr(0, max).find_last_of(" ");
            if (found > (int)userTag.length() && found > max - 6)
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

void SKO_PacketHandler::parsePacket(unsigned char userId, std::string packet)
{
    // Construct a packet parser from this packet
    SKO_PacketParser *parser = new SKO_PacketParser(packet);
    
    //Check packet sanity
    if (packet.length() < 2)
    {
        printf(kRed "[FATAL] SKO_PacketHandler::parsePacket() called with incomplete packet!\n" kNormal);
        printf(kRed "[       SKO_PacketHandler::parsePacket(%i, %s)] \n" kNormal, userId, parser->toString().c_str());
        return;
    }

    //Check packet sanity
    if (packet.length() < parser->getPacketLength())
    {
        printf(kRed "[FATAL] SKO_PacketHandler::parsePacket() called with incomplete packet!\n" kNormal);
        printf(kRed "[       SKO_PacketHandler::parsePacket(%i, %s)] \n" kNormal, userId, parser->toString().c_str());
        return;
    }
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
        parseRegister(userId, parser);
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

    default:
        // Disconnect clients sending nonsense packets
        //TODO//User[userId].Sock->Close();
        printf(kRed "[FATAL] SKO_PacketHandler::parsePacket() called with unknown packet type!\n" kNormal);
        printf(kRed "[       SKO_PacketHandler::parsePacket(%i, %s)] \n" kNormal, userId, parser->toString().c_str());
        break;
    }

//TODO - keep it up nate :) 
    else if (code == TRADE)
    {
        //what kind of trade action, yo?
        char action = User[userId].Sock->Data[2];
        char playerA = 0;
        char playerB = 0;

        switch (action)
        {

        //TODO: don't let a player confirm without checking trade statuses
        case INVITE:

            playerB = User[userId].Sock->Data[3];
            printf("userId=%i playerB=%i\n", userId, playerB);
            //FFFF, don't troll me. Is playerB even real? >___>
            if (playerB < MAX_CLIENTS && playerB != userId && User[playerB].Ident)
            {
                printf("someone wants to trade...legit?\n");
                // make sure both players aren't busy!!
                if (User[userId].tradeStatus == 0 && User[playerB].tradeStatus == 0)
                {
                    printf("%s trade with %s.\n", User[userId].Nick.c_str(), User[playerB].Nick.c_str());

                    //Send 'trade with A' to player B
                    network->sendTradeInvite(playerB, userId);

                    //hold status of what the players are trying to do
                    // (tentative...)
                    User[userId].tradeStatus = INVITE;
                    User[userId].tradePlayer = playerB;
                    User[playerB].tradeStatus = INVITE;
                    User[playerB].tradePlayer = userId;
                } // end not busy, lets request, bro!
                else
                {
                    printf("[ERR] A trade status=%i B trade status=%i\n", User[userId].tradeStatus, User[playerB].tradeStatus);
                }
            } //end invite trade

            break; //end INVITE

        case ACCEPT:
            //hold status of what the players are trying to do
            // (trading)
            playerB = User[userId].tradePlayer;

            if (playerB >= 0 && User[userId].tradeStatus == INVITE && User[playerB].tradeStatus == INVITE)
            {
                User[userId].tradeStatus = ACCEPT;
                User[playerB].tradeStatus = ACCEPT;
            }

            //tell both parties that they are now trading

            //Send 'trade with A' to player B
            network->sendTradeAccept(playerB, userId);

            //Send 'trade with B' to player A
            network->sendTradeAccept(userId, playerB);

            break;

        case CANCEL:

            //okay, kill the trade for this player and the sellected player
            playerB = User[userId].tradePlayer;

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

            break;

        case OFFER:

            playerB = User[userId].tradePlayer;
            //only do something if both parties are in accept trade mode
            if (User[userId].tradeStatus == ACCEPT && User[User[userId].tradePlayer].tradeStatus == ACCEPT)
            {
                unsigned char item = User[userId].Sock->Data[3];

                int amount = 0;
                ((char *)&amount)[0] = User[userId].Sock->Data[4];
                ((char *)&amount)[1] = User[userId].Sock->Data[5];
                ((char *)&amount)[2] = User[userId].Sock->Data[6];
                ((char *)&amount)[3] = User[userId].Sock->Data[7];

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

            break;

        case CONFIRM:
            //easy to understand if we use A & B
            playerA = userId;
            playerB = User[userId].tradePlayer;

            if (playerB < 0)
            {
                //tell both players cancel trade
                network->sendTradeCancel(userId);
                User[userId].tradeStatus = -1;
                User[userId].tradePlayer = 0;
                break;
            }

            //make sure playerA is in accept mode before confirming
            if (User[playerA].tradeStatus == ACCEPT)
                User[playerA].tradeStatus = CONFIRM;

            //tell both players :)
            network->sendTradeReady(playerA, (char)1);
            network->sendTradeReady(playerB, (char)2);

            //if both players are now confirm, transact and reset!
            if (User[playerA].tradeStatus == CONFIRM && User[playerB].tradeStatus == CONFIRM)
            {
                //lets make sure players have that many items
                bool error = false;
                for (int i = 0; i < 256; i++)
                {
                    //compare the offer to the owned items
                    if (User[playerA].inventory[i] < User[playerA].localTrade[i])
                    {
                        error = true;
                        break;
                    }
                    //compare the offer to the owned items
                    if (User[playerB].inventory[i] < User[playerB].localTrade[i])
                    {
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
                if (!error)
                {
                    printf("trade transaction!\n");

                    //go through each item and add them
                    for (unsigned char i = 0; i < 256; i++)
                    {
                        //easy to follow with these variables broski :P
                        int aOffer = User[playerA].localTrade[i];
                        int bOffer = User[playerB].localTrade[i];

                        //give A's stuff  to B
                        if (aOffer > 0)
                        {
                            printf(kGreen "%s gives %i of item %i to %s\n" kNormal, User[playerA].Nick.c_str(), aOffer, i, User[playerB].Nick.c_str());
                            //trade the item and tell the player!
                            User[playerB].inventory[i] += aOffer;
                            User[playerA].inventory[i] -= aOffer;

                            //put in players inventory
                            unsigned int amount = User[playerB].inventory[i];
                            network->SendPocketItem(playerB, i, amount);

                            //take it out of A's inventory
                            amount = User[playerA].inventory[i];
                            network->SendPocketItem(playerA, i, amount);
                        }

                        //give B's stuff  to A
                        if (bOffer > 0)
                        {
                            printf(kGreen "%s gives %i of item %i to %s\n" kNormal, User[playerB].Nick.c_str(), aOffer, i, User[playerA].Nick.c_str());
                            //trade the item and tell the player!
                            User[playerA].inventory[i] += bOffer;
                            User[playerB].inventory[i] -= bOffer;

                            //put in players inventory
                            unsigned int amount = User[playerA].inventory[i];
                            network->SendPocketItem(playerA, i, amount);

                            //take it away from B
                            amount = User[playerB].inventory[i];
                            network->SendPocketItem(playerB, i, amount);
                        }

                        //clear the items
                        User[playerA].localTrade[i] = 0;
                        User[playerB].localTrade[i] = 0;
                    }

                    //Save all profiles after a trade
                    network->saveAllProfiles();
                }
            }
            break;
        } //end trade action switch

    } //invite to trade
    else if (code == PARTY)
    {
        int mode = User[userId].Sock->Data[2];
        unsigned char playerB = 0;
        int partyID = -1;

        switch (mode)
        {

        case INVITE:

            playerB = User[userId].Sock->Data[3];

            //invite the other user
            //if the other user is not in your party
            if (User[playerB].partyStatus == 0)
            {
                partyID = User[userId].party;

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
            break;

        case ACCEPT:
            //only if I was invited :)
            if (User[userId].partyStatus == INVITE)
            {
                //tell everyone in the party that player has joined
                for (int pl = 0; pl < MAX_CLIENTS; pl++)
                    //tell the user about all the players in the party
                    if (User[pl].Ident && pl != userId && User[pl].partyStatus == PARTY && User[pl].party == User[userId].party)
                    {
                        //
                        // Notify all members in the party that player is joining
                        //
                        network->sendPartyAccept(pl, userId, User[userId].party);
                        //tell existing party members the new player's stats
                        unsigned char hp = (int)((User[userId].hp / (float)User[userId].max_hp) * 80);
                        unsigned char xp = (int)((User[userId].xp / (float)User[userId].max_xp) * 80);
                        unsigned char level = User[userId].level;

                        network->SendBuddyStatHp(pl, userId, hp);
                        network->SendBuddyStatXp(pl, userId, xp);
                        network->SendBuddyStatLevel(pl, userId, level);

                        //
                        // Notify player of each existing party member
                        //
                        network->sendPartyAccept(userId, pl, User[pl].party);
                        // Send player the current party member stats
                        hp = (int)((User[pl].hp / (float)User[pl].max_hp) * 80);
                        xp = (int)((User[pl].xp / (float)User[pl].max_xp) * 80);
                        level = User[pl].level;

                        // Update party stats for client party player list
                        network->SendBuddyStatHp(userId, pl, hp);
                        network->SendBuddyStatXp(userId, pl, xp);
                        network->SendBuddyStatLevel(userId, pl, level);
                    }
            }
            break;

        case CANCEL:
            //quit them out of this party
            quitParty(userId);
            break;

        default:
            break;
        } //end switch mode
    }     //invite to party
    else if (code == CLAN)
    {
        int mode = User[userId].Sock->Data[2];
        int playerB = 0;
        std::string clanId = "";

        printf("Clan mode is %i\n", mode);
        switch (mode)
        {

        case INVITE:

            if (User[userId].Clan[0] == '(')
            {
                network->sendChat(userId, "You are not in a clan.");
                return;
            }

            if (User[playerB].Clan[0] != '(')
            {
                network->sendChat(userId, "Sorry, " + User[playerB].Nick + " Is already in a clan.");
                return;
            }

            playerB = User[userId].Sock->Data[3];
            clanId = repository->getOwnerClanId(User[userId].Nick);

            if (!clanId.length())
            {
                network->sendChat(userId, "Sorry, only the clan owner can invite new members.");
                return;
            }

            //set the clan status of the invited person
            User[playerB].clanStatus = INVITE;
            User[playerB].tempClanId = clanId;

            //ask the user to clan
            network->sendClanInvite(playerB, userId);

            break;

        case ACCEPT:
            //TODO do not disconnect the player when they accept clan invites
            //only if I was invited :)
            if (User[userId].clanStatus == INVITE)
            {
                repository->setClanId(User[userId].Nick, User[userId].tempClanId);

                // TODO - make new packet type to update clan instead of booting player
                // This relies on client to automatically reconnect
                User[userId].Sock->Close();
                User[userId].Sock->Connected = false;
            }

            break;

        case CANCEL:
            //quit them out of this clan
            User[userId].clanStatus = -1;
            break;

        default:
            break;
        } //end switch mode
    }     //invite to clan
    else if (code == SHOP)
    {
        int action = User[userId].Sock->Data[2];
        int stallId = 0;
        unsigned char mapId = User[userId].mapId;

        switch (action)
        {
        case INVITE:
            stallId = User[userId].Sock->Data[3];
            if (stallId < map[mapId].num_stalls)
            {
                if (map[mapId].Stall[stallId].shopid == 0)
                {
                    if (User[userId].tradeStatus < 1)
                    {
                        network->sendBankOpen(userId);

                        //set trade status to banking
                        User[userId].tradeStatus = BANK;
                    }
                }
                if (map[mapId].Stall[stallId].shopid > 0)
                {
                    //tell the user "hey, shop #? opened up"
                    unsigned char shopId = map[mapId].Stall[stallId].shopid;
                    network->sendShopOpen(userId, shopId);

                    //set trade status to shopping
                    User[userId].tradeStatus = SHOP;
                    User[userId].tradePlayer = map[mapId].Stall[stallId].shopid;
                }
            }
            break;

        case BUY:
        {
            int sitem, amount;
            sitem = User[userId].Sock->Data[3];
            //hold the result...
            //build an int from 4 bytes
            ((char *)&amount)[0] = User[userId].Sock->Data[4];
            ((char *)&amount)[1] = User[userId].Sock->Data[5];
            ((char *)&amount)[2] = User[userId].Sock->Data[6];
            ((char *)&amount)[3] = User[userId].Sock->Data[7];

            int i = 0, item = 0, price = 0;
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 6; x++)
                {
                    if (i == sitem)
                    {
                        item = map[mapId].Shop[User[userId].tradePlayer].item[x][y][0];
                        price = map[mapId].Shop[User[userId].tradePlayer].item[x][y][1];
                    }
                    i++;
                }

            //printf("%s wants to buy %d of item #%d\n", User[userId].Nick.c_str(), amount, item);

            //if the player can afford it
            if (item > 0 && User[userId].inventory[ITEM_GOLD] >= price * amount)
            {
                //take away gold
                User[userId].inventory[ITEM_GOLD] -= price * amount;

                //give them the item
                User[userId].inventory[item] += amount;

                //put in client players inventory
                unsigned int amount = User[userId].inventory[item];
                network->SendPocketItem(userId, item, amount);

                //Take gold out of player's inventory
                amount = User[userId].inventory[ITEM_GOLD];
                network->SendPocketItem(userId, (unsigned char)ITEM_GOLD, amount);
            }
        }

        break;

        case SELL:
        {
            printf("SELL baby!\n");
            int item = 0, amount = 0, price = 0;
            item = User[userId].Sock->Data[3];
            price = Item[item].price;

            //hold the result...
            //build an int from 4 bytes
            ((char *)&amount)[0] = User[userId].Sock->Data[4];
            ((char *)&amount)[1] = User[userId].Sock->Data[5];
            ((char *)&amount)[2] = User[userId].Sock->Data[6];
            ((char *)&amount)[3] = User[userId].Sock->Data[7];

            //if they even have the item to sell
            if (price > 0 && User[userId].inventory[item] >= amount)
            {
                //take away gold
                User[userId].inventory[item] -= amount;

                //give them the item
                User[userId].inventory[ITEM_GOLD] += price * amount;

                //take out of client player's inventory
                unsigned int amount = User[userId].inventory[item];
                network->SendPocketItem(userId, item, amount);

                //put gold into player's inventory
                amount = User[userId].inventory[ITEM_GOLD];
                network->SendPocketItem(userId, (char)ITEM_GOLD, amount);
            }
        }

        break;

        case CANCEL:
            printf("CANCEL, BABY!\n");
            User[userId].tradeStatus = 0;
            User[userId].tradePlayer = 0;
            break;

        default:
            printf("DEFAULT BABY!\n");
            break;
        }
        printf("\n");
    } //end shop
    else if (code == BANK)
    {
        int action = User[userId].Sock->Data[2];
        std::string packet;
        switch (action)
        {
        case CANCEL:
            if (User[userId].tradeStatus == BANK)
            {
                User[userId].tradeStatus = 0;
            }
            break;

        case BANK_ITEM:
            if (User[userId].tradeStatus == BANK)
            { //TODO sanity check for packet length
                //get item type and amount
                unsigned char item = User[userId].Sock->Data[3];
                unsigned int amount = 0;
                ((char *)&amount)[0] = User[userId].Sock->Data[4];
                ((char *)&amount)[1] = User[userId].Sock->Data[5];
                ((char *)&amount)[2] = User[userId].Sock->Data[6];
                ((char *)&amount)[3] = User[userId].Sock->Data[7];

                if (item < NUM_ITEMS && User[userId].inventory[item] >= amount)
                {
                    User[userId].inventory[item] -= amount;
                    User[userId].bank[item] += amount;

                    //send deposit notification to user
                    unsigned int deposit = User[userId].bank[item];
                    network->SendBankItem(userId, item, deposit);

                    //update client player's inventory
                    unsigned int withdrawal = User[userId].inventory[item];
                    network->SendPocketItem(userId, item, withdrawal);
                }
            }
            break;

        case DEBANK_ITEM:
            if (User[userId].tradeStatus == BANK)
            { //TODO sanity check on packet length
                unsigned char item = User[userId].Sock->Data[3];
                unsigned int amount = 0;
                ((char *)&amount)[0] = User[userId].Sock->Data[4];
                ((char *)&amount)[1] = User[userId].Sock->Data[5];
                ((char *)&amount)[2] = User[userId].Sock->Data[6];
                ((char *)&amount)[3] = User[userId].Sock->Data[7];

                if (item < NUM_ITEMS && User[userId].bank[item] >= amount)
                {
                    User[userId].bank[item] -= amount;
                    User[userId].inventory[item] += amount;

                    //send deposit notification to user
                    unsigned int deposit = User[userId].bank[item];
                    network->SendBankItem(userId, item, amount);

                    //update client player's inventory
                    unsigned int withdrawal = User[userId].inventory[item];
                    network->SendPocketItem(userId, item, withdrawal);
                }
            }
            break;

        //shouldn't happen
        default:
            break;
        }
    } //end bank
    else if (code == INVENTORY)
    {
        printf("inventory order!\n");
        std::string inventory_order = User[userId].Sock->Data.substr(2);
        User[userId].inventory_order = inventory_order;
    }
    else if (code == MAKE_CLAN)
    {
        //check if the player has enough money.
        if (User[userId].inventory[ITEM_GOLD] < 100000) // 100000 TODO make a const for this
        {
            network->sendChat(userId, "You cannot afford to establish a clan. It costs 100K gold pieces.");
            return;
        } //end you did not have enough $$

        std::string clanTag = trim(User[userId].Sock->Data.substr(2));
        int result = repository->createClan(User[userId].Nick, clanTag);

        if (result == 0)
        {
            //send to all players so everyone knows a new clan was formed!
            for (int cu1 = 0; cu1 < MAX_CLIENTS; cu1++)
            {
                if (User[cu1].Ident)
                    network->sendChat(cu1, "Clan (" + clanTag + ") has been established by owner " + User[userId].Nick + ".");
            } //end loop all clients

            User[userId].inventory[ITEM_GOLD] -= 100000;

            // TODO - when below fix is complete, notify player their gold has decreased by 100K
            //TODO do not rely on client reconnect when forming or joining a clan
            User[userId].Sock->Close();
            User[userId].Sock->Connected = false;
        }
        else if (result == 1)
        {
            //Clan already exists
            network->sendChat(userId, "You cannot establish [" + clanTag + "] because it already exists.");
        }
    } //end make clan
}