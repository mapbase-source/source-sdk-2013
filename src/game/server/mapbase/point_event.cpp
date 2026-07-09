//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ====
//
// Purpose: An expanded version of logic_eventlistener that allows event data
//			to be passed directly to I/O.
//
//=============================================================================

#include "cbase.h"
#include "GameEventListener.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define POINT_EVENT_NUM_VALUES 8

class CPointEvent : public CPointEntity,
					public CGameEventListener
{
public:
	DECLARE_CLASS( CPointEvent, CPointEntity );

	CPointEvent();

	void Activate();

	bool KeyValue( const char *szKeyName, const char *szValue );

	string_t m_KeyNames[POINT_EVENT_NUM_VALUES];

	void ListenForEvents();

	void FireGameEvent( IGameEvent *event );

	// Input handlers
	void InputEnable( inputdata_t &inputdata );
	void InputDisable( inputdata_t &inputdata );
	void InputToggle( inputdata_t &inputdata );
	void InputSetAllEvents( inputdata_t &inputdata );
	void InputAddEvent( inputdata_t &inputdata );
	//void InputRemoveEvent( inputdata_t &inputdata );

	DECLARE_DATADESC();

	string_t m_iszEventName;

	// Outputs
	COutputEvent m_OnEventFired;
	COutputString m_OutEventName;
	COutputString m_OutValue[POINT_EVENT_NUM_VALUES];

protected:

	bool m_bDisabled;
};

LINK_ENTITY_TO_CLASS(point_event, CPointEvent);


BEGIN_DATADESC( CPointEvent )

	DEFINE_KEYFIELD(m_KeyNames[0], FIELD_STRING, "KeyName01"),
	DEFINE_KEYFIELD(m_KeyNames[1], FIELD_STRING, "KeyName02"),
	DEFINE_KEYFIELD(m_KeyNames[2], FIELD_STRING, "KeyName03"),
	DEFINE_KEYFIELD(m_KeyNames[3], FIELD_STRING, "KeyName04"),
	DEFINE_KEYFIELD(m_KeyNames[4], FIELD_STRING, "KeyName05"),
	DEFINE_KEYFIELD(m_KeyNames[5], FIELD_STRING, "KeyName06"),
	DEFINE_KEYFIELD(m_KeyNames[6], FIELD_STRING, "KeyName07"),
	DEFINE_KEYFIELD(m_KeyNames[7], FIELD_STRING, "KeyName08"),

	// Inputs
	DEFINE_INPUTFUNC(FIELD_VOID, "Enable", InputEnable),
	DEFINE_INPUTFUNC(FIELD_VOID, "Disable", InputDisable),
	DEFINE_INPUTFUNC(FIELD_VOID, "Toggle", InputToggle),
	DEFINE_INPUTFUNC(FIELD_STRING, "SetAllEvents", InputSetAllEvents),
	DEFINE_INPUTFUNC(FIELD_STRING, "AddEvent", InputAddEvent),
	//DEFINE_INPUTFUNC(FIELD_STRING, "RemoveEvent", InputRemoveEvent),

	// Outputs
	DEFINE_OUTPUT(m_OnEventFired, "OnEventFired"),
	DEFINE_OUTPUT(m_OutEventName, "OutEventName"),
	DEFINE_OUTPUT(m_OutValue[0], "OutValue01"),
	DEFINE_OUTPUT(m_OutValue[1], "OutValue02"),
	DEFINE_OUTPUT(m_OutValue[2], "OutValue03"),
	DEFINE_OUTPUT(m_OutValue[3], "OutValue04"),
	DEFINE_OUTPUT(m_OutValue[4], "OutValue05"),
	DEFINE_OUTPUT(m_OutValue[5], "OutValue06"),
	DEFINE_OUTPUT(m_OutValue[6], "OutValue07"),
	DEFINE_OUTPUT(m_OutValue[7], "OutValue08"),

END_DATADESC()

//-----------------------------------------------------------------------------
// Purpose: Constructor.
//-----------------------------------------------------------------------------
CPointEvent::CPointEvent(void)
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointEvent::Activate()
{
	BaseClass::Activate();
	ListenForEvents();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CPointEvent::KeyValue(const char *szKeyName, const char *szValue)
{
	if (FStrEq(szKeyName, "IsEnabled"))
	{
		m_bDisabled = !FStrEq(szValue, "0");
	}
	else
		return BaseClass::KeyValue(szKeyName, szValue);

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CPointEvent::ListenForEvents()
{
	// Could easily do this with strtok...
	// Oh well. I don't know the performance difference.
	CUtlStringList vecEvents;
	Q_SplitString(STRING(m_iszEventName), ":", vecEvents);
	FOR_EACH_VEC(vecEvents, i)
	{
		ListenForGameEvent(vecEvents[i]);
	}
}

//-----------------------------------------------------------------------------
// Purpose: called when a game event is fired
//-----------------------------------------------------------------------------
void CPointEvent::FireGameEvent( IGameEvent *event )
{
	if (m_bDisabled)
		return;
	
	m_OnEventFired.FireOutput(this, this);
	m_OutEventName.Set(AllocPooledString(event->GetName()), this, this);

	for (int i = 0; i < POINT_EVENT_NUM_VALUES; i++)
	{
		const char *szValue = event->GetString(STRING(m_KeyNames[i]), NULL);
		if (szValue != NULL)
		{
			m_OutValue[i].Set(AllocPooledString(szValue), this, this);
		}
	}
}

//------------------------------------------------------------------------------
// Purpose: Turns on the entity, allowing it to fire outputs.
//------------------------------------------------------------------------------
void CPointEvent::InputEnable( inputdata_t &inputdata )
{
	m_bDisabled = false;
}

//------------------------------------------------------------------------------
// Purpose: Turns off the entity, preventing it from firing outputs.
//------------------------------------------------------------------------------
void CPointEvent::InputDisable( inputdata_t &inputdata )
{ 
	m_bDisabled = true;
}

//------------------------------------------------------------------------------
// Purpose: Toggles the enabled/disabled state of the entity.
//------------------------------------------------------------------------------
void CPointEvent::InputToggle( inputdata_t &inputdata )
{ 
	m_bDisabled = !m_bDisabled;
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CPointEvent::InputSetAllEvents( inputdata_t &inputdata )
{
	StopListeningForAllEvents();

	if (inputdata.value.StringID() != NULL_STRING)
	{
		CUtlStringList vecEvents;
		Q_SplitString(inputdata.value.String(), ":", vecEvents);
		FOR_EACH_VEC(vecEvents, i)
		{
			ListenForGameEvent(vecEvents[i]);
		}
	}
}

//------------------------------------------------------------------------------
// Purpose: 
//------------------------------------------------------------------------------
void CPointEvent::InputAddEvent( inputdata_t &inputdata )
{
	ListenForGameEvent(inputdata.value.String());
}
