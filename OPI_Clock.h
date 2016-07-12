#ifndef __OPI_CLOCK_H_
#define __OPI_CLOCK_H_

#include <SDL/SDL.h>

static unsigned int Clock()
{
	return (unsigned int)SDL_GetTicks();
}

static void Sleep(unsigned int ms)
{
	SDL_Delay(ms);
}
#endif
