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

#include "SKO_Game/SKO_Player.h"
#include "SKO_Game/SKO_Portal.h"
#include "SKO_Game/SKO_ItemObject.h"
#include "SKO_Game/SKO_Item.h"
#include "SKO_Game/SKO_Map.h"

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
extern bool blocked(unsigned char mapId, float box1_x1, float box1_y1, float box1_x2, float box1_y2, bool npc);
extern void GiveLoot(int enemy, int player);
extern void Attack(unsigned char userId, float x, float y);
extern void Jump(unsigned char userId, float x, float y);
extern void Left(unsigned char userId, float x, float y);
extern void Right(unsigned char userId, float x, float y);
extern void Stop(unsigned char userId, float x, float y);
extern void GiveXP(unsigned char userId, int xp);
extern void quitParty(unsigned char userId);
extern void Warp(unsigned char userId, SKO_Portal *portal);
extern void SpawnLoot(unsigned char mapId, SKO_ItemObject lootItem);
extern void CastSpell(unsigned char userId);
extern void ShopBuy(unsigned char userId, unsigned char shopItem, unsigned int amount); 
extern void SpendStatPoint(unsigned char userId, unsigned char statId);
extern void UnequipItem(unsigned char userId, unsigned char slot);
extern void UseItem(unsigned char userId, unsigned char itemId);
extern void DropItem(unsigned char userId, unsigned char itemId, unsigned int amount); 
extern void InvitePlayerToTrade(unsigned char userId, unsigned char playerId);
extern void AcceptTrade(unsigned char userId);
extern void CancelTrade(unsigned char userId);
extern void OfferTradeItem(unsigned char userId, unsigned char itemId, unsigned int amount);
extern void ConfirmTrade(unsigned char userId);
extern void InvitePlayerToParty(unsigned char userId, unsigned char playerB);
extern void AcceptPartyInvite(unsigned char userId);
extern void OpenStall(unsigned char userId, unsigned char stallId);
extern void ShopSell(unsigned char userId, unsigned char itemId, unsigned int amount);
extern void DepositBankItem(unsigned char userId, unsigned char itemId, unsigned int amount);
extern void WithdrawalBankItem(unsigned char userId, unsigned char itemId, unsigned int amount);
extern void DeclineClanInvite(unsigned char userId);

#endif