#include "SKO_Enemy.h"

SKO_Enemy::SKO_Enemy()
{
	dead = false;
      facing_right = true;
      attacking = false;
      current_frame = 0;
      animation_ticker = 0;
      attack_ticker = 0;
      ground = true;
      hp_draw = 0;
	  lootAmount = 0;
	  dibsPlayer = -1;
	  dibsTicker = 0;
	  xp = 5;
	  
	  //damage done by each player
	  for (int i = 0; i < MAX_CLIENTS; i++)
		dibsDamage[i] = 0;
	AI_ticker = 0;
	AI_period = 0;
	x = 0;
	y = 0;
	x_speed = 0;
	y_speed = 0;
respawn_ticker = 0;
	
}

SKO_Enemy::SKO_Enemy(int x1_in, int y1_in, int x2_in, int y2_in, int sx_in, int sy_in)
{
SKO_Enemy();
     facing_right = true;
      attacking = false;
      current_frame = 0;
      animation_ticker = 0;
      attack_ticker = 0;
      ground = true;
      
     //collision rect
     x1 = x1_in;
     x2 = x2_in;
     y1 = y1_in;
     y2 = y2_in;
     //spawn
     sx = sx_in;
     sy = sy_in;
     
     defence = 1;
     strength = 4;
     
     AI_ticker = 0;
     AI_period = 3000; 
     AI_pos = 0;
     
     x = sx;
     y = sy;
     x_speed = 0;
     y_speed = 0;
     
     //TODO
     hp_max = 25;
     hp = hp_max;
	
	 lootAmount = 0;
	 dibsPlayer = -1;
	 dibsTicker = 0;
	 xp = 5;
	 
	 //damage done by each player
	  for (int i = 0; i < MAX_CLIENTS; i++)
		dibsDamage[i] = 0;

}

void SKO_Enemy::Respawn()
{
     printf("Enemy Respawn - Well, sx is %i and sy is %i and x1 is %i and x2 is %i\n", sx, sy, x1, x2);
     x = sx;
     y = sy;
     x_speed = 0;
     y_speed = 0;
     
     hp = hp_max;

     dibsPlayer = -1;
     dibsTicker = 0;	 
	 
	 //damage done by each player
	  for (int i = 0; i < MAX_CLIENTS; i++)
		dibsDamage[i] = 0;

     dead = false;
}

void SKO_Enemy::addLoot(unsigned int item_id, unsigned int amount, float chance)
{
	//make a new item drop for this enemy
	Loot loot;
	loot.item_id = item_id;
	loot.amount = amount;
	loot.chance = chance;
	
	//add to this enemy's loot vector
	lootItem.insert(lootItem.end(), loot);
}

SKO_ItemObject SKO_Enemy::getLootItem()
{
    //give them this item
    SKO_ItemObject item = SKO_ItemObject();
	bool itemGiven = false;
	
	int startRollAt = rand() % lootAmount;
	
	//printf("rolling...(start at %i", startRollAt);
	//roll until you get an item!
	for (int dontInf = 0; dontInf < 1000 && !itemGiven; dontInf++)
	{
		//loop through all my loot items!
		for (int i = startRollAt; i < lootItem.size(); i++)
		{
			//roll for this item!
			if (Roll(lootItem[i].chance))
			{
                item.itemID = lootItem[i].item_id;
				item.amount = lootItem[i].amount;
				item.owner = -1;
				itemGiven = true;
				break;
			}
		}
		
		if (itemGiven)
			printf("done!\n");
		else
			printf("rolling...");
		
		startRollAt = 0;
	}
	
	return item;
}

	   
bool Roll(float chance) // 1.00 = 100 %
{
	unsigned int moo = rand() + rand();
	moo *= 3.14;
	moo %= 1000000;
	if (moo < chance * 10000)
		return true;
	return false;
}



