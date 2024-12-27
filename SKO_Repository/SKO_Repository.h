#ifndef __SKO_REPOSITORY_H_
#define __SKO_REPOSITORY_H_

#include "../SKO_Hub/SKO_HubClient.h"

class SKO_Repository
{
	public:
	SKO_Repository(SKO_HubClient hubClient);
	std::string Connect();

	int loginPlayer(std::string username, std::string password);
	int loadPlayerData(unsigned char userId);
	bool savePlayerData(unsigned char userId);

	std::string GetClanTag(unsigned char userId, std::string defaultValue);
	void LogPlayerAddress(unsigned char userId);
	bool IsAddressBanned(std::string ipAddress);
	int BanAddress(std::string IP, std::string Reason);
    int createPlayer(std::string Username, std::string Password, std::string IP);
	std::string getOwnerClanId(std::string clanOwner);
	void setClanId(std::string username, std::string clanId);
	int createClan(std::string username, std::string clanId);

	//TODO change userId to moderatorName like in banIP
	int mutePlayer(unsigned char userId, std::string username, std::string reason, int flag);
	int banPlayer(unsigned char userId, std::string username, std::string reason, int flag);
	int banIP(std::string moderatorName, std::string IP, std::string reason);
	std::string getIP(std::string username);
	void saveServerStatus(unsigned int playersLinux, unsigned int playersWindows, unsigned int playersMac, unsigned int averagePing);

	private:
};

#endif
