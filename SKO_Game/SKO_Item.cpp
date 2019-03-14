#include "SKO_Item.h"

SKO_Item:: SKO_Item(int w_in, int h_in, int type_in, int def_in, int str_in, int hp_in, int reach_in, int equipID_in, int price_in)
{
        w = w_in;
        h = h_in;
        reach = 0;
        hp = hp_in;
        str = str_in;
        def = def_in;
        type = type_in; //0 nothing, 1 food, 2 weapon, 3 hat
        reach = reach_in;
        equipID = equipID_in;
        
        price = price_in;
}
SKO_Item::SKO_Item(){}
