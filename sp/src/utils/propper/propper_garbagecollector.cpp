//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================//
//
// Purpose: Contains all the vbsp code that is needed for propper to compile.
//
// Author(s): Unusuario2
//
// $NoKeywords: $
//=======================================================================================//

// Explanation: Since Propper shares most of its code with VBSP, there is a lot of unused code  
// that is required for compiling Propper. This unused code clutters the codebase, making it  
// messier and much harder to understand.  
//
// To avoid these issues, I have created separate .cpp/.h files to keep the Propper source code  
// clean and readable. These files should only be modified when major changes occur in VBSP.  
// Otherwise, they should remain untouched.

#include "propper_garbagecollector.h"

//-------------------------------+---------------+---------------------------------------//
//-------------------------------| vbsp.cpp code |---------------------------------------//
//-------------------------------+---------------+---------------------------------------//
vec_t		microvolume = 1.0;
qboolean	noprune;
qboolean	glview;
qboolean	nodetail;
qboolean	fulldetail;
qboolean	onlyents;
qboolean	nomerge;
qboolean	nomergewater = false;
qboolean	nowater;
qboolean	nocsg;
qboolean	noweld;
qboolean	noshare;
qboolean	nosubdiv;
qboolean	notjunc;
qboolean	noopt;
qboolean	leaktest;
qboolean	verboseentities;
qboolean	dumpcollide = false;
qboolean	g_bLowPriority = false;
qboolean	g_DumpStaticProps = false;
qboolean	g_bSkyVis = false;			// skybox vis is off by default, toggle this to enable it
bool		onlyprops;
bool		g_bLightIfMissing = false;
bool		g_snapAxialPlanes = false;
bool		g_bKeepStaleZip = false;
bool		g_NodrawTriggers = false;
bool		g_DisableWaterLighting = false;
bool		g_bAllowDetailCracks = false;
bool		g_bNoVirtualMesh = false;
bool		g_BumpAll = false;
bool		g_bNoHiddenManifestMaps = false;
bool		g_bNoDefaultCubemaps = true;
bool		g_bSkyboxCubemaps = false;
bool		g_bPropperInsertAllAsStatic = false;
bool		g_bPropperStripEntities = false;
int			g_iDefaultCubemapSize = 32;
int			noleaktest = true;
int			g_nDXLevel = 0; // default dxlevel if you don't specify it on the command-line.
float		g_defaultLuxelSize = DEFAULT_LUXEL_SIZE;
float		g_luxelScale = 1.0f;
float		g_minLuxelScale = 1.0f;

CUtlVector<int> g_SkyAreas;


bool Is3DSkyboxArea(int area)
{
	for (int i = g_SkyAreas.Count(); --i >= 0; )
	{
		if (g_SkyAreas[i] == area)
			return true;
	}
	return false;
}


bool IsFuncOccluder(int entity_num)
{
	entity_t* mapent = &entities[entity_num];
	const char* pClassName = ValueForKey(mapent, "classname");
	return (strcmp("func_occluder", pClassName) == 0);
}
//--------------------------------+-------------------+----------------------------------//
//--------------------------------| End vbsp.cpp code |----------------------------------//
//--------------------------------+-------------------+----------------------------------//