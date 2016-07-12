#include "SKO_Target.h"

SKO_Target::SKO_Target()
{	
	x    = 0;
	y    = 0;
	w    = 0;
	h    = 0;
	id   = 0;
	loot = 0; 
	active = true;
	required_weapon = -1;
}

SKO_Target::SKO_Target(int x_in, int y_in, int w_in, int h_in, int id_in, int loot_in)
{
	x    = x_in;
	y    = y_in;
	w    = w_in;
	h    = h_in;
	id   = id_in;
	loot = loot_in;
	active = true;
	required_weapon = -1;
}

SKO_ItemObject SKO_Target::getLootItem()
{
    //give them this item
    SKO_ItemObject item = SKO_ItemObject(
    	loot,
	x + (w/2),
	y,
	0,
	-3,
	1
    );

        return item;
}
