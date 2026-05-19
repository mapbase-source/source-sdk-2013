//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
/*

  glapp.c - Simple OpenGL shell
  
	There are several options allowed on the command line.  They are:
	-height : what window/screen height do you want to use?
	-width  : what window/screen width do you want to use?
	-bpp    : what color depth do you want to use?
	-window : create a rendering window rather than full-screen
	-fov    : use a field of view other than 90 degrees
*/


//
//                 Half-Life Model Viewer (c) 1999 by Mete Ciragan
//
// file:           MatSysWindow.cpp
// last modified:  May 04 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
// version:        1.2
//
// email:          mete@swissquake.ch
// web:            http://www.swissquake.ch/chumbalum-soft/
//
#include <mx/mx.h>
#include <mx/mxMessageBox.h>
#include <mx/mxTga.h>
#include <mx/mxPcx.h>
#include <mx/mxBmp.h>
#include <mx/mxMatSysWindow.h>
// #include "gl.h"
// #include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include "MatSysWin.h"
#include "MDLViewer.h"
#include "StudioModel.h"
#include "ControlPanel.h"
#include "ViewerSettings.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imaterialproxyfactory.h"
#include "FileSystem.h"
#include "materialsystem/IMesh.h"
#include "materialsystem/IMaterialSystemHardwareConfig.h"
#include "materialsystem/ITexture.h"
#include "materialsystem/MaterialSystem_Config.h"
#include "tier0/dbg.h"
#include "istudiorender.h"
#include "tier0/icommandline.h"
#include "vmatrix.h"
#include "studio_render.h"
#include "vstdlib/cvar.h"
#include "SoundEmitterSystem/isoundemittersystembase.h"
#include "soundsystem/isoundsystem.h"
#include "soundchars.h"
#include "camera.h"

extern char g_appTitle[];
extern bool g_bInError;
extern int g_dxlevel;

extern ISoundEmitterSystemBase *g_pSoundEmitterBase;
extern ISoundSystem *g_pSoundSystem;

CCamera g_cam;


void UpdateSounds()
{
	static double prev = 0;
	double curr = (double) mx::getTickCount () / 1000.0;
	if ( prev != 0 )
	{
		double dt = (curr - prev);
		g_pSoundSystem->Update( dt * g_viewerSettings.speedScale );
	}
	prev = curr;
}


// FIXME: move all this to mxMatSysWin

class DummyMaterialProxyFactory : public IMaterialProxyFactory
{
public:
	virtual IMaterialProxy *CreateProxy( const char *proxyName )	{return NULL;}
	virtual void DeleteProxy( IMaterialProxy *pProxy )				{}
};
DummyMaterialProxyFactory	g_DummyMaterialProxyFactory;


static void ReleaseMaterialSystemObjects()
{
	StudioModel::ReleaseStudioModel();
}

static void RestoreMaterialSystemObjects( int nChangeFlags )
{
	StudioModel::RestoreStudioModel();
	g_ControlPanel->OnLoadModel();
}

void InitMaterialSystemConfig(MaterialSystem_Config_t *pConfig)
{
	if ( g_viewerSettings.enableNormalMapping )
	{
		pConfig->m_Flags &= ~MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP;
	}
	else
	{
		pConfig->m_Flags |= MATSYS_VIDCFG_FLAGS_DISABLE_BUMPMAP;
	}

	if ( g_viewerSettings.enableSpecular)
	{
		pConfig->m_Flags &= ~MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR;
	}
	else
	{
		pConfig->m_Flags |= MATSYS_VIDCFG_FLAGS_DISABLE_SPECULAR;
	}

	if ( g_viewerSettings.enableParallaxMapping )
	{
		pConfig->m_Flags |= MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING;
	}
	else
	{
		pConfig->m_Flags &= ~MATSYS_VIDCFG_FLAGS_ENABLE_PARALLAX_MAPPING;
	}


	// JasonM...did we foul this up?

}

MatSysWindow *g_MatSysWindow = 0;

Vector g_vright( 50, 50, 0 );		// needs to be set to viewer's right in order for chrome to work

IMaterial *g_materialBackground = NULL;
IMaterial *g_materialWireframe = NULL;
IMaterial *g_materialFlatshaded = NULL;
IMaterial *g_materialSmoothshaded = NULL;
IMaterial *g_materialBones = NULL;
IMaterial *g_materialLines = NULL;
IMaterial *g_materialFloor = NULL;
IMaterial *g_materialVertexColor = NULL;
IMaterial *g_materialShadow = NULL;
IMaterial *g_materialHitbox = NULL;


IMaterial* LoadMat(const char* name, const char* group, bool complain = true)
{
	IMaterial* pMat = g_pMaterialSystem->FindMaterial(name, group, complain);
	if (pMat)
		pMat->AddRef();
	return pMat;
}

MatSysWindow::MatSysWindow (mxWindow *parent, int x, int y, int w, int h, const char *label, int style)
: mxMatSysWindow (parent, x, y, w, h, label, style)
{
	g_pMaterialSystem->SetMaterialProxyFactory( &g_DummyMaterialProxyFactory );

	m_pCubemapTexture = NULL;
	m_hWnd = (HWND)getHandle();

	MaterialSystem_Config_t config;
	config = g_pMaterialSystem->GetCurrentConfigForVideoCard();
	InitMaterialSystemConfig(&config);
	if ( g_dxlevel != 0 )
	{
		config.dxSupportLevel = g_dxlevel;
	}
	
//	config.m_VideoMode.m_Width = config.m_VideoMode.m_Height = 0;
	config.SetFlag( MATSYS_VIDCFG_FLAGS_WINDOWED, true );
	config.SetFlag( MATSYS_VIDCFG_FLAGS_RESIZING, true );

	if (!g_pMaterialSystem->SetMode( ( void * )m_hWnd, config ) )
	{
		return;
	}
	
	g_pMaterialSystem->OverrideConfig( config, false );

	g_pMaterialSystem->AddReleaseFunc( ReleaseMaterialSystemObjects );
	g_pMaterialSystem->AddRestoreFunc( RestoreMaterialSystemObjects );

	m_pCubemapTexture = g_pMaterialSystem->FindTexture( "hlmv/cubemap", NULL, true );
	m_pCubemapTexture->AddRef();
	g_pMaterialSystem->GetRenderContext()->BindLocalCubemap( m_pCubemapTexture );

	g_materialBackground	= LoadMat("hlmv/background", TEXTURE_GROUP_OTHER, true);
	g_materialWireframe		= LoadMat("debug/debugmrmwireframe", TEXTURE_GROUP_OTHER, true);
	g_materialFlatshaded	= LoadMat("debug/debugdrawflatpolygons", TEXTURE_GROUP_OTHER, true);
	g_materialSmoothshaded	= LoadMat("debug/debugmrmfullbright2", TEXTURE_GROUP_OTHER, true);
	g_materialBones			= LoadMat("debug/debugskeleton", TEXTURE_GROUP_OTHER, true);
	g_materialLines			= LoadMat("debug/debugwireframevertexcolor", TEXTURE_GROUP_OTHER, true);
	g_materialFloor			= LoadMat("hlmv/floor", TEXTURE_GROUP_OTHER, true);
	g_materialVertexColor   = LoadMat("debug/debugvertexcolor", TEXTURE_GROUP_OTHER, true);
	g_materialShadow		= LoadMat("hlmv/shadow", TEXTURE_GROUP_OTHER, true);
	g_materialHitbox		= LoadMat("debug/debughitbox", TEXTURE_GROUP_OTHER, true);

	if (!parent)
		setVisible (true);
	else
		mx::setIdleWindow (this);
}



MatSysWindow::~MatSysWindow ()
{
	if (m_pCubemapTexture)
	{
		m_pCubemapTexture->DecrementReferenceCount();
	}
	mx::setIdleWindow (0);
}



int
MatSysWindow::handleEvent (mxEvent *event)
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	static float oldrx = 0, oldry = 0, oldtz = 50, oldtx = 0, oldto = 0, oldty = 0;
	static float oldlrx = 0, oldlry = 0;
	static int oldx, oldy;

	switch (event->event)
	{

	case mxEvent::Idle:
	{
		static double prev;
		double curr = (double) mx::getTickCount () / 1000.0;
		double dt = (curr - prev);

		// clamp to 100fps
		if (dt >= 0.0 && dt < 0.01)
		{
			Sleep( 10 - dt * 1000.0 );
			return 1;
		}

		if ( prev != 0.0 )
		{
	//		dt = 0.001;

			g_pStudioModel->AdvanceFrame ( dt * g_viewerSettings.speedScale );
			g_ControlPanel->updateFrameSlider( );
			g_ControlPanel->updateGroundSpeed( );
		}
		prev = curr;

		if (!g_viewerSettings.pause)
			redraw ();

		g_ControlPanel->updateTransitionAmount();

		UpdateSounds();

		return 1;
	}
	break;

	case mxEvent::MouseWheeled:
	{
		g_cam.m_orbit.zoom -= event->wheeldelta;
	}
	break;

	case mxEvent::MouseUp:
	{
		g_viewerSettings.mousedown = false;
	}
	break;

	case mxEvent::MouseDown:
	{
		g_viewerSettings.mousedown = true;

		oldrx = g_cam.m_orbit.angles[0];
		oldry = g_cam.m_orbit.angles[1];
		oldto = g_cam.m_orbit.zoom;
		oldtx = g_cam.m_orbit.origin[0];
		oldty = g_cam.m_orbit.origin[1];
		oldtz = g_cam.m_orbit.origin[2];
		oldx = event->x;
		oldy = event->y;
		oldlrx = g_viewerSettings.lightrot[1];
		oldlry = g_viewerSettings.lightrot[0];
		g_viewerSettings.pause = false;

		float r = 1.0/3.0 * min( w(), h() );

		float d = sqrt( ( float )( (event->x - w()/2) * (event->x - w()/2) + (event->y - h()/2) * (event->y - h()/2) ) );

		if (d < r)
			g_viewerSettings.rotating = false;
		else
			g_viewerSettings.rotating = true;

		return 1;
	}
	break;

	case mxEvent::MouseDrag:
	{
		auto fnOrbitPanView = [&]() {
			Vector up, right;
			AngleVectors(g_cam.m_orbit.angles, nullptr, &right, &up);


			g_cam.m_orbit.origin = Vector(oldtx, oldty, oldtz)
					                - right * (float) (event->x - oldx) / 4.0f
					                + up    * (float) (event->y - oldy) / 4.0f;
		};
		
		if ( event->buttons & mxEvent::MouseMiddleButton )
		{
			fnOrbitPanView();
		} 
		else if (event->buttons & mxEvent::MouseLeftButton)
		{
			if (event->modifiers & mxEvent::KeyShift)
			{
				fnOrbitPanView();
			}
			else if (event->modifiers & mxEvent::KeyCtrl)
			{
				float ry = (float) (event->y - oldy);
				float rx = (float) (event->x - oldx);
				oldx = event->x;
				oldy = event->y;

				QAngle movement = QAngle( ry, rx, 0 );

				matrix3x4_t tmp1, tmp2, tmp3;
				AngleMatrix( g_viewerSettings.lightrot, tmp1 );
				AngleMatrix( movement, tmp2 );
				ConcatTransforms( tmp2, tmp1, tmp3 );
				MatrixAngles( tmp3, g_viewerSettings.lightrot );

				// g_viewerSettings.lightrot[0] = oldlrx + (float) (event->y - oldy);
				// g_viewerSettings.lightrot[1] = oldlry + (float) (event->x - oldx);
			}
			else
			{
				if (!g_viewerSettings.rotating)
				{
					float ry = (float) (event->y - oldy);
					float rx = (float) (event->x - oldx);
					oldx = event->x;
					oldy = event->y;

					g_cam.m_orbit.angles += QAngle (ry, -rx, 0);
					
				}
				else
				{
					float ang1 = (180 / 3.1415) * atan2( oldx - w()/2.0, oldy - h()/2.0 );
					float ang2 = (180 / 3.1415) * atan2( event->x - w()/2.0, event->y - h()/2.0 );
					oldx = event->x;
					oldy = event->y;

					g_cam.m_orbit.angles += QAngle(0, 0, ang2 - ang1);
					
				}
			}
		}
		else if (event->buttons & mxEvent::MouseRightButton)
		{
			g_cam.m_orbit.zoom = oldto + (float) (event->y - oldy);
		}
		redraw ();

		return 1;
	}
	break;

	case mxEvent::KeyDown:
	{
		switch (event->key)
		{
		case VK_F5: // F5
			{
				g_MDLViewer->Refresh();
				break;
			}
		case 32:
			{
				int iSeq = g_pStudioModel->GetSequence ();
				if (iSeq == g_pStudioModel->SetSequence (iSeq + 1))
				{
					g_pStudioModel->SetSequence (0);
				}
			}
			break;
		}
	}
	break;

	} // switch (event->event)

	return 1;
}


void DrawBackground()
{
	if (!g_viewerSettings.showBackground)
		return;

	CMatRenderContextPtr ctx( g_pMaterialSystem );

	ctx->Bind(g_materialBackground);
	{
		IMesh* pMesh = ctx->GetDynamicMesh();
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

		float dist=-15000.0f;
		float tMin=0, tMax=1;
		
		meshBuilder.Position3f(-dist, dist, dist);
		meshBuilder.TexCoord2f( 0, tMin,tMax );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( dist, dist, dist);
		meshBuilder.TexCoord2f( 0, tMax,tMax );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f( dist,-dist, dist);
		meshBuilder.TexCoord2f( 0, tMax,tMin );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.Position3f(-dist,-dist, dist);
		meshBuilder.TexCoord2f( 0, tMin,tMin );
		meshBuilder.Color4ub( 255, 255, 255, 255 );
		meshBuilder.AdvanceVertex();

		meshBuilder.End();
		pMesh->Draw();
	}
}

void DrawHelpers()
{
	if (g_viewerSettings.mousedown)
	{
		IMesh* pMesh = g_pMaterialSystem->GetRenderContext()->GetDynamicMesh(true, nullptr, nullptr, g_materialVertexColor );

		const int lineCount = 72;
		CMeshBuilder meshBuilder;
		meshBuilder.Begin( pMesh, MATERIAL_LINES, lineCount );

		if (g_viewerSettings.rotating)
			meshBuilder.Color3ub( 255, 255, 0 );
		else
			meshBuilder.Color3ub(   0, 255, 0 );

		for (int i = 0; i < lineCount; i++ )
		{
			float a = i / (float)(lineCount) * M_PI * 2;

			if (g_viewerSettings.rotating)
				meshBuilder.Color3ub( 255, 255, 0 );
			else
				meshBuilder.Color3ub(   0, 255, 0 );

			meshBuilder.Position3f( sin( a ), cos( a ), -3.0f );
			meshBuilder.AdvanceVertex();
		}
		meshBuilder.End();
		pMesh->Draw();
	}
}


void DrawGroundPlane()
{
	if (!g_viewerSettings.showGround)
		return;

	CMatRenderContextPtr ctx( g_pMaterialSystem );
	ctx->Bind(g_materialFloor);

	static Vector tMap( 0, 0, 0 );
	static Vector dxMap( 1, 0, 0 );
	static Vector dyMap( 0, 1, 0 );

	Vector deltaPos;
	QAngle deltaAngles;

	g_pStudioModel->GetMovement( g_pStudioModel->m_prevGroundCycles, deltaPos, deltaAngles );

	IMesh* pMesh = ctx->GetDynamicMesh();
	CMeshBuilder meshBuilder;
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, 1 );

	float scale = 10.0;
	float dist=-100.0f;

	float dpdd = scale / dist;

	tMap.x = tMap.x + dxMap.x * deltaPos.x * dpdd + dxMap.y * deltaPos.y * dpdd;	
	tMap.y = tMap.y + dyMap.x * deltaPos.x * dpdd + dyMap.y * deltaPos.y * dpdd;

	while (tMap.x < 0.0) tMap.x +=  1.0;
	while (tMap.x > 1.0) tMap.x += -1.0;
	while (tMap.y < 0.0) tMap.y +=  1.0;
	while (tMap.y > 1.0) tMap.y += -1.0;

	VectorYawRotate( dxMap, -deltaAngles.y, dxMap );
	VectorYawRotate( dyMap, -deltaAngles.y, dyMap );

	// ARRGHHH, this is right but I don't know what I've done
	meshBuilder.Position3f( -dist, dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (-dxMap.x - dyMap.x) * scale, tMap.y + (dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( dist, dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (dxMap.x - dyMap.x) * scale, tMap.y + (-dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( dist, -dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (dxMap.x + dyMap.x) * scale, tMap.y + (-dxMap.y - dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 255 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -dist, -dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x - (dxMap.x - dyMap.x) * scale, tMap.y - (-dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 255 );
	meshBuilder.AdvanceVertex();

	// draw underside
	meshBuilder.Position3f( -dist, dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (-dxMap.x - dyMap.x) * scale, tMap.y + (dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( -dist, -dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x - (dxMap.x - dyMap.x) * scale, tMap.y - (-dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( dist, -dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (dxMap.x + dyMap.x) * scale, tMap.y + (-dxMap.y - dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 128 );
	meshBuilder.AdvanceVertex();

	meshBuilder.Position3f( dist, dist, 0 );
	meshBuilder.TexCoord2f( 0, tMap.x + (dxMap.x - dyMap.x) * scale, tMap.y + (-dxMap.y + dyMap.y) * scale );
	meshBuilder.Color4ub( 128, 128, 128, 128 );
	meshBuilder.AdvanceVertex();

	
	
	meshBuilder.End();
	pMesh->Draw();

}


void DrawMovementBoxes()
{
	if (!g_viewerSettings.showMovement)
		return;

	CMatRenderContextPtr ctx( g_pMaterialSystem );
	ctx->Bind(g_materialFloor);

	static matrix3x4_t mStart( 1, 0, 0, 0 ,  0, 1, 0, 0 ,  0, 0, 1, 0 );
	matrix3x4_t mTemp;
	static float prevframes[5];

	Vector deltaPos;
	QAngle deltaAngles;

	g_pStudioModel->GetMovement( prevframes, deltaPos, deltaAngles );

	AngleMatrix( deltaAngles, deltaPos, mTemp );
	MatrixInvert( mTemp, mTemp );
	ConcatTransforms( mTemp, mStart, mStart );

	Vector bboxMin, bboxMax;
	g_pStudioModel->ExtractBbox( bboxMin, bboxMax  );

	static float prevCycle = 0.0;

	if (fabs( g_pStudioModel->GetFrame( 0 ) - prevCycle) > 0.5)
	{
		SetIdentityMatrix( mStart );
	}
	prevCycle = g_pStudioModel->GetFrame( 0 );

	// starting position
	{
		float color[] = { 0.7, 1, 0, 0.5 };
		float wirecolor[] = { 1, 1, 0, 1.0 };
		g_pStudioModel->drawTransparentBox( bboxMin, bboxMax, mStart, color, wirecolor );
	}

	// current position
	{
		float color[] = { 1, 0.7, 0, 0.5 };
		float wirecolor[] = { 1, 0, 0, 1.0 };
		SetIdentityMatrix( mTemp );
		g_pStudioModel->drawTransparentBox( bboxMin, bboxMax, mTemp, color, wirecolor );
	}

}


char const *HLMV_TranslateSoundName( char const *soundname, StudioModel *model )
{
	if ( Q_stristr( soundname, ".wav" ) )
		return PSkipSoundChars( soundname );

	if ( model )
	{
		return PSkipSoundChars( g_pSoundEmitterBase->GetWavFileForSound( soundname, model->GetFileName() ) );
	}

	return PSkipSoundChars( g_pSoundEmitterBase->GetWavFileForSound( soundname, NULL ) );
}


void PlaySound( const char *pSoundName, StudioModel *pStudioModel )
{
	if ( pSoundName == NULL || pSoundName[ 0 ] == '\0' )
		return;

	const char *pSoundFileName = HLMV_TranslateSoundName( pSoundName, pStudioModel );

	char filename[ 256 ];
	sprintf( filename, "sound/%s", pSoundFileName );
	CAudioSource *pAudioSource = g_pSoundSystem->FindOrAddSound( filename );
	if ( pAudioSource == NULL )
		return;

	float volume = VOL_NORM;
	gender_t gender = GENDER_NONE;
	if ( pStudioModel )
	{
		gender = g_pSoundEmitterBase->GetActorGender( pStudioModel->GetFileName() );
	}

	CSoundParameters params;
	if ( !Q_stristr( pSoundName, ".wav" ) &&
		g_pSoundEmitterBase->GetParametersForSound( pSoundName, params, gender ) )
	{
		volume = params.volume;
	}

	g_pSoundSystem->PlaySound( pAudioSource, volume, NULL );
}


// copied from baseentity.cpp
// HACK:  This must match the #define in cl_animevent.h in the client .dll code!!!
#define CL_EVENT_SOUND				5004
#define CL_EVENT_FOOTSTEP_LEFT		6004
#define CL_EVENT_FOOTSTEP_RIGHT		6005
#define CL_EVENT_MFOOTSTEP_LEFT		6006
#define CL_EVENT_MFOOTSTEP_RIGHT	6007

// copied from scriptevent.h
#define SCRIPT_EVENT_SOUND			1004		// Play named wave file (on CHAN_BODY)
#define SCRIPT_EVENT_SOUND_VOICE	1008		// Play named wave file (on CHAN_VOICE)


void PlaySounds( StudioModel *pStudioModel )
{
	if ( pStudioModel == NULL )
		return;

	int iLayer = g_ControlPanel->getFrameSelection();
	float flFrame = pStudioModel->GetFrame( iLayer );
	float flTime = flFrame / 30.0f; // pStudioModel->GetSequenceTime()

	float prevtime = flTime - pStudioModel->GetTimeDelta();
	float currtime = flTime;

	float duration = pStudioModel->GetDuration();

	prevtime = fmod( prevtime, duration );
	currtime = fmod( currtime, duration );

	float prevcycle = prevtime / duration;
	float currcycle = currtime / duration;

	CStudioHdr *pStudioHdr = pStudioModel->GetStudioHdr();
	if ( pStudioHdr == NULL )
		return;

	int seq = pStudioModel->GetSequence();
	mstudioseqdesc_t &seqdesc = pStudioHdr->pSeqdesc( seq );

	for ( int i = 0; i < (int)seqdesc.numevents; ++i )
	{
		mstudioevent_t *pEvent = seqdesc.pEvent( i );
		//const char *pEventName = pEvent->pszEventName();

		if ( pEvent->cycle <= prevcycle || pEvent->cycle > currcycle )
			continue;

		// largely copied from BuildAnimationEventSoundList in baseentity.cpp
		switch ( pEvent->event )
		{
		case 0:
			if ( Q_strcmp( pEvent->pszEventName(), "AE_CL_PLAYSOUND" ) == 0 )
			{
				PlaySound( pEvent->pszOptions(), pStudioModel );
				continue;
			}
			break;

		case CL_EVENT_SOUND: // Old-style client .dll animation event
			// fall-through intentional
		case SCRIPT_EVENT_SOUND:
			// fall-through intentional
		case SCRIPT_EVENT_SOUND_VOICE:
			PlaySound( pEvent->pszOptions(), pStudioModel );
			break;

		case CL_EVENT_FOOTSTEP_LEFT:
		case CL_EVENT_FOOTSTEP_RIGHT:
			{
				char soundname[256];
				char const *options = pEvent->pszOptions();
				if ( !options || !options[0] )
				{
					options = "NPC_CombineS";
				}

				Q_snprintf( soundname, 256, "%s.RunFootstepLeft", options );
				PlaySound( soundname, pStudioModel );
				Q_snprintf( soundname, 256, "%s.RunFootstepRight", options );
				PlaySound( soundname, pStudioModel );
				Q_snprintf( soundname, 256, "%s.FootstepLeft", options );
				PlaySound( soundname, pStudioModel );
				Q_snprintf( soundname, 256, "%s.FootstepRight", options );
				PlaySound( soundname, pStudioModel );
			}
			break;
/*
		case AE_CL_PLAYSOUND:
			if ( !( pEvent->type & AE_TYPE_CLIENT ) )
				break;

			if ( pEvent->options[0] )
			{
				PlaySound( pEvent->options, pStudioModel );
			}
			else
			{
				Warning( "-- Error --:  empty soundname, .qc error on AE_CL_PLAYSOUND in model %s, sequence %s, animevent # %i\n", 
					pStudioHdr->name(), seqdesc.pszLabel(), i + 1 );
			}
			break;
*/
		default:
			break;
		}
	}
}


void MatSysWindow::draw ()
{
	MDLCACHE_CRITICAL_SECTION_( g_pMDLCache );

	if ( g_bInError || !g_pStudioModel->GetStudioRender() )
		return;

	UpdateSounds(); // need to call this multiple times per frame to avoid audio stuttering

	g_cam.m_fov = g_viewerSettings.fov;

	g_cam.UpdateView();

	VMatrix viewMatrix;
	VMatrix projMatrix;
	g_cam.GetViewMatrix(viewMatrix);
	g_cam.GetProjectionMatrix(projMatrix, w(), h());


	g_pMaterialSystem->BeginFrame(0);
	g_pStudioModel->GetStudioRender()->BeginFrame();

	CMatRenderContextPtr ctx( g_pMaterialSystem );
	ctx->ClearColor3ub(g_viewerSettings.bgColor[0] * 255, g_viewerSettings.bgColor[1] * 255, g_viewerSettings.bgColor[2] * 255);
	// g_pMaterialSystem->ClearColor3ub(0, 0, 0 );
	g_pMaterialSystem->ClearBuffers(true, true);

	ctx->Viewport( 0, 0, w(), h() );

	// Back Layer
	ctx->MatrixMode(MATERIAL_MODEL);
	ctx->PushMatrix();
	ctx->LoadIdentity();
	ctx->MatrixMode(MATERIAL_VIEW);
	ctx->PushMatrix();
	ctx->LoadIdentity();
	DrawBackground();

	// 3D Stuff Layer
	ctx->MatrixMode( MATERIAL_PROJECTION );
	ctx->LoadMatrix( projMatrix );
	ctx->MatrixMode( MATERIAL_VIEW );
	ctx->LoadMatrix( viewMatrix );

	DrawGroundPlane();
	DrawMovementBoxes();


	g_pStudioModel->DrawModel();
	int polycount = g_pStudioModel->GetDrawMetrics().PolyCount;

	g_pStudioModel->GetStudioRender()->EndFrame();

	UpdateSounds(); // need to call this multiple times per frame to avoid audio stuttering

	g_ControlPanel->setModelInfo();

	int lod = g_pStudioModel->GetDrawMetrics().LodUsed;
	float metric = g_pStudioModel->GetDrawMetrics().LodMetric;
	g_ControlPanel->setLOD( lod, true, false );
	g_ControlPanel->setLODMetric( metric );

	g_ControlPanel->setPolycount( polycount );
	g_ControlPanel->setTransparent( g_pStudioModel->m_bIsTransparent );

	g_ControlPanel->updatePoseParameters( );
	
	// draw what ever else is loaded
	int i;
	for (i = 0; i < 4; i++)
	{
		if (g_pStudioExtraModel[i] != NULL)
		{
			g_pStudioModel->GetStudioRender()->BeginFrame();
			g_pStudioExtraModel[i]->DrawModel( true );
			g_pStudioModel->GetStudioRender()->EndFrame();
		}
	}

	g_pStudioModel->IncrementFramecounter();

	PlaySounds( g_pStudioModel );
	UpdateSounds(); // need to call this multiple times per frame to avoid audio stuttering


	// Front UI Layer
	ctx->MatrixMode(MATERIAL_MODEL);
	ctx->PushMatrix();
	ctx->LoadIdentity();
	ctx->MatrixMode(MATERIAL_VIEW);
	ctx->PushMatrix();
	ctx->LoadIdentity();
	DrawHelpers();

    g_pMaterialSystem->SwapBuffers();
	
	g_pMaterialSystem->EndFrame();
}


#if 0
int MatSysWindow::loadTexture (const char *filename, int name)
{
	if (!filename || !strlen (filename))
	{
		if (d_textureNames[name])
		{
			glDeleteTextures (1, (const GLuint *) &d_textureNames[name]);
			d_textureNames[name] = 0;

			if (name == 0)
				strcpy (g_viewerSettings.backgroundTexFile, "");
			else
				strcpy (g_viewerSettings.groundTexFile, "");
		}

		return 0;
	}

	mxImage *image = 0;

	char ext[16];
	strcpy (ext, mx_getextension (filename));

	if (!mx_strcasecmp (ext, ".tga"))
		image = mxTgaRead (filename);
	else if (!mx_strcasecmp (ext, ".pcx"))
		image = mxPcxRead (filename);
	else if (!mx_strcasecmp (ext, ".bmp"))
		image = mxBmpRead (filename);

	if (image)
	{
		if (name == 0)
			strcpy (g_viewerSettings.backgroundTexFile, filename);
		else
			strcpy (g_viewerSettings.groundTexFile, filename);

		d_textureNames[name] = name + 1;

		if (image->bpp == 8)
		{
			mstudiotexture_t texture;
			texture.width = image->width;
			texture.height = image->height;

			g_pStudioModel->UploadTexture (&texture, (byte *) image->data, (byte *) image->palette, name + 1);
		}
		else
		{
			glBindTexture (GL_TEXTURE_2D, d_textureNames[name]);
			glTexImage2D (GL_TEXTURE_2D, 0, 3, image->width, image->height, 0, GL_RGB, GL_UNSIGNED_BYTE, image->data);
			glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

		delete image;

		return name + 1;
	}

	return 0;
}
#endif


void MatSysWindow::dumpViewport (const char *filename)
{
	redraw ();
	int w = w2 ();
	int h = h2 ();

	mxImage *image = new mxImage ();
	if (image->create (w, h, 24))
	{
#if 0
		glReadBuffer (GL_FRONT);
		glReadPixels (0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, image->data);
#else
		HDC hdc = GetDC ((HWND) getHandle ());
		byte *data = (byte *) image->data;
		int i = 0;
		for (int y = 0; y < h; y++)
		{
			for (int x = 0; x < w; x++)
			{
				COLORREF cref = GetPixel (hdc, x, y);
				data[i++] = (byte) ((cref >> 0)& 0xff);
				data[i++] = (byte) ((cref >> 8) & 0xff);
				data[i++] = (byte) ((cref >> 16) & 0xff);
			}
		}
		ReleaseDC ((HWND) getHandle (), hdc);
#endif
		if (!mxTgaWrite (filename, image))
			mxMessageBox (this, "Error writing screenshot.", g_appTitle, MX_MB_OK | MX_MB_ERROR);

		delete image;
	}
}

