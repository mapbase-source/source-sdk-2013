class IDialoguePanel
{
public:
	virtual void		Create(vgui::VPANEL parent) = 0;
	virtual void		Destroy(void) = 0;
	virtual void		Activate(void) = 0;
	virtual void		Show(void) = 0;
	virtual void		Hide(void) = 0;
	virtual void		LoadFile(const char* filePath) = 0;
	virtual void		ShowNode(const char* nodeName) = 0;
	virtual void		ApplySettings(bool bTypewriter, float flSpeed, const char* szTypewriterSound, const char* szOpenSound, const char* szCloseSound) = 0;
	virtual void		ApplyFocusPosition(float x, float y, float z) = 0;
};

extern IDialoguePanel* g_pDialoguePanel;
