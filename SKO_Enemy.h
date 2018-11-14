#include <ctime>
#include <vector>
#include "SKO_ItemObject.h"
#include <cstdio>
#include <iostream>
#include <cstdlib>
#include <cmath>

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
      
      int defence;
      int strength;
      int hp;
	  int xp;
      int hp_max;
      int hp_draw;
 
      bool facing_right; 
      bool attacking;
      bool ground;
      int  current_frame;
      unsigned long int animation_ticker;
      unsigned long int attack_ticker;
	  
	  //loot
	  int lootAmount;
	  std::vector<Loot> lootItem;
      
	  //keep track of players who hit me
	  unsigned int dibsPlayer;
	  unsigned long int dibsTicker;
	  int dibsDamage[MAX_CLIENTS];
	  
	  
      //AI
      bool dead;
      unsigned int respawn_ticker;
      unsigned int AI_ticker;
      unsigned int AI_period;
      int      AI_pos;      
      
};

#endif
