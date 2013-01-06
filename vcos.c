static int initialized = 0;

#ifdef PI

#include "vcos.h"

void initializeVCOS()
{
	if (initialized) return;

	vcos_init();

	initialized = 1;
}

#else

void initializeVCOS()
{
	initialized = 1;
}

#endif
