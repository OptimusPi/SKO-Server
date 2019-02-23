#include "SKO_Map.h"
#include <sstream>

SKO_Map::SKO_Map()
{
    death_pit = 0;
    num_enemies = 0;
    num_portals = 0;
    num_shops = 0;
    num_stalls = 0;
    num_targets = 0;
    num_npcs = 0;
    
    for (int i = 0; i < 256; i++)
        ItemObj[i] = SKO_ItemObject();
}

SKO_Map::SKO_Map (std::string FileName)
{ 

    SKO_Map();
    std::stringstream mapFileLoc;
	mapFileLoc << "SKO_Content/" << FileName << ".map";

     printf("loading map: %s\n", FileName.c_str());
     std::ifstream MapFile(mapFileLoc.str().c_str(), std::ios::in|std::ios::binary|std::ios::ate);
      
     if (MapFile.is_open())
     {
        //loading variables
        std::ifstream::pos_type size;
        char * memblock;  

        //allocate memory
        size = MapFile.tellg();
        memblock = (char *)malloc(size);

        //load the file into memory
        MapFile.seekg (0, std::ios::beg);
        MapFile.read (memblock, size);
        //close file
        MapFile.close();

        //hold the result...
        unsigned int num;
        
        //build an int from 4 bytes
        ((char*)&num)[0] = memblock[0];
        ((char*)&num)[1] = memblock[1];
        ((char*)&num)[2] = memblock[2];
        ((char*)&num)[3] = memblock[3];

        
        //store the number into variables
        number_of_tiles = num;              

        //build an int from 4 bytes
        ((char*)&num)[0] = memblock[4];
        ((char*)&num)[1] = memblock[5];
        ((char*)&num)[2] = memblock[6];
        ((char*)&num)[3] = memblock[7];

        //store the number into variables
        number_of_fringe = num;
        
        //build an int from 4 bytes
        ((char*)&num)[0] = memblock[8];
        ((char*)&num)[1] = memblock[9];
        ((char*)&num)[2] = memblock[10];
        ((char*)&num)[3] = memblock[11];

        //store the number into variables
        number_of_rects = num;
        
        printf("tiles: %i\n", number_of_tiles);
        printf("fringe: %i\n", number_of_fringe);
        printf("rect: %i\n", number_of_rects);
        
        //
        //tiles
        //
        int last_i = 11;
        
        for (int i = 0; i < number_of_tiles; i++)
        {
            //9 bytes per tile silly ;)
            
            //build an int from 4 bytes
            ((char*)&num)[0] = memblock[last_i+1+i*9];
            ((char*)&num)[1] = memblock[last_i+2+i*9];
            ((char*)&num)[2] = memblock[last_i+3+i*9];
            ((char*)&num)[3] = memblock[last_i+4+i*9];

            //store the number into variables
            tile_x[i] = num;


            //build an int from 4 bytes
            ((char*)&num)[0] = memblock[last_i+5+i*9];
            ((char*)&num)[1] = memblock[last_i+6+i*9];
            ((char*)&num)[2] = memblock[last_i+7+i*9];
            ((char*)&num)[3] = memblock[last_i+8+i*9];

            //store the number into variables
            tile_y[i] = num;

            //store the number into variables
            tile[i] = memblock[last_i+9+i*9];
            
            
        }

        last_i += number_of_tiles*9;
        //
        //fringe tiles
        //
        for (int i = 0; i < number_of_fringe; i++)
        {
            //9 bytes per tile silly ;)
            
            //build an int from 4 bytes
            ((char*)&num)[0] = memblock[last_i+1+i*9];
            ((char*)&num)[1] = memblock[last_i+2+i*9];
            ((char*)&num)[2] = memblock[last_i+3+i*9];
            ((char*)&num)[3] = memblock[last_i+4+i*9];

            //store the number into variables
            fringe_x[i] = num;


            //build an int from 4 bytes
            ((char*)&num)[0] = memblock[last_i+5+i*9];
            ((char*)&num)[1] = memblock[last_i+6+i*9];
            ((char*)&num)[2] = memblock[last_i+7+i*9];
            ((char*)&num)[3] = memblock[last_i+8+i*9];

            //store the number into variables
            fringe_y[i] = num;

            //store the number into variables
            fringe[i] = memblock[last_i+9+i*9];
            
            
        }
        last_i += number_of_fringe * 9;
        //
        //rects
        //
        for (int i = 0; i < number_of_rects; i++)
        {
            //read the map file
            //build an int from 4 bytes
            ((char*)&num)[0] = memblock[last_i+1+i*16];
            ((char*)&num)[1] = memblock[last_i+2+i*16];
            ((char*)&num)[2] = memblock[last_i+3+i*16];
            ((char*)&num)[3] = memblock[last_i+4+i*16];

            //store the number into variables
            collision_rect[i].x = num;


            //build an int from 4 bytes
            ((char*)&num)[0] = memblock[last_i+5+i*16];
            ((char*)&num)[1] = memblock[last_i+6+i*16];
            ((char*)&num)[2] = memblock[last_i+7+i*16];
            ((char*)&num)[3] = memblock[last_i+8+i*16];

            //store the number into variables
            collision_rect[i].y = num;


            //build an int from 4 bytes
            ((char*)&num)[0] = memblock[last_i+9+i*16];
            ((char*)&num)[1] = memblock[last_i+10+i*16];
            ((char*)&num)[2] = memblock[last_i+11+i*16]; 
            ((char*)&num)[3] = memblock[last_i+12+i*16];

            //store the number into variables
            collision_rect[i].w = num;
            
            //build an int from 4 bytes
            ((char*)&num)[0] = memblock[last_i+13+i*16];
            ((char*)&num)[1] = memblock[last_i+14+i*16];
            ((char*)&num)[2] = memblock[last_i+15+i*16];
            ((char*)&num)[3] = memblock[last_i+16+i*16];


            //store the number into variables
            collision_rect[i].h = num;
         }

         free(memblock);
     } else {
           printf("Can't load map %s.\n", FileName.c_str());
           exit(1);
     }

     std::stringstream mapConfLoc; 
        mapConfLoc << "SKO_Content/" << FileName << ".ini";

	 //open map config file
     printf("Reading Map config from: %s\n", mapConfLoc.str().c_str());
	 INIReader configFile(mapConfLoc.str());
	 if (configFile.ParseError() < 0) {
		printf("error: Can't load '%s'\n", mapConfLoc.str().c_str());
		return;
	 }

	 //load portals
	 num_portals = configFile.GetInteger("count", "portals", 0);
	printf("num_portals is %i", num_portals);
	for (int portal = 0; portal < num_portals; portal++)
	{
		std::stringstream ss;
		ss << "portal" << portal;
		
		//load portal parameters
		Portal[portal] = new SKO_Portal();
                Portal[portal]->x = configFile.GetInteger(ss.str(), "x", 0);
                Portal[portal]->y = configFile.GetInteger(ss.str(), "y", 0);
                Portal[portal]->w = configFile.GetInteger(ss.str(), "w", 0);
                Portal[portal]->h = configFile.GetInteger(ss.str(), "h", 0);
                Portal[portal]->mapId = configFile.GetInteger(ss.str(), "map", 0);
                Portal[portal]->spawn_x = configFile.GetInteger(ss.str(), "spawn_x", 0);
                Portal[portal]->spawn_y = configFile.GetInteger(ss.str(), "spawn_y", 0);
                Portal[portal]->level_required = configFile.GetInteger(ss.str(), "level_required", 0);
	} 
	
	 //load enemies
	 num_enemies = configFile.GetInteger("count", "enemies", 0);
	 printf("num_enemies is %i\n", num_enemies);
	 for (int enemy = 0; enemy < num_enemies; enemy++){
		 std::stringstream ss;
		 ss << "enemy" << enemy;

		 //load sprite type from file
		 Enemy[enemy] = new SKO_Enemy();
		 Enemy[enemy]->x1 = configFile.GetInteger(ss.str(), "x1", 0);
		 Enemy[enemy]->x2 = configFile.GetInteger(ss.str(), "x2", 0);
		 Enemy[enemy]->y1 = configFile.GetInteger(ss.str(), "y1", 0);
                 Enemy[enemy]->y2 = configFile.GetInteger(ss.str(), "y2", 0);

		 Enemy[enemy]->sx = configFile.GetInteger(ss.str(), "spawn_x", 0);
		 Enemy[enemy]->sy = configFile.GetInteger(ss.str(), "spawn_y", 0);
		 Enemy[enemy]->x = Enemy[enemy]->sx;
		 Enemy[enemy]->y = Enemy[enemy]->sy;

		 Enemy[enemy]->hp_max = configFile.GetInteger(ss.str(), "hp", 0);
		 Enemy[enemy]->hp = Enemy[enemy]->hp_max;
		 Enemy[enemy]->strength = configFile.GetInteger(ss.str(), "strength", 0);
		 Enemy[enemy]->defense = configFile.GetInteger(ss.str(), "defense", 0);
		 Enemy[enemy]->xp = configFile.GetInteger(ss.str(), "xp", 0);

		Enemy[enemy]->lootAmount = configFile.GetInteger(ss.str(), "loots_dropped", 0);
		
		int loot_count = configFile.GetInteger(ss.str(), "loot_count", 0);

		for (int loot = 0; loot < loot_count; loot++)
		{
			std::stringstream ss1;
			ss1 << "loot" << loot << "_item";
			
			std::stringstream ss2;
                        ss2 << "loot" << loot << "_amount";

			std::stringstream ss3;
                        ss3 << "loot" << loot << "_chance";

			int loot_item = configFile.GetInteger(ss.str(), ss1.str(), 0);
			int loot_amount = configFile.GetInteger(ss.str(), ss2.str(), 0);
			int loot_chance = configFile.GetInteger(ss.str(), ss3.str(), 0);


			Enemy[enemy]->addLoot(loot_item, loot_amount, loot_chance);
			
			
		}
                 
		

	 }

	 //load shops
	 num_shops = configFile.GetInteger("count", "shops", 0);
	 printf("num_shops is %i\n", num_shops);

	 //start at i= 1 ... why?
	 //  because ID 0 is reserved for the bank
	 for (int i = 0; i <= num_shops; i++){
		 Shop[i] = SKO_Shop();
		 std::stringstream ss1;
		 ss1 << "shop" << i;
		 std::string shopStr = ss1.str();
		 //loop all the X Y places
		 for (int x = 0; x < 6; x++){
			 for (int y = 0; y < 4; y++){
				 std::stringstream ss2, ss3;
				 ss2 << "item"  << "_" << "x" << x << "_" << "y" << y;
				 ss3 << "price" << "_" << "x" << x << "_" << "y" << y;

				 //x  y  (item[0], price[1])
				 std::string itemStr = ss2.str().c_str();
				 std::string priceStr = ss3.str().c_str();

				 //now load from config file the items and prices
				 Shop[i].item[x][y][0]
				        = configFile.GetInteger(shopStr.c_str(), itemStr.c_str(), 0);
				 Shop[i].item[x][y][1]
				        = configFile.GetInteger(shopStr.c_str(), priceStr.c_str(), 0);
			 }
		 }

		 //all done loading shops for now.
		 //in the future, maybe a shop title etc.
	 }

	 //load stalls
	 num_stalls = configFile.GetInteger("count", "stalls", 0);
	 printf("num_stalls is %i\n", num_stalls);

	 for (int i = 0; i < num_stalls; i++){
		 std::stringstream ss1;
		 ss1 << "stall" << i;
		 std::string stallStr = ss1.str();

		 Stall[i] = SKO_Stall();
		 Stall[i].shopId = configFile.GetInteger(stallStr, "shopId", 0);
		 Stall[i].x      = configFile.GetInteger(stallStr, "x", 0);
		 Stall[i].y      = configFile.GetInteger(stallStr, "y", 0);
		 Stall[i].w      = configFile.GetInteger(stallStr, "w", 0);
		 Stall[i].h      = configFile.GetInteger(stallStr, "h", 0);
	 }

	 //load targets
	 num_targets = configFile.GetInteger("count", "targets", 0);
	 printf("num_targets is %i\n", num_targets);

	 for (int i = 0; i < num_targets; i++){
		 std::stringstream ss1;
		 ss1 << "target" << i;
		 std::string targetStr = ss1.str();

		 Target[i] = SKO_Target();
		 Target[i].x      = configFile.GetInteger(targetStr, "x", 0);
		 Target[i].y      = configFile.GetInteger(targetStr, "y", 0);
		 Target[i].w      = configFile.GetInteger(targetStr, "w", 0);
		 Target[i].h      = configFile.GetInteger(targetStr, "h", 0);
		 Target[i].loot   = configFile.GetInteger(targetStr, "loot", 0);
		printf("Target[%i]: (%i,%i)\n", i, Target[i].x, Target[i].y); 
	}

	 //load npcs
         num_npcs = configFile.GetInteger("count", "npcs", 0);
         printf("num_npcs is %i\n", num_npcs);

         for (int i = 0; i < num_npcs; i++){
                 std::stringstream ss1;
                 ss1 << "npc" << i;
                 std::string npcStr = ss1.str();

                 NPC[i] = new SKO_NPC();
                 NPC[i]->sx = NPC[i]->x     = configFile.GetInteger(npcStr, "x", 0);
                 NPC[i]->sy = NPC[i]->y     = configFile.GetInteger(npcStr, "y", 0);
                printf("NPC[%i]: (%i,%i)\n", i, (int)NPC[i]->x, (int)NPC[i]->y);
        }






	//when things fall this low they are erased or respawned
	 death_pit = configFile.GetInteger("map", "death_pit", 10000);
	 printf("death_pit is: %i\n", (int)death_pit);

	 spawn_x = configFile.GetInteger("map", "spawn_x", 0);
         printf("spawn_x is: %i\n", spawn_x);
 
	 spawn_y = configFile.GetInteger("map", "spawn_y", 0);
         printf("spawn_y is: %i\n", spawn_y);
}


