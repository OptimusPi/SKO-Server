#include "SKO_NPC.h"


SKO_NPC::SKO_NPC()
{
x1 = 16;
y1 = 13;
x2 = 48;
y2 = 63;

	facing_right = false;
	ground = false;
	current_frame = 0;
	sx = 0;
	sy = 0;
	x_speed = 0;
	y_speed = 0;
	animation_ticker = 0;
	x = 0;
	y = 0;

		w = 0;
		h = 0;
	AI_ticker = 0;
	AI_period = 0;
}

void SKO_NPC::Respawn()
{
     printf("Well, npc respawn\n");//sx is %i and sy is %i and x1 is %i and x2 is %i\n", sx, sy, x1, x2);
     x = sx;
     y = sy;
     x_speed = 0;
     y_speed = 0;
     current_frame = 0;
     animation_ticker = 0;
}

