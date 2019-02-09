
#ifndef __SKO_STALL_H_
#define __SKO_STALL_H_

class SKO_Stall
{
      //to hold shops and banks...maybe more!
      public:
      
      //where is this stand at?
      int x, y;
      //how wide is it? how tall? I want a button for it man.
      int w, h;
      
            
      //is it a shop?
      int shopid;
      
      SKO_Stall(int shopid, int x_in, int y_in, int w_in, int h_in);
      SKO_Stall();
};

#endif
