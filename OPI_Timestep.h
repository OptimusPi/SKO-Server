#ifndef __OPI_TIMESTEP_H_
#define __OPI_TIMESTEP_H_

#include <iostream>
 
class OPI_Timestep
{
      private:
            //indicator
            bool ready; 
             
            //timestep
            unsigned long long int frameTime;
            unsigned long long int currentTime;
            unsigned long long int accumulator;
            unsigned long long int newTime;

      public:
            //functions
            OPI_Timestep(unsigned int FPS);
            void Update();
            bool Check();
            
};

#endif
