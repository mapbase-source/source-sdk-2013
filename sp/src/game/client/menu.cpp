//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//
//
// menu.cpp
//
// generic menu handler
//
#include "cbase.h"
#include "text_message.h"
#include "hud_macros.h"
#include "iclientmode.h"
#include "weapon_selection.h"

#include <vgui/VGUI.h>
#include <vgui/ISurface.h>
#include <vgui/ILocalize.h>
#include <KeyValues.h>
#include <vgui_controls/AnimationController.h>

#ifdef MAPBASE
#include "hud_closecaption.h"

ConVar hud_menu_complex_max_pixels( "hud_menu_complex_max_pixels", "800" );
ConVar hud_menu_center_border_x( "hud_menu_center_border_x", "64" );
ConVar hud_menu_center_cc_margin( "hud_menu_center_cc_margin", "4" );
ConVar hud_menu_big_text_margin( "hud_menu_big_text_margin", "12" );

#define MENU_CC_SPEED_REVERT	1.0f
#define MAX_MENU_STRING	1024
#else
#define MAX_MENU_STRING	512
#endif
wchar_t g_szMenuString[MAX_MENU_STRING];
char g_szPrelocalisedMenuString[MAX_MENU_STRING];

#include "menu.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//
//-----------------------------------------------------
//

DECLARE_HUDELEMENT( CHudMenu );
DECLARE_HUD_MESSAGE( CHudMenu, ShowMenu );
#ifdef MAPBASE
DECLARE_HUD_MESSAGE( CHudMenu, ShowMenuComplex );
#endif

//
//-----------------------------------------------------
//

static char* ConvertCRtoNL( char *str )
{
	for ( char *ch = str; *ch != 0; ch++ )
		if ( *ch == '\r' )
			*ch = '\n';
	return str;
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CHudMenu::CHudMenu( const char *pElementName ) :
	CHudElement( pElementName ), BaseClass(NULL, "HudMenu")
{
	m_nSelectedItem = -1;

	vgui::Panel *pParent = g_pClientMode->GetViewport();
	SetParent( pParent );
	
	SetHiddenBits( HIDEHUD_MISCSTATUS );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::Init( void )
{
	HOOK_HUD_MESSAGE( CHudMenu, ShowMenu );
#ifdef MAPBASE
	HOOK_HUD_MESSAGE( CHudMenu, ShowMenuComplex );
#endif

	m_bMenuTakesInput = false;
	m_bMenuDisplayed = false;
	m_bitsValidSlots = 0;
	m_Processed.RemoveAll();
	m_nMaxPixels = 0;
	m_nHeight = 0;
	Reset();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::LevelInit()
{
	CHudElement::LevelInit();

#ifdef MAPBASE
	if (m_bMapDefinedMenu)
	{
		// Fixes menu retaining on level change/reload
		// TODO: Would non-map menus benefit from this as well?
		m_bMenuTakesInput = false;
		m_bMenuDisplayed = false;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::Reset( void )
{
	g_szPrelocalisedMenuString[0] = 0;
	m_fWaitingForMore = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Output : Returns true on success, false on failure.
//-----------------------------------------------------------------------------
bool CHudMenu::IsMenuOpen( void )
{
	return m_bMenuDisplayed && m_bMenuTakesInput;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::VidInit( void )
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::OnThink()
{
	float flSelectionTimeout = MENU_SELECTION_TIMEOUT;

	// If we've been open for a while without input, hide
#ifdef MAPBASE
	if ( m_bMenuDisplayed && ( gpGlobals->curtime - m_flSelectionTime > flSelectionTimeout && !m_bMapDefinedMenu ) )
#else
	if ( m_bMenuDisplayed && ( gpGlobals->curtime - m_flSelectionTime > flSelectionTimeout ) )
#endif
	{
		m_bMenuDisplayed = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CHudMenu::ShouldDraw( void )
{
	bool draw = CHudElement::ShouldDraw() && m_bMenuDisplayed;
	if ( !draw )
		return false;

	// check for if menu is set to disappear
	if ( m_flShutoffTime > 0 )
	{  
#ifdef MAPBASE
		if ( m_bMapDefinedMenu && !m_bPlayingFadeout && (m_flShutoffTime - m_flOpenCloseTime) <= GetMenuTime() )
		{
			// Begin closing the menu
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( "MenuClose" );
			m_bMenuTakesInput = false;
			m_bPlayingFadeout = true;

			if ( IsCentered() )
			{
				CHudCloseCaption *pHudCloseCaption = (CHudCloseCaption *)GET_HUDELEMENT( CHudCloseCaption );
				if ( pHudCloseCaption && pHudCloseCaption->IsUsingCustomDimensions( ToHandle() ) )
				{
					// Reset if it's still using custom dimensions we defined
					pHudCloseCaption->StopUsingCustomDimensions( MENU_CC_SPEED_REVERT );
				}
			}
		}
		else
#endif
		if ( m_flShutoffTime <= GetMenuTime() )
		{
			// times up, shutoff
			m_bMenuDisplayed = false;
			return false;
		}
	}

	return draw;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *text - 
//			textlen - 
//			font - 
//			x - 
//			y - 
//-----------------------------------------------------------------------------
void CHudMenu::PaintString( const wchar_t *text, int textlen, vgui::HFont& font, int x, int y )
{
	vgui::surface()->DrawSetTextFont( font );
	vgui::surface()->DrawSetTextPos( x, y );

	for ( int ch = 0; ch < textlen; ch++ )
	{
#ifdef MAPBASE
		if ( text[ch] == '\n' )
		{
			// Line break
			y += vgui::surface()->GetFontTall( font );
			vgui::surface()->DrawSetTextPos( x, y );
			continue;
		}
		else if ( ch == 3 && text[1] == '.' && text[2] == ' ' )
		{
			// If this is a menu item (e.g. '1. Item') and we've drawn the number part,
			// store the text pos so that line breaks are indented
			vgui::surface()->DrawGetTextPos( x, y );
		}
#endif

		vgui::surface()->DrawUnicodeChar( text[ch] );
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::Paint()
{
	if ( !m_bMenuDisplayed )
		return;

	// center it
	int x = m_nBorder;

	Color	menuColor = m_MenuColor;
	Color itemColor = m_ItemColor;
	Color boxColor = m_BoxColor;

	vgui::HFont hItemFont = m_hItemFont;
	vgui::HFont hItemFontPulsing = m_hItemFontPulsing;

	int c = m_Processed.Count();

	int wide = m_nMaxPixels + m_nBorder;
	int tall = m_nHeight + m_nBorder;

	int y = ( ScreenHeight() - tall ) * 0.5f;

	int borderX = m_nBorder / 2;
	int borderY = borderX;

#ifdef MAPBASE
	if ( IsCentered() )
	{
		y = m_nCenterY;
		borderX += (hud_menu_center_border_x.GetInt() / 2);

		if ( m_nCenterX == 0 )
		{
			// Center with existing menu width
			x = ( ScreenWidth() - wide ) * 0.5f;
			wide += hud_menu_center_border_x.GetInt();
		}
		else
		{
			x = m_nCenterX + (hud_menu_center_border_x.GetInt()/2);
			wide = m_nCenterWide /*- (hud_menu_center_border_x.GetInt()/2)*/;
		}
	}

	if ( IsUsingBigFonts() )
	{
		if ( m_hItemFontBig != vgui::INVALID_FONT && m_hItemFontBigPulsing != vgui::INVALID_FONT )
		{
			hItemFont = m_hItemFontBig;
			hItemFontPulsing = m_hItemFontBigPulsing;
		}
	}

	if ( m_flBoxAlphaOverride != 0.0f )
		boxColor[3] = m_flBoxAlphaOverride;
#endif

	DrawBox( x - borderX, y - borderY, wide, tall, boxColor, m_flSelectionAlphaOverride / 255.0f );

	//DrawTexturedBox( x - borderX, y - borderY, wide, tall, boxColor, m_flSelectionAlphaOverride / 255.0f );

	menuColor[3] = menuColor[3] * ( m_flSelectionAlphaOverride / 255.0f );
	itemColor[3] = itemColor[3] * ( m_flSelectionAlphaOverride / 255.0f );

	for ( int i = 0; i < c; i++ )
	{
		ProcessedLine *line = &m_Processed[ i ];
		Assert( line );

#ifdef MAPBASE
		bool isItem = true;
		if (line->menuitem == 0 && line->startchar < (MAX_MENU_STRING-1) && g_szMenuString[ line->startchar ] != L'0' && g_szMenuString[ line->startchar+1 ] != L'.')
		{
			// Can't use 0 directly because it gets conflated with the cancel item
			isItem = false;
		}
#else
		bool isItem = line->menuitem != 0;
#endif

		Color clr = isItem ? itemColor : menuColor;

		bool canblur = false;
		if ( line->menuitem != 0 &&
			m_nSelectedItem >= 0 && 
			( line->menuitem == m_nSelectedItem ) )
		{
			canblur = true;
		}
		
		vgui::surface()->DrawSetTextColor( clr );

		int drawLen = line->length;
		if (isItem)
		{
			drawLen *= m_flTextScan;
		}

		vgui::surface()->DrawSetTextFont( isItem ? hItemFont : m_hTextFont );

		int itemX = x;

#ifdef MAPBASE
		if ( IsCentered() )
		{
			if ( !isItem )
			{
				// Center title
				itemX = ( ScreenWidth() - line->pixels ) * 0.5f;

				PaintString( &g_szMenuString[ line->startchar ], drawLen,
					isItem ? hItemFont : m_hTextFont, itemX, y );
			}
			else
			{
				// Paint the numbers with the title color
				vgui::surface()->DrawSetTextColor( menuColor );
				PaintString( &g_szMenuString[ line->startchar ], 3,
					hItemFont, itemX, y );
				vgui::surface()->DrawSetTextColor( itemColor );

				int nTextX, nTextY;
				vgui::surface()->DrawGetTextPos( nTextX, nTextY );

				PaintString( &g_szMenuString[ line->startchar + 3 ], drawLen - 3,
					isItem ? hItemFont : m_hTextFont, nTextX, nTextY );
			}
		}
		else
#endif
		PaintString( &g_szMenuString[ line->startchar ], drawLen, 
			isItem ? hItemFont : m_hTextFont, itemX, y );

		if ( canblur )
		{
			// draw the overbright blur
			for (float fl = m_flBlur; fl > 0.0f; fl -= 1.0f)
			{
				if (fl >= 1.0f)
				{
					PaintString( &g_szMenuString[ line->startchar ], drawLen, hItemFontPulsing, itemX, y );
				}
				else
				{
					// draw a percentage of the last one
					Color col = clr;
					col[3] *= fl;
					vgui::surface()->DrawSetTextColor(col);
					PaintString( &g_szMenuString[ line->startchar ], drawLen, hItemFontPulsing, itemX, y );
				}
			}
		}

		y += line->height;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline float CHudMenu::GetMenuTime( void )
{
#ifdef MAPBASE
	// In singleplayer, use the curtime instead. This fixes issues with menus disappearing after pausing
	if (gpGlobals->maxClients <= 1)
		return gpGlobals->curtime;
#endif

	return gpGlobals->realtime;
}

//-----------------------------------------------------------------------------
// Purpose: selects an item from the menu
//-----------------------------------------------------------------------------
void CHudMenu::SelectMenuItem( int menu_item )
{
	// if menu_item is in a valid slot,  send a menuselect command to the server
	if ( (menu_item > 0) && (m_bitsValidSlots & (1 << (menu_item-1))) )
	{
		char szbuf[32];
		Q_snprintf( szbuf, sizeof( szbuf ), "menuselect %d\n", menu_item );
		engine->ClientCmd( szbuf );

		m_nSelectedItem = menu_item;
		// Pulse the selection
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MenuPulse");

		// remove the menu quickly
		m_bMenuTakesInput = false;
		m_flShutoffTime = GetMenuTime() + m_flOpenCloseTime;
		g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MenuClose");

#ifdef MAPBASE
		if ( IsCentered() )
		{
			CHudCloseCaption *pHudCloseCaption = (CHudCloseCaption *)GET_HUDELEMENT( CHudCloseCaption );
			if ( pHudCloseCaption && pHudCloseCaption->IsUsingCustomDimensions( ToHandle() ) )
			{
				// Reset if it's still using custom dimensions we defined
				pHudCloseCaption->StopUsingCustomDimensions( MENU_CC_SPEED_REVERT );
			}
		}
#endif
	}
}

void CHudMenu::ProcessText( void )
{
	m_Processed.RemoveAll();
	m_nMaxPixels = 0;
	m_nHeight = 0;

	int i = 0;
	int startpos = i;
	int menuitem = 0;
	while ( i < MAX_MENU_STRING  )
	{
		wchar_t ch = g_szMenuString[ i ];
		if ( ch == 0 )
			break;

		if ( i == startpos && 
			( ch == L'-' && g_szMenuString[ i + 1 ] == L'>' ) )
		{
			// Special handling for menu item specifiers
			swscanf( &g_szMenuString[ i + 2 ], L"%d", &menuitem );
			i += 2;
			startpos += 2;

			continue;
		}

		// Skip to end of line
		while ( i < MAX_MENU_STRING && g_szMenuString[i] != 0 && g_szMenuString[i] != L'\n' )
		{
			i++;
		}

		// Store off line
		if ( ( i - startpos ) >= 1 )
		{
			ProcessedLine line;
			line.menuitem = menuitem;
			line.startchar = startpos;
			line.length = i - startpos;
			line.pixels = 0;
			line.height = 0;

			m_Processed.AddToTail( line );
		}

		menuitem = 0;

		// Skip delimiter
		if ( g_szMenuString[i] == '\n' )
		{
			i++;
		}
		startpos = i;
	}

	// Add final block
	if ( i - startpos >= 1 )
	{
		ProcessedLine line;
		line.menuitem = menuitem;
		line.startchar = startpos;
		line.length = i - startpos;
		line.pixels = 0;
		line.height = 0;

		m_Processed.AddToTail( line );
	}

	// Now compute pixels needed
	int c = m_Processed.Count();
	for ( i = 0; i < c; i++ )
	{
		ProcessedLine *l = &m_Processed[ i ];
		Assert( l );
		
#ifdef MAPBASE
		bool isItem = true;
		if (l->menuitem == 0 && l->startchar < (MAX_MENU_STRING-1) && g_szMenuString[ l->startchar ] != L'0' && g_szMenuString[ l->startchar+1 ] != L'.')
		{
			// Can't use 0 directly because it gets conflated with the cancel item
			isItem = false;
		}
#else
		bool isItem = l->menuitem != 0;
#endif

		int pixels = 0;
		vgui::HFont font = isItem ? m_hItemFont : m_hTextFont;

#ifdef MAPBASE
		if ( IsUsingBigFonts() && font == m_hItemFont)
			font = m_hItemFontBig;

		// We use a more complex algorithm to support breaking lines when word wrapping is enabled
		int nMaxPixels = ( IsFillingWidth() ? m_nCenterWide - hud_menu_center_border_x.GetInt() : hud_menu_complex_max_pixels.GetInt() ) - (m_nBorder*2);
		int nHighestPixels = 0, nItemPixels = 0, nNumLines = 1, iLastSpace = 0;
		for ( int ch = 0; ch < l->length; ch++ )
		{
			pixels += vgui::surface()->GetCharacterWidth( font, g_szMenuString[ ch + l->startchar ] );

			if ( ShouldWordWrap() )
			{
				if ( pixels > nMaxPixels && iLastSpace != 0 )
				{
					// Replace last space with a newline and increment lines
					g_szMenuString[iLastSpace] = '\n';
					nNumLines++;
					if ( nNumLines == 2 )
					{
						// Account for indent
						nMaxPixels -= nItemPixels;
					}

					if ( pixels > nHighestPixels )
						nHighestPixels = pixels;
					pixels = 0;
				}
				else if ( ch == 3 && isItem )
				{
					// If this is a menu item (e.g. '1. Item') and we've drawn the number part,
					// store how many pixels it is so that line breaks are indented properly
					nItemPixels = pixels;
				}
			}
			
			if ( g_szMenuString[ ch + l->startchar ] == ' ' )
				iLastSpace = ch + l->startchar;
		}

		l->pixels = nHighestPixels == 0 ? pixels : nHighestPixels;
		l->height = vgui::surface()->GetFontTall( font ) * nNumLines;

		if ( IsUsingBigFonts() && i+1 != c ) // All but the last
			l->height += hud_menu_big_text_margin.GetInt();
#else
		for ( int ch = 0; ch < l->length; ch++ )
		{
			pixels += vgui::surface()->GetCharacterWidth( font, g_szMenuString[ ch + l->startchar ] );
		}

		l->pixels = pixels;
		l->height = vgui::surface()->GetFontTall( font );
#endif

		if ( l->pixels > m_nMaxPixels )
		{
			m_nMaxPixels = l->pixels;
		}
		m_nHeight += l->height;
	}
}
//-----------------------------------------------------------------------------
// Purpose: Local method to hide a menu, mirroring code found in
//          MsgFunc_ShowMenu.
//-----------------------------------------------------------------------------
void CHudMenu::HideMenu( void )
{
	m_bMenuTakesInput = false;
	m_flShutoffTime = GetMenuTime() + m_flOpenCloseTime;
	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MenuClose");

#ifdef MAPBASE
	CHudCloseCaption *pHudCloseCaption = (CHudCloseCaption *)GET_HUDELEMENT( CHudCloseCaption );
	if ( pHudCloseCaption && pHudCloseCaption->IsUsingCustomDimensions( ToHandle() ) )
	{
		// Reset if it's still using custom dimensions we defined
		pHudCloseCaption->StopUsingCustomDimensions( MENU_CC_SPEED_REVERT );
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Local method to bring up a menu, mirroring code found in
//          MsgFunc_ShowMenu.
//
//   takes two values:
//		menuName  : menu name string 
//		validSlots: a bitfield describing the valid keys
//-----------------------------------------------------------------------------
void CHudMenu::ShowMenu( const char * menuName, int validSlots )
{
	m_flShutoffTime = -1;
	m_bitsValidSlots = validSlots;
	m_fWaitingForMore = 0;
	m_nBorder = 20;
#ifdef MAPBASE
	m_bMapDefinedMenu = false;
	m_bPlayingFadeout = false;
	m_nMenuFlags = 0;
	m_flBoxAlphaOverride = 0.0f;

	CHudCloseCaption *pHudCloseCaption = (CHudCloseCaption *)GET_HUDELEMENT( CHudCloseCaption );
	if ( pHudCloseCaption && pHudCloseCaption->IsUsingCustomDimensions( ToHandle() ) )
	{
		// Reset if it's still using custom dimensions we defined
		pHudCloseCaption->StopUsingCustomDimensions();
	}
#endif

	Q_strncpy( g_szPrelocalisedMenuString, menuName, sizeof( g_szPrelocalisedMenuString ) );

	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MenuOpen");
	m_nSelectedItem = -1;

	// we have the whole string, so we can localise it now
	char szMenuString[MAX_MENU_STRING];
	Q_strncpy( szMenuString, ConvertCRtoNL( hudtextmessage->BufferedLocaliseTextString( g_szPrelocalisedMenuString ) ), sizeof( szMenuString ) );
	g_pVGuiLocalize->ConvertANSIToUnicode( szMenuString, g_szMenuString, sizeof( g_szMenuString ) );
	
	ProcessText();

	m_bMenuDisplayed = true;
	m_bMenuTakesInput = true;

	m_flSelectionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::ShowMenu_KeyValueItems( KeyValues *pKV )
{
	m_flShutoffTime = -1;
	m_fWaitingForMore = 0;
	m_bitsValidSlots = 0;
	m_nBorder = 20;
#ifdef MAPBASE
	m_bMapDefinedMenu = false;
	m_bPlayingFadeout = false;
	
	m_nMenuFlags = 0;
	m_flBoxAlphaOverride = 0.0f;

	CHudCloseCaption *pHudCloseCaption = (CHudCloseCaption *)GET_HUDELEMENT( CHudCloseCaption );
	if ( pHudCloseCaption && pHudCloseCaption->IsUsingCustomDimensions( ToHandle() ) )
	{
		// Reset if it's still using custom dimensions we defined
		pHudCloseCaption->StopUsingCustomDimensions();
	}
#endif

	g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MenuOpen");
	m_nSelectedItem = -1;
	
	g_szMenuString[0] = '\0';
	wchar_t *pWritePosition = g_szMenuString;
	int		nRemaining = sizeof( g_szMenuString ) / sizeof( wchar_t );
	int		nCount;

	int i = 0;
	for ( KeyValues *item = pKV->GetFirstSubKey(); item != NULL; item = item->GetNextKey() )
	{
		// Set this slot valid
		m_bitsValidSlots |= (1<<i);

		const char *pszItem = item->GetName();
		const wchar_t *wLocalizedItem = g_pVGuiLocalize->Find( pszItem );

#ifdef MAPBASE
		nCount = _snwprintf( pWritePosition, nRemaining, L"->%d. %ls\n", i+1, wLocalizedItem );
#else
		nCount = _snwprintf( pWritePosition, nRemaining, L"%d. %ls\n", i+1, wLocalizedItem );
#endif
		nRemaining -= nCount;
		pWritePosition += nCount;

		i++;
	}

	// put a cancel on the end
	m_bitsValidSlots |= (1<<9);

#ifdef MAPBASE
	nCount = _snwprintf( pWritePosition, nRemaining, L"->0. %ls\n", g_pVGuiLocalize->Find( "#Cancel" ) );
#else
	nCount = _snwprintf( pWritePosition, nRemaining, L"0. %ls\n", g_pVGuiLocalize->Find( "#Cancel" ) );
#endif
	nRemaining -= nCount;
	pWritePosition += nCount;

	ProcessText();

	m_bMenuDisplayed = true;
	m_bMenuTakesInput = true;

	m_flSelectionTime = gpGlobals->curtime;
}

//-----------------------------------------------------------------------------
// Purpose: Message handler for ShowMenu message
//   takes four values:
//		short: a bitfield of keys that are valid input
//		char : the duration, in seconds, the menu should stay up. -1 means is stays until something is chosen.
//		byte : a boolean, TRUE if there is more string yet to be received before displaying the menu, false if it's the last string
//		string: menu string to display
//  if this message is never received, then scores will simply be the combined totals of the players.
//-----------------------------------------------------------------------------
void CHudMenu::MsgFunc_ShowMenu( bf_read &msg)
{
	m_bitsValidSlots = (short)msg.ReadWord();
	int DisplayTime = msg.ReadChar();
	int NeedMore = msg.ReadByte();

	if ( DisplayTime > 0 )
	{
		m_flShutoffTime = m_flOpenCloseTime + DisplayTime + GetMenuTime();
	}
	else
	{
		m_flShutoffTime = -1;
	}

	if ( m_bitsValidSlots )
	{
		char szString[2048];
		msg.ReadString( szString, sizeof(szString) );

		if ( !m_fWaitingForMore ) // this is the start of a new menu
		{
			Q_strncpy( g_szPrelocalisedMenuString, szString, sizeof( g_szPrelocalisedMenuString ) );
		}
		else
		{  // append to the current menu string
			Q_strncat( g_szPrelocalisedMenuString, szString, sizeof( g_szPrelocalisedMenuString ), COPY_ALL_CHARACTERS );
		}

		if ( !NeedMore )
		{  
			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence("MenuOpen");
			m_nSelectedItem = -1;
			
			// we have the whole string, so we can localise it now
			char szMenuString[MAX_MENU_STRING];
			Q_strncpy( szMenuString, ConvertCRtoNL( hudtextmessage->BufferedLocaliseTextString( g_szPrelocalisedMenuString ) ), sizeof( szMenuString ) );
			g_pVGuiLocalize->ConvertANSIToUnicode( szMenuString, g_szMenuString, sizeof( g_szMenuString ) );
			
			ProcessText();
		}

		m_bMenuDisplayed = true;
		m_bMenuTakesInput = true;

		m_flSelectionTime = gpGlobals->curtime;
	}
	else
	{
		HideMenu();
	}

	m_fWaitingForMore = NeedMore;
	m_nBorder = 20;
#ifdef MAPBASE
	m_bMapDefinedMenu = false;
	m_bPlayingFadeout = false;
	
	m_nMenuFlags = 0;
	m_flBoxAlphaOverride = 0.0f;

	CHudCloseCaption *pHudCloseCaption = (CHudCloseCaption *)GET_HUDELEMENT( CHudCloseCaption );
	if ( pHudCloseCaption && pHudCloseCaption->IsUsingCustomDimensions( ToHandle() ) )
	{
		// Reset if it's still using custom dimensions we defined
		pHudCloseCaption->StopUsingCustomDimensions();
	}
#endif
}

#ifdef MAPBASE
ConVar	hud_menu_complex_border( "hud_menu_complex_border", "30" );

//-----------------------------------------------------------------------------
// Purpose: Message handler for ShowMenu message with more options for game_menu
//   takes four values:
//		short : a bitfield of keys that are valid input
//		float : the duration, in seconds, the menu should stay up. -1 means it stays until something is chosen.
//		byte  : a boolean, TRUE if there is more string yet to be received before displaying the menu, false if it's the last string
//		string: menu string to display
//  if this message is never received, then scores will simply be the combined totals of the players.
//-----------------------------------------------------------------------------
void CHudMenu::MsgFunc_ShowMenuComplex( bf_read &msg)
{
	m_bitsValidSlots = (short)msg.ReadWord();
	float DisplayTime = msg.ReadFloat();
	int NeedMore = msg.ReadByte();
	m_nMenuFlags = msg.ReadByte();

	m_nBorder = hud_menu_complex_border.GetInt();
	m_bMapDefinedMenu = true;
	m_bPlayingFadeout = false;

	if ( DisplayTime > 0 )
	{
		m_flShutoffTime = m_flOpenCloseTime + DisplayTime + GetMenuTime();
	}
	else
	{
		m_flShutoffTime = -1;
	}

	if ( m_bitsValidSlots > -1 )
	{
		char szString[2048];
		msg.ReadString( szString, sizeof(szString) );

		if ( !m_fWaitingForMore ) // this is the start of a new menu
		{
			Q_strncpy( g_szPrelocalisedMenuString, szString, sizeof( g_szPrelocalisedMenuString ) );
		}
		else
		{  // append to the current menu string
			Q_strncat( g_szPrelocalisedMenuString, szString, sizeof( g_szPrelocalisedMenuString ), COPY_ALL_CHARACTERS );
		}

		if ( !NeedMore )
		{  
			if ( IsFillingWidth() )
			{
				CHudCloseCaption *pHudCloseCaption = (CHudCloseCaption *)GET_HUDELEMENT( CHudCloseCaption );
				if ( pHudCloseCaption )
				{
					// Need to get this before we process the text
					m_nCenterWide = pHudCloseCaption->GetWide();
				}
			}

			const char *pszAnimName = "MenuOpenSlow";
			if ( ShouldMenuFlash() )
			{
				if ( IsCentered() )
					pszAnimName = "MenuOpenFlashTitle";
				else
					pszAnimName = "MenuOpenFlash";
			}

			g_pClientMode->GetViewportAnimationController()->StartAnimationSequence( pszAnimName );
			m_nSelectedItem = -1;
			
			// we have the whole string, so we can localise it now
			wchar_t *pWritePosition = g_szMenuString;
			int		nRemaining = sizeof( g_szMenuString ) / sizeof( wchar_t );
			int		nCount;

			char *pszToken = strtok( szString, "\n" );
			int nCurItem = 0;
			for (; pszToken != NULL; pszToken = strtok( NULL, "\n" ), nCurItem++)
			{
				if (!*pszToken || *pszToken == ' ')
					continue;

				wchar_t wszMenuItem[256];

				const wchar_t *wLocalizedItem = g_pVGuiLocalize->Find( pszToken );
				if (wLocalizedItem)
				{
					V_wcsncpy( wszMenuItem, wLocalizedItem, sizeof( wszMenuItem ) );
				}
				else
				{
					g_pVGuiLocalize->ConvertANSIToUnicode( pszToken, wszMenuItem, sizeof( wszMenuItem ) );
				}

				if (nCurItem == 0)
				{
					// First item is title
					nCount = _snwprintf( pWritePosition, nRemaining, L"%ls\n", wszMenuItem );
				}
				else
				{
					// If this item isn't valid, skip until it is
					//while (!(m_bitsValidSlots & (1 << nCurItem)) && nCurItem < 10)
					//{
					//	nCurItem++;
					//}

					if (nCurItem == 10)
						nCurItem = 0;

					nCount = _snwprintf( pWritePosition, nRemaining, L"->%d. %ls\n", nCurItem, wszMenuItem );
				}

				nRemaining -= nCount;
				pWritePosition += nCount;
			}
			
			ProcessText();

			if ( IsCentered() )
			{
				int tall = m_nHeight + m_nBorder;
				RepositionAndFollowCloseCaption( tall );
			}
			else
			{
				m_flBoxAlphaOverride = 0.0f;

				CHudCloseCaption *pHudCloseCaption = (CHudCloseCaption *)GET_HUDELEMENT( CHudCloseCaption );
				if ( pHudCloseCaption && pHudCloseCaption->IsUsingCustomDimensions( ToHandle() ) )
				{
					// Reset if it's still using custom dimensions we defined
					pHudCloseCaption->StopUsingCustomDimensions();
				}
			}
		}

		m_bMenuDisplayed = true;

		if (m_bitsValidSlots > 0)
			m_bMenuTakesInput = true;
		else
			m_bMenuTakesInput = false;

		m_flSelectionTime = gpGlobals->curtime;
	}
	else
	{
		HideMenu();
	}

	m_fWaitingForMore = NeedMore;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CHudMenu::RepositionAndFollowCloseCaption( int yOffset )
{
	// Invert the Y axis
	//SetPos( m_iTypeAudioX, ScreenHeight() - m_iTypeAudioY );

	// Place underneath the close caption element
	CHudCloseCaption *pHudCloseCaption = (CHudCloseCaption *)GET_HUDELEMENT( CHudCloseCaption );
	if (pHudCloseCaption /*&& !pHudCloseCaption->IsUsingCustomDimensions()*/)
	{
		int ccX, ccY;
		pHudCloseCaption->GetDefaultPos( ccX, ccY );

		//if (!pHudCloseCaption->IsUsingCustomDimensions( ToHandle() ))
		{
			pHudCloseCaption->StartUsingCustomDimensions( ToHandle(), -1, ccY - yOffset - hud_menu_center_cc_margin.GetInt() );
		}

		m_nCenterY = ccY + pHudCloseCaption->GetTall() - yOffset + m_nBorder/2;

		if ( IsFillingWidth() )
		{
			m_nCenterX = ccX + m_nBorder/2;
		}
		else
		{
			m_nCenterX = 0;
		}

		m_flBoxAlphaOverride = pHudCloseCaption->GetBackgroundAlpha();
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: hud scheme settings
//-----------------------------------------------------------------------------
void CHudMenu::ApplySchemeSettings(vgui::IScheme *pScheme)
{
	BaseClass::ApplySchemeSettings(pScheme);

	SetPaintBackgroundEnabled( false );

	// set our size
	int screenWide, screenTall;
	int x, y;
	GetPos(x, y);
	GetHudSize(screenWide, screenTall);
	SetBounds(0, y, screenWide, screenTall - y);

	ProcessText();
}