//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: TF2 lesson actions + new Game Instructor lesson types which use TF HUD elements
// 
// Author: Blixibon
//
//=============================================================================//

#ifndef C_TF_LESSON_H
#define C_TF_LESSON_H
#ifdef _WIN32
#pragma once
#endif

#include "c_gameinstructor.h"
#include "c_baselesson.h"

//-----------------------------------------------------------------------------
// Purpose: An instructor lesson that integrates a TF HUD element instead of
// a traditional icon hint.
// 
// TODO: Lesson classes are seemingly set up so that there can be non-icon
// lessons, but those lessons cannot be loaded by mod_lessons or use game events.
// An overhaul of the lesson system would be needed for this lesson type to derive
// directly from these non-icon lesson classes.
//-----------------------------------------------------------------------------
class CTFBaseLesson : public CScriptedIconLesson
{
public:
	DECLARE_LESSON( CTFBaseLesson, CScriptedIconLesson )

	void Init( void ); 	// NOT virtual, each constructor calls their own
	virtual void Update( void );
	virtual void UpdateInactive( void );

	void MarkAsDisplayed() { m_bWasDisplayed = true; }
	virtual bool ShouldDisplay( void ) const;
	virtual void FireGameEvent( IGameEvent *event );

private:
};

//-----------------------------------------------------------------------------
// Purpose: An instructor lesson that uses the training objective UI.
//-----------------------------------------------------------------------------
class CTFObjectiveLesson : public CTFBaseLesson
{
public:
	DECLARE_LESSON( CTFObjectiveLesson, CTFBaseLesson )

	void Init( void ); 	// NOT virtual, each constructor calls their own
	virtual void Start( void );
	virtual void Stop( void );

	virtual CScriptedIconLesson *CreateLessonCopy()	{ return new CTFObjectiveLesson( GetName(), false, true ); }

private:

	// Pointer to the lesson currently displayed on the HUD
	static CTFObjectiveLesson *g_pActiveTFObjectiveLesson;
};

//-----------------------------------------------------------------------------
// Purpose: An instructor lesson that uses the notification UI.
//-----------------------------------------------------------------------------
class CTFNotificationLesson : public CTFBaseLesson
{
public:
	DECLARE_LESSON( CTFNotificationLesson, CTFBaseLesson )

	void Init( void ); 	// NOT virtual, each constructor calls their own
	virtual void Start( void );
	virtual void Stop( void );

	virtual CScriptedIconLesson *CreateLessonCopy()	{ return new CTFNotificationLesson( GetName(), false, true ); }

private:

	// Pointer to the lesson currently displayed on the HUD
	static CTFNotificationLesson *g_pActiveTFNotificationLesson;
};

#endif
