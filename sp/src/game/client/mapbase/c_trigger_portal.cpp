//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Entity which teleports touched entities and reorients their physics
// 
// Author(s): Krispy: https://github.com/FriskTheFallenHuman/source-sdk-2013
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//////////////////////////////////////////////////////////////////////////
// C_TriggerPortal
// Moves touched entity to a target location, changing the model's orientation
// to match the exit target. It differs from CTriggerTeleport in that it
// reorients physics and has inputs to enable/disable its function.
//////////////////////////////////////////////////////////////////////////
class C_TriggerPortal : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_TriggerPortal, C_BaseEntity );
	DECLARE_CLIENTCLASS();

private:
	C_TriggerPortal* m_hRemotePortal;

};

IMPLEMENT_CLIENTCLASS_DT( C_TriggerPortal, DT_TriggerPortal, CTriggerPortal )
	RecvPropBool( RECVINFO( m_hRemotePortal ) ),
END_RECV_TABLE()
