#ifndef __SKO_CHATHANDLER_H_
#define __SKO_CHATHANDLER_H_

#include <string> 
#include <cstdlib>
#include "SKO_Network.h"

// Forward-declare connected classes
class SKO_Network;

class SKO_ChatHandler
{
public:
    SKO_ChatHandler(SKO_Network *network);
    void parseChat(unsigned char userId, std::string message);

private:
    void parseSlashBan(unsigned char userId, std::string parameters);
    void parseSlashUnban(unsigned char userId, std::string parameters);
    void parseSlashMute(unsigned char userId, std::string parameters);
    void parseSlashUnmute(unsigned char userId, std::string parameters);
    void parseSlashKick(unsigned char userId, std::string parameters);
    void parseSlashWho(unsigned char userId);
    void parseSlashIpban(unsigned char userId, std::string parameters); 
    void parseSlashGetip(unsigned char userId, std::string parameters);
    void parseSlashWarp(unsigned char userId, std::string parameters);
    void parseSlashPing(unsigned char userId, std::string parameters);
    void parseSlashCommand(unsigned char userId, std::string message);
    
    //Network for calling functions
    SKO_Network *network;
};
#endif