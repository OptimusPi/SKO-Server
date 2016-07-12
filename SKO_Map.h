#include <fstream>
#include <cstring>
#include <SDL/SDL.h>
#include "SKO_Stall.h"
#include "SKO_Shop.h"
#include "SKO_Item.h"
#include "SKO_ItemObject.h"
#include "SKO_Enemy.h"
#include "SKO_item_defs.h"
#include "SKO_Portal.h"
#include "SKO_Target.h"
#include "SKO_NPC.h"

#include "INIReader.h"

#ifndef __SKO_MAP_H_
#define __SKO_MAP_H_

#define MAX_TILES 32768

//shop type
#define SHOP_BANK 0
#define SHOP_FOOD 1
#define SHOP_WEAPONS 2

class SKO_Map
{
      public:
         
	SDL_Rect collision_rect[MAX_TILES];
        int tile_x[MAX_TILES], tile_y[MAX_TILES];
        int fringe_x[MAX_TILES], fringe_y[MAX_TILES];
        unsigned char tile[MAX_TILES];
        unsigned char fringe[MAX_TILES];
        int number_of_tiles;
        int number_of_rects;
        int number_of_fringe;


	int spawn_x;
	int spawn_y;
	
	//current item that is going to be spawned
	int currentItem;
    
	//fall too much, then you die!
	float death_pit;
		
	//** entities that live on the map! **/
        //number existing for this map
        int num_enemies;
        int num_stalls;
        int num_shops; 
	int num_portals;       
 	int num_targets;
	int num_npcs;

        //a byte max for the amount for each
        SKO_Enemy *Enemy[25];
        SKO_ItemObject ItemObj[256];
        SKO_Stall Stall[25];
        SKO_Shop  Shop[25];
	SKO_Portal Portal[25];
	SKO_Target Target[25];
	SKO_NPC	*NPC[25];

      //initialize stuff!
      void init(int mp);
      
      //constructors
      SKO_Map();
      SKO_Map(std::string filename);
      
};

#endif
