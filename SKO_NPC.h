#ifndef __SKO_NPC_H_
#define __SKO_NPC_H_


#include <iostream>
#include <cstdio>

class SKO_NPC
{

public:
	  SKO_NPC();
      void Respawn();
      
      //coords
      float x, y;
      int x1,x2,y1,y2;
	float x_speed, y_speed;
int w, h;
      //spawn
      int sx, sy;
     int AI_ticker;       
	int AI_period;
       
      bool facing_right; 
      bool ground;
      int current_frame;
      int animation_ticker;




};

#endif

