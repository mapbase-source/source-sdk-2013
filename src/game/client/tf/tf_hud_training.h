//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef TF_HUD_TRAINING_H
#define TF_HUD_TRAINING_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui_controls/EditablePanel.h>
#include <vgui_controls/ImagePanel.h>
#include "tf_controls.h"
#include "GameEventListener.h"
#include "c_tf_objective_resource.h"
#include "IconPanel.h"
#include "tf_gamerules.h"
#include "proxyentity.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"

#define MAX_TRAINING_MSG_LENGTH 512

extern int Training_GetCompletedTrainingClasses();
extern void Training_MarkClassComplete( int iClass, int iStage );

//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
class CTFHudTraining : public vgui::EditablePanel, public CGameEventListener
{
	DECLARE_CLASS_SIMPLE( CTFHudTraining, vgui::EditablePanel );

public:

	CTFHudTraining( vgui::Panel *parent, const char *name );
	virtual ~CTFHudTraining();

	static bool   FormatTrainingText( const char* input, wchar_t* output );

	virtual void  ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void  PerformLayout();
	virtual void  Reset();
	virtual void  FireGameEvent( IGameEvent *event );
	virtual bool  IsVisible( void );
	virtual void  OnTick();

	void          SetTrainingText( const char *msg );
	void          SetTrainingObjective( const char *msg );
#ifdef MAPBASE
	void          SetTrainingImage( const char *image );
	void          SetTrainingOverridePos( bool bOverride, float flX, float flY );

	void          SetTrainingGameInstructor( bool bIsInstructor );
#endif

private:

	CExRichText		*m_pMsgLabel;
	CExLabel		*m_pPressSpacebarToContinueLabel;
#ifdef MAPBASE
	CExLabel		*m_pGoalLabel;
	CExLabel		*m_pGoalLabelShadow;
	CExRichText		*m_pMsgLabelShadow;
	CTFImagePanel	*m_pMsgBG;

	vgui::ImagePanel	*m_pImage;
	char			m_szImage[MAX_PATH];

	bool			m_bOverridePos = false;
	float			m_flOverrideX, m_flOverrideY = 0.0f;

	bool			m_bIsGameInstructor = false;
#endif
};


#endif	// TF_HUD_TRAINING_H
