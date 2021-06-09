#include "cbase.h"
#include "novelui.h"
#include "vgui/ISurface.h"
#include "tier1/KeyValues.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CGameTitle : public CBaseNovelUIElement {
    DECLARE_CLASS_SIMPLE( CGameTitle, CBaseNovelUIElement );
public:
    CGameTitle();
    void Initialize();
    void Paint();

private:
    wchar_t m_szLabel[256];
    vgui::HFont m_hFont;
    Color m_color;
};
REGISTER_UI_ELEMENT( CGameTitle );

CGameTitle::CGameTitle() : BaseClass( "GameTitle" )
{
    // ????
    SetPaintBackgroundEnabled( false );
}

void CGameTitle::Initialize()
{
    vgui::IScheme *pScheme = g_pVGuiSchemeManager->GetIScheme( GetScheme() );

    int iWidth = Q_atoi( pScheme->GetResourceString( "GameTitle.Width" ) );
    int iHeight = Q_atoi( pScheme->GetResourceString( "GameTitle.Height" ) );
    int iXpos = Q_atoi( pScheme->GetResourceString( "GameTitle.X" ) );
    int iYpos = Q_atoi( pScheme->GetResourceString( "GameTitle.Y" ) );
    SetPropBounds( iXpos, iYpos, iWidth, iHeight );

    m_hFont = pScheme->GetFont( pScheme->GetResourceString( "GameTitle.Font" ), true );
    m_color = pScheme->GetColor( pScheme->GetResourceString( "GameTitle.Color" ), Color( 0, 0, 0, 255 ) );

    KeyValues *pGameinfoKV = new KeyValues( "Gameinfo" );
    pGameinfoKV->LoadFromFile( filesystem, "gameinfo.txt", "GAME" );
    Q_wcsncpy( m_szLabel, pGameinfoKV->GetWString( "novel_title", L"MAPBASE" ), sizeof( m_szLabel ) ); // REFRACTION <3 MAPBASE
    pGameinfoKV->deleteThis();
}

void CGameTitle::Paint()
{
    g_pVGuiSurface->DrawSetTextColor( m_color );
    g_pVGuiSurface->DrawSetTextFont( m_hFont );
    g_pVGuiSurface->DrawSetTextPos( 0, 0 );
    g_pVGuiSurface->DrawPrintText( m_szLabel, Q_wcslen( m_szLabel ) );
}
