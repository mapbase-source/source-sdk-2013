//==========================================================================//
//
// Purpose: VGUI Keypad made by Maestro Fenix 2015
//
// $NoKeywords: $
//===========================================================================//
class CPointKeypad : public CPointEntity
{
public:
	DECLARE_CLASS(CPointKeypad, CPointEntity);

	void	Spawn(void);
	void	Precache(void);

	bool KeyValue(const char *szKeyName, const char *szValue);

	//BOOL		m_bNeedsUpdate;

	string_t	s_code;
	string_t	s_frame_label;

	bool		b_hide_pass;

	void FireTarget(void);
	void WrongCode(void);


private:
	void Enable(inputdata_t &inputdata);

	COutputEvent OnCorrectCode;
	COutputEvent OnIncorrectCode;

	DECLARE_DATADESC();
};
