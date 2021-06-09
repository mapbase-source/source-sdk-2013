#include "cbase.h"
#include "novelui.h"
#include "engine/IEngineSound.h"
#include "tier0/icommandline.h"
#include "tier1/KeyValues.h"
#include "vgui/ISurface.h"
#include "filesystem.h"
#include "cdll_int.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CGameMenuButton : public CBaseNovelUIElement {
    DECLARE_CLASS_SIMPLE( CGameMenuButton, CBaseNovelUIElement );
public:
    CGameMenuButton();
    void Initialize();
    void Paint();
    void PaintBackground();

    void OnCursorEntered();
    void OnMousePressed( vgui::MouseCode code );

    void SetLabelText( const char *psz );
    void SetLabelText( const wchar_t *psz );
    void SetCommand( const char *psz );
private:
    vgui::HFont m_hFont;
    int m_iTextX, m_iTextY;

    wchar_t m_szLabelText[256];
    char m_szCommand[256];

    char m_szRolloverSound[256];
    char m_szClickSound[256];
};

CGameMenuButton::CGameMenuButton() : BaseClass( "MenuButton" )
{

}

void CGameMenuButton::Initialize()
{
    const char *pszResourceString;
    vgui::IScheme *pScheme = g_pVGuiSchemeManager->GetIScheme( GetScheme() );

    SetBgColor( pScheme->GetColor( pScheme->GetResourceString( "GameMenu.Button.BgColor" ), Color() ) );
    SetFgColor( pScheme->GetColor( pScheme->GetResourceString( "GameMenu.Button.FgColor" ), Color() ) );

    m_hFont = pScheme->GetFont( pScheme->GetResourceString( "GameMenu.Button.Font" ), true );
    m_iTextX = g_pVGuiSchemeManager->GetProportionalScaledValue( Q_atoi( pScheme->GetResourceString( "GameMenu.Button.TextX" ) ) );
    m_iTextY = g_pVGuiSchemeManager->GetProportionalScaledValue( Q_atoi( pScheme->GetResourceString( "GameMenu.Button.TextY" ) ) );

    int iWidth = Q_atoi( pScheme->GetResourceString( "GameMenu.Button.Width" ) );
    int iHeight = Q_atoi( pScheme->GetResourceString( "GameMenu.Button.Height" ) );
    SetPropSize( iWidth, iHeight );

    // Rollover sound
    pszResourceString = pScheme->GetResourceString( "GameMenu.Button.RolloverSound" );
    if( pszResourceString && pszResourceString[0] ) {
        Q_strncpy( m_szRolloverSound, pszResourceString, 256 );
    }

    // Click sound
    pszResourceString = pScheme->GetResourceString( "GameMenu.Button.ClickSound" );
    if( pszResourceString && pszResourceString[0] ) {
        Q_strncpy( m_szClickSound, pszResourceString, 256 );
    }

    enginesound->PrecacheSound( m_szRolloverSound, true, true );
    enginesound->PrecacheSound( m_szClickSound, true, true );
}

void CGameMenuButton::Paint()
{
    Color fgColor = GetFgColor();
    if( IsCursorOver() ) {
        fgColor = GetBgColor();
        fgColor[0] = 255 - fgColor[0];  // r
        fgColor[1] = 255 - fgColor[1];  // g
        fgColor[2] = 255 - fgColor[2];  // b
        fgColor[3] = 255;               // a
    }

    g_pVGuiSurface->DrawSetTextColor( fgColor );
    g_pVGuiSurface->DrawSetTextFont( m_hFont );
    g_pVGuiSurface->DrawSetTextPos( m_iTextX, m_iTextY );
    g_pVGuiSurface->DrawPrintText( m_szLabelText, Q_wcslen( m_szLabelText ) );
}

void CGameMenuButton::PaintBackground()
{
    if( IsCursorOver() ) {
        int wide, tall;
        Color bg = GetBgColor();
        g_pVGuiSurface->GetScreenSize( wide, tall );
        g_pVGuiSurface->DrawSetColor( bg );
        g_pVGuiSurface->DrawFilledRectFade( 0, 0, wide, tall, bg.a(), 0, true );
    }
}

void CGameMenuButton::OnCursorEntered()
{
    BaseClass::OnCursorEntered();
    g_pVGuiSurface->PlaySound( m_szRolloverSound );
}

void CGameMenuButton::OnMousePressed( vgui::MouseCode code )
{
    BaseClass::OnMousePressed( code );
    if( code == vgui::MouseCode::MOUSE_FIRST ) {
        g_pVGuiSurface->PlaySound( m_szClickSound );
        g_pNovelUI->GameUI()->SendMainMenuCommand( m_szCommand );
    }
}

void CGameMenuButton::SetLabelText( const char *psz )
{
    g_pNovelUI->GetLocalizedString( psz, m_szLabelText, 256 );
}

void CGameMenuButton::SetLabelText( const wchar_t *psz )
{
    Q_wcsncpy( m_szLabelText, psz, sizeof( m_szLabelText ) );
}

void CGameMenuButton::SetCommand( const char *psz )
{
    Q_strncpy( m_szCommand, psz, 256 );
}

class CGameMenu : public CBaseNovelUIElement {
    DECLARE_CLASS_SIMPLE( CGameMenu, CBaseNovelUIElement );
public:
    CGameMenu();
    void Clear();

    void Initialize();
    void Shutdown();
    void OnThink();

private:
    bool m_bWasInGame;
    CUtlVector<CGameMenuButton *> m_vecButtons;
};
REGISTER_UI_ELEMENT( CGameMenu );

CGameMenu::CGameMenu() : BaseClass( "Menu" ), m_vecButtons()
{
    // We are invisible -- just a canvas for buttons.
    SetPaintBackgroundEnabled( false );
    SetPaintBorderEnabled( false );
    SetPaintEnabled( false );

    m_bWasInGame = engine->IsInGame();
}

void CGameMenu::Clear()
{
    for( int i = 0; i < m_vecButtons.Count(); i++ ) {
        if( m_vecButtons[i] ) {
            m_vecButtons[i]->SetParent( ( vgui::VPANEL )0 );
            delete m_vecButtons[i];
        }
    }

    m_vecButtons.RemoveAll();
}

void CGameMenu::Initialize()
{
    vgui::IScheme *pScheme = g_pVGuiSchemeManager->GetIScheme( GetScheme() );
    Clear();

    int iXpos = Q_atoi( pScheme->GetResourceString( "GameMenu.X" ) );
    int iYpos = Q_atoi( pScheme->GetResourceString( "GameMenu.Y" ) );
    int iButtonHeight = Q_atoi( pScheme->GetResourceString( "GameMenu.Button.Height" ) );
    int iWidth = Q_atoi( pScheme->GetResourceString( "GameMenu.Width" ) );
    int iHeight = 0;
    SetPropPos( iXpos, iYpos );

    KeyValues *pLayoutKV = new KeyValues( "GameMenu" );
    pLayoutKV->LoadFromFile( filesystem, "resource/menulayout.res", "GAME" );
    for( KeyValues *pSub = pLayoutKV->GetFirstSubKey(); pSub; pSub = pSub->GetNextTrueSubKey() ) {
        const char *pszButtonName = pSub->GetName();
        const char *pszButtonCommand = pSub->GetString( "command", "" );
        bool bIsBlank = pSub->GetBool( "blank", false );
        bool bShowInGame = pSub->GetBool( "in_game", true );
        bool bShowInMenu = pSub->GetBool( "in_menu", true );

        // in_game is 0, so skip -- we are in game
        if( m_bWasInGame && !bShowInGame ) {
            continue;
        }

        // in_menu is 0, so skil -- we are in menu
        if( !m_bWasInGame && !bShowInMenu ) {
            continue;
        }

        // blank -- just skip empty space.
        if( bIsBlank ) {
            iHeight += iButtonHeight;
            continue;
        }

        CGameMenuButton *pButton = new CGameMenuButton();
        pButton->SetParent( this );
        pButton->Initialize();
        pButton->SetCommand( pszButtonCommand );
        pButton->SetLabelText( pszButtonName );
        pButton->SetPropPos( 0, iHeight );

        iHeight += iButtonHeight;
        m_vecButtons.AddToTail( pButton );
    }
    pLayoutKV->deleteThis();
    SetPropSize( iWidth, iHeight );
}

void CGameMenu::Shutdown()
{
    Clear();
}

void CGameMenu::OnThink()
{
    bool bInGame = engine->IsInGame() && !engine->IsLevelMainMenuBackground();

    // Hey! I need an update!
    if( bInGame != m_bWasInGame ) {
        m_bWasInGame = bInGame;
        Initialize();
    }
}
