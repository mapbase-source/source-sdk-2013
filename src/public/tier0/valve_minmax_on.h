//========= Copyright Valve Corporation, All rights reserved. ============//
#if !defined(POSIX) && (!defined(SDK_MP) || !defined(MINMAX_H))
#ifndef min
	#define min(a,b)  (((a) < (b)) ? (a) : (b))
#endif
#ifndef max
	#define max(a,b)  (((a) > (b)) ? (a) : (b))
#endif
#endif
