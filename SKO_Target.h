#include "SKO_ItemObject.h"

#ifndef	__SKO_TARGET_H_
#define __SKO_TARGET_H_


#define TARGET_BULLSEYE 0
#define TARGET_CRATE	1
#define TARGET_KEG	2

class SKO_Target
{
	public:
		int 	x;
		int 	y;
		int 	w;
		int 	h;
		int 	id;
		bool 	active;
		int 	loot;
		int 	respawn_ticker;
		int	required_weapon;

	SKO_Target();
	SKO_Target(int x_in, int y_in, int w_in, int h_in, int id_in, int loot_in);
	SKO_ItemObject getLootItem();
};

#endif
