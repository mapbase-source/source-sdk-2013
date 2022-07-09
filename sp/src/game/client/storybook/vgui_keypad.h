//==========================================================================//
//
// Purpose: VGUI Keypad made by Maestro Fenix 2015
//
// $NoKeywords: $
//===========================================================================//

using namespace vgui;

#include <vgui/IVGui.h>
#include <vgui_controls/Frame.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls\Panel.h>
#include <vgui\ISurface.h> 
#include "usermessages.h" //For user messages at keypad
#include "hud_macros.h" //Needed for networking messages
#include "hudelement.h" //Needed for networking messages
#include "iclientmode.h" //Needed for getting the viewport of the player
#include "engine/IEngineSound.h" //For sounds

class CVgui_keypad : public vgui::Frame, public CHudElement
{
	DECLARE_CLASS(CVgui_keypad, vgui::Frame);

public:
	CVgui_keypad(const char *pElementName); 	// Constructor vgui::VPANEL parent
	~CVgui_keypad(){};				// Destructor

	void CheckPass(int number);
	void OnTick();
	void Init();
	void MsgFunc_KeypadHandler(bf_read &msg);

protected:
	//VGUI overrides:
	void PaintBackground();
	void Paint();
	virtual void OnCommand(const char* pcCommand);
	virtual void OnThink(void);

private:
	//Other used VGUI control Elements:
	ImagePanel *m_pBackgroundPanel;
	Button *m_pCloseButton;
	Button *m_pClearButton;
	Button *m_pMenuButtons[100];
	Label *m_pScreenTitle;
	Label *m_pLabel;
	ImagePanel *m_imgMenuButtons[100];
	ImagePanel *m_imgClearButton;
	ImagePanel *m_imgCloseButton;
	char szCode[32];
	int m_pPressedNumbers;
	char szEnteredNumbers[32];
	bool m_pHidePass;
	char szFrameLabel[32];
	bool m_showKeypad;
};
