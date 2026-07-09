//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Generic timer HUD element for HL2-based mods
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"
#include "hud.h"
#include "hud_macros.h"
#include "hudelement.h"
#include "hud_numericdisplay.h"
#include "iclientmode.h"
#include <vgui/ILocalize.h>
#include <vgui/ISurface.h>
#include <vgui_controls/AnimationController.h>
#include <vgui_controls/EditablePanel.h>
#include "baseviewport.h"
#include "mapbase/game_timer.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern bool g_bAnyGameTimerActive;

ConVar hud_timer_max_bars( "hud_timer_max_bars", "15" );

//-----------------------------------------------------------------------------
// Purpose: Timer panel
//-----------------------------------------------------------------------------
class CHudGenericGameTimer : public CHudElement, public vgui::Panel
{
	DECLARE_CLASS_SIMPLE( CHudGenericGameTimer, vgui::Panel );

public:
	CHudGenericGameTimer( const char *pElementName );
	virtual void Init( void );
	virtual void VidInit( void );
	virtual void Reset( void );
	virtual void OnThink();
	virtual void ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual bool ShouldDraw( void );
	virtual void Paint();

	C_GameTimer *GetTimer();
	bool FindTimer();

	void SetDisplayValue( int value ) { m_iValue = value; }
	void SetLabelText( const wchar_t *text );

	virtual void PaintNumbers( vgui::HFont font, int xpos, int ypos );
	virtual void PaintBars( int xpos, int ypos, int wide );

	int GetTimerIndex() const { return m_iTimerIndex; }

private:
	void SetTimerLabel( void );

private:
	
	int		m_iTimerIndex;

	bool	m_bTimerDisplayed;
	bool	m_bPlayingFadeout;
	float	m_flShutoffTime;

	bool	m_bTimerPaused;
	bool	m_bTimerWarned;

	int		m_iValue;
	int		m_iTimerMaxLength;
	bool	m_bShowTimeRemaining;
	int		m_nProgressBarMax;
	int		m_nProgressBarOverride;
	float	m_flOverrideX, m_flOverrideY;
	wchar_t	m_LabelText[32];

	int		m_nLabelWidth, m_nLabelHeight;
	int		m_nTimerWidth, m_nTimerHeight;

	CPanelAnimationVarAliasType( float, m_flMinWidth, "MinWidth", "100", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBorder, "Border", "24", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flLabelTimerSpacing, "LabelTimerSpacing", "2", "proportional_float" );
	
	CPanelAnimationVarAliasType( float, m_flBarHeight, "BarHeight", "5", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarChunkGap, "BarChunkGap", "3", "proportional_float" );
	CPanelAnimationVarAliasType( float, m_flBarVerticalGap, "BarVerticalGap", "8", "proportional_float" );
	CPanelAnimationVar( int, m_iBarDisabledAlpha, "BarDisabledAlpha", "70" );

	CPanelAnimationVar( float, m_flBlur, "Blur", "0" );

	CPanelAnimationVar( vgui::HFont, m_hNumberFont, "NumberFont", "HudHintTextLarge" );
	CPanelAnimationVar( vgui::HFont, m_hNumberGlowFont, "NumberGlowFont", "HudHintTextLarge" );
	CPanelAnimationVar( vgui::HFont, m_hTextFont, "TextFont", "HudHintTextSmall" );
};

DECLARE_HUDELEMENT( CHudGenericGameTimer );

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CHudGenericGameTimer::CHudGenericGameTimer( const char *pElementName )
	: CHudElement( pElementName ),
	BaseClass( NULL, "HudGenericGameTimer" )
{
	Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );

	SetHiddenBits( HIDEHUD_MISCSTATUS );

	m_iTimerIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudGenericGameTimer::Init()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudGenericGameTimer::Reset()
{
	SetDisplayValue( 0 );
	SetAlpha( 0 );

	m_flShutoffTime = 0.0f;
	m_bTimerDisplayed = false;
	m_iTimerIndex = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudGenericGameTimer::VidInit()
{
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudGenericGameTimer::OnThink()
{
	C_BasePlayer *local = C_BasePlayer::GetLocalPlayer();
	if ( !local )
	{
		// Not ready to init!
		return;
	}

	// If our timer has been disabled, close the menu
	C_GameTimer *pTimer = GetTimer();
	if ( !pTimer || pTimer->IsDisabled() || pTimer->IsMarkedForDeletion() )
	{
		if ( m_flShutoffTime == 0.0f )
		{
			m_flShutoffTime = gpGlobals->curtime + 1.0f;
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "GenericGameTimerClose" );
		}
		m_iTimerIndex = 0;
		return;
	}

	// Check pause state
	if ( m_bTimerPaused && !pTimer->IsTimerPaused() )
	{
		m_bTimerPaused = false;
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "GenericGameTimerPulse" );
	}
	else if ( pTimer->IsTimerPaused() )
	{
		m_bTimerPaused = true;
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "GenericGameTimerPulseOnce" );
	}

	float flTimeRemaining = floorf( pTimer->GetTimeRemaining() );

	// Check warn state
	float flWarnTime = pTimer->GetWarnTime();

	if (flWarnTime > 0.0f)
	{
		if ( m_bTimerWarned && flTimeRemaining > flWarnTime )
		{
			// Turn back to normal
			m_bTimerWarned = false;
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "GenericGameTimerUnwarn" );
		}
		else if ( flTimeRemaining <= flWarnTime )
		{
			// Turn red
			m_bTimerWarned = true;
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "GenericGameTimerWarn" );
		}
	}

	SetDisplayValue( flTimeRemaining );

	m_iTimerMaxLength = pTimer->GetTimerMaxLength();
	m_bShowTimeRemaining = pTimer->ShowTimeRemaining();

	m_nProgressBarMax = pTimer->GetProgressBarMaxSegments();
	if (m_nProgressBarMax == -1)
	{
		// Default to timer max length
		m_nProgressBarMax = min( m_iTimerMaxLength, hud_timer_max_bars.GetInt() );
	}

	m_nProgressBarOverride = pTimer->GetProgressBarOverride();

	pTimer->GetPositionOverride( m_flOverrideX, m_flOverrideY );
}

//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudGenericGameTimer::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	SetPaintBackgroundEnabled( false );

	// set our size
	int screenWide, screenTall;
	int x, y;
	GetPos( x, y );
	GetHudSize( screenWide, screenTall );
	SetBounds( 0, y, screenWide, screenTall - y );

	// Start with a 0:00 timer
	m_nTimerWidth = (vgui::surface()->GetCharacterWidth( m_hNumberFont, '0' ) * 3);
	m_nTimerWidth += vgui::surface()->GetCharacterWidth( m_hNumberFont, ':' );

	m_nTimerHeight = vgui::surface()->GetFontTall( m_hNumberFont );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudGenericGameTimer::ShouldDraw( void )
{
	if ( !CHudElement::ShouldDraw() )
		return false;

	if ( !m_bTimerDisplayed )
	{
		// Check if we should find a new timer
		if ( g_bAnyGameTimerActive )
		{
			if ( FindTimer() )
				return true;
		}

		return false;
	}

	// check for if menu is set to disappear
	if ( m_flShutoffTime > 0 && m_flShutoffTime <= gpGlobals->curtime )
	{ 
		// times up, shutoff
		m_bTimerDisplayed = false;
		return false;
	}

	if ( m_flOverrideX == -1.0f && m_flOverrideY == -1.0f )
	{
		// Don't overlap with the weapon selection HUD
		vgui::Panel *pWeaponSelection = GetParent()->FindChildByName( "HudWeaponSelection" );
		if ( pWeaponSelection && pWeaponSelection->IsVisible() )
			return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline C_GameTimer *CHudGenericGameTimer::GetTimer()
{
	if (m_iTimerIndex <= 0)
		return NULL;

	// Need to do a dynamic_cast because this entity index could've been replaced since it was last used
	return dynamic_cast<C_GameTimer*>(C_BaseEntity::Instance( m_iTimerIndex ));
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudGenericGameTimer::FindTimer()
{
	// Find a new timer
	for ( int i = 0; i < IGameTimerAutoList::AutoList().Count(); i++ )
	{
		C_GameTimer *pTimer = static_cast<C_GameTimer *>( IGameTimerAutoList::AutoList()[i] );
		if ( !pTimer->IsDisabled() && !pTimer->IsMarkedForDeletion() )
		{
			m_iTimerIndex = pTimer->entindex();
			break;
		}
	}

	C_GameTimer *pTimer = GetTimer();
	if ( pTimer )
	{
		// New timer selected, set the caption
		wchar_t wszLabelText[128];
		const wchar_t *wLocalizedItem = g_pVGuiLocalize->Find( pTimer->GetTimerCaption() );
		if (wLocalizedItem)
		{
			V_wcsncpy( wszLabelText, wLocalizedItem, sizeof( wszLabelText ) );
		}
		else
		{
			g_pVGuiLocalize->ConvertANSIToUnicode( pTimer->GetTimerCaption(), wszLabelText, sizeof( wszLabelText ) );
		}
		SetLabelText( wszLabelText );

		m_flShutoffTime = 0;
		m_bTimerDisplayed = true;
		m_bTimerPaused = pTimer->IsTimerPaused();
		m_bTimerWarned = false;
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( m_bTimerPaused ? "GenericGameTimerShow" : "GenericGameTimerShowFlash" );

		SetDisplayValue( ceil( pTimer->GetTimeRemaining() ) );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: data accessor
//-----------------------------------------------------------------------------
void CHudGenericGameTimer::SetLabelText( const wchar_t *text )
{
	wcsncpy( m_LabelText, text, sizeof( m_LabelText ) / sizeof( wchar_t ) );
	m_LabelText[(sizeof( m_LabelText ) / sizeof( wchar_t )) - 1] = 0;

	m_nLabelWidth = 0;
	m_nLabelHeight = 0;
	
	if (m_LabelText[0] != '\0')
	{
		int nLabelLen = V_wcslen( m_LabelText );
		for (int ch = 0; ch < nLabelLen; ch++)
		{
			m_nLabelWidth += vgui::surface()->GetCharacterWidth( m_hTextFont, m_LabelText[ch] );
		}

		m_nLabelHeight = vgui::surface()->GetFontTall( m_hTextFont );
		m_nLabelHeight += m_flLabelTimerSpacing;
	}
}

//-----------------------------------------------------------------------------
// Purpose: paints a number at the specified position
//-----------------------------------------------------------------------------
void CHudGenericGameTimer::PaintNumbers( vgui::HFont font, int xpos, int ypos )
{
	vgui::surface()->DrawSetTextFont(font);

	int nTimeToDisplay = m_iValue;

	if ( !m_bShowTimeRemaining )
	{
		nTimeToDisplay = m_iTimerMaxLength - nTimeToDisplay;
	}

	int nMinutes = 0;
	int nSeconds = 0;
	wchar_t unicode[6];

	if (nTimeToDisplay <= 0)
	{
		nMinutes = 0;
		nSeconds = 0;
	}
	else
	{
		nMinutes = nTimeToDisplay / 60;
		nSeconds = nTimeToDisplay % 60;
	}

	V_snwprintf( unicode, ARRAYSIZE(unicode), L"%d:%02d", nMinutes, nSeconds );

	vgui::surface()->DrawSetTextPos( xpos, ypos );
	vgui::surface()->DrawUnicodeString( unicode );

	// draw the overbright blur
	for (float fl = m_flBlur; fl > 0.0f; fl -= 1.0f)
	{
		if (fl >= 1.0f)
		{
			vgui::surface()->DrawSetTextPos( xpos, ypos );
			vgui::surface()->DrawUnicodeString( unicode );
		}
		else
		{
			// draw a percentage of the last one
			Color col = GetFgColor();
			col[3] *= fl;
			vgui::surface()->DrawSetTextColor( col );
			vgui::surface()->DrawSetTextPos( xpos, ypos );
			vgui::surface()->DrawUnicodeString( unicode );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: paints bars at the specified position
//-----------------------------------------------------------------------------
void CHudGenericGameTimer::PaintBars( int xpos, int ypos, int wide )
{
	// get bar chunks
	int barChunkWidth = (wide / m_nProgressBarMax) - m_flBarChunkGap;

	int enabledChunks;
	if (m_nProgressBarOverride > -1)
		enabledChunks = m_nProgressBarOverride;
	else
	{
		enabledChunks = (int)floorf( ((float)m_iValue / (float)m_iTimerMaxLength) * (float)m_nProgressBarMax );
		if (!m_bShowTimeRemaining)
		{
			enabledChunks = m_nProgressBarMax - enabledChunks;
		}
	}

	// draw the suit power bar
	vgui::surface()->DrawSetColor( GetFgColor() );
	for (int i = 0; i < enabledChunks; i++)
	{
		vgui::surface()->DrawFilledRect( xpos, ypos, xpos + barChunkWidth, ypos + m_flBarHeight );
		xpos += (barChunkWidth + m_flBarChunkGap);
	}
	// draw the exhausted portion of the bar.
	Color clrExhausted = GetFgColor();
	clrExhausted[3] = ((float)clrExhausted[3] / 255.0f) * m_iBarDisabledAlpha;
	vgui::surface()->DrawSetColor( clrExhausted );
	for (int i = enabledChunks; i < m_nProgressBarMax; i++)
	{
		vgui::surface()->DrawFilledRect( xpos, ypos, xpos + barChunkWidth, ypos + m_flBarHeight );
		xpos += (barChunkWidth + m_flBarChunkGap);
	}
}

//-----------------------------------------------------------------------------
// Purpose: renders the vgui panel
//-----------------------------------------------------------------------------
void CHudGenericGameTimer::Paint()
{
	// Check for 00:00 timer
	int nTimerWidth = m_nTimerWidth;
	if (m_iValue > 600)
		nTimerWidth += vgui::surface()->GetCharacterWidth( m_hNumberFont, '0' );

	// draw the background
	int wide = max( nTimerWidth, m_nLabelWidth ) + m_flBorder;
	if (m_flMinWidth > wide)
		wide = m_flMinWidth;

	int tall = m_nTimerHeight + m_nLabelHeight + (m_flBorder/2);
	if (m_nProgressBarMax > 0)
		tall += m_flBarHeight + m_flBarVerticalGap;
	else
		tall += (m_flBorder/2);

	int screenW, screenH;
	GetHudSize( screenW, screenH );

	float flScalarX = (m_flOverrideX != -1.0f ? m_flOverrideX : 0.5f);
	float flScalarY = (m_flOverrideY != -1.0f ? m_flOverrideY : 0.05f);

	int x = (screenW - wide) * flScalarX;
	int y = (screenH - tall) * flScalarY;

	DrawBox( x, y, wide, tall, GetBgColor(), 1.0f);
	
	y += (m_flBorder/2);

	// draw our bars
	if (m_nProgressBarMax > 0)
	{
		int barX = x + (m_flBorder/2);
		PaintBars( barX, y, wide - m_flBorder );
		y += m_flBarVerticalGap;
	}

	if (m_nLabelHeight > 0)
	{
		vgui::surface()->DrawSetTextFont( m_hTextFont );
		vgui::surface()->DrawSetTextColor( GetFgColor() );
		vgui::surface()->DrawSetTextPos( x + ((wide - m_nLabelWidth) * 0.5f), y );
		vgui::surface()->DrawUnicodeString( m_LabelText );
		y += m_nLabelHeight;
	}

	// draw our numbers
	vgui::surface()->DrawSetTextColor( GetFgColor() );

	int digitX = x + ((wide - nTimerWidth) * 0.5f);
	int digitY = y;
	PaintNumbers( m_hNumberFont, digitX, digitY );
}
