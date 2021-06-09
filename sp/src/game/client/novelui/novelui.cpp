#include "cbase.h"
#include "ienginevgui.h"
#include "vgui/ILocalize.h"
#include "novelui.h"
#include "rootpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CNovelUI : public INovelUI {
public:
    const char * Name( void );
    bool Init( void );
    void PostInit( void );
    void Shutdown( void );
    void VidInit( void );

public:
    void GetLocalizedString( const char *pszSource, wchar_t *szDestination, int iDestChars );
    vgui::VPANEL GetRoot( void );
    IGameUI * GameUI( void );

private:
    IGameUI *m_pGameUI;
    CNovelUIRootPanel *m_pRootPanel;
};

static CNovelUI g_NovelUI;
static CDllDemandLoader s_GameUIDll( "GameUI" );
CNovelUIElementReg *CNovelUIElementReg::s_pElements = NULL;
INovelUI *g_pNovelUI = &g_NovelUI;

const char * CNovelUI::Name( void )
{
    return "NovelUI";
}

bool CNovelUI::Init( void )
{
    CreateInterfaceFn factory = s_GameUIDll.GetFactory();
    if( !factory )
        return false;

    m_pGameUI = (IGameUI *)factory( GAMEUI_INTERFACE_VERSION, NULL );
    if( !m_pGameUI )
        return false;
    
    g_pVGuiLocalize->AddFile( "resource/novelui_%language%.txt", "GAME", true );

    m_pRootPanel = new CNovelUIRootPanel( enginevgui->GetPanel( PANEL_GAMEUIDLL ) );
    m_pGameUI->SetMainMenuOverride( m_pRootPanel->GetVPanel() );

    return true;
}

void CNovelUI::PostInit( void )
{
    vgui::HScheme hScheme = g_pVGuiSchemeManager->LoadSchemeFromFile( "resource/uischeme.res", "UIScheme" );
    int iFound = 0;
    for( CNovelUIElementReg *pItem = CNovelUIElementReg::s_pElements; pItem; pItem = pItem->m_pNext ) {
        pItem->Create();
        pItem->m_pElement->SetParent( m_pRootPanel );
        pItem->m_pElement->SetScheme( hScheme );
        pItem->m_pElement->Initialize();
        iFound++;
    }
    ConColorMsg( LOG_COLOR_B_GREEN, "NovelUI: Initialized %d panels.\n", iFound );
}

void CNovelUI::Shutdown( void )
{
    for( CNovelUIElementReg *pItem = CNovelUIElementReg::s_pElements; pItem; pItem = pItem->m_pNext ) {
        pItem->m_pElement->Shutdown();
        pItem->Delete();
    }

    m_pGameUI->SetMainMenuOverride( ( vgui::VPANEL )0 );
    delete m_pRootPanel;
}

void CNovelUI::VidInit( void )
{
    int iUpdated = 0;
    for( CNovelUIElementReg *pItem = CNovelUIElementReg::s_pElements; pItem; pItem = pItem->m_pNext ) {
        if( pItem->m_pElement ) {
            // Initialize() usually loads data from scheme, so here we are.
            pItem->m_pElement->Initialize();
            iUpdated++;
        }
    }
    ConColorMsg( LOG_COLOR_B_GREEN, "NovelUI: VidInit() called, %d elements should be updated...\n", iUpdated );
}

void CNovelUI::GetLocalizedString( const char *pszSource, wchar_t *szDestination, int iDestChars )
{
    const wchar_t *pszValue = g_pVGuiLocalize->Find( pszSource );
    if( pszValue ) {
        Q_wcsncpy( szDestination, pszValue, sizeof( wchar_t ) * iDestChars );
        return;
    }
    g_pVGuiLocalize->ConvertANSIToUnicode( pszSource, szDestination, iDestChars );
}

vgui::VPANEL CNovelUI::GetRoot( void )
{
    if( !m_pRootPanel )
        return enginevgui->GetPanel( PANEL_GAMEUIDLL );
    return m_pRootPanel->GetVPanel();
}

IGameUI * CNovelUI::GameUI( void )
{
    return m_pGameUI;
}

CBaseNovelUIElement::CBaseNovelUIElement( const char *pszElementName ) : BaseClass( NULL, pszElementName )
{

}

void CBaseNovelUIElement::Initialize( void )
{

}

void CBaseNovelUIElement::Shutdown( void )
{

}

void CBaseNovelUIElement::SetPropBounds( int xpos, int ypos, int wide, int tall )
{
    xpos = g_pVGuiSchemeManager->GetProportionalScaledValue( xpos );
    ypos = g_pVGuiSchemeManager->GetProportionalScaledValue( ypos );
    wide = g_pVGuiSchemeManager->GetProportionalScaledValue( wide );
    tall = g_pVGuiSchemeManager->GetProportionalScaledValue( tall );
    SetBounds( xpos, ypos, wide, tall );
}

void CBaseNovelUIElement::SetPropSize( int wide, int tall )
{
    wide = g_pVGuiSchemeManager->GetProportionalScaledValue( wide );
    tall = g_pVGuiSchemeManager->GetProportionalScaledValue( tall );
    SetSize( wide, tall );
}

void CBaseNovelUIElement::SetPropPos( int xpos, int ypos )
{
    xpos = g_pVGuiSchemeManager->GetProportionalScaledValue( xpos );
    ypos = g_pVGuiSchemeManager->GetProportionalScaledValue( ypos );
    SetPos( xpos, ypos );
}

CNovelUIElementReg::CNovelUIElementReg( CreateElementFn pCreateFn, DeleteElementFn pDeleteFn ) : m_pCreateFn( pCreateFn ), m_pDeleteFn( pDeleteFn )
{
    m_pNext = s_pElements;
    s_pElements = this;
}

void CNovelUIElementReg::Create( void )
{
    if( !m_pElement ) {
        m_pCreateFn( &m_pElement );
    }
}

void CNovelUIElementReg::Delete( void )
{
    if( m_pElement ) {
        m_pElement->SetParent( ( vgui::VPANEL )0 );
        m_pDeleteFn( &m_pElement );
    }
}
