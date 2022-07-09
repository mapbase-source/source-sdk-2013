//==========================================================================//
//
// Purpose: VGUI Keypad made by Maestro Fenix 2015
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "vgui_keypad.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//Disable this and all the related for debug purposes
//ConVar cl_showkeypad("cl_showkeypad", "0", FCVAR_HIDDEN, "Toggles keypad panel <state>");

DECLARE_HUDELEMENT(CVgui_keypad);
DECLARE_HUD_MESSAGE(CVgui_keypad, KeypadHandler)

CVgui_keypad::CVgui_keypad(const char *pElementName) : CHudElement(pElementName), BaseClass(NULL, "Vgui_keypad") //vgui::VPANEL parent
{

	m_showKeypad = false;

	vgui::Panel *parent = g_pClientMode->GetViewport();
	SetParent(parent);

	SetTitleBarVisible(false);
	SetMinimizeButtonVisible(false);
	SetMaximizeButtonVisible(false);
	SetCloseButtonVisible(false);
	SetSizeable(false);
	SetMoveable(false);
	SetSize(300, 600);

	vgui::ivgui()->AddTickSignal(GetVPanel(), 100);

	SetScheme(vgui::scheme()->LoadSchemeFromFile("resource/SourceScheme.res", "SourceScheme"));

	LoadControlSettings("resource/ui/keypad.res");

	//Keypad background
	m_pBackgroundPanel = FindControl<ImagePanel>("KeypadBackground", true); //Searches at the RES file the button, so we can modify it here
	m_pBackgroundPanel->SetImage(scheme()->GetImage("keypad/keypad_base", false));
	m_pBackgroundPanel->SetSize(512, 1024);

	//Close Button
	m_pCloseButton = FindControl<Button>("ButtonCls", true);
	m_pCloseButton->SetDepressedSound("buttons/blip1.wav");
	m_pCloseButton->SetAlpha(0);
	m_pCloseButton->SetSize(64, 64);
	m_imgCloseButton = FindControl<ImagePanel>("ButtonClsImg", true);
	m_imgCloseButton->SetImage("keypad/keypad_close");
	m_imgCloseButton->SetSize(64, 64);

	//Clear Button
	m_pClearButton = FindControl<Button>("ButtonClr", true);
	m_pClearButton->SetAlpha(0);
	m_pClearButton->SetDepressedSound("buttons/blip1.wav");
	m_pClearButton->SetSize(64, 64);
	m_imgClearButton = FindControl<ImagePanel>("ButtonClrImg", true);
	m_imgClearButton->SetImage("keypad/keypad_clear");
	m_imgClearButton->SetSize(64, 64);

	//Code viewport
	m_pLabel = FindControl<Label>("KeypadLabel", true);
	m_pLabel->SetVisible(true);
	m_pLabel->SetText("");
	IScheme *pScheme = scheme()->GetIScheme(GetScheme());
	HFont font = pScheme->GetFont("CreditsText");
	m_pLabel->SetFont(font);
	m_pLabel->SetWide(512);

	//Keypad screen text
	m_pScreenTitle = FindControl<Label>("KeypadLabelTitle", true);
	m_pScreenTitle->SetVisible(true);
	font = pScheme->GetFont("CreditsText");
	m_pScreenTitle->SetFont(font);
	m_pScreenTitle->SetWide(512);

	//NumPad Buttons
	//FUgly, find a way to do it on a loop
	m_pMenuButtons[0] = FindControl<Button>("KeypadButton0", true);
	m_pMenuButtons[1] = FindControl<Button>("KeypadButton1", true);
	m_pMenuButtons[2] = FindControl<Button>("KeypadButton2", true);
	m_pMenuButtons[3] = FindControl<Button>("KeypadButton3", true);
	m_pMenuButtons[4] = FindControl<Button>("KeypadButton4", true);
	m_pMenuButtons[5] = FindControl<Button>("KeypadButton5", true);
	m_pMenuButtons[6] = FindControl<Button>("KeypadButton6", true);
	m_pMenuButtons[7] = FindControl<Button>("KeypadButton7", true);
	m_pMenuButtons[8] = FindControl<Button>("KeypadButton8", true);
	m_pMenuButtons[9] = FindControl<Button>("KeypadButton9", true);

	m_imgMenuButtons[0] = FindControl<ImagePanel>("KeypadButton0img", true);
	m_imgMenuButtons[0]->SetImage("keypad/keypad_0");

	m_imgMenuButtons[1] = FindControl<ImagePanel>("KeypadButton1img", true);
	m_imgMenuButtons[1]->SetImage("keypad/keypad_1");

	m_imgMenuButtons[2] = FindControl<ImagePanel>("KeypadButton2img", true);
	m_imgMenuButtons[2]->SetImage("keypad/keypad_2");

	m_imgMenuButtons[3] = FindControl<ImagePanel>("KeypadButton3img", true);
	m_imgMenuButtons[3]->SetImage("keypad/keypad_3");

	m_imgMenuButtons[4] = FindControl<ImagePanel>("KeypadButton4img", true);
	m_imgMenuButtons[4]->SetImage("keypad/keypad_4");

	m_imgMenuButtons[5] = FindControl<ImagePanel>("KeypadButton5img", true);
	m_imgMenuButtons[5]->SetImage("keypad/keypad_5");

	m_imgMenuButtons[6] = FindControl<ImagePanel>("KeypadButton6img", true);
	m_imgMenuButtons[6]->SetImage("keypad/keypad_6");

	m_imgMenuButtons[7] = FindControl<ImagePanel>("KeypadButton7img", true);
	m_imgMenuButtons[7]->SetImage("keypad/keypad_7");

	m_imgMenuButtons[8] = FindControl<ImagePanel>("KeypadButton8img", true);
	m_imgMenuButtons[8]->SetImage("keypad/keypad_8");

	m_imgMenuButtons[9] = FindControl<ImagePanel>("KeypadButton9img", true);
	m_imgMenuButtons[9]->SetImage("keypad/keypad_9");

	for (int i = 0; i < 10; i++)
	{
		m_pMenuButtons[i]->SetDepressedSound("buttons/blip1.wav");
		m_pMenuButtons[i]->SetAlpha(0);
		m_pMenuButtons[i]->SetSize(32, 32);
		m_imgMenuButtons[i]->SetSize(32, 32);
	}
}

void CVgui_keypad::Init()
{
	//Gets the message sent from from func_keypad: code, keypad title and if code must be hidden
	HOOK_HUD_MESSAGE(CVgui_keypad, KeypadHandler);
}

void CVgui_keypad::PaintBackground()
{
	//vgui::surface()->DrawSetColor(255, 0, 0, 255);	
}

void CVgui_keypad::Paint()
{
	//Change this value if you want a longer pass
	if (m_pPressedNumbers == 4)
	{
		//Disable to check into DEV mode if the codes are correct

		//Transforms the char into a int so we really get the number itself
		/*char * numberC;
		int numberN = strtol(szEnteredNumbers, &numberC, 10);
		DevMsg("Code entered is: %i \n", numberN);
		numberN = strtol(szCode, &numberC, 10);
		DevMsg("Correct code is: %i \n", numberN);*/

		//Checks if the code is the good one
		if (strcmp(szEnteredNumbers, szCode))
		{
			engine->ClientCmd("keypad_codematch");

			/*Mysteriously enough, as soon as keypad_codematch is fired,
			it runs the stuff at the other side. That's why stuff
			is mixed on both places*/

			//Wrong code is set
			surface()->PlaySound("buttons/button2.wav");
			m_pPressedNumbers = 0;
		}
		else
		{
			engine->ClientCmd("keypad_codedismatch");

			/*Mysteriously enough, as soon as keypad_codedismatch is fired,
			it runs the stuff at the other side. That's why stuff
			is mixed on both places*/

			//Correct code is set
			surface()->PlaySound("buttons/button5.wav");
			SetVisible(false);
			m_showKeypad = false;
			//cl_showkeypad.SetValue(0);
			m_pPressedNumbers = 0;
		}
	}

	//Makes buttons go grey if they are pushed
	for (int i = 0; i < 10; i++)
	{
		if (m_pMenuButtons[i]->IsArmed())
		{
			m_imgMenuButtons[i]->SetAlpha(255);
		}
		else
		{
			m_imgMenuButtons[i]->SetAlpha(150);
		}
	}

	if (m_pClearButton->IsArmed())
	{
		m_imgClearButton->SetAlpha(255);
	}
	else
	{
		m_imgClearButton->SetAlpha(150);
	}

	if (m_pCloseButton->IsArmed())
	{
		m_imgCloseButton->SetAlpha(255);
	}
	else
	{
		m_imgCloseButton->SetAlpha(150);
	}
}

void CVgui_keypad::OnTick()
{
	BaseClass::OnTick();
}

void CVgui_keypad::OnCommand(const char* pcCommand)
{
	BaseClass::OnCommand(pcCommand);

	if (!Q_stricmp(pcCommand, "clear"))
	{
		m_pPressedNumbers = 0;
		m_pLabel->SetText("");
	}

	if (!Q_stricmp(pcCommand, "turnoff"))
	{
		//cl_showkeypad.SetValue(0);
		m_showKeypad = false;
	}

	//ToDo: Search a better way to do this
	if (!Q_stricmp(pcCommand, "0"))
	{
		CheckPass(0);
	}
	if (!Q_stricmp(pcCommand, "1"))
	{
		CheckPass(1);
	}
	if (!Q_stricmp(pcCommand, "2"))
	{
		CheckPass(2);
	}
	if (!Q_stricmp(pcCommand, "3"))
	{
		CheckPass(3);
	}
	if (!Q_stricmp(pcCommand, "4"))
	{
		CheckPass(4);
	}
	if (!Q_stricmp(pcCommand, "5"))
	{
		CheckPass(5);
	}
	if (!Q_stricmp(pcCommand, "6"))
	{
		CheckPass(6);
	}
	if (!Q_stricmp(pcCommand, "7"))
	{
		CheckPass(7);
	}
	if (!Q_stricmp(pcCommand, "8"))
	{
		CheckPass(8);
	}
	if (!Q_stricmp(pcCommand, "9"))
	{
		CheckPass(9);
	}
}

void CVgui_keypad::CheckPass(int number)
{
	//Disable if you wanna check on DEV mode if the number pressed is correct
	//DevMsg("Number entered is: %i \n", number);

	//We need the number as a string
	char numberN[8];
	sprintf(numberN, "%i", number);

	//Used to check how many numbers we are adding
	m_pPressedNumbers++;

	//Add the new number, otherwise, display the first one
	if (m_pPressedNumbers >1)
		strcat(szEnteredNumbers, numberN);
	else
		strcpy(szEnteredNumbers, numberN);

	//Disable if you wanna check on DEV mode if the code is correct
	//DevMsg("Code entered is: %i \n", szEnteredNumbers);

	m_pLabel->SetVisible(true);

	//If is enabled to hide numbers, do so
	if (m_pHidePass)
	{
		//SysOp: so fucking lazy, blame me
		if (m_pPressedNumbers == 1)
			m_pLabel->SetText("*");
		if (m_pPressedNumbers == 2)
			m_pLabel->SetText("**");
		if (m_pPressedNumbers == 3)
			m_pLabel->SetText("***");
		if (m_pPressedNumbers == 4)
			m_pLabel->SetText("****");
	}
	else
	{
		//Show the number itself then
		m_pLabel->SetText(szEnteredNumbers);
	}
}

// declare the user message handler
void CVgui_keypad::MsgFunc_KeypadHandler(bf_read &msg)
{
	msg.ReadString(szCode, sizeof(szCode));

	msg.ReadString(szFrameLabel, sizeof(szFrameLabel));
	m_pScreenTitle->SetText(szFrameLabel);

	m_pHidePass = msg.ReadByte();

	//cl_showkeypad.SetValue(1);
	m_showKeypad = true;
}

void CVgui_keypad::OnThink(void)
{
	if (m_showKeypad) //cl_showkeypad.GetBool()
	{
		SetVisible(true);
		SetMouseInputEnabled(true);

		//Fix for known VGUI bug with SetMouseInputEnabled making the keyboard not accepting two or more pressed buttons
		SetKeyBoardInputEnabled(true);
	}
	else
	{
		SetVisible(false);
		SetMouseInputEnabled(false);

		//Fix for known VGUI bug with SetMouseInputEnabled making the keyboard not accepting two or more pressed buttons
		SetKeyBoardInputEnabled(false);
	}

	BaseClass::OnThink();
}