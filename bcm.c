static int initialized = 0;

#ifdef PI

#include "bcm.h"

void initializeBCM()
{
	if (initialized) return;

	bcm_host_init();

	initialized = 1;
}

#else

void initializeBCM()
{
	initialized = 1;
}

#endif
