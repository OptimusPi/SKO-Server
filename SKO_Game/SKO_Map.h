#include <fstream>
#include <cstring>
#include "SKO_Stall.h"
#include "SKO_Shop.h"
#include "SKO_Item.h"
#include "SKO_ItemObject.h"
#include "SKO_Enemy.h"
#include "SKO_item_defs.h"
#include "SKO_Portal.h"
#include "SKO_Target.h"
#include "SKO_NPC.h"

#include "../OPI_Utilities/INIReader.h"

#ifndef __SKO_MAP_H_
#define __SKO_MAP_H_

#define MAX_TILES 32768

class SKO_Map
{
      public:
         
        //to avoid SDL dependency, just use struct SDL_Rect
        typedef struct SDL_Rect
        {
                signed short int x, y;
                unsigned short int w, h;
        } SDL_Rect;

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
	SKO_Portal *Portal[25];
	SKO_Target Target[25];
	SKO_NPC	*NPC[25];

      //initialize stuff!
      void init(int mp);
      
      //constructors
      SKO_Map();
      SKO_Map(std::string filename);
};

#endif
