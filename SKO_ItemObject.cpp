#include "SKO_ItemObject.h"

SKO_ItemObject::SKO_ItemObject()
{
          status = false;
          itemID = 0;
          x = 0;
          y = 0;
          x_speed = 0;
          y_speed = 0;
          amount = 1;
	  owner = -1;
	  ownerTicker = 0;
}

SKO_ItemObject::SKO_ItemObject(char ITEM_ID_in, float X, float Y, float X_speed, float Y_speed, int amount_in)
{
          //set the tiem (i.e. gold)
          itemID = ITEM_ID_in;  
               
          //set it to true   
          status = true;   
          
          x = X;
          y = Y;
          x_speed = X_speed;
          y_speed = Y_speed;
          amount = amount_in;
	  owner = -1;
	  ownerTicker = 0;
}

void SKO_ItemObject::remove()
{
           //set status to false
           status = false;
           
           itemID = 0;
           x = 0;
           y = 0;
           x_speed = 0;
           y_speed = 0;
}
