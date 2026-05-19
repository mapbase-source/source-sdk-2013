//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef MATSYSWIN_H
#define MATSYSWIN_H

#ifdef _WIN32
#pragma once
#endif


#include <mx/mxMatSysWindow.h>
#include "materialsystem/imaterialsystem.h"
#include "interface.h"

class ITexture;
class MatSysWindow : public mxMatSysWindow
{
public:

	// CREATORS
	MatSysWindow( mxWindow *parent, int x, int y, int w, int h, const char *label, int style );
	~MatSysWindow( );

	// MANIPULATORS
	void dumpViewport (const char *filename);
	virtual int handleEvent( mxEvent *event );
	virtual void draw( );

    void			*m_hWnd;
	// void			*m_hDC;

	CSysModule *m_hMaterialSystemInst;
	ITexture *m_pCubemapTexture;

};


extern MatSysWindow *g_MatSysWindow;
extern IMaterial *g_materialBackground;
extern IMaterial *g_materialWireframe;
extern IMaterial *g_materialFlatshaded;
extern IMaterial *g_materialSmoothshaded;
extern IMaterial *g_materialBones;
extern IMaterial *g_materialLines;
extern IMaterial *g_materialFloor;
extern IMaterial *g_materialVertexColor;
extern IMaterial *g_materialShadow;
extern IMaterial *g_materialHitbox;


#endif // GLAPP_H
