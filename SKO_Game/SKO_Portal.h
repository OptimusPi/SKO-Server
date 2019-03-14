#ifndef __SKO_PORTAL_H_
#define __SKO_PORTAL_H_

class SKO_Portal{

public:

	SKO_Portal(){};

	//this portal rect
	int x, y, w, h;
	
	//level requirement
	int level_required;

	//where does this portal throw you?
	int spawn_x, spawn_y;
	//what map does it throw you to?
	unsigned char mapId;
};

#endif
