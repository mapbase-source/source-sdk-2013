//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose:		Timer that can be added independently of the round timer.
//
// Author:		Blixibon
//
//=============================================================================//

#include "cbase.h"
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include "vgui_controls/ScalableImagePanel.h"

#include "iclientmode.h"
#include "tf_time_panel.h"
#include "tf_gamerules.h"
#include "econ_controls.h"
#include "mapbase/game_timer.h"
#include "baseviewport.h"

using namespace vgui;

void AddSubKeyNamed( KeyValues *pKeys, const char *pszName );
bool ShouldUseMatchHUD();

extern bool g_bAnyGameTimerActive;

class CTFHudExternalTimerManager;

//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
class CTFHudExternalTimer : public CTFHudTimeStatus
{
	DECLARE_CLASS_SIMPLE( CTFHudExternalTimer, CTFHudTimeStatus );

public:
	CTFHudExternalTimer( Panel *parent, const char *name, int nEntIdx );
	~CTFHudExternalTimer();

	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void OnThink();

	virtual void SetExtraTimePanels() OVERRIDE;
	virtual void SetTeamBackground() OVERRIDE;

	virtual int GetRenderGroupPriority( void ) { return 80; }	// higher than build menus

	CTFHudExternalTimerManager *GetParentManager();

private:

	CExLabel			*m_pGameTimerLabel;
	vgui::Panel			*m_pGameTimerBG;
};

//-----------------------------------------------------------------------------
// Purpose:  
//-----------------------------------------------------------------------------
class CTFHudExternalTimerManager : public CHudElement, public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CTFHudExternalTimerManager, vgui::EditablePanel );

public:
	CTFHudExternalTimerManager( const char *pElementName );
	~CTFHudExternalTimerManager();

	virtual void Think( void ) OVERRIDE;

	virtual int GetRenderGroupPriority( void ) { return 80; }	// higher than build menus

	int GetPanelForTimer( int nEntIdx, bool bCreate = false );
	int CreateTimerPanel( C_GameTimer *pTimer );
	void RemoveTimerPanel( CTFHudExternalTimer *pTimerPanel );
	void RemoveTimerPanel( int i );
	void RemoveAllTimerPanels();

private:

	CUtlVector<CTFHudExternalTimer *>	m_Timers;

	CPanelAnimationVarAliasType( float, m_flTimerGap, "TimerGap", "48", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flTimerSpecOffset, "TimerSpecOffset", "28", "proportional_float" );
};

DECLARE_HUDELEMENT( CTFHudExternalTimerManager );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudExternalTimer::CTFHudExternalTimer( Panel *parent, const char *name, int nEntIdx ) : BaseClass( parent, name )
{
	m_iTimerIndex = nEntIdx;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudExternalTimer::~CTFHudExternalTimer()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudExternalTimer::ApplySchemeSettings( IScheme *pScheme )
{
	KeyValues *pConditions = NULL;

	// if ( TFGameRules() )
	// {
	// 	if ( ShouldUseMatchHUD() )
	// 	{
	// 		pConditions = new KeyValues( "conditions" );
	// 		AddSubKeyNamed( pConditions, "if_match" );
	// 	}
	// }

	LoadControlSettings( "resource/UI/HudExternalTimePanel.res", NULL, NULL, pConditions );

	m_pProgressBar = dynamic_cast<CTFProgressBar *>( FindChildByName( "TimePanelProgressBar" ) );

	m_pGameTimerLabel = dynamic_cast<CExLabel *>( FindChildByName( "GameTimerLabel" ) );
	m_pGameTimerBG = FindChildByName( "GameTimerBG" );

	m_pTimerBG = dynamic_cast<ScalableImagePanel *>( FindChildByName( "TimePanelBG" ) );

	m_flNextThink = 0.0f;

	SetExtraTimePanels();
	SetTeamBackground();

	// Don't need to do CTFHudTimeStatus::ApplySchemeSettings
	BaseClass::BaseClass::ApplySchemeSettings( pScheme );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudExternalTimer::OnThink()
{
	if ( m_iTimerIndex <= 0 )
		return;

	if ( m_flNextThink < gpGlobals->curtime )
	{
		C_GameTimer *pTimer = dynamic_cast< C_GameTimer* >( ClientEntityList().GetEnt( m_iTimerIndex ) );

		// get the time remaining (in seconds)
		if ( pTimer && !pTimer->IsDisabled() )
		{
			int nTotalTime = pTimer->GetTimerMaxLength();
			int nTimeRemaining = pTimer->GetTimeRemaining();
			int nTimeToDisplay = nTimeRemaining;

			if ( !pTimer->ShowTimeRemaining() )
			{
				nTimeToDisplay = nTotalTime - nTimeToDisplay;
			}

			if ( pTimer->GetTeamNumber() != m_nTeam )
			{
				m_nTeam = pTimer->GetTeamNumber();
				SetTeamBackground();
			}

			if ( m_pTimeValue && m_pTimeValue->IsVisible() )
			{
				// set our label
				int nMinutes = 0;
				int nSeconds = 0;
				char temp[256];

				if ( nTimeToDisplay <= 0 )
				{
					nMinutes = 0;
					nSeconds = 0;
				}
				else
				{
					nMinutes = nTimeToDisplay / 60;
					nSeconds = nTimeToDisplay % 60;
				}				

				Q_snprintf( temp, sizeof( temp ), "%d:%02d", nMinutes, nSeconds );
				m_pTimeValue->SetText( temp );
			}
	
			// let the progress bar know the percentage of time that's passed ( 0.0 -> 1.0 )
			if ( m_pProgressBar && m_pProgressBar->IsVisible() )
			{
				//if (pTimer->GetWarnTime() > -1.0f)
				{
					m_pProgressBar->SetPercentWarning( 1.0f - (pTimer->GetWarnTime() / pTimer->GetTimerMaxLength()) );
				}

				if ( pTimer->GetProgressBarOverride() > -1 )
				{
					float flMax = pTimer->GetProgressBarMaxSegments();
					if (flMax == -1.0f)
						flMax = (float)nTotalTime;
					m_pProgressBar->SetPercentage( (float)pTimer->GetProgressBarOverride() / flMax );
				}
				else if ( nTotalTime == 0 )
				{
					m_pProgressBar->SetPercentage( 0 );
				}
				else 
				{
					m_pProgressBar->SetPercentage( ( (float)nTotalTime - nTimeRemaining ) / (float)nTotalTime );
				}
			}
		}
		// Not thread safe. Just rely on CTFHudExternalTimerManager::Think to clean us up when needed
		/*else
		{
			// Stop showing the panel
			m_iTimerIndex = 0;
			CTFHudExternalTimerManager *pManager = GetParentManager();
			if (pManager)
				pManager->RemoveTimerPanel( this );
		}*/

		m_flNextThink = gpGlobals->curtime + 0.1f;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudExternalTimer::SetExtraTimePanels()
{
	if ( !m_pGameTimerBG || !m_pGameTimerLabel )
		return;

	C_GameTimer *pTimer = dynamic_cast< C_GameTimer* >( ClientEntityList().GetEnt( m_iTimerIndex ) );

	if ( pTimer )
	{
		SetVisible( true );
		m_nTeam = pTimer->GetTeamNumber();

		if ( pTimer->GetTimerCaption()[0] != '\0' )
		{
			m_pGameTimerBG->SetVisible( true );
			m_pGameTimerLabel->SetText( pTimer->GetTimerCaption() );
			CheckClockLabelLength( m_pGameTimerLabel, m_pGameTimerBG );

			m_pGameTimerLabel->SetVisible( true );
		}
		else
		{
			m_pGameTimerLabel->SetVisible( false );
			m_pGameTimerBG->SetVisible( false );
		}
	}
	// Not thread safe. Just rely on CTFHudExternalTimerManager::Think to clean us up when needed
	/*else
	{
		// Stop showing the panel
		m_iTimerIndex = 0;
		CTFHudExternalTimerManager *pManager = GetParentManager();
		if (pManager)
			pManager->RemoveTimerPanel( this );
	}*/
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudExternalTimer::SetTeamBackground( void )
{
	m_pTimerBG = dynamic_cast<ScalableImagePanel *>( FindChildByName( "TimePanelBG" ) );
	if ( m_pTimerBG )
	{
		int iTeam = m_nTeam;
		if ( iTeam == TEAM_ANY )
			iTeam = GetLocalPlayerTeam();

		if ( iTeam == TF_TEAM_RED )
		{
			m_pTimerBG->SetImage( "../hud/objectives_timepanel_red_bg" );
		}
		else if ( iTeam == TF_TEAM_BLUE )
		{
			m_pTimerBG->SetImage( "../hud/objectives_timepanel_blue_bg" );
		}
		else
		{
			m_pTimerBG->SetImage( "../hud/objectives_timepanel_neutral_bg" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudExternalTimerManager *CTFHudExternalTimer::GetParentManager()
{
	vgui::Panel *pPanel = GetParent();
	if (pPanel == g_pClientMode->GetViewport())
	{
		pPanel = pPanel->FindChildByName( "HudExternalTimer" );
	}

	return dynamic_cast<CTFHudExternalTimerManager *>(pPanel);
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudExternalTimerManager::CTFHudExternalTimerManager( const char *pElementName )
	: CHudElement( pElementName ),
	BaseClass( NULL, "HudExternalTimer" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( 0 );

	RegisterForRenderGroup( "mid" );
	RegisterForRenderGroup( "commentary" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CTFHudExternalTimerManager::~CTFHudExternalTimerManager()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudExternalTimerManager::Think( void )
{
	if ( !IsVisible() || !g_bAnyGameTimerActive )
	{
		if (m_Timers.Count() > 0)
			RemoveAllTimerPanels();

		return;
	}

	for ( int i = 0; i < IGameTimerAutoList::AutoList().Count(); i++ )
	{
		C_GameTimer *pTimer = static_cast<C_GameTimer *>( IGameTimerAutoList::AutoList()[i] );
		int nTimerPanel = GetPanelForTimer( pTimer->entindex() );
		if ( pTimer->IsDisabled() )
		{
			// If it's disabled and it already has a panel, remove it
			if (nTimerPanel != m_Timers.InvalidIndex())
				RemoveTimerPanel( nTimerPanel );
		}
		else if (nTimerPanel == m_Timers.InvalidIndex())
		{
			// Add a panel for this timer
			CreateTimerPanel( pTimer );
		}
	}

	// Reposition timers
	int xPos = 0;
	int yPos = 0;

	IViewPortPanel *pSpecGuiPanel = gViewPortInterface->FindPanelByName( PANEL_SPECGUI );
	if (pSpecGuiPanel && pSpecGuiPanel->IsVisible())
		yPos += m_flTimerSpecOffset;

	for (int i = 0; i < m_Timers.Count(); i++)
	{
		if (!m_Timers[i] || m_Timers[i]->IsMarkedForDeletion())
			continue;

		// While we're here, verify that this timer's entity is still valid
		C_GameTimer *pTimer = dynamic_cast<C_GameTimer*>(C_BaseEntity::Instance( m_Timers[i]->GetTimerIndex() ));
		if (!pTimer)
		{
			RemoveTimerPanel( i );
			i--;
			continue;
		}

		// Ignore timers that aren't part of us
		if (m_Timers[i]->GetParent() != this)
			continue;

		m_Timers[i]->SetPos( xPos, yPos );
		yPos += m_flTimerGap;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFHudExternalTimerManager::GetPanelForTimer( int nEntIdx, bool bCreate )
{
	// Find the timer in our list
	for (int i = 0; i < m_Timers.Count(); i++)
	{
		if (m_Timers[i] && m_Timers[i]->GetTimerIndex() == nEntIdx)
			return i;
	}

	return bCreate ? CreateTimerPanel( dynamic_cast<C_GameTimer*>(C_BaseEntity::Instance( nEntIdx )) ) : m_Timers.InvalidIndex();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CTFHudExternalTimerManager::CreateTimerPanel( C_GameTimer *pTimer )
{
	int i = m_Timers.AddToTail();
	m_Timers[i] = new CTFHudExternalTimer( this, VarArgs( "Timer%02i", i ), pTimer->entindex() );

	int wide, tall;
	GetSize( wide, tall );
	m_Timers[i]->SetSize( wide, tall );

	if (pTimer->OverridesPosition())
	{
		// Position is overridden relative to viewport
		Panel *pParent = g_pClientMode->GetViewport();
		m_Timers[i]->SetParent( pParent );

		float flX, flY;
		pTimer->GetPositionOverride( flX, flY );

		int x, y;
		GetPos( x, y );

		if (flX != -1.0f)
			x = ((GetXPos()*2) * flX);	// (ScreenWidth() * flX);
		if (flY != -1.0f)
			y = (ScreenHeight() * flY);

		m_Timers[i]->SetPos( x, y );
	}
	else
	{
		m_Timers[i]->SetPos( 0, 0 );
	}

	return i;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudExternalTimerManager::RemoveTimerPanel( CTFHudExternalTimer *pTimerPanel )
{
	// Find the timer in our list
	for (int i = 0; i < m_Timers.Count(); i++)
	{
		if (m_Timers[i] == pTimerPanel)
		{
			if (!m_Timers[i]->IsMarkedForDeletion())
 				vgui::ipanel()->DeletePanel( m_Timers[i]->GetVPanel() );
			m_Timers.Remove( i );
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudExternalTimerManager::RemoveTimerPanel( int i )
{
	if ( i < 0 || i >= m_Timers.Count() || m_Timers[i]->IsMarkedForDeletion() )
		return;

	if (m_Timers[i])
		vgui::ipanel()->DeletePanel( m_Timers[i]->GetVPanel() );
	m_Timers.Remove( i );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CTFHudExternalTimerManager::RemoveAllTimerPanels()
{
	for (int i = m_Timers.Count()-1; i >= 0; i--)
	{
		if (m_Timers[i] && !m_Timers[i]->IsMarkedForDeletion())
			vgui::ipanel()->DeletePanel( m_Timers[i]->GetVPanel() );
		m_Timers.Remove( i );
	}
	m_Timers.Purge();
}
