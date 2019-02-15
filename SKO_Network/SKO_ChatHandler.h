#ifndef __SKO_CHATHANDLER_H_
#define __SKO_CHATHANDLER_H_

#include <string> 
#include <cstdlib>

class SKO_ChatHandler
{
public:
    SKO_ChatHandler();
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

    // Helper function to parse out the next string in a slash command.
    std::string nextParameter(std::string &parameters);
    
};
#endif