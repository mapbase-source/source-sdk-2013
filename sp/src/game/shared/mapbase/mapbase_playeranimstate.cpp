//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Single Player animation state 'handler'. This utility is used
//            to evaluate the pose parameter value based on the direction
//            and speed of the player.
// 
// ------------------------------------------------------------------------------
//
// This was originally based on the following VDC article:
// https://developer.valvesoftware.com/wiki/Fixing_the_player_animation_state_(Single_Player)
//
// It has been modified by Blixibon to derive from CBasePlayerAnimState instead and support 9-way blends.
// Much of the work done to make this derive from CBasePlayerAnimState utilized code from the Alien Swarm SDK.
// 
//=============================================================================//

#include "cbase.h"
#include "mapbase_playeranimstate.h"
#include "tier0/vprof.h"
#include "animation.h"
#include "studio.h"
#include "apparent_velocity_helper.h"
#include "utldict.h"
#include "filesystem.h"
#include "in_buttons.h"
#include "gamemovement.h"
#include "datacache/imdlcache.h"
#ifdef CLIENT_DLL
#include "input.h"
#ifdef HL2MP
#include "c_hl2mp_player.h"
#endif
#endif

extern ConVar mp_facefronttime, mp_feetyawrate;

ConVar sv_playeranimstate_animtype( "sv_playeranimstate_animtype", "0", FCVAR_NONE, "The leg animation type used by the Mapbase animation state. 9way = 0, 8way = 1, GoldSrc = 2" );
ConVar sv_playeranimstate_bodyyaw( "sv_playeranimstate_bodyyaw", "45.0", FCVAR_NONE, "The maximum body yaw used by the Mapbase animation state." );
ConVar sv_playeranimstate_use_aim_sequences( "sv_playeranimstate_use_aim_sequences", "0", FCVAR_NONE, "Allows the Mapbase animation state to use aim sequences." );
ConVar sv_playeranimstate_use_walk_anims( "sv_playeranimstate_use_walk_anims", "0", FCVAR_NONE, "Allows the Mapbase animation state to use walk animations when the player is walking." );

#define MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION        15.0f

#define WEAPON_RELAX_TIME       0.5f

#define FIRESEQUENCE_LAYER		(AIMSEQUENCE_LAYER+NUM_AIMSEQUENCE_LAYERS)
#define RELOADSEQUENCE_LAYER	(FIRESEQUENCE_LAYER + 1)
#define MISCSEQUENCE_LAYER	    (RELOADSEQUENCE_LAYER + 1)
#define NUM_LAYERS_WANTED		(MISCSEQUENCE_LAYER + 1)

CMapbasePlayerAnimState *CreatePlayerAnimationState( CBasePlayer *pPlayer )
{
    MDLCACHE_CRITICAL_SECTION();

    CMapbasePlayerAnimState *pState = new CMapbasePlayerAnimState( pPlayer );

    // Setup the movement data.
    CModAnimConfig movementData;
    movementData.m_LegAnimType = (LegAnimType_t)sv_playeranimstate_animtype.GetInt();
    movementData.m_flMaxBodyYawDegrees = sv_playeranimstate_bodyyaw.GetFloat();
    movementData.m_bUseAimSequences = sv_playeranimstate_use_aim_sequences.GetBool();

    pState->Init( pPlayer, movementData );

    return pState;
}

// Below this many degrees, slow down turning rate linearly
#define FADE_TURN_DEGREES    45.0f
// After this, need to start turning feet
#define MAX_TORSO_ANGLE        90.0f
// Below this amount, don't play a turning animation/perform IK
#define MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION        15.0f

//static ConVar tf2_feetyawrunscale( "tf2_feetyawrunscale", "2", FCVAR_REPLICATED, "Multiplier on tf2_feetyawrate to allow turning faster when running." );
extern ConVar sv_backspeed;
extern ConVar mp_feetyawrate;
extern ConVar mp_facefronttime;

CMapbasePlayerAnimState::CMapbasePlayerAnimState( CBasePlayer *pPlayer ): m_pPlayer( pPlayer )
{
	if (pPlayer)
	{
		m_nPoseAimYaw = pPlayer->LookupPoseParameter( "aim_yaw" );
		m_nPoseAimPitch = pPlayer->LookupPoseParameter( "aim_pitch" );
		m_nPoseHeadPitch = pPlayer->LookupPoseParameter( "head_pitch" );
		m_nPoseWeaponLower = pPlayer->LookupPoseParameter( "weapon_lower" );
	}
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Activity CMapbasePlayerAnimState::CalcMainActivity()
{
    float speed = GetOuter()->GetAbsVelocity().Length2D();

    if (m_pPlayer->GetLaggedMovementValue() != 1.0f)
        speed *= m_pPlayer->GetLaggedMovementValue();

	// May not always be precise
	if (speed < 0.01f)
		speed = 0.0f;

    if ( HandleJumping() )
	{
		return ACT_HL2MP_JUMP;
	}
    else
    {
        Activity idealActivity = ACT_HL2MP_IDLE;

        if ( GetOuter()->GetFlags() & ( FL_FROZEN | FL_ATCONTROLS ) )
	    {
		    speed = 0;
	    }
        else
        {
            bool bDucking = (GetOuter()->GetFlags() & FL_DUCKING) ? true : false;

            // (currently singleplayer-exclusive since clients can't read whether other players are holding down IN_DUCK)
            if (m_pPlayer->m_Local.m_flDucktime > 0 && gpGlobals->maxClients == 1)
            {
                // Consider ducking if half-way through duck time
                bDucking = (m_pPlayer->m_Local.m_flDucktime < (GAMEMOVEMENT_DUCK_TIME * 0.9f));

                // Unducking
#ifdef CLIENT_DLL
                if (!((m_pPlayer->IsLocalPlayer() ? input->GetButtonBits( 0 ) : m_pPlayer->GetCurrentUserCommand()->buttons) & IN_DUCK))
#else
                if (!(m_pPlayer->m_nButtons & IN_DUCK))
#endif
                    bDucking = !bDucking;
            }

            if ( bDucking )
			{
				if ( speed > 0 )
				{
					idealActivity = ACT_HL2MP_WALK_CROUCH;
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE_CROUCH;
				}
			}
			else
			{
				if ( speed > 0 )
				{
#if EXPANDED_HL2DM_ACTIVITIES
					if ( m_pPlayer->m_nButtons & IN_WALK && sv_playeranimstate_use_walk_anims.GetBool() )
					{
						idealActivity = ACT_HL2MP_WALK;
					}
					else
#endif
					{
						idealActivity = ACT_HL2MP_RUN;
					}
				}
				else
				{
					idealActivity = ACT_HL2MP_IDLE;
				}
			}
        }

        return idealActivity;
    }

    //return m_pPlayer->GetActivity();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::SetPlayerAnimation( PLAYER_ANIM playerAnim )
{
    if ( playerAnim == PLAYER_ATTACK1 )
    {
        m_iFireSequence = SelectWeightedSequence( TranslateActivity( ACT_HL2MP_GESTURE_RANGE_ATTACK ) );
        m_bFiring = m_iFireSequence != -1;
        m_flFireCycle = 0;

		// Be sure to stop reloading
		m_bReloading = false;
		m_flReloadCycle = 0;
    }
    else if ( playerAnim == PLAYER_ATTACK2 )
    {
#if EXPANDED_HL2DM_ACTIVITIES
        m_iFireSequence = SelectWeightedSequence( TranslateActivity( ACT_HL2MP_GESTURE_RANGE_ATTACK2 ) );
#else
        m_iFireSequence = SelectWeightedSequence( TranslateActivity( ACT_HL2MP_GESTURE_RANGE_ATTACK ) );
#endif
        m_bFiring = m_iFireSequence != -1;
        m_flFireCycle = 0;

		// Be sure to stop reloading
		m_bReloading = false;
		m_flReloadCycle = 0;
    }
    else if ( playerAnim == PLAYER_JUMP )
    {
        // Play the jump animation.
        if (!m_bJumping)
        {
            m_bJumping = true;
            m_bDuckJumping = (GetOuter()->GetFlags() & FL_DUCKING) ? true : false; //m_pPlayer->m_nButtons & IN_DUCK;
            m_bFirstJumpFrame = true;
            m_flJumpStartTime = gpGlobals->curtime;
        }
    }
    else if ( playerAnim == PLAYER_RELOAD )
    {
        m_iReloadSequence = SelectWeightedSequence( TranslateActivity( ACT_HL2MP_GESTURE_RELOAD ) );
        if (m_iReloadSequence != -1)
        {
            // clear other events that might be playing in our layer
			m_bWeaponSwitching = false;
            m_fReloadPlaybackRate = 1.0f;
			m_bReloading = true;			
			m_flReloadCycle = 0;
        }
    }
    else if ( playerAnim == PLAYER_UNHOLSTER || playerAnim == PLAYER_HOLSTER )
    {
        m_iWeaponSwitchSequence = SelectWeightedSequence( TranslateActivity( playerAnim == PLAYER_UNHOLSTER ? ACT_ARM : ACT_DISARM ) );
        if (m_iWeaponSwitchSequence != -1)
        {
            // clear other events that might be playing in our layer
            //m_bPlayingMisc = false;
            m_bReloading = false;

            m_bWeaponSwitching = true;
            m_flWeaponSwitchCycle = 0;
            //m_flMiscBlendOut = 0.1f;
            //m_flMiscBlendIn = 0.1f;
            m_bMiscNoOverride = false;
        }
    }
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
Activity CMapbasePlayerAnimState::TranslateActivity( Activity actDesired )
{
#if defined(CLIENT_DLL) && !defined(MAPBASE_MP)
	return actDesired;
#else
    return m_pPlayer->Weapon_TranslateActivity( actDesired );
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
bool CMapbasePlayerAnimState::HandleJumping()
{
	if ( m_bJumping )
	{
		if ( m_bFirstJumpFrame )
		{
			m_bFirstJumpFrame = false;
			RestartMainSequence();	// Reset the animation.
		}

		// Don't check if he's on the ground for a sec.. sometimes the client still has the
		// on-ground flag set right when the message comes in.
		if (m_flJumpStartTime > gpGlobals->curtime)
			m_flJumpStartTime = gpGlobals->curtime;
		if ( gpGlobals->curtime - m_flJumpStartTime > 0.2f)
		{
			if ( m_pOuter->GetFlags() & FL_ONGROUND || GetOuter()->GetGroundEntity() != NULL)
			{
				m_bJumping = false;
				m_bDuckJumping = false;
				RestartMainSequence();	// Reset the animation.				
			}
		}
	}

	// Are we still jumping? If so, keep playing the jump animation.
	return m_bJumping;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::ComputeSequences( CStudioHdr *pStudioHdr )
{
    CBasePlayerAnimState::ComputeSequences(pStudioHdr);

	ComputeFireSequence();
	ComputeMiscSequence();
	ComputeReloadSequence();
	ComputeWeaponSwitchSequence();
    ComputeRelaxSequence();

#if defined(HL2MP) && defined(CLIENT_DLL)
	C_HL2MP_Player *pHL2MPPlayer = static_cast<C_HL2MP_Player*>(GetOuter());
	if (pHL2MPPlayer)
	{
		pHL2MPPlayer->UpdateLookAt();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::AddMiscSequence( int iSequence, float flBlendIn, float flBlendOut, float flPlaybackRate, bool bHoldAtEnd, bool bOnlyWhenStill )
{
    Assert( iSequence != -1 );

	m_iMiscSequence = iSequence;
    m_flMiscBlendIn = flBlendIn;
    m_flMiscBlendOut = flBlendOut;

    m_bPlayingMisc = true;
    m_bMiscHoldAtEnd = bHoldAtEnd;
    //m_bReloading = false;
    m_flMiscCycle = 0;
    m_bMiscOnlyWhenStill = bOnlyWhenStill;
    m_bMiscNoOverride = true;
    m_fMiscPlaybackRate = flPlaybackRate;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::StartWeaponRelax()
{
    if (m_bWeaponRelaxing)
        return;

    m_bWeaponRelaxing = true;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::StopWeaponRelax()
{
    if (!m_bWeaponRelaxing)
        return;

    m_bWeaponRelaxing = false;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::ClearAnimationState()
{
	m_bJumping = false;
	m_bDuckJumping = false;
	m_bFiring = false;
	m_bReloading = false;
	m_bWeaponSwitching = false;
	m_bWeaponRelaxing = false;
	m_flWeaponRelaxAmount = 0.0f;
	m_bPlayingMisc = false;
    m_flReloadBlendIn = 0.0f;
    m_flReloadBlendOut = 0.0f;
    m_flMiscBlendIn = 0.0f;
    m_flMiscBlendOut = 0.0f;
	CBasePlayerAnimState::ClearAnimationState();
}

void CMapbasePlayerAnimState::ClearAnimationLayers()
{
	VPROF( "CBasePlayerAnimState::ClearAnimationLayers" );

	// In c_baseanimatingoverlay.cpp, this sometimes desyncs from the interpolated overlays and causes a crash in ResizeAnimationLayerCallback when the player dies. (pVec->Count() != pVecIV->Count())
	// Is there a better way of getting around this issue?
#ifndef CLIENT_DLL
	if ( !m_pOuter )
		return;

	m_pOuter->SetNumAnimOverlays( NUM_LAYERS_WANTED );
	for ( int i=0; i < m_pOuter->GetNumAnimOverlays(); i++ )
	{
        // If we're not using aim sequences, leave the aim layers alone
        // (allows them to be used outside of anim state)
        if ( !m_AnimConfig.m_bUseAimSequences && i <= NUM_AIMSEQUENCE_LAYERS )
            continue;

		m_pOuter->GetAnimOverlay( i )->SetOrder( CBaseAnimatingOverlay::MAX_OVERLAYS );
#ifndef CLIENT_DLL
		m_pOuter->GetAnimOverlay( i )->m_fFlags = 0;
#endif
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CMapbasePlayerAnimState::CalcAimLayerSequence( float *flCycle, float *flAimSequenceWeight, bool bForceIdle )
{
    // TODO?
    return m_pOuter->LookupSequence( "soldier_Aim_9_directions" );
}

void CMapbasePlayerAnimState::UpdateLayerSequenceGeneric( int iLayer, bool &bEnabled,
					float &flCurCycle, int &iSequence, bool bWaitAtEnd,
					float fBlendIn, float fBlendOut, bool bMoveBlend, float fPlaybackRate, bool bUpdateCycle /* = true */ )
{
	if ( !bEnabled )
		return;

	CStudioHdr *hdr = GetOuter()->GetModelPtr();
	if ( !hdr )
		return;

	if ( iSequence < 0 || iSequence >= hdr->GetNumSeq() )
		return;

	// Increment the fire sequence's cycle.
	if ( bUpdateCycle )
	{
		flCurCycle += m_pOuter->GetSequenceCycleRate( hdr, iSequence ) * gpGlobals->frametime * fPlaybackRate;
	}

	// temp: if the sequence is looping, don't override it - we need better handling of looping anims, 
	//  especially in misc layer from melee (right now the same melee attack is looped manually in asw_melee_system.cpp)
	bool bLooping = m_pOuter->IsSequenceLooping( hdr, iSequence );

	if ( flCurCycle > 1 && !bLooping )
	{
		if ( iLayer == RELOADSEQUENCE_LAYER )
		{
			m_bReloading = false;
		}
		if ( bWaitAtEnd )
		{
			flCurCycle = 1;
		}
		else
		{
			// Not firing anymore.
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	// if this animation should blend out as we move, then check for dropping it completely since we're moving too fast
	float speed = 0;
	if (bMoveBlend)
	{
		Vector vel;
		GetOuterAbsVelocity( vel );

		float speed = vel.Length2D();

		if (speed > 50)
		{
			bEnabled = false;
			iSequence = 0;
			return;
		}
	}

	// Now dump the state into its animation layer.
	CAnimationLayer *pLayer = m_pOuter->GetAnimOverlay( iLayer );

	pLayer->m_flCycle = flCurCycle;
	pLayer->m_nSequence = iSequence;

	pLayer->m_flPlaybackRate = fPlaybackRate;
	pLayer->m_flWeight = 1.0f;

	if (fBlendIn > 0.0f || fBlendOut > 0.0f)
	{
		// blend this layer in and out for smooth reloading
		if (flCurCycle < fBlendIn && fBlendIn>0)
		{
			pLayer->m_flWeight = ( clamp<float>(flCurCycle / fBlendIn,
				0.001f, 1.0f) );
		}
		else if (flCurCycle >= (1.0f - fBlendOut) && fBlendOut>0)
		{
			pLayer->m_flWeight = ( clamp<float>((1.0f - flCurCycle) / fBlendOut,
				0.001f, 1.0f) );
		}
		else
		{
			pLayer->m_flWeight = 1.0f;
		}
	}
	else
	{
		pLayer->m_flWeight = 1.0f;
	}
	if (bMoveBlend)		
	{
		// blend the animation out as we move faster
		if (speed <= 50)			
			pLayer->m_flWeight = ( pLayer->m_flWeight * (50.0f - speed) / 50.0f );
	}

#ifndef CLIENT_DLL
	pLayer->m_fFlags |= ANIM_LAYER_ACTIVE;
#endif
	pLayer->SetOrder( iLayer );
}

void CMapbasePlayerAnimState::ComputeFireSequence()
{
	UpdateLayerSequenceGeneric( FIRESEQUENCE_LAYER, m_bFiring, m_flFireCycle, m_iFireSequence, false );
}

void CMapbasePlayerAnimState::ComputeReloadSequence()
{
	UpdateLayerSequenceGeneric( RELOADSEQUENCE_LAYER, m_bReloading, m_flReloadCycle, m_iReloadSequence, false, m_flReloadBlendIn, m_flReloadBlendOut, false, m_fReloadPlaybackRate );
}

void CMapbasePlayerAnimState::ComputeWeaponSwitchSequence()
{
	UpdateLayerSequenceGeneric( RELOADSEQUENCE_LAYER, m_bWeaponSwitching, m_flWeaponSwitchCycle, m_iWeaponSwitchSequence, false, 0, 0.5f );
}

void CMapbasePlayerAnimState::ComputeRelaxSequence()
{
    bool bRelaxing = m_bWeaponRelaxing;
    float flRelaxSpeed = 0.05f;

    if ((m_bFiring && m_flFireCycle < 1.0f) || m_bReloading)
    {
        // Keep weapon raised
        bRelaxing = false;
        flRelaxSpeed = 0.5f;
        //GetOuter()->SetPoseParameter( GetOuter()->LookupPoseParameter( "weapon_lower" ), 0.0f );
    }

    if (bRelaxing ? m_flWeaponRelaxAmount != 1.0f : m_flWeaponRelaxAmount != 0.0f)
    {
        if (bRelaxing)
            m_flWeaponRelaxAmount += flRelaxSpeed;
        else
            m_flWeaponRelaxAmount -= flRelaxSpeed;

        m_flWeaponRelaxAmount = clamp( m_flWeaponRelaxAmount, 0.0f, 1.0f );

        GetOuter()->SetPoseParameter( m_nPoseWeaponLower, m_flWeaponRelaxAmount );

        /*int nPose = GetOuter()->LookupPoseParameter( "weapon_lower" );
        if (nPose != -1)
        {
            float flValue = RemapValClamped( (m_flWeaponRelaxTime - gpGlobals->curtime), 0.0f, 0.5f, 0.0f, 1.0f );

            if (flValue <= 0.0f)
            {
                // All done
                m_flWeaponRelaxTime = FLT_MAX;
            }

            if (m_bWeaponRelaxing)
                flValue = 1.0f - flValue;

            GetOuter()->SetPoseParameter( nPose, SimpleSpline( flValue ) );
        }*/
    }
    else if (bRelaxing)
    {
        GetOuter()->SetPoseParameter( m_nPoseWeaponLower, 1.0f );
    }

    /*bool bEnabled = m_bWeaponRelaxing;
    bool bUpdateCycle = true;
    if (bEnabled)
    {
        if (m_flWeaponRelaxCycle >= 0.5f)
        {
            // Pause at 0.5
            m_flWeaponRelaxCycle = 0.5f;
            bUpdateCycle = false;
        }
    }
    else if (m_flWeaponRelaxCycle < 1.0f)
    {
        // Make sure we exit the relax
        bEnabled = true;
    }

    UpdateLayerSequenceGeneric( AIMSEQUENCE_LAYER, bEnabled, m_flWeaponRelaxCycle, m_iWeaponRelaxSequence, false, 0.5f, 0.5f, false, 1.0f, bUpdateCycle );*/
}

// does misc gestures if we're not firing
void CMapbasePlayerAnimState::ComputeMiscSequence()
{
	UpdateLayerSequenceGeneric( MISCSEQUENCE_LAYER, m_bPlayingMisc, m_flMiscCycle, m_iMiscSequence, m_bMiscHoldAtEnd, m_flMiscBlendIn, m_flMiscBlendOut, m_bMiscOnlyWhenStill, m_fMiscPlaybackRate );
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  :  - 
// Output : float
//-----------------------------------------------------------------------------
float CMapbasePlayerAnimState::GetCurrentMaxGroundSpeed()
{
	CStudioHdr *pStudioHdr = GetOuter()->GetModelPtr();

	if ( pStudioHdr == NULL )
		return 1.0f;

    int iMoveX = GetOuter()->LookupPoseParameter( "move_x" );
    int iMoveY = GetOuter()->LookupPoseParameter( "move_y" );

	float prevX = GetOuter()->GetPoseParameter( iMoveX );
	float prevY = GetOuter()->GetPoseParameter( iMoveY );

	float d = MAX( fabs( prevX ), fabs( prevY ) );
	float newX, newY;
	if ( d == 0.0 )
	{ 
		newX = 1.0;
		newY = 0.0;
	}
	else
	{
		newX = prevX / d;
		newY = prevY / d;
	}

    GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, newX );
    GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, newY );

	float speed = GetOuter()->GetSequenceGroundSpeed(GetOuter()->GetSequence() );

    GetOuter()->SetPoseParameter( pStudioHdr, iMoveX, prevX );
    GetOuter()->SetPoseParameter( pStudioHdr, iMoveY, prevY );

	return speed;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
inline bool CMapbasePlayerAnimState::ShouldUseAimPoses( void ) const
{
    return GetAimPoseBlend() > 0.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CMapbasePlayerAnimState::GetAimPoseBlend( void ) const
{
    if (!GetOuter()->MyCombatCharacterPointer() || !GetOuter()->MyCombatCharacterPointer()->GetActiveWeapon()
        || GetOuter()->MyCombatCharacterPointer()->GetActiveWeapon()->IsEffectActive( EF_NODRAW ))
        return 0.0f;

    return 1.0f - m_flWeaponRelaxAmount;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float CMapbasePlayerAnimState::SetOuterBodyYaw( float flValue )
{
    float flAimPoseBlend = GetAimPoseBlend();

    GetOuter()->SetPoseParameter( m_nPoseAimYaw, flValue * flAimPoseBlend );
    return CBasePlayerAnimState::SetOuterBodyYaw( flValue * (1.0f - flAimPoseBlend) );
}

//-----------------------------------------------------------------------------
// Purpose: Override for backpeddling
// Input  : dt -
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::ComputePoseParam_BodyYaw( void )
{
    CBasePlayerAnimState::ComputePoseParam_BodyYaw();

    //ComputePoseParam_BodyLookYaw();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::ComputePoseParam_BodyLookYaw( void )
{
    // See if we even have a blender for pitch
    int upper_body_yaw = m_nPoseAimYaw;
    if ( upper_body_yaw < 0 )
    {
        return;
    }

    // Assume upper and lower bodies are aligned and that we're not turning
    float flGoalTorsoYaw = 0.0f;
    int turning = TURN_NONE;
    float turnrate = 360.0f;

    Vector vel;
   
    GetOuterAbsVelocity( vel );

    bool isMoving = ( vel.Length() > 1.0f ) ? true : false;

    if ( !isMoving )
    {
        // Just stopped moving, try and clamp feet
        if ( m_flLastTurnTime <= 0.0f )
        {
            m_flLastTurnTime    = gpGlobals->curtime;
            m_flLastYaw            = GetOuter()->EyeAngles().y;
            // Snap feet to be perfectly aligned with torso/eyes
            m_flGoalFeetYaw        = GetOuter()->EyeAngles().y;
            m_flCurrentFeetYaw    = m_flGoalFeetYaw;
            m_nTurningInPlace    = TURN_NONE;
        }

        // If rotating in place, update stasis timer

        if ( m_flLastYaw != GetOuter()->EyeAngles().y )
        {
            m_flLastTurnTime    = gpGlobals->curtime;
            m_flLastYaw            = GetOuter()->EyeAngles().y;
        }

        if ( m_flGoalFeetYaw != m_flCurrentFeetYaw )
        {
            m_flLastTurnTime    = gpGlobals->curtime;
        }

        turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, m_AnimConfig.m_flMaxBodyYawDegrees, gpGlobals->frametime, m_flCurrentFeetYaw );

        QAngle eyeAngles = GetOuter()->EyeAngles();
        QAngle vAngle = GetOuter()->GetLocalAngles();

        // See how far off current feetyaw is from true yaw
        float yawdelta = GetOuter()->EyeAngles().y - m_flCurrentFeetYaw;
        yawdelta = AngleNormalize( yawdelta );

        bool rotated_too_far = false;

        float yawmagnitude = fabs( yawdelta );

        // If too far, then need to turn in place
        if ( yawmagnitude > 45 )
        {
            rotated_too_far = true;
        }

        // Standing still for a while, rotate feet around to face forward
        // Or rotated too far
        // FIXME:  Play an in place turning animation
        if ( rotated_too_far ||
            ( gpGlobals->curtime > m_flLastTurnTime + mp_facefronttime.GetFloat() ) )
        {
            m_flGoalFeetYaw        = GetOuter()->EyeAngles().y;
            m_flLastTurnTime    = gpGlobals->curtime;

        /*    float yd = m_flCurrentFeetYaw - m_flGoalFeetYaw;
            if ( yd > 0 )
            {
                m_nTurningInPlace = TURN_RIGHT;
            }
            else if ( yd < 0 )
            {
                m_nTurningInPlace = TURN_LEFT;
            }
            else
            {
                m_nTurningInPlace = TURN_NONE;
            }

            turning = ConvergeAngles( m_flGoalFeetYaw, turnrate, gpGlobals->frametime, m_flCurrentFeetYaw );
            yawdelta = GetOuter()->EyeAngles().y - m_flCurrentFeetYaw;*/

        }

        // Snap upper body into position since the delta is already smoothed for the feet
        flGoalTorsoYaw = yawdelta;
        m_flCurrentTorsoYaw = flGoalTorsoYaw;
    }
    else
    {
        m_flLastTurnTime = 0.0f;
        m_nTurningInPlace = TURN_NONE;
        m_flCurrentFeetYaw = m_flGoalFeetYaw = GetOuter()->EyeAngles().y;
        flGoalTorsoYaw = 0.0f;
        m_flCurrentTorsoYaw = GetOuter()->EyeAngles().y - m_flCurrentFeetYaw;
    }

    if ( turning == TURN_NONE )
    {
        m_nTurningInPlace = turning;
    }

    if ( m_nTurningInPlace != TURN_NONE )
    {
        // If we're close to finishing the turn, then turn off the turning animation
        if ( fabs( m_flCurrentFeetYaw - m_flGoalFeetYaw ) < MIN_TURN_ANGLE_REQUIRING_TURN_ANIMATION )
        {
            m_nTurningInPlace = TURN_NONE;
        }
    }

    GetOuter()->SetPoseParameter( upper_body_yaw, clamp( m_flCurrentTorsoYaw, -60.0f, 60.0f ) );

    /*
    // FIXME: Adrian, what is this?
    int body_yaw = GetOuter()->LookupPoseParameter( "body_yaw" );

    if ( body_yaw >= 0 )
    {
        GetOuter()->SetPoseParameter( body_yaw, 30 );
    }
    */

}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::ComputePoseParam_BodyPitch( CStudioHdr *pStudioHdr )
{
    // Get pitch from v_angle
    float flPitch = m_flEyePitch;

    if ( flPitch > 180.0f )
    {
        flPitch -= 360.0f;
    }
    flPitch = clamp( flPitch, -90, 90 );

    //float flAimPoseBlend = GetAimPoseBlend();

    // See if we have a blender for pitch
	if (m_nPoseAimPitch >= 0)
		GetOuter()->SetPoseParameter( pStudioHdr, m_nPoseAimPitch, flPitch );
	if (m_nPoseHeadPitch >= 0)
		GetOuter()->SetPoseParameter( pStudioHdr, m_nPoseHeadPitch, flPitch );

    //ComputePoseParam_HeadPitch( pStudioHdr );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CMapbasePlayerAnimState::ComputePoseParam_HeadPitch( CStudioHdr *pStudioHdr )
{
    float flPitch = m_flEyePitch;

    if ( flPitch > 180.0f )
    {
        flPitch -= 360.0f;
    }
    flPitch = clamp( flPitch, -90, 90 );

    GetOuter()->SetPoseParameter( pStudioHdr, m_nPoseHeadPitch, flPitch );
}
