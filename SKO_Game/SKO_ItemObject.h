
#ifndef __SKO_ITEMOBJECT_
#define __SKO_ITEMOBJECT_

class SKO_ItemObject
{
public:
      
      //is it used
      bool status;
      
      //which item is it
      unsigned char itemID;
      
      //physics
      float x, y, x_speed, y_speed;
      
      //value
      unsigned int amount;
      
      //who owns the item
      int owner;
      unsigned long long int ownerTicker;
      
      //constructor
      SKO_ItemObject();
      SKO_ItemObject(unsigned char ITEM_ID_in, float X, float Y, float X_speed, float Y_speed, unsigned int amount_in);
      
      //delete the object
      void remove();
};
#endif