#include "library.h"

#ifndef __SKO_ITEM_H_
#define __SKO_ITEM_H_

class SKO_Item
{
      public:
             
      int w, h, reach, hp, str, def, type, equipID;
      int price;
      
      SKO_Item(int w_in, int h_in, int type_in, int def_in, int str_in, int hp_in, int reach_in, int equipID_in, int price_in);
      SKO_Item();
};

#endif
