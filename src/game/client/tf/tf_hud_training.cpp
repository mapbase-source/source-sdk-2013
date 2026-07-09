//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
//=============================================================================//

#include "cbase.h"
#include "tf_hud_escort.h"
#include <vgui/IVGui.h>
#include "tf_hud_freezepanel.h"
#include "teamplayroundbased_gamerules.h"
#include "iclientmode.h"
#include "tf_gamerules.h"
#include "tf_hud_training.h"
#include <vgui/ILocalize.h>

using namespace vgui;

extern void AddSubKeyNamed( KeyValues *pKeys, const char *pszName );

//-----------------------------------------------------------------------------
// Purpose: Localize text for training messages.  Used in annotations and in the training hud. Assumes the output string size is greater than or equal to MAX_TRAINING_MSG_LENGTH
//-----------------------------------------------------------------------------
bool CTFHudTraining::FormatTrainingText(const char* inputString, wchar_t* outputString)
{
	static wchar_t szBuf[MAX_TRAINING_MSG_LENGTH];
	static wchar_t *pszBuf;

	if ( !inputString || !inputString[0] )
	{
		return false;
	}

	// init buffers & pointers
	outputString[0] = 0;
	szBuf[0] = 0;
	pszBuf = szBuf;

	// try to localize
	pszBuf = g_pVGuiLocalize->Find( inputString );

	if ( !pszBuf )
	{
		// use plain ASCII string 
		g_pVGuiLocalize->ConvertANSIToUnicode( inputString, szBuf, sizeof(szBuf) );
		pszBuf = szBuf;
	}

	// Replace bindings with the keys
	// parse out the text into a label set
	wchar_t *ws = pszBuf;
	while ( *ws )
	{
		wchar_t token[MAX_TRAINING_MSG_LENGTH];
		bool isVar = false;

		// check for variables
		if ( *ws == '%' )
		{
			isVar = true;
			++ws;
		}

		// parse out the string
		wchar_t *end = wcschr( ws, '%' );
		if ( end )
		{
			wcsncpy( token, ws, end - ws );
			token[end - ws] = 0;
		}
		else
		{
			V_wcscpy_safe( token, ws );
		}
		ws += wcslen( token );
		if ( isVar )
		{
			// move over the end of the variable
			++ws; 
		}

		// modify the label if necessary
		if ( isVar )
		{
			// lookup key names
			char binding[64];
			g_pVGuiLocalize->ConvertUnicodeToANSI( token, binding, sizeof(binding) );

			//!! change some key names into better names
			char friendlyName[64];
			const char *key = engine->Key_LookupBinding( *binding == '+' ? binding + 1 : binding );
			if ( !key )
			{
				key = "< not bound >";
			}

			Q_snprintf( friendlyName, sizeof(friendlyName), "#%s", key );
			Q_strupr( friendlyName );

			// set the variable text - key may need to be localized (button images for example)
			wchar_t *locName = g_pVGuiLocalize->Find( friendlyName );
			if ( !locName || wcslen(locName) <= 0)
			{
				wchar_t wszFriendly[64];
				g_pVGuiLocalize->ConvertANSIToUnicode( friendlyName+1, wszFriendly, sizeof( wszFriendly ) );
				V_wcsncat( outputString, wszFriendly, MAX_TRAINING_MSG_LENGTH );
			}
			else
			{
				V_wcsncat( outputString, locName, MAX_TRAINING_MSG_LENGTH );
			}
		}
		else
		{
			V_wcsncat( outputString, token, MAX_TRAINING_MSG_LENGTH );
		}
	}
  return true;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudTraining::CTFHudTraining( Panel *parent, const char *name ) : EditablePanel( parent, name )
{
	m_pMsgLabel = NULL;
	m_pPressSpacebarToContinueLabel = NULL;

	ivgui()->AddTickSignal( GetVPanel(), 10 );

	ListenForGameEvent( "teamplay_round_start" );

#ifdef MAPBASE
	m_szImage[0] = '\0';
#endif
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudTraining::~CTFHudTraining()
{
	ivgui()->RemoveTickSignal( GetVPanel() );
}


//-----------------------------------------------------------------------------
// Purpose: Hide when we take a freeze cam shot
//-----------------------------------------------------------------------------
bool CTFHudTraining::IsVisible( void )
{
	if( IsTakingAFreezecamScreenshot() )
		return false;

	if ( IsInFreezeCam() )
		return false;

	return BaseClass::IsVisible();
}

void CTFHudTraining::Reset( void )
{
	const char *emptyText = "";

	SetDialogVariable( "goal", emptyText );

	if (m_pMsgLabel) m_pMsgLabel->SetText(emptyText);
#ifdef MAPBASE
	if (m_pMsgLabelShadow) m_pMsgLabelShadow->SetText(emptyText);
#endif
	if (m_pPressSpacebarToContinueLabel) m_pPressSpacebarToContinueLabel->SetVisible( false );

}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudTraining::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	KeyValues *pConditions = NULL;

#ifdef MAPBASE
	if (m_szImage[0])
	{
		if ( !pConditions )
			pConditions = new KeyValues( "conditions" );

		AddSubKeyNamed( pConditions, "if_image" );
	}

	if ( m_bIsGameInstructor )
	{
		if ( !pConditions )
			pConditions = new KeyValues( "conditions" );

		AddSubKeyNamed( pConditions, "if_instructor" );
	}
#endif

	// load control settings...
	LoadControlSettings( "resource/UI/HudTraining.res", NULL, NULL, pConditions );

	m_pMsgLabel = dynamic_cast<CExRichText *>( FindChildByName("MsgLabel") );
	m_pPressSpacebarToContinueLabel = dynamic_cast<CExLabel *>( FindChildByName("PressSpacebarToContinue") );

#ifdef MAPBASE
	m_pGoalLabel = dynamic_cast<CExLabel *>(FindChildByName( "GoalLabel" ));
	m_pGoalLabelShadow = dynamic_cast<CExLabel *>(FindChildByName( "GoalLabelShadow" ));
	m_pMsgLabelShadow = dynamic_cast<CExRichText *>(FindChildByName( "MsgLabelShadow" ));
	m_pMsgBG = dynamic_cast<CTFImagePanel *>(FindChildByName( "HudTrainingMsgBG" ));

	m_pImage = dynamic_cast<vgui::ImagePanel *>(FindChildByName( "HudTrainingImage" ));
	if (m_pImage && m_szImage[0])
	{
		m_pImage->SetImage( m_szImage );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudTraining::PerformLayout()
{
	BaseClass::PerformLayout();

#ifdef MAPBASE
	//m_pMsgLabel->SetFont( scheme()->GetIScheme( m_pMsgLabel->GetScheme() )->GetFont( "InstructionalText", true ) );

	if ( m_bIsGameInstructor )
	{
		int x, y, wide, tall;

		if ( m_bOverridePos )
		{
			x = (ScreenWidth() * m_flOverrideX) - (GetWide() / 2);
			y = (ScreenHeight() * m_flOverrideY) - (GetTall() / 2);
			SetPos( x, y );
		}

		/*if (m_pMsgLabel->GetNumLines() > 5)
		{
			m_pMsgLabel->SetFont( scheme()->GetIScheme( m_pMsgLabel->GetScheme() )->GetFont( "InstructionalTextSmall", true ) );
		}*/

		int iOldTall = m_pMsgLabel->GetTall();

		m_pMsgLabel->SetToFullHeight();
		m_pMsgLabel->SetVerticalScrollbar( false );
		m_pMsgLabelShadow->SetToFullHeight();
		m_pMsgLabelShadow->SetVerticalScrollbar( false );

		int iTallDiff = m_pMsgLabel->GetTall() - iOldTall;

		if ( iTallDiff > 0 )
		{
			GetBounds( x, y, wide, tall );

			// Artificial padding
			const int iPadding = iTallDiff/8;
			y -= iPadding;
			tall += iTallDiff;

			SetBounds( x, y, wide, tall );

			if (m_pGoalLabel)
			{
				m_pGoalLabel->GetPos( x, y );
				y += iPadding;
				m_pGoalLabel->SetPos( x, y );
			}

			if (m_pGoalLabelShadow)
			{
				m_pGoalLabelShadow->GetPos( x, y );
				y += iPadding;
				m_pGoalLabelShadow->SetPos( x, y );
			}

			if (m_pMsgLabel)
			{
				m_pMsgLabel->GetPos( x, y );
				y += iPadding;
				m_pMsgLabel->SetPos( x, y );
			}

			if (m_pMsgLabelShadow)
			{
				m_pMsgLabelShadow->GetPos( x, y );
				y += iPadding;
				m_pMsgLabelShadow->SetPos( x, y );
			}

			if (m_pMsgBG)
			{
				m_pMsgBG->GetBounds( x, y, wide, tall );
				tall += iTallDiff + (iPadding*2);
				m_pMsgBG->SetBounds( x, y, wide, tall );
			}

			if (m_pImage)
			{
				m_pImage->GetPos( x, y );
				y += iPadding;
				m_pImage->SetPos( x, y );
			}
		}
	}
#endif
}

void CTFHudTraining::SetTrainingObjective(const char *szRawString)
{
	wchar_t wszText[MAX_TRAINING_MSG_LENGTH];

	if (!FormatTrainingText(szRawString, wszText))
	{
		SetDialogVariable( "goal", "" );
		return;
	}
	
	SetDialogVariable( "goal", wszText );

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		pLocalPlayer->EmitSound( "Hud.TrainingMsgUpdate" );
	}
}


void CTFHudTraining::SetTrainingText(const char *szRawString)
{
	static wchar_t wszText[MAX_TRAINING_MSG_LENGTH];

	InvalidateLayout( false, true );

	if ( !m_pMsgLabel )
		return;
	
	if (!FormatTrainingText(szRawString, wszText))
	{
		m_pMsgLabel->SetText( "" );
#ifdef MAPBASE
		if ( m_pMsgLabelShadow )
			m_pMsgLabelShadow->SetText( "" );
#endif
		return;
	}

	// clear the text first
	m_pMsgLabel->SetText("");
#ifdef MAPBASE
	if ( m_pMsgLabelShadow )
		m_pMsgLabelShadow->SetText( "" );
#endif

	enum
	{
		COLOR_NORMAL = 1,
		COLOR_HINT = 2,
	};
	static wchar_t wszInsertedText[MAX_TRAINING_MSG_LENGTH];
	Color color = m_pMsgLabel->GetFgColor();
	Color newColor = color;
	int startIdx = 0;
	int endIdx = 0;
	bool bContinue = true;
	while ( bContinue )
	{
		bool bSetText = false;
		switch ( wszText[endIdx] )
		{
		case 0:
			bContinue = false;
			bSetText = true;
			break;
		case COLOR_NORMAL:
			newColor = m_pMsgLabel->GetFgColor();
			bSetText = true;
			break;
		case COLOR_HINT:
			newColor = m_pMsgLabel->GetSchemeColor( "HudTrainingHint", Color(255, 255, 255, 255), scheme()->GetIScheme( m_pMsgLabel->GetScheme() ) );
			bSetText = true;
			break;
		}
		if ( bSetText )
		{
			if ( startIdx != endIdx )
			{
				int len = endIdx - startIdx + 1;
				wcsncpy( wszInsertedText, wszText + startIdx, len );
				wszInsertedText[len-1] = 0;
				m_pMsgLabel->InsertColorChange( color );
				m_pMsgLabel->InsertString( wszInsertedText );

#ifdef MAPBASE
				if ( m_pMsgLabelShadow )
				{
					m_pMsgLabelShadow->InsertString( wszInsertedText );
				}
#endif

				// skip past the color change character
				startIdx = endIdx + 1;
			}
			color = newColor;
		}
		++endIdx;
	}

	//m_pMessageFlashEndTime = gpGlobals->curtime + MESSAGE_FLASH_TIME;

#ifdef MAPBASE
	if (m_bIsGameInstructor)
	{
		m_pMsgLabel->SetUnusedScrollbarInvisible( true );
		if ( m_pMsgLabelShadow )
			m_pMsgLabelShadow->SetUnusedScrollbarInvisible( true );

		InvalidateLayout( true, false );

		/*
		int x, y;
		GetPos( x, y );

		g_pClientMode->GetViewportAnimationController()->RunAnimationCommand( this, "Position", Color( x, y - 60, 0, 0 ), 0.0f, 0.0f, vgui::AnimationController::INTERPOLATOR_LINEAR );
		g_pClientMode->GetViewportAnimationController()->RunAnimationCommand( this, "Position", Color( x, y, 0, 0 ), 0.0f, 2.0f, vgui::AnimationController::INTERPOLATOR_BOUNCE );
		*/

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TrainingHudBlink" );
	}
	else
#endif
	{
		m_pMsgLabel->SetUnusedScrollbarInvisible( false );
		if ( m_pMsgLabelShadow )
			m_pMsgLabelShadow->SetUnusedScrollbarInvisible( false );

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "TrainingHudBounce");
	}

	C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();
	if ( pLocalPlayer )
	{
		pLocalPlayer->EmitSound( "Hud.TrainingMsgUpdate" );
	}
}


#ifdef MAPBASE
void CTFHudTraining::SetTrainingImage( const char *image )
{
	V_strncpy( m_szImage, image, sizeof( m_szImage ) );
	InvalidateLayout( false, true );
}

void CTFHudTraining::SetTrainingOverridePos( bool bOverride, float flX, float flY )
{
	m_bOverridePos = bOverride;
	m_flOverrideX = flX;
	m_flOverrideY = flY;
}

void CTFHudTraining::SetTrainingGameInstructor( bool bIsInstructor )
{
	m_bIsGameInstructor = bIsInstructor;
}
#endif


//-----------------------------------------------------------------------------
// Purpose: Receive messages about changes in state
//-----------------------------------------------------------------------------
void CTFHudTraining::FireGameEvent( IGameEvent *event )
{	
	const char *eventName = event->GetName();

	if ( FStrEq( "teamplay_round_start", eventName ) )
	{
		InvalidateLayout( false, true );
	}
}


void CTFHudTraining::OnTick( )
{
	bool bShouldBeVisible = TFGameRules() && TFGameRules()->IsWaitingForTrainingContinue();
	if ( m_pPressSpacebarToContinueLabel && bShouldBeVisible != m_pPressSpacebarToContinueLabel->IsVisible() )
	{
		m_pPressSpacebarToContinueLabel->SetVisible( bShouldBeVisible );

		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( bShouldBeVisible ? "TrainingPressSpacebarBlink" : "TrainingPressSpacebarBlinkStop" );
	}
}