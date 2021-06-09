#include "cbase.h"
#include "vgui/ISurface.h"
#include "rootpanel.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CNovelUIRootPanel::CNovelUIRootPanel( vgui::VPANEL hParent ) : BaseClass( NULL, "NovelUIRoot" )
{
    // The hParent is possibly PANEL_GAMEUIDLL
    SetParent( hParent );
}

void CNovelUIRootPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
    BaseClass::ApplySchemeSettings( pScheme );

    // Resize the panel to the screen size, otherwise
    // it will be just a little dot in the top-left corner.
    int iWide, iTall;
    g_pVGuiSurface->GetScreenSize( iWide, iTall );
    SetBounds( 0, 0, iWide, iTall );
}
