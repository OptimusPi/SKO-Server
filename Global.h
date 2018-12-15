#pragma once

// TODO: Make this better, move it to a variadic function
//Colors for fun console log statements
#define kNormal     "\x1B[0m"
#define kRed        "\x1B[31m"
#define kGreen      "\x1B[32m"
#define kYellow     "\x1B[33m"
#define kBlue       "\x1B[34m"
#define kMagenta    "\x1B[35m"
#define kCyan       "\x1B[36m"
#define kWhite      "\x1B[37m"

#define MAX_CLIENTS 16

#include "SKO_Player.h"
#include "SKO_Portal.h"
#include "SKO_ItemObject.h"
#include "SKO_Item.h"
#include "SKO_Map.h"

//Quit all threads global flag
extern bool SERVER_QUIT;

//Shared objects
extern SKO_Player User[];
extern SKO_Item Item[];
extern SKO_Map map[];
extern unsigned char hpBar; 
extern const bool HOLIDAY;
extern unsigned const char HOLIDAY_NPC_DROP, HOLIDAY_BOX_DROP;
extern const char NUM_MAPS;

//TODO move to game logic
extern bool blocked(int current_map, float box1_x1, float box1_y1, float box1_x2, float box1_y2, bool npc);
extern void GiveLoot(int enemy, int player);
extern void Attack(int CurrSock, float x, float y);
extern void Jump(int CurrSock, float x, float y);
extern void Left(int CurrSock, float x, float y);
extern void Right(int CurrSock, float x, float y);
extern void Stop(int CurrSock, float x, float y);
extern void GiveXP(int CurrSock, int xp);
extern void quitParty(int CurrSock);

// TODO move to Utilities
void trim(std::string s)
{
	std::stringstream ss;
	ss << s;
	ss.clear();
	ss >> s;
}

std::string lower(std::string myString)
{
	const int length = myString.length();
	for (int i = 0; i != length; ++i)
	{
		myString[i] = std::tolower(myString[i]);
	}
	return myString;
}

