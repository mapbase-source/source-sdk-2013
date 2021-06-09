#ifndef NOVELUI_H
#define NOVELUI_H
#ifdef _WIN32
#pragma once
#endif
#include "GameUI/IGameUI.h"
#include "igamesystem.h"
#include "vgui_controls/Panel.h"

class INovelUI : public CBaseGameSystem {
public:
    virtual void GetLocalizedString( const char *pszSource, wchar_t *szDestination, int iDestChars ) = 0;
    virtual vgui::VPANEL GetRoot( void ) = 0;
    virtual IGameUI * GameUI( void ) = 0;
};
extern INovelUI *g_pNovelUI;

class CBaseNovelUIElement : public vgui::Panel {
DECLARE_CLASS_SIMPLE( CBaseNovelUIElement, vgui::Panel );
public:
    CBaseNovelUIElement( const char *pszElementName );
    virtual void Initialize( void );
    virtual void Shutdown( void );

    void SetPropBounds( int xpos, int ypos, int wide, int tall );
    void SetPropSize( int wide, int tall );
    void SetPropPos( int xpos, int ypos );
};

// Used to internally register UI elements.
class CNovelUIElementReg {
public:
    typedef void( *CreateElementFn )( CBaseNovelUIElement ** );
    typedef void( *DeleteElementFn )( CBaseNovelUIElement ** );

public:
    static CNovelUIElementReg *s_pElements;
    CNovelUIElementReg *m_pNext;

    CreateElementFn m_pCreateFn;
    DeleteElementFn m_pDeleteFn;
    CBaseNovelUIElement *m_pElement = NULL;

public:
    CNovelUIElementReg( CreateElementFn pCreateFn, DeleteElementFn pDeleteFn );
    void Create( void );
    void Delete( void );
};

// Declares UI element...
#define REGISTER_UI_ELEMENT(classname)                              \
    static void classname##_Create(CBaseNovelUIElement **pElement)  \
    {                                                               \
        *pElement = (CBaseNovelUIElement *)(new classname);         \
    }                                                               \
    static void classname##_Delete(CBaseNovelUIElement **pElement)  \
    {                                                               \
        delete *pElement;                                           \
        *pElement = nullptr;                                        \
    }                                                               \
    static CNovelUIElementReg s_##classname##_reg(classname##_Create, classname##_Delete)

#endif
