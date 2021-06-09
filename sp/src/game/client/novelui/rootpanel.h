#ifndef NOVELUI_ROOTPANEL_H
#define NOVELUI_ROOTPANEL_H
#ifdef _WIN32
#pragma once
#endif
#include "vgui_controls/Panel.h"

class CNovelUIRootPanel : public vgui::Panel {
    DECLARE_CLASS_SIMPLE( CNovelUIRootPanel, vgui::Panel );
public:
    CNovelUIRootPanel( vgui::VPANEL hParent );
    void ApplySchemeSettings( vgui::IScheme *pScheme );
};

#endif
