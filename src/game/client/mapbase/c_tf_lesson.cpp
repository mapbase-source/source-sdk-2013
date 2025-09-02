//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: TF2 lesson actions + new Game Instructor lesson types which use TF HUD elements
// 
// Author: Blixibon
//
//=============================================================================//

#include "cbase.h"

#include "c_tf_lesson.h"
#include "tf_hud_training.h"
#include "tf_hud_objectivestatus.h"
#include "tf_hud_notification_panel.h"

//-----------------------------------------------------------------------------

extern ConVar gameinstructor_verbose;

enum Mod_LessonAction
{
	// Enum starts from end of LessonAction
	LESSON_ACTION_IS_TF_CLASS = LESSON_ACTION_MOD_START,
	LESSON_ACTION_IS_TF_GAMEMODE,
	LESSON_ACTION_IS_TRAINING,
	LESSON_ACTION_IS_ROUND_STATE,

	LESSON_ACTION_WEAPON_ID_HAS,
	LESSON_ACTION_WEAPON_ATTR_HAS,

	LESSON_ACTION_ENEMY_TEAM_HAS_TF_WEAPON,

	//LESSON_ACTION_IN_RESPAWN_ROOM,

	LESSON_ACTION_TOTAL
};


void CScriptedIconLesson::Mod_PreReadLessonsFromFile( void )
{
	// Add custom actions to the map
	CScriptedIconLesson::LessonActionMap.Insert( "is class", LESSON_ACTION_IS_TF_CLASS );
	CScriptedIconLesson::LessonActionMap.Insert( "is gamemode", LESSON_ACTION_IS_TF_GAMEMODE );
	CScriptedIconLesson::LessonActionMap.Insert( "is training", LESSON_ACTION_IS_TRAINING );
	CScriptedIconLesson::LessonActionMap.Insert( "is roundstate", LESSON_ACTION_IS_ROUND_STATE );

	CScriptedIconLesson::LessonActionMap.Insert( "weapon id has", LESSON_ACTION_WEAPON_ID_HAS );
	CScriptedIconLesson::LessonActionMap.Insert( "weapon attr has", LESSON_ACTION_WEAPON_ATTR_HAS );

	CScriptedIconLesson::LessonActionMap.Insert( "enemy team has weapon", LESSON_ACTION_ENEMY_TEAM_HAS_TF_WEAPON );
}

bool CScriptedIconLesson::Mod_ProcessElementAction( int iAction, bool bNot, const char *pchVarName, EHANDLE &hVar, const CGameInstructorSymbol *pchParamName, float fParam, C_BaseEntity *pParam, const char *pchParam, bool &bModHandled )
{
	// Assume we're going to handle the action
	bModHandled = true;

	C_BaseEntity *pVar;
	pVar = hVar.Get();

	switch ( iAction )
	{
		case LESSON_ACTION_IS_TF_CLASS:
		{
			const char *pszPlayerClass = "undefined";

			C_TFPlayer *pTFPlayer = dynamic_cast<C_TFPlayer *>( pVar );
			if ( pTFPlayer )
			{
				C_TFPlayerClass *pClass = pTFPlayer->GetPlayerClass();
				if (pClass)
				{
					pszPlayerClass = pClass->GetName();
				}
			}

			bool bIsClass = FStrEq( pchParam, pszPlayerClass );

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->IsTFClass() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", pszPlayerClass );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%s] " ) : ( " == [%s] " ), pchParam );
			}

			return bNot ? !bIsClass : bIsClass;
		}

		case LESSON_ACTION_IS_TF_GAMEMODE:
		{
			bool bIsGameMode = false;
			const char *pszGameType = "undefined";

			if ( TFGameRules() )
			{
				// Special cases first, then determine directly from game type
				if ( FStrEq( pchParam, "plr" ) )
				{
					bIsGameMode = TFGameRules()->HasMultipleTrains();
					pszGameType = bIsGameMode ? "plr" : "not plr";
				}
				else if ( TFGameRules()->GetGameType() != TF_GAMETYPE_UNDEFINED )
				{
					// The +10 is to skip the localization prefix
					// (removing #Gametype from "#Gametype_CTF")
					pszGameType = (TFGameRules()->GetGameTypeName() + 10);
					bIsGameMode = FStrEq( pszGameType, pchParam );
				}
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->IsTFGamemode() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", pszGameType );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%s] " ) : ( " == [%s] " ), pchParam );
			}

			return bNot ? !bIsGameMode : bIsGameMode;
		}

		case LESSON_ACTION_IS_TRAINING:
		{
			bool bIsTraining = false;

			if ( TFGameRules() )
			{
				bIsTraining = TFGameRules()->IsInTraining();
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->IsInTraining() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%s] " ) : ( " == [%s] " ), bIsTraining ? "true" : "false" );
			}

			return bNot ? !bIsTraining : bIsTraining;
		}

		case LESSON_ACTION_IS_ROUND_STATE:
		{
			bool bIsRoundState = false;
			const char *pszRoundState = "undefined";

			if ( TFGameRules() )
			{
				int nRoundState = TFGameRules()->State_Get();
				switch (nRoundState)
				{
					case GR_STATE_PREROUND:			pszRoundState = "preround"; break;
					case GR_STATE_RND_RUNNING:		pszRoundState = "running"; break;
					case GR_STATE_TEAM_WIN:			pszRoundState = "win"; break;
					case GR_STATE_RESTART:			pszRoundState = "restart"; break;
					case GR_STATE_STALEMATE:		pszRoundState = "stalemate"; break;
					case GR_STATE_GAME_OVER:		pszRoundState = "gameover"; break;
					case GR_STATE_BONUS:			pszRoundState = "bonus"; break;
					case GR_STATE_BETWEEN_RNDS:		pszRoundState = "between"; break;
				}

				bIsRoundState = FStrEq( pszRoundState, pchParam );
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->IsTFRoundState() ", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerboseName, "%s ", pszRoundState );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%s] " ) : ( " == [%s] " ), pchParam );
			}

			return bNot ? !bIsRoundState : bIsRoundState;
		}

		case LESSON_ACTION_WEAPON_ID_HAS:
		{
			C_BaseCombatCharacter *pBCC = NULL;
			C_BaseCombatWeapon *pWeapon = NULL;

			if ( pVar )
			{
				pBCC = pVar->MyCombatCharacterPointer();
				if ( !pBCC )
				{
					pWeapon = pVar->MyCombatWeaponPointer();
				}
			}

			if ( !pBCC && !pWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->HasWeaponID([%llu] " ) : ( "\t[%s]->HasWeaponID([%llu] " ), pchVarName, (itemid_t)fParam );
					ConColorMsg( CBaseLesson::m_rgbaVerboseName, "\"%s\"", pchParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ")\n" );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseCombatCharacter or BaseCombatWeapon returned NULL!\n" );
				}

				return false;
			}
			
			if ( pBCC )
			{
				for (int i=0;i<MAX_WEAPONS;i++) 
				{
					if ( pBCC->GetWeapon(i) )
					{
						if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
						{
							ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->HasWeaponID([%llu]) " ) : ( "\t[%s]->HasWeaponID([%llu]) " ), pchVarName, (itemid_t)fParam );
							ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%hu (%s)]\n" ) : ( " == [%hu (%s)]\n" ), pBCC->GetWeapon(i)->GetAttributeContainer()->GetItem()->GetItemDefIndex(), pBCC->GetWeapon(i)->GetClassname() );
						}

						if ( pBCC->GetWeapon(i)->GetAttributeContainer()->GetItem()->GetItemDefIndex() == (itemid_t)fParam )
						{
							pWeapon = pBCC->GetWeapon(i);
							break;
						}
					}
				}

				/*if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->HasWeaponID([%llu]) " ) : ( "\t[%s]->HasWeaponID([%llu]) " ), pchVarName, (itemid_t)fParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%llu] " ) : ( " == [%llu] " ), (itemid_t)fParam );
				}*/
			}
			else if ( pWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->HasWeaponID([%llu]) " ) : ( "\t[%s]->HasWeaponID([%llu]) " ), pchVarName, (itemid_t)fParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%hu (%s)]\n" ) : ( " == [%hu (%s)]\n" ), pWeapon->GetAttributeContainer()->GetItem()->GetItemDefIndex(), pWeapon->GetClassname() );
				}

				if ( pWeapon->GetAttributeContainer()->GetItem()->GetItemDefIndex() != (itemid_t)fParam )
				{
					pWeapon = NULL;
				}
			}

			return bNot ? pWeapon == NULL : pWeapon != NULL;
		}

		case LESSON_ACTION_WEAPON_ATTR_HAS:
		{
			// TODO: Figure out way to anonymously check if weapon has an attribute
			Warning( "\"weapon attr has\" not implemented\n" );
			return bNot ? !false : false;

#if 0
			C_BaseCombatCharacter *pBCC = NULL;
			C_BaseCombatWeapon *pWeapon = NULL;

			if ( pVar )
			{
				pBCC = pVar->MyCombatCharacterPointer();
				if ( !pBCC )
				{
					pWeapon = pVar->MyCombatWeaponPointer();
				}
			}

			if ( !pBCC && !pWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->HasWeaponAttribute([%s] " ) : ( "\t[%s]->HasWeaponAttribute([%s])\n" ), pchVarName, pchParam );
					ConColorMsg( CBaseLesson::m_rgbaVerboseClose, "\tVar handle as BaseCombatCharacter or BaseCombatWeapon returned NULL!\n" );
				}

				return false;
			}

			if ( pBCC )
			{
				for (int i=0;i<MAX_WEAPONS;i++) 
				{
					if ( pBCC->GetWeapon(i) )
					{
						if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
						{
							ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->HasWeaponAttribute([%s]) " ) : ( "\t[%s]->HasWeaponAttribute([%s]) " ), pchVarName, pchParam );
							ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%hu (%s)]\n" ) : ( " == [%hu (%s)]\n" ), pBCC->GetWeapon(i)->GetAttributeContainer()->GetItem()->GetItemDefIndex(), pBCC->GetWeapon(i)->GetClassname() );
						}
						
						if ( pBCC->GetWeapon(i)->GetAttributeList()->GetAttributeByName( pchParam ) )
						{
							pWeapon = pBCC->GetWeapon(i);
							break;
						}
					}
				}

				/*if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->HasWeaponID([%llu]) " ) : ( "\t[%s]->HasWeaponID([%llu]) " ), pchVarName, (itemid_t)fParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%llu] " ) : ( " == [%llu] " ), (itemid_t)fParam );
				}*/
			}
			else if ( pWeapon )
			{
				if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
				{
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->HasWeaponAttribute([%llu]) " ) : ( "\t[%s]->HasWeaponAttribute([%llu]) " ), pchVarName, (itemid_t)fParam );
					ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%hu (%s)]\n" ) : ( " == [%hu (%s)]\n" ), pWeapon->GetAttributeContainer()->GetItem()->GetItemDefIndex(), pWeapon->GetClassname() );
				}

				if ( !pWeapon->GetAttributeList()->GetAttributeByName( pchParam ) )
				{
					pWeapon = NULL;
				}
			}

			return bNot ? !pWeapon : pWeapon;
#endif
		}

		case LESSON_ACTION_ENEMY_TEAM_HAS_TF_WEAPON:
		{
			bool bEnemyHasWeapon = false;

			C_TFPlayer *pLocalPlayer = dynamic_cast<C_TFPlayer *>(pVar);

			if ( pLocalPlayer )
			{
				for ( int i = 1; i < gpGlobals->maxClients; i++ )
				{
					C_TFPlayer *pTFPlayer = ToTFPlayer( UTIL_PlayerByIndex( i ) );
					if ( !pTFPlayer || pTFPlayer->GetTeamNumber() == pLocalPlayer->GetTeamNumber() || !IsValidTFTeam( pTFPlayer->GetTeamNumber() ) )
						continue;

					if ( pchParam && *pchParam )
					{
						if ( pTFPlayer->Weapon_OwnsThisType( pchParam ) )
						{
							bEnemyHasWeapon = true;
							break;
						}
					}
					else
					{
						// Check by direct ID
						for (int i=0;i<MAX_WEAPONS;i++) 
						{
							if ( pTFPlayer->GetWeapon(i) )
							{
								if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
								{
									ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( "\t![%s]->EnemyHasTFWeapon([%llu]) " ) : ( "\t[%s]->EnemyHasTFWeapon([%llu]) " ), pchVarName, (itemid_t)fParam );
									ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%hu (%s)]\n" ) : ( " == [%hu (%s)]\n" ), pTFPlayer->GetWeapon(i)->GetAttributeContainer()->GetItem()->GetItemDefIndex(), pTFPlayer->GetWeapon(i)->GetClassname() );
								}

								if ( pTFPlayer->GetWeapon(i)->GetAttributeContainer()->GetItem()->GetItemDefIndex() == (itemid_t)fParam )
								{
									bEnemyHasWeapon = true;
									break;
								}
							}
						}
					}
				}
			}

			if ( gameinstructor_verbose.GetInt() > 0 && ShouldShowSpew() )
			{
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, "\t[%s]->EnemyHasTFWeapon()", pchVarName );
				ConColorMsg( CBaseLesson::m_rgbaVerbosePlain, ( bNot ) ? ( " != [%s] " ) : ( " == [%s] " ), pchParam );
			}

			return bNot ? !bEnemyHasWeapon : bEnemyHasWeapon;
		}

		default:
			// Didn't handle this action
			bModHandled = false;
			break;
	}

	return false;
}


bool C_GameInstructor::Mod_HiddenByOtherElements( void )
{
	return false;
}

bool C_GameInstructor::Mod_DefineLessonType( const char *pszLessonName, const char *pszLessonType )
{
	if ( FStrEq( pszLessonType, "tf_training_objective" ) )
	{
		DefineLesson( new CTFObjectiveLesson( m_pScriptKeys->GetName(), false, false ) );
		return true;
	}
	else if ( FStrEq( pszLessonType, "tf_notification" ) )
	{
		DefineLesson( new CTFNotificationLesson( m_pScriptKeys->GetName(), false, false ) );
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------
//
// CTFBaseLesson
//
//-----------------------------------------------------------------------------

#define TF_LESSON_MIN_TIME_ON_SCREEN_TO_MARK_DISPLAYED 1.5f
#define TF_LESSON_DISTANCE_UPDATE_RATE 0.25f

void CTFBaseLesson::Init()
{
	ListenForGameEvent( "localplayer_respawn" );
	//ListenForGameEvent( "client_disconnect" );
	//ListenForGameEvent( "round_start" );
}

//=========================================================
//=========================================================
void CTFBaseLesson::Update()
{
	if ( !DoDelayedPlayerSwaps() )
		return;

	// Check if it has been onscreen long enough to count as being displayed
	if ( m_fOnScreenStartTime != 0.0f )
	{
		if ( gpGlobals->curtime - m_fOnScreenStartTime >= TF_LESSON_MIN_TIME_ON_SCREEN_TO_MARK_DISPLAYED )
		{
			// Lesson on screen long enough to be counted as displayed
			m_bWasDisplayed = true;

			// Also apply to root (since we use it to keep track of when to show these hints)
			if ( GetRoot() )
			{
				CTFBaseLesson *pRoot = assert_cast<CTFBaseLesson*>(GetRoot());
				if ( pRoot )
				{
					pRoot->MarkAsDisplayed();
				}
			}
		}
	}

	if ( m_fUpdateDistanceTime < gpGlobals->curtime )
	{
		// Update it's distance from the local player
		C_BaseEntity *pTarget = m_hEntity1.Get(); // m_hIconTarget.Get()
		C_BasePlayer *pLocalPlayer = GetGameInstructor().GetLocalPlayer();

		if ( !pLocalPlayer || !pTarget || pLocalPlayer == pTarget )
		{
			m_fCurrentDistance = 0.0f;
		}
		else
		{
			m_fCurrentDistance = pLocalPlayer->EyePosition().DistTo( pTarget->WorldSpaceCenter() );
		}

		m_fUpdateDistanceTime = gpGlobals->curtime + TF_LESSON_DISTANCE_UPDATE_RATE;
	}

	//BaseClass::Update();
}

//=========================================================
//=========================================================
void CTFBaseLesson::UpdateInactive()
{
	if ( !DoDelayedPlayerSwaps() )
		return;

	if ( m_fUpdateDistanceTime < gpGlobals->curtime )
	{
		// Update it's distance from the local player
		C_BaseEntity *pTarget = m_hEntity1.Get(); // m_hIconTarget.Get()
		C_BasePlayer *pLocalPlayer = GetGameInstructor().GetLocalPlayer();

		if ( !pLocalPlayer || !pTarget || pLocalPlayer == pTarget )
		{
			m_fCurrentDistance = 0.0f;
		}
		else
		{
			m_fCurrentDistance = pLocalPlayer->EyePosition().DistTo( pTarget->WorldSpaceCenter() );
		}

		m_fUpdateDistanceTime = gpGlobals->curtime + TF_LESSON_DISTANCE_UPDATE_RATE;
	}

	//BaseClass::UpdateInactive();
}

//=========================================================
//=========================================================
bool CTFBaseLesson::ShouldDisplay() const
{
	if ( !IsInstructing() )
	{
		// Already displayed this session
		if ( GetRoot() && GetRoot()->WasDisplayed() )
			return false;

		if ( TFGameRules() )
		{
			// Not while waiting for players
			if ( TFGameRules()->IsInWaitingForPlayers() )
				return false;

			// For now, no hints in arena prep stage so we don't overlap with the player count
			if ( TFGameRules()->IsInArenaMode() && TFGameRules()->State_Get() == GR_STATE_PREROUND )
				return false;
		}
	}

	// Ok to display
	return BaseClass::ShouldDisplay();
}

//=========================================================
//=========================================================
void CTFBaseLesson::FireGameEvent( IGameEvent *event )
{
	if ( FStrEq( event->GetName(), "localplayer_respawn" ) && IsInstructing() && !m_bCanOpenWhenDead )
	{
		// Close active lessons on respawn
		SetCloseReason( "Player respawned" );
		GetGameInstructor().CloseOpportunity( this );
	}
	/*else if ( FStrEq( event->GetName(), "client_disconnect" ) )
	{
		// UNDONE: Allow TF lessons to show again when connecting to another server
		GetGameInstructor().CloseOpportunity( this );
		m_bWasDisplayed = false;
	}*/
	/*else if ( FStrEq( event->GetName(), "round_start" ) )
	{
		// UNDONE: Reset on round start
		GetGameInstructor().CloseOpportunity( this );
		m_bWasDisplayed = false;
	}*/

	BaseClass::FireGameEvent( event );
}

//-----------------------------------------------------------------------------
//
// CTFObjectiveLesson
//
//-----------------------------------------------------------------------------

#define TF_LESSON_MIN_TIME_ON_SCREEN_TO_MARK_DISPLAYED 1.5f
#define TF_LESSON_DISTANCE_UPDATE_RATE 0.25f

CTFObjectiveLesson *CTFObjectiveLesson::g_pActiveTFObjectiveLesson = NULL;

void CTFObjectiveLesson::Init()
{
}

//=========================================================
//=========================================================
void CTFObjectiveLesson::Start()
{
	if ( !DoDelayedPlayerSwaps() )
		return;

	// Shouldn't show these lessons if we're actually in training
	if ( !TFGameRules() || TFGameRules()->IsInTraining() )
		return;

	CTFHudObjectiveStatus *pStatus = GET_HUDELEMENT( CTFHudObjectiveStatus );
	if ( pStatus )
	{
		if ( g_pActiveTFObjectiveLesson && g_pActiveTFObjectiveLesson->IsInstructing() )
		{
			// Replace the currently active lesson
			g_pActiveTFObjectiveLesson->SetCloseReason( "Replaced with another lesson" );
			GetGameInstructor().CloseOpportunity( g_pActiveTFObjectiveLesson );
		}

		g_pActiveTFObjectiveLesson = this;

		pStatus->SetTrainingGameInstructor( true );

		if ( V_strchr( m_szOnscreenIcon.String(), '%' ) )
		{
			char szImage[MAX_PATH] = { 0 };

			char szToken[256];
			V_strncpy( szToken, m_szOnscreenIcon.String(), sizeof( szToken ) );

			bool bVar = false;
			const char *token = strtok( szToken, "%" );
			while (token != NULL)
			{
				if (bVar)
				{
					if (V_strnicmp( token, "team", 4 ) == 0)
					{
						C_BasePlayer *pPlayer = GetGameInstructor().GetLocalPlayer();
						int iTeam = pPlayer ? pPlayer->GetTeamNumber() : TEAM_UNASSIGNED;
						if (iTeam < 0 || iTeam >= TF_TEAM_COUNT)
							iTeam = TEAM_UNASSIGNED;

						if (FStrEq(token + 4, "_short"))
						{
							V_strncat( szImage, g_aTeamNamesShort[iTeam], sizeof( szImage ) );
						}
						else
						{
							V_strncat( szImage, g_aTeamNames[iTeam], sizeof( szImage ) );
						}
					}
				}
				else
				{
					V_strncat( szImage, token, sizeof( szImage ) );
				}

				bVar = !bVar;
				token = strtok( NULL, "%" );
			}

			pStatus->SetTrainingImage( szImage );
		}
		else
		{
			pStatus->SetTrainingImage( m_szOnscreenIcon.String() );
		}

		pStatus->SetTrainingObjective( m_szDisplayParamText.String() );
		pStatus->SetTrainingText( m_szDisplayText.String() );

		pStatus->SetTrainingOverridePos( m_bFixedPosition, m_fFixedPositionX, m_fFixedPositionY );

		TFGameRules()->SetShowingTFObjectiveLesson( true );

		// HACKHACK: Can't put these in Init() due to base class usage
		m_bNoIconTarget			= true;
		m_bAllowNodrawTarget	= true;

		m_fOnScreenStartTime = gpGlobals->curtime;
	}
}

//=========================================================
//=========================================================
void CTFObjectiveLesson::Stop()
{
	if ( !DoDelayedPlayerSwaps() )
		return;
	
	if ( !TFGameRules() )
		return;

	if ( IsInstructing() )
	{
		// Set to false within CTFHudTraining
		TFGameRules()->SetShowingTFObjectiveLesson( false );
		
		CTFHudObjectiveStatus *pStatus = GET_HUDELEMENT( CTFHudObjectiveStatus );
		if ( pStatus )
		{
			pStatus->SetTrainingGameInstructor( false );
		}

		Assert( g_pActiveTFObjectiveLesson == this );
		g_pActiveTFObjectiveLesson = NULL;
	}

	m_fOnScreenStartTime = 0.0f;
	m_fStartTime = 0.0f;
}

//-----------------------------------------------------------------------------
//
// CTFNotificationLesson
//
//-----------------------------------------------------------------------------

CTFNotificationLesson *CTFNotificationLesson::g_pActiveTFNotificationLesson = NULL;

void CTFNotificationLesson::Init()
{
	ListenForGameEvent( "localplayer_respawn" );
}

//=========================================================
//=========================================================
void CTFNotificationLesson::Start()
{
	if ( !DoDelayedPlayerSwaps() )
		return;

	CHudNotificationPanel *pNotificationPanel = GET_HUDELEMENT( CHudNotificationPanel );
	if ( pNotificationPanel )
	{
		if ( g_pActiveTFNotificationLesson && g_pActiveTFNotificationLesson->IsInstructing() )
		{
			// Replace the currently active lesson
			g_pActiveTFNotificationLesson->SetCloseReason( "Replaced with another lesson" );
			GetGameInstructor().CloseOpportunity( g_pActiveTFNotificationLesson );
		}

		g_pActiveTFNotificationLesson = this;

		char szImage[MAX_PATH] = { 0 };

		if ( V_strchr( m_szOnscreenIcon.String(), '%' ) )
		{
			char szToken[256];
			V_strncpy( szToken, m_szOnscreenIcon.String(), sizeof( szToken ) );

			bool bVar = false;
			const char *token = strtok( szToken, "%" );
			while (token != NULL)
			{
				if (bVar)
				{
					if (V_strnicmp( token, "team", 4 ) == 0)
					{
						C_BasePlayer *pPlayer = GetGameInstructor().GetLocalPlayer();
						int iTeam = pPlayer ? pPlayer->GetTeamNumber() : TEAM_UNASSIGNED;
						if (iTeam < 0 || iTeam >= TF_TEAM_COUNT)
							iTeam = TEAM_UNASSIGNED;

						if (FStrEq(token + 4, "_short"))
						{
							V_strncat( szImage, g_aTeamNamesShort[iTeam], sizeof( szImage ) );
						}
						else
						{
							V_strncat( szImage, g_aTeamNames[iTeam], sizeof( szImage ) );
						}
					}
				}
				else
				{
					V_strncat( szImage, token, sizeof( szImage ) );
				}

				bVar = !bVar;
				token = strtok( NULL, "%" );
			}
		}

		wchar_t wszText[MAX_TRAINING_MSG_LENGTH];
		CTFHudTraining::FormatTrainingText( m_szDisplayText.String(), wszText );

		C_BasePlayer *pLocalPlayer = C_BasePlayer::GetLocalPlayer();

		pNotificationPanel->SetupNotifyCustom( wszText, szImage, pLocalPlayer ? pLocalPlayer->GetTeamNumber() : TEAM_UNASSIGNED, m_fTimeout );

		// HACKHACK: Can't put these in Init() due to base class usage
		m_bNoIconTarget			= true;
		m_bAllowNodrawTarget	= true;

		m_fOnScreenStartTime = gpGlobals->curtime;
	}
}

//=========================================================
//=========================================================
void CTFNotificationLesson::Stop()
{
	if ( !DoDelayedPlayerSwaps() )
		return;
	
	if ( !TFGameRules() )
		return;

	if ( IsInstructing() )
	{
		CHudNotificationPanel *pNotificationPanel = GET_HUDELEMENT( CHudNotificationPanel );
		if ( pNotificationPanel )
		{
			pNotificationPanel->HideNotify();
		}

		Assert( g_pActiveTFNotificationLesson == this );
		g_pActiveTFNotificationLesson = NULL;
	}

	m_fOnScreenStartTime = 0.0f;
	m_fStartTime = 0.0f;
}
