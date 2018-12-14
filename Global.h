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


//Quit all threads global flag
extern bool SERVER_QUIT;

//Shared objects
extern SKO_Player User[];

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


extern void despawnTarget(int target, int current_map);
extern void spawnTarget(int target, int current_map);
extern void Respawn(int current_map, int i);
extern void Warp(int i, SKO_Portal portal);
extern void DivideLoot(int enemy, int party);
extern void KillEnemy(int current_map, int enemy);
extern void SpawnLoot(int current_map, SKO_ItemObject loot);
extern void GiveLoot(int enemy, int player);
extern void EnemyAttack(int i, int current_map);
extern void Attack(int CurrSock, float x, float y);
extern void Jump(int CurrSock, float x, float y);
extern void Left(int CurrSock, float x, float y);
extern void Right(int CurrSock, float x, float y);
extern void Stop(int CurrSock, float x, float y);
extern void GiveXP(int CurrSock, int xp);
extern void quitParty(int CurrSock);