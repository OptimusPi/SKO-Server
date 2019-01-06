#ifndef __GLOBAL_H_
#define __GLOBAL_H_

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
extern bool SERVER_QUIT;

//TODO move to game logic
extern bool blocked(unsigned char mapId, float box1_x1, float box1_y1, float box1_x2, float box1_y2, bool npc);
extern void GiveLoot(int enemy, int player);
extern void Attack(unsigned char userId, float x, float y);
extern void Jump(unsigned char userId, float x, float y);
extern void Left(unsigned char userId, float x, float y);
extern void Right(unsigned char userId, float x, float y);
extern void Stop(unsigned char userId, float x, float y);
extern void GiveXP(unsigned char userId, int xp);
extern void quitParty(unsigned char userId);
extern void Warp(int userId, SKO_Portal portal);
extern void SpawnLoot(unsigned char mapId, SKO_ItemObject lootItem);

//Utilities
extern std::string trim(std::string s);
extern std::string lower(std::string myString) ;

#endif