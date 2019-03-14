#include <ctime>
#include <vector>
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cmath>

#include "SKO_ItemObject.h"

#define MAX_CLIENTS 16

#ifndef __SKO_ENEMY_H_
#define __SKO_ENEMY_H_

bool Roll(float chance);

typedef struct
{
	unsigned int item_id;
	unsigned int amount;
	float chance;
} Loot;

class SKO_Enemy
{
	public:
		SKO_Enemy();
		SKO_Enemy(int x1_in, int y1_in, int x2_in, int y2_in, int sx_in, int sy_in);  
		void Respawn();
		void addLoot(unsigned int item_id, unsigned int amount, float chance);
		SKO_ItemObject getLootItem();

		//coords
		float x, y;
		float x_speed, y_speed;
		//collision rect
		int x1,x2,y1,y2;
		//spawn
		int sx, sy;

		unsigned int defense;
		unsigned int strength;
		unsigned int hp;
		unsigned int xp;
		unsigned int hp_max;
		int hp_draw;

		bool facing_right; 
		bool attacking;
		bool ground;
		int  current_frame;
		unsigned long long int animation_ticker;
		unsigned long long int attack_ticker;

		//loot
		int lootAmount;
		std::vector<Loot> lootItem;

		//keep track of players who hit me
		int dibsPlayer;
		unsigned long long int dibsTicker;
		int dibsDamage[MAX_CLIENTS];


		//AI
		bool dead;
		unsigned long long int respawn_ticker;
		unsigned long long int AI_ticker;
		unsigned long long int AI_period;
		int      AI_pos;      
      
};

#endif
