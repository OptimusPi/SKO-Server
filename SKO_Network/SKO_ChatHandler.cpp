#include "SKO_ChatHandler.h"
#include "../SKO_Utilities/SKO_Utilities.h"


SKO_ChatHandler::SKO_ChatHandler(SKO_Network *network)
{
    this->network = network;  
}

// [CHAT]["/ban"][" "]["<username>"][" "]["<reason>"]
void SKO_ChatHandler::parseSlashBan(unsigned char userId, std::string parameters)
{
    std::string username = SKO_Utilities::nextParameter(parameters);
    std::string reason = parameters;
    network->banPlayer(userId, username, reason);
}

// [CHAT]["/ban"][" "]["<username>"][" "]["<reason>"]
void SKO_ChatHandler::parseSlashUnban(unsigned char userId, std::string parameters)
{
    //strip the appropriate data
    std::string username = parameters;
    network->unbanPlayer(userId, username);
}

// [CHAT]["/mute"][" "]["<username>"][" "]["<reason>"]
void SKO_ChatHandler::parseSlashMute(unsigned char userId, std::string parameters)
{
    //strip the appropriate data
    std::string username =SKO_Utilities::nextParameter(parameters);
    std::string reason = parameters;
    network->mutePlayer(userId, username, reason);
}
 
// [CHAT]["/unmute"][" "]["<username>"]
void SKO_ChatHandler::parseSlashUnmute(unsigned char userId, std::string parameters)
{
    std::string username =SKO_Utilities::nextParameter(parameters);
    network->unmutePlayer(userId, username);
}

// [CHAT]["/ban"][" "]["<username>"][" "]["<reason>"]
void SKO_ChatHandler::parseSlashKick(unsigned char userId, std::string parameters)
{
    std::string username =SKO_Utilities::nextParameter(parameters);
    std::string reason = parameters;
    network->kickPlayer(userId, username, reason);
}

// [CHAT]["/who"]
void SKO_ChatHandler::parseSlashWho(unsigned char userId)
{
    //nothing special to parse here!
    network->sendWho(userId);
}

// [CHAT]["/ipban"][" "]["<ipv4 address>"][" "][<reason>]
void SKO_ChatHandler::parseSlashIpban(unsigned char userId, std::string parameters)
{
    std::string ip =SKO_Utilities::nextParameter(parameters);
    std::string reason = parameters;
    network->banAddress(userId, ip, reason);
}

// [CHAT]["/getip"][" "]["<player>"]
void SKO_ChatHandler::parseSlashGetip(unsigned char userId, std::string parameters)
{
    std::string username =SKO_Utilities::nextParameter(parameters);
    network->getIp(userId, username);
}
// [CHAT]["/warp"][" "]["<player>"][" "][(string)x][" "][(string)y][" "][(string)mapId]
void SKO_ChatHandler::parseSlashWarp(unsigned char userId, std::string parameters)
{
    std::string username = SKO_Utilities::nextParameter(parameters);
    int x = atoi(SKO_Utilities::nextParameter(parameters).c_str());
    int y = atoi(SKO_Utilities::nextParameter(parameters).c_str());
    int mapId = atoi(SKO_Utilities::nextParameter(parameters).c_str());
    network->warpPlayer(userId, username, x, y, mapId);                
}

// [CHAT]["/ping"]
void SKO_ChatHandler::parseSlashPing(unsigned char userId, std::string parameters)
{
    std::string username =SKO_Utilities::nextParameter(parameters);
    network->showPlayerPing(userId, username);
}

// [CHAT]["/"]["<command>"][" "]["<parameters>"]
void SKO_ChatHandler::parseSlashCommand(unsigned char userId, std::string message)
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

// TODO- Move to chat class? or somewhere else? NEtwork?
// [CHAT]["<message body, either a plain string or slash command>"]
void SKO_ChatHandler::parseChat(unsigned char userId, std::string message)
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