//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: This file contains brand new VScript singletons and singletons replicated from API
//			documentation in other games.
//
//			See vscript_funcs_shared.cpp for more information.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include <vgui_controls/Controls.h> 
#include <vgui/ILocalize.h>
#include "ammodef.h"
#include "tier1/utlcommon.h"

#include "soundenvelope.h"
#include "saverestore_utlvector.h"
#include "stdstring.h"

#ifndef CLIENT_DLL
#include "ai_speech.h"
#include "ai_memory.h"
#include "ai_squad.h"
#endif // !CLIENT_DLL

#include "usermessages.h"
#include "filesystem.h"
#include "igameevents.h"
#include "engine/ivdebugoverlay.h"
#include "icommandline.h"

#ifdef CLIENT_DLL
#include "IEffects.h"
#include "fx.h"
#include "itempents.h"
#include "c_te_legacytempents.h"
#include "iefx.h"
#include "dlight.h"

#if !defined(NO_STEAM)
#include "steam/steam_api.h"
#endif
#endif

#include "vscript_singletons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern IScriptManager *scriptmanager;


#ifdef GAME_DLL
	extern void SendProxy_StringT_To_String(const SendProp*, const void*, const void*, DVariant*, int, int);
	extern void SendProxy_UtlVectorLength(const SendProp*, const void*, const void*, DVariant*, int, int);
	class CSendProxyRecipients;
	extern void* SendProxy_LengthTable(const SendProp*, const void*, const void* pData, CSendProxyRecipients*, int);
	#define DataTableProxy_EHandle SendProxy_EHandleToInt
	#define DataTableProxy_String SendProxy_StringToString
	#define DataTableProxy_TableLength SendProxy_LengthTable
	#define DataTableProxy_UtlVectorLength SendProxy_UtlVectorLength
#else
	extern void RecvProxy_UtlVectorLength(const CRecvProxyData*, void*, void*);
	extern void DataTableRecvProxy_LengthProxy(const RecvProp*, void**, void*, int);
	#define DataTableProxy_EHandle RecvProxy_IntToEHandle
	#define DataTableProxy_String RecvProxy_StringToString
	#define DataTableProxy_TableLength DataTableRecvProxy_LengthProxy
	#define DataTableProxy_UtlVectorLength RecvProxy_UtlVectorLength
#endif
extern ISaveRestoreOps* GetPhysObjSaveRestoreOps( PhysInterfaceId_t );
extern ISaveRestoreOps* ActivityDataOps();
extern ISaveRestoreOps* GetSoundSaveRestoreOps();
extern ISaveRestoreOps* GetStdStringDataOps();
#ifdef GAME_DLL
	#define UTLVECTOR_DATAOPS( fieldType, dataType )\
		CUtlVectorDataopsInstantiator< fieldType >::GetDataOps( (CUtlVector< dataType >*)0 )
	#define IS_EHANDLE_UTLVECTOR( td )\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CBaseEntity > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CBaseFlex > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CBaseAnimating > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CBaseCombatWeapon > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CBasePlayer > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CAI_BaseNPC > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CSceneEntity > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CSceneListManager > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CRagdollBoogie > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CFish > ) ||\
		td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_EHANDLE, CHandle< CVGuiScreen > )

	class CSceneListManager;
	class CRagdollBoogie;
	class CFish;
	#ifdef _DEBUG
		class CStringTableSaveRestoreOps;
		extern CStringTableSaveRestoreOps g_VguiScreenStringOps;
		extern INetworkStringTable *g_pStringTableVguiScreen;
		extern ISaveRestoreOps *thinkcontextFuncs;
		class CAI_EnemiesListSaveRestoreOps;
		extern CAI_EnemiesListSaveRestoreOps g_AI_MemoryListSaveRestoreOps;
		class CConceptHistoriesDataOps;
		extern CConceptHistoriesDataOps g_ConceptHistoriesSaveDataOps;
	#endif
#endif

//=============================================================================
// Net Prop Manager
// Based on L4D2 API
//=============================================================================
class CScriptNetPropManager
{
private:
#if GAME_DLL
	typedef SendProp NetProp;
	typedef SendTable NetTable;
	typedef ServerClass NetworkClass;

	NetworkClass *GetNetworkClass( CBaseEntity* p ) { return p->GetServerClass(); }
	NetTable *GetNetTable( NetworkClass* p ) { return p->m_pTable; }

	void NetworkStateChanged( CBaseEntity* p, int o ) { p->NetworkProp()->NetworkStateChanged( o ); }
#else
	typedef RecvProp NetProp;
	typedef RecvTable NetTable;
	typedef ClientClass NetworkClass;

	NetworkClass *GetNetworkClass( CBaseEntity* p ) { return p->GetClientClass(); }
	NetTable *GetNetTable( NetworkClass* p ) { return p->m_pRecvTable; }

	void NetworkStateChanged( CBaseEntity*, int ) {}
#endif

	int GetClassID( CBaseEntity *p )
	{
		return GetNetworkClass( p )->m_ClassID;
	}

	int GetIntPropSize( NetProp *pProp )
	{
		Assert( pProp->GetType() == DPT_Int );

#ifdef GAME_DLL
		extern void SendProxy_UInt8ToInt32( const SendProp*, const void*, const void*, DVariant*, int, int );
		extern void SendProxy_UInt16ToInt32( const SendProp*, const void*, const void*, DVariant*, int, int );
		extern void SendProxy_UInt32ToInt32( const SendProp*, const void*, const void*, DVariant*, int, int );

		SendVarProxyFn proxy = pProp->GetProxyFn();

		if ( proxy == SendProxy_Int8ToInt32 || proxy == SendProxy_UInt8ToInt32 )
			return 8;
		if ( proxy == SendProxy_Int16ToInt32 || proxy == SendProxy_UInt16ToInt32 )
			return 16;
		if ( proxy == SendProxy_Int32ToInt32 || proxy == SendProxy_UInt32ToInt32 )
			return 32;

		return pProp->m_nBits;
#else
		RecvVarProxyFn proxy = pProp->GetProxyFn();

		if ( proxy == RecvProxy_Int32ToInt8 )
			return 8;
		if ( proxy == RecvProxy_Int32ToInt16 )
			return 16;
		if ( proxy == RecvProxy_Int32ToInt32 )
			return 32;

		return 0;
#endif
	}

	bool IsEHandle( NetProp *pProp )
	{
		return ( pProp->GetProxyFn() == DataTableProxy_EHandle );
	}

	bool IsUtlVector( NetProp *pProp )
	{
#ifdef GAME_DLL
		SendVarProxyFn proxy = pProp->GetProxyFn();
#else
		RecvVarProxyFn proxy = pProp->GetProxyFn();
#endif

		return ( proxy == DataTableProxy_UtlVectorLength );
	}

private:
	enum types
	{
		_INT1			= ( 1 << 0 ),
		_INT8			= ( 1 << 1 ),
		_INT16			= ( 1 << 2 ),
		_INT32			= ( 1 << 3 ),
		_FLOAT			= ( 1 << 4 ),
		_VEC3			= ( 1 << 5 ),
		_VEC2			= ( 1 << 6 ),
		_EHANDLE		= ( 1 << 7 ),
		_CLASSPTR		= ( 1 << 8 ),
		_EDICT			= ( 1 << 9 ),
		_CSTRING		= ( 1 << 10 ),
		_STRING_T		= ( 1 << 11 ),
		_ARRAY			= ( 1 << 12 ),
		_DATATABLE		= ( 1 << 13 ),

		_PHYS			= ( 1 << 14 ),
		_STDSTRING		= _CSTRING | _STRING_T,

		_DAR_EHANDLE	= _EHANDLE | _ARRAY,
		_DAR_CLASSPTR	= _CLASSPTR | _ARRAY,
		_DAR_INT		= _INT32 | _ARRAY,
		_DAR_FLOAT		= _FLOAT | _ARRAY,

		//_MAX			= ( 1 << 15 )
	};

	// UNDONE: Special case for GetPropType() to be able to return the table/array itself
	#define INDEX_GET_TYPE 0

	#define MASK_INT_SIZE( _size ) ( ( 1 << (_size - 1) ) | ( (1 << (_size - 1)) - 1 ) )
	#define MASK_NEAREST_BYTE( _bits ) ( ( (1 << ALIGN_TO_NEAREST_BYTE(_bits)) - 1 ) & ~((1 << _bits) - 1) )
	#define ALIGN_TO_NEAREST_BYTE( _bits ) ( (_bits + 7) & ~7 )
	#define VARINFO_ARRAYSIZE_BITS 12

	struct varinfo_t
	{
		int offset : 32; // actually a short

		union
		{
			int mask : 32;
			int stringsize : 32;
		};

		enum types datatype : 16;

		// element size in bytes
		unsigned int elemsize : 8;
		unsigned int arraysize : VARINFO_ARRAYSIZE_BITS;

		// Following are only used in integer netprops to handle unsigned and size casting
		bool isUnsigned : 1;
		bool isNotNetworked : 1;

		int GetOffset( int index )
		{
			return offset + index * elemsize;
		}
	};

	// Wrapper to be able to set case sensitive comparator in node insertion
	class vardict_t : public CUtlDict< varinfo_t >
	{
	public:
		vardict_t() : CUtlDict< varinfo_t >( k_eDictCompareTypeCaseSensitive ) {}
	};

	// NOTE: This is lazy and inefficient.
	// Simply map highest level class id to unique caches.
	CUtlVector< int > m_EntMap;
	CUtlVector< vardict_t > m_VarDicts;

	varinfo_t* CacheNew( CBaseEntity *pEnt, const char *szProp )
	{
		int idx = m_EntMap.Find( GetClassID( pEnt ) );
		if ( idx == m_EntMap.InvalidIndex() )
		{
			// Vector indices are kept in parallel as a workaround for encapsulating maps
			idx = m_EntMap.AddToTail( GetClassID( pEnt ) );
			m_VarDicts.AddToTail();
		}

		vardict_t &dict = m_VarDicts.Element( idx );

		idx = dict.Find( szProp );
		if ( idx == dict.InvalidIndex() )
			idx = dict.Insert( szProp );

		varinfo_t *pInfo = &dict.Element( idx );
		V_memset( pInfo, 0, sizeof( varinfo_t ) );
		return pInfo;
	}

	varinfo_t* CacheFetch( CBaseEntity *pEnt, const char *szProp )
	{
		int idx = m_EntMap.Find( GetClassID( pEnt ) );
		if ( idx == m_EntMap.InvalidIndex() )
			return NULL;

		vardict_t &dict = m_VarDicts.Element( idx );
		idx = dict.Find( szProp );
		if ( idx == dict.InvalidIndex() )
			return NULL;

		varinfo_t *pInfo = &dict.Element( idx );
		return pInfo;
	}

public:
	~CScriptNetPropManager()
	{
		PurgeCache();
	}

	void PurgeCache()
	{
		m_EntMap.Purge();
		m_VarDicts.Purge();
	}

private:
	typedescription_t *FindField( char *pBase, datamap_t *map, const char *szName, int *offset )
	{
		if ( map->baseMap )
		{
			typedescription_t* p = FindField( pBase, map->baseMap, szName, offset );
			if ( p )
				return p;
		}

		typedescription_t *pFields = map->dataDesc;
		int numFields = map->dataNumFields;

		for ( int i = 0; i < numFields; i++ )
		{
			typedescription_t* td = &pFields[i];
			int fieldType = td->fieldType;
			int fieldOffset = td->fieldOffset[ TD_OFFSET_NORMAL ];

			if ( td->flags & (FTYPEDESC_FUNCTIONTABLE | FTYPEDESC_INPUT | FTYPEDESC_OUTPUT) )
				continue;

			if ( fieldType == FIELD_VOID || fieldType == FIELD_FUNCTION )
				continue;

			if ( !V_strcmp( td->fieldName, szName ) )
			{
				*offset += fieldOffset;

				if ( td->flags & FTYPEDESC_PTR )
				{
					// Follow the pointer
					char * const pRef = *(char**)( pBase + *offset );
					Assert( pRef );
					*offset = pRef - pBase;
				}

				return td;
			}
		}

		return NULL;
	}

	NetProp *FindProp( char *pBase, NetTable *pTable, const char *szName, int *offset )
	{
		int numProps = pTable->GetNumProps();

		for ( int i = 0; i < numProps; i++ )
		{
			NetProp* pProp = pTable->GetProp(i);

			if ( pProp->IsInsideArray() )
				continue;

			if ( !V_strcmp( pProp->GetName(), szName ) )
			{
				*offset += pProp->GetOffset();
				return pProp;
			}

			// Go into inherited fields but not member tables, they are looked up explicitly
			// This is only a problem with m_AnimOverlay
			if ( ( pProp->GetFlags() & SPROP_COLLAPSIBLE ) ||
					( pProp->GetType() == DPT_DataTable && pProp->GetOffset() == 0 ) )
			{
				// Don't go into lengthproxy
				if ( pProp->GetDataTableProxyFn() == DataTableProxy_TableLength )
					continue;

				NetProp *p = FindProp( pBase + pProp->GetOffset(), pProp->GetDataTable(), szName, offset );
				if ( p )
				{
					*offset += pProp->GetOffset();
					return p;
				}
			}
		}

		return NULL;
	}

	typedescription_t *FindInDataMap( char * const pBase, datamap_t *map, const char *szFullProp, int *offset )
	{
		*offset = 0;

		// Look for exact match
		typedescription_t *pField = FindField( pBase, map, szFullProp, offset );
		if ( pField )
			return pField;

		// Look for members
		const char *pszProp = szFullProp;
		const char *pszPropEnd = V_strnchr( pszProp, '.', 512 );
		if ( !pszPropEnd )
			return NULL;
		do
		{
			// this string comes from squirrel stringtable, it can be modified
			*((char*)pszPropEnd) = 0;
			pField = FindField( pBase, map, pszProp, offset );
			*((char*)pszPropEnd) = '.';
			pszProp = pszPropEnd + 1;

			if ( !pField || ( map = pField->td ) == NULL )
				return NULL;

			// Look for exact match again, just in case
			pField = FindField( pBase, map, pszProp, offset );
			if ( pField )
				return pField;
		} while ( ( pszPropEnd = V_strnchr( pszProp, '.', 512 ) ) != NULL );

		return FindField( pBase, map, pszProp, offset );
	}

	NetProp *FindInNetTable( char * const pBase, NetTable *pTable, const char *szFullProp, int *offset )
	{
		*offset = 0;

		// Look for exact match
		NetProp *pProp = FindProp( pBase, pTable, szFullProp, offset );
		if ( pProp )
			return pProp;

		// Look for members
		const char *pszProp = szFullProp;
		const char *pszPropEnd = V_strnchr( pszProp, '.', 512 );
		if ( !pszPropEnd )
			return NULL;
		do
		{
			// this string comes from squirrel stringtable, it can be modified
			*((char*)pszPropEnd) = 0;
			pProp = FindProp( pBase, pTable, pszProp, offset );
			*((char*)pszPropEnd) = '.';
			pszProp = pszPropEnd + 1;

			if ( !pProp || ( pTable = pProp->GetDataTable() ) == NULL )
				return NULL;

			// Look for exact match again for fields such as m_Local{m_skybox3d.scale}
			pProp = FindProp( pBase, pTable, pszProp, offset );
			if ( pProp )
				return pProp;
		} while ( ( pszPropEnd = V_strnchr( pszProp, '.', 512 ) ) != NULL );

		return FindProp( pBase, pTable, pszProp, offset );
	}

	// Searches NetTable first to handle overwritten member network variables - see
	// CPlayerResource::m_iHealth and CBaseEntity::m_iHealth
	varinfo_t *GetVarInfo( CBaseEntity *pEnt, const char *szProp, int index )
	{
		int offset = 0;
		NetTable *pTable = GetNetTable( GetNetworkClass( pEnt ) );
		NetProp *pProp = FindInNetTable( (char*)pEnt, pTable, szProp, &offset );
		if ( pProp )
		{

#define SetVarInfo()\
				varinfo_t *pInfo = CacheNew( pEnt, szProp );\
				pInfo->isNotNetworked = 0;\
				pInfo->elemsize = pProp->GetElementStride();\
				pInfo->arraysize = pProp->GetNumElements();\
				pInfo->offset = offset;

			switch ( pProp->GetType() )
			{
			case DPT_Int:
			{
				if ( IsUtlVector( pProp ) )
				{
					return NULL;
				}

				if ( index < 0 || index >= pProp->GetNumElements() )
				{
					Warning( "NetProp element index out of range! %s[%d]\n", szProp, index );
					return NULL;
				}

				Assert( index == 0 || pProp->GetElementStride() > 0 );

				if ( IsEHandle( pProp ) )
				{
					Assert( pProp->GetElementStride() == sizeof(int) || pProp->GetElementStride() < 0 );

					SetVarInfo();
					pInfo->datatype = types::_EHANDLE;
					return pInfo;
				}
				else
				{
					const int size = GetIntPropSize( pProp );
#ifdef CLIENT_DLL
					// Client might be reading any amount of bits in a custom RecvProxy
					// Break and check the datamaps
					if ( size == 0 )
						break;
#endif
					Assert( size <= pProp->GetElementStride() || pProp->GetElementStride() < 0 );

					SetVarInfo();
					pInfo->mask = MASK_INT_SIZE( size );
					pInfo->datatype = types::_INT32;
					return pInfo;
				}
			}
			case DPT_Float:
			{
				if ( index < 0 || index >= pProp->GetNumElements() )
				{
					Warning( "NetProp element index out of range! %s[%d]\n", szProp, index );
					return NULL;
				}

				Assert( index == 0 || pProp->GetElementStride() > 0 );
				Assert( pProp->GetElementStride() == sizeof(float) || pProp->GetElementStride() < 0 );

				SetVarInfo();
				pInfo->datatype = types::_FLOAT;
				return pInfo;
			}
			case DPT_Vector:
			{
				if ( index < 0 || index >= pProp->GetNumElements() )
				{
					Warning( "NetProp element index out of range! %s[%d]\n", szProp, index );
					return NULL;
				}

				Assert( index == 0 || pProp->GetElementStride() > 0 );
				Assert( pProp->GetElementStride() == sizeof(float)*3 || pProp->GetElementStride() < 0 );

				SetVarInfo();
				pInfo->datatype = types::_VEC3;
				return pInfo;
			}
			case DPT_VectorXY:
			{
				if ( index < 0 || index >= pProp->GetNumElements() )
				{
					Warning( "NetProp element index out of range! %s[%d]\n", szProp, index );
					return NULL;
				}

				Assert( index == 0 || pProp->GetElementStride() > 0 );
				Assert( pProp->GetElementStride() == sizeof(float)*2 || pProp->GetElementStride() < 0 );

				SetVarInfo();
				pInfo->datatype = types::_VEC2;
				return pInfo;
			}
			case DPT_String:
			{
				if ( index < 0 || index >= pProp->GetNumElements() )
				{
					Warning( "NetProp element index out of range! %s[%d]\n", szProp, index );
					return NULL;
				}

				Assert( index == 0 || pProp->GetElementStride() > 0 );

				SetVarInfo();
#ifdef GAME_DLL
				pInfo->stringsize = 0;
#else
				pInfo->stringsize = pProp->m_StringBufferSize;
#endif
#ifdef GAME_DLL
				if ( pProp->GetProxyFn() == SendProxy_StringT_To_String )
				{
					pInfo->datatype = types::_STRING_T;
				}
				else
#endif
				{
					Assert( pProp->GetProxyFn() == DataTableProxy_String );
					pInfo->datatype = types::_CSTRING;
				}
				return pInfo;
			}
			case DPT_DataTable:
			{
				NetTable* pArray = pProp->GetDataTable();

				if ( V_strcmp( pProp->GetName(), pArray->GetName() ) != 0 )
				{
					Warning( "DT is not an array! %s(%s)\n", pProp->GetName(), pArray->GetName() );
					return NULL;
				}

				if ( index < 0 || index >= pArray->GetNumProps() )
				{
					Warning( "NetProp element index out of range! %s[%d]\n", szProp, index );
					return NULL;
				}

				pProp = pArray->GetProp( index );

				switch ( pProp->GetType() )
				{
				case DPT_Int:
				{
					if ( IsEHandle( pProp ) )
					{
						varinfo_t *pInfo = CacheNew( pEnt, szProp );
						pInfo->elemsize = sizeof(int);
						pInfo->arraysize = pArray->GetNumProps();
						pInfo->offset = offset;
						pInfo->datatype = types::_EHANDLE;
						return pInfo;
					}
					else
					{
						const int size = GetIntPropSize( pProp );
#ifdef CLIENT_DLL
						// Client might be reading any amount of bits in a custom RecvProxy
						// Break and check the datamaps
						if ( size == 0 )
							break;
#endif
						varinfo_t *pInfo = CacheNew( pEnt, szProp );

						if ( pArray->GetNumProps() > 1 )
						{
							pInfo->elemsize = pArray->GetProp(1)->GetOffset() - pArray->GetProp(0)->GetOffset();
						}
						else
						{
							// Doesn't matter for an array of a single element
							pInfo->elemsize = 0;
						}

						pInfo->arraysize = pArray->GetNumProps();
						pInfo->offset = offset;
						pInfo->mask = MASK_INT_SIZE( size );
						pInfo->datatype = types::_INT32;
						return pInfo;
					}
				}
				case DPT_Float:
				{
					varinfo_t *pInfo = CacheNew( pEnt, szProp );
					pInfo->elemsize = sizeof(float);
					pInfo->arraysize = pArray->GetNumProps();
					pInfo->offset = offset;
					pInfo->datatype = types::_FLOAT;
					return pInfo;
				}
				case DPT_Vector:
				{
					varinfo_t *pInfo = CacheNew( pEnt, szProp );
					pInfo->elemsize = sizeof(float)*3;
					pInfo->arraysize = pArray->GetNumProps();
					pInfo->offset = offset;
					pInfo->datatype = types::_VEC3;
					return pInfo;
				}
				case DPT_VectorXY:
				{
					varinfo_t *pInfo = CacheNew( pEnt, szProp );
					pInfo->elemsize = sizeof(float)*2;
					pInfo->arraysize = pArray->GetNumProps();
					pInfo->offset = offset;
					pInfo->datatype = types::_VEC2;
					return pInfo;
				}
				case DPT_DataTable:
				{
					AssertMsg( 0, "DT in DT" );
					return NULL;
				}
				case DPT_Array:
				{
					AssertMsg( 0, "Array in DT" );
					return NULL;
				}
				case DPT_String:
				{
					AssertMsg( 0, "String in DT" );
					return NULL;
				}
				default: UNREACHABLE();
				}
#ifdef CLIENT_DLL
				// DPT_Int can break into here for datamap fallback
				break;
#else
				UNREACHABLE();
#endif
			} // DPT_DataTable
			case DPT_Array:
			{
				Assert( pProp->GetArrayProp() );

				NetProp *pArray = pProp->GetArrayProp();
				offset += pArray->GetOffset();

				if ( index < 0 || index >= pProp->GetNumElements() )
				{
					Warning( "NetProp element index out of range! %s[%d]\n", szProp, index );
					return NULL;
				}

				switch ( pArray->GetType() )
				{
				case DPT_Int:
				{
					Assert( index == 0 || pProp->GetElementStride() > 0 );

					if ( IsEHandle( pArray ) )
					{
						SetVarInfo();
						pInfo->datatype = types::_EHANDLE;
						return pInfo;
					}
					else
					{
						const int size = GetIntPropSize( pArray );
#ifdef CLIENT_DLL
						// Client might be reading any amount of bits in a custom RecvProxy
						// Break and check the datamaps
						if ( size == 0 )
							break;
#endif
						SetVarInfo();
						pInfo->mask = MASK_INT_SIZE( size );
						pInfo->datatype = types::_INT32;
						return pInfo;
					}
				}
				case DPT_Float:
				{
					SetVarInfo();
					pInfo->datatype = types::_FLOAT;
					return pInfo;
				}
				case DPT_Vector:
				{
					SetVarInfo();
					pInfo->datatype = types::_VEC3;
					return pInfo;
				}
				case DPT_VectorXY:
				{
					SetVarInfo();
					pInfo->datatype = types::_VEC2;
					return pInfo;
				}
				case DPT_String:
				{
					AssertMsg( 0, "String array not implemented" );
					return NULL;
				}
				case DPT_Array:
				case DPT_DataTable: AssertMsg( 0, "DT in array" );
				default: UNREACHABLE();
				}
#ifdef CLIENT_DLL
				// DPT_Int can break into here for datamap fallback
				break;
#else
				UNREACHABLE();
#endif
			} // DPT_Array
			default: UNREACHABLE();
			}
			// ambigious int size on client, check the datamaps
#undef SetVarInfo
		}

		datamap_t *map = pEnt->GetDataDescMap();
		typedescription_t *pField = FindInDataMap( (char*)pEnt, map, szProp, &offset );
		if ( pField )
		{
#ifdef CLIENT_DLL
find_field:
#endif
			if ( index < 0 || index >= pField->fieldSize )
			{
				Warning( "NetProp element index out of range! %s[%d]\n", szProp, index );
				return NULL;
			}

#define SetVarInfo()\
				varinfo_t *pInfo = CacheNew( pEnt, szProp );\
				pInfo->isNotNetworked = 1;\
				pInfo->elemsize = pField->fieldSizeInBytes / pField->fieldSize;\
				pInfo->arraysize = pField->fieldSize;\
				pInfo->offset = offset;

			switch ( pField->fieldType )
			{
			case FIELD_INTEGER:
			case FIELD_MATERIALINDEX:
			case FIELD_MODELINDEX:
			case FIELD_COLOR32:
			case FIELD_TICK:
			case FIELD_BOOLEAN:
			case FIELD_CHARACTER:
			case FIELD_SHORT:
			{
				SetVarInfo();
				pInfo->isUnsigned = ( pField->flags & SPROP_UNSIGNED ) != 0;
				pInfo->isNotNetworked = 1;
				switch ( pField->fieldType )
				{
					case FIELD_INTEGER:
					case FIELD_MATERIALINDEX:
					case FIELD_MODELINDEX:
					case FIELD_COLOR32:
					case FIELD_TICK:
						pInfo->datatype = types::_INT32; break;
					case FIELD_BOOLEAN:
						pInfo->datatype = types::_INT1; break;
					case FIELD_CHARACTER:
						Assert( pField->fieldSizeInBytes == pField->fieldSize );
						pInfo->stringsize = pField->fieldSizeInBytes;
						pInfo->datatype = types::_INT8; break;
					case FIELD_SHORT:
						pInfo->datatype = types::_INT16; break;
					default: UNREACHABLE();
				}
				return pInfo;
			}
			case FIELD_FLOAT:
			case FIELD_TIME:
			{
				Assert( sizeof(float) == pField->fieldSizeInBytes / pField->fieldSize );

				SetVarInfo();
				pInfo->datatype = types::_FLOAT;
				return pInfo;
			}
			case FIELD_EHANDLE:
			{
				Assert( sizeof(int) == pField->fieldSizeInBytes / pField->fieldSize );

				SetVarInfo();
				pInfo->datatype = types::_EHANDLE;
				return pInfo;
			}
#ifdef GAME_DLL
			case FIELD_CLASSPTR:
			{
				Assert( sizeof(int*) == pField->fieldSizeInBytes / pField->fieldSize );

				SetVarInfo();
				pInfo->datatype = types::_CLASSPTR;
				return pInfo;
			}
			case FIELD_EDICT:
			{
				Assert( sizeof(int*) == pField->fieldSizeInBytes / pField->fieldSize );

				SetVarInfo();
				pInfo->datatype = types::_EDICT;
				return pInfo;
			}
#endif
			case FIELD_VECTOR:
			case FIELD_POSITION_VECTOR:
			{
				Assert( sizeof(float)*3 == pField->fieldSizeInBytes / pField->fieldSize );

				SetVarInfo();
				pInfo->datatype = types::_VEC3;
				return pInfo;
			}
			case FIELD_STRING:
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
			{
				SetVarInfo();
				pInfo->stringsize = 0;
				pInfo->datatype = types::_STRING_T;
				return pInfo;
			}
			case FIELD_CUSTOM:
			{
				if ( pField->pSaveRestoreOps == GetPhysObjSaveRestoreOps( PIID_IPHYSICSOBJECT ) )
				{
					SetVarInfo();
					pInfo->datatype = types::_PHYS;
					return pInfo;
				}
				else if ( pField->pSaveRestoreOps == ActivityDataOps() )
				{
					SetVarInfo();
					pInfo->datatype = types::_INT32;
					return pInfo;
				}
#ifdef GAME_DLL
				else if ( IS_EHANDLE_UTLVECTOR( pField ) )
				{
					SetVarInfo();
					pInfo->arraysize = ( 1 << VARINFO_ARRAYSIZE_BITS ) - 1; // dynamic, check on get
					pInfo->datatype = types::_DAR_EHANDLE;
				}
				else if ( pField->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_CLASSPTR, CBaseEntity* ) )
				{
					SetVarInfo();
					pInfo->arraysize = ( 1 << VARINFO_ARRAYSIZE_BITS ) - 1; // dynamic, check on get
					pInfo->datatype = types::_DAR_CLASSPTR;
				}
				else if ( pField->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_INTEGER, int ) )
				{
					SetVarInfo();
					pInfo->arraysize = ( 1 << VARINFO_ARRAYSIZE_BITS ) - 1; // dynamic, check on get
					pInfo->datatype = types::_DAR_INT;
				}
				else if ( pField->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_FLOAT, float ) ||
						pField->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_TIME, float ) )
				{
					SetVarInfo();
					pInfo->arraysize = ( 1 << VARINFO_ARRAYSIZE_BITS ) - 1; // dynamic, check on get
					pInfo->datatype = types::_DAR_FLOAT;
				}
				// Only used by CAI_PlayerAlly::m_PendingConcept
				else if ( pField->pSaveRestoreOps == GetStdStringDataOps() )
				{
					SetVarInfo();
					pInfo->datatype = types::_STDSTRING;
					return pInfo;
				}
#endif
				return NULL;
			}
			case FIELD_EMBEDDED:
				return NULL;
			default:
				AssertMsg( 0, "Unknown type %d\n", pField->fieldType );
				return NULL;
			}
			UNREACHABLE();
#undef SetVarInfo
		}
#ifdef CLIENT_DLL
		else
		{
			map = pEnt->GetPredDescMap();
			pField = FindInDataMap( (char*)pEnt, map, szProp, &offset );
			if ( pField )
			{
				goto find_field;
			}
		}
#endif
		return NULL;
	}

public:
	// FIXME: Cannot get datatable/arrays at the moment
	bool HasProp( HSCRIPT hEnt, const char *szProp )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return false;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, INDEX_GET_TYPE );

			if ( !pInfo )
				return false;
		}

		return true;
	}

	// FIXME: Cannot get datatable/arrays at the moment
	const char *GetPropType( HSCRIPT hEnt, const char *szProp )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return NULL;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, INDEX_GET_TYPE );

			if ( !pInfo )
				return NULL;
		}

		switch ( pInfo->datatype )
		{
			case types::_INT1:
			case types::_INT8:
			case types::_INT16:
			case types::_INT32:
				return "integer";
			case types::_FLOAT:
				return "float";
			case types::_VEC3:
				return "vector";
			case types::_VEC2:
				return "vector2d";
			case types::_CSTRING:
			case types::_STRING_T:
			case types::_STDSTRING:
				return "string";
			case types::_EHANDLE:
			case types::_CLASSPTR:
			case types::_EDICT:
				return "entity";
			case types::_PHYS:
				return "phys";
			case types::_ARRAY:
				return "array";
			case types::_DATATABLE:
				return "datatable";
		}

		if ( pInfo->arraysize > 1 )
			return "array";

		return "<unknown>";
	}

	int GetPropArraySize( HSCRIPT hEnt, const char *szProp )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return -1;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, INDEX_GET_TYPE );

			if ( !pInfo )
				return -1;
		}
#ifdef GAME_DLL
		switch ( pInfo->datatype )
		{
			case types::_DAR_EHANDLE:
			{
				CUtlVector< EHANDLE > &vec = *(CUtlVector< EHANDLE >*)((char*)pEnt + pInfo->offset);
				if ( !vec.Base() )
					return -1;
				return vec.Count();
			}
			case types::_DAR_CLASSPTR:
			{
				CUtlVector< CBaseEntity* > &vec = *(CUtlVector< CBaseEntity* >*)((char*)pEnt + pInfo->offset);
				if ( !vec.Base() )
					return -1;
				return vec.Count();
			}
			case types::_DAR_INT:
			{
				CUtlVector< int > &vec = *(CUtlVector< int >*)((char*)pEnt + pInfo->offset);
				if ( !vec.Base() )
					return -1;
				return vec.Count();
			}
			case types::_DAR_FLOAT:
			{
				CUtlVector< float > &vec = *(CUtlVector< float >*)((char*)pEnt + pInfo->offset);
				if ( !vec.Base() )
					return -1;
				return vec.Count();
			}
		}
#endif
		return pInfo->arraysize;
	}

public:
	int GetPropIntArray( HSCRIPT hEnt, const char *szProp, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return -1;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return -1;
		}

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return -1;

		if ( pInfo->isNotNetworked )
		{
			switch ( pInfo->datatype )
			{
			case types::_INT32:
				if ( pInfo->isUnsigned )
					return *(unsigned int*)((char*)pEnt + pInfo->GetOffset( index ));
				return *(int*)((char*)pEnt + pInfo->GetOffset( index ));
			case types::_INT1:
				return *(bool*)((char*)pEnt + pInfo->GetOffset( index ));
			case types::_INT8:
				if ( pInfo->isUnsigned )
					return *(unsigned char*)((char*)pEnt + pInfo->GetOffset( index ));
				return *(char*)((char*)pEnt + pInfo->GetOffset( index ));
			case types::_INT16:
				if ( pInfo->isUnsigned )
					return *(unsigned short*)((char*)pEnt + pInfo->GetOffset( index ));
				return *(short*)((char*)pEnt + pInfo->GetOffset( index ));
#ifdef GAME_DLL
			case types::_DAR_INT:
			{
				CUtlVector< int > &vec = *(CUtlVector< int >*)((char*)pEnt + pInfo->offset);
				if ( !vec.Base() )
					return -1;
				if ( index >= vec.Count() )
					return -1;
				return vec[ index ];
			}
#endif
			}
		}
		else
		{
			switch ( pInfo->datatype )
			{
			case types::_INT32:
				return (*(int*)((char*)pEnt + pInfo->GetOffset( index ))) & pInfo->mask;
			}
		}

		return -1;
	}

	void SetPropIntArray( HSCRIPT hEnt, const char *szProp, int value, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return;
		}

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return;

		if ( pInfo->isNotNetworked )
		{
			switch ( pInfo->datatype )
			{
			case types::_INT32:
				if ( pInfo->isUnsigned )
				{
					*(unsigned int*)((char*)pEnt + pInfo->GetOffset( index )) = value;
					NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
					break;
				}
				*(int*)((char*)pEnt + pInfo->GetOffset( index )) = value;
				NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
				break;
			case types::_INT1:
				*(bool*)((char*)pEnt + pInfo->GetOffset( index )) = value;
				NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
				break;
			case types::_INT8:
				if ( pInfo->isUnsigned )
				{
					*(unsigned char*)((char*)pEnt + pInfo->GetOffset( index )) = value;
					NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
					break;
				}
				*(char*)((char*)pEnt + pInfo->GetOffset( index )) = value;
				NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
				break;
			case types::_INT16:
				if ( pInfo->isUnsigned )
				{
					*(unsigned short*)((char*)pEnt + pInfo->GetOffset( index )) = value;
					NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
					break;
				}
				*(short*)((char*)pEnt + pInfo->GetOffset( index )) = value;
				NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
				break;
#ifdef GAME_DLL
			case types::_DAR_INT:
			{
				CUtlVector< int > &vec = *(CUtlVector< int >*)((char*)pEnt + pInfo->offset);
				if ( !vec.Base() )
					return;
				if ( index >= vec.Count() )
					return;
				vec[ index ] = value;
				NetworkStateChanged( pEnt, pInfo->offset );
				break;
			}
#endif
			}
		}
		else
		{
			switch ( pInfo->datatype )
			{
			case types::_INT32:
			{
				int *dest = (int*)((char*)pEnt + pInfo->GetOffset( index ));
				*dest = (*dest & ~pInfo->mask) | (value & pInfo->mask);
				NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
				break;
			}
			}
		}
	}

	float GetPropFloatArray( HSCRIPT hEnt, const char *szProp, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return -1;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return -1;
		}

		if ( pInfo->datatype == types::_VEC3 )
			index /= 3;

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return -1;

		switch ( pInfo->datatype )
		{
		case types::_VEC3:
		case types::_FLOAT:
			return *(float*)((char*)pEnt + pInfo->GetOffset( index ));
#ifdef GAME_DLL
		case types::_DAR_FLOAT:
		{
			CUtlVector< float > &vec = *(CUtlVector< float >*)((char*)pEnt + pInfo->offset);
			if ( !vec.Base() )
				return -1;
			if ( index >= vec.Count() )
				return -1;
			return vec[ index ];
		}
#endif
		}

		return -1;
	}

	void SetPropFloatArray( HSCRIPT hEnt, const char *szProp, float value, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return;
		}

		if ( pInfo->datatype == types::_VEC3 )
			index /= 3;

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return;

		switch ( pInfo->datatype )
		{
		case types::_VEC3:
		case types::_FLOAT:
			*(float*)((char*)pEnt + pInfo->GetOffset( index )) = value;
			NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
			break;
#ifdef GAME_DLL
		case types::_DAR_FLOAT:
		{
			CUtlVector< float > &vec = *(CUtlVector< float >*)((char*)pEnt + pInfo->offset);
			if ( !vec.Base() )
				return;
			if ( index >= vec.Count() )
				return;
			vec[ index ] = value;
			NetworkStateChanged( pEnt, pInfo->offset );
			break;
		}
#endif
		}
	}

	HSCRIPT GetPropEntityArray( HSCRIPT hEnt, const char *szProp, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return NULL;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return NULL;
		}

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return NULL;

		switch ( pInfo->datatype )
		{
		case types::_EHANDLE:
		{
			EHANDLE &iEHandle = *(EHANDLE*)((char*)pEnt + pInfo->GetOffset( index ));
			return ToHScript( iEHandle );
		}
#ifdef GAME_DLL
		case types::_CLASSPTR:
		{
			CBaseEntity* ptr = *(CBaseEntity**)((char*)pEnt + pInfo->GetOffset( index ));
			return ToHScript( ptr );
		}
		case types::_EDICT:
		{
			edict_t* ptr = *(edict_t**)((char*)pEnt + pInfo->GetOffset( index ));
			return ToHScript( GetContainingEntity( ptr ) );
		}
		case types::_DAR_EHANDLE:
		{
			CUtlVector< EHANDLE > &vec = *(CUtlVector< EHANDLE >*)((char*)pEnt + pInfo->offset);
			if ( !vec.Base() )
				return NULL;
			if ( index >= vec.Count() )
				return NULL;
			return ToHScript( vec[ index ] );
		}
		case types::_DAR_CLASSPTR:
		{
			CUtlVector< CBaseEntity* > &vec = *(CUtlVector< CBaseEntity* >*)((char*)pEnt + pInfo->offset);
			if ( !vec.Base() )
				return NULL;
			if ( index >= vec.Count() )
				return NULL;
			return ToHScript( vec[ index ] );
		}
#endif
		case types::_PHYS:
		{
			IPhysicsObject* ptr = *(IPhysicsObject**)((char*)pEnt + pInfo->GetOffset( index ));
			return ptr ? g_pScriptVM->RegisterInstance( ptr ) : NULL;
		}
		}

		return NULL;
	}

	void SetPropEntityArray( HSCRIPT hEnt, const char *szProp, HSCRIPT value, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return;
		}

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return;

		switch ( pInfo->datatype )
		{
		case types::_EHANDLE:
			*(EHANDLE*)((char*)pEnt + pInfo->GetOffset( index )) = ToEnt( value );
			NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
			break;
#ifdef GAME_DLL
		case types::_CLASSPTR:
			*(CBaseEntity**)((char*)pEnt + pInfo->GetOffset( index )) = ToEnt( value );
			NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
			break;
		case types::_EDICT:
		{
			CBaseEntity* ptr = ToEnt( value );
			*(edict_t**)((char*)pEnt + pInfo->GetOffset( index )) = ptr ? ptr->edict() : NULL;
			NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
			break;
		}
		case types::_DAR_EHANDLE:
		{
			CUtlVector< EHANDLE > &vec = *(CUtlVector< EHANDLE >*)((char*)pEnt + pInfo->offset);
			if ( !vec.Base() )
				return;
			if ( index >= vec.Count() )
				return;
			vec[ index ] = ToEnt( value );
			NetworkStateChanged( pEnt, pInfo->offset );
			break;
		}
		case types::_DAR_CLASSPTR:
		{
			CUtlVector< CBaseEntity* > &vec = *(CUtlVector< CBaseEntity* >*)((char*)pEnt + pInfo->offset);
			if ( !vec.Base() )
				return;
			if ( index >= vec.Count() )
				return;
			vec[ index ] = ToEnt( value );
			NetworkStateChanged( pEnt, pInfo->offset );
			break;
		}
#endif
		}
	}

	const Vector &GetPropVectorArray( HSCRIPT hEnt, const char *szProp, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return vec3_invalid;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return vec3_invalid;
		}

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return vec3_invalid;

		switch ( pInfo->datatype )
		{
		case types::_VEC3:
			return *(Vector*)((char*)pEnt + pInfo->GetOffset( index ));
		}

		return vec3_invalid;
	}

	void SetPropVectorArray( HSCRIPT hEnt, const char *szProp, const Vector &value, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return;
		}

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return;

		switch ( pInfo->datatype )
		{
		case types::_VEC3:
			*(Vector*)((char*)pEnt + pInfo->GetOffset( index )) = value;
			NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
			break;
		}
	}

	const char *GetPropStringArray( HSCRIPT hEnt, const char *szProp, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return NULL;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return NULL;
		}

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return NULL;

		switch ( pInfo->datatype )
		{
		case types::_CSTRING:
			return (const char*)((char*)pEnt + pInfo->GetOffset( index ));
		case types::_STRING_T: // Identical to _CSTRING on client
			return STRING( *(string_t*)((char*)pEnt + pInfo->GetOffset( index )) );
		case types::_INT8:
		{
			if ( !pInfo->stringsize )
				return NULL;

			char * const pVar = ((char*)pEnt + pInfo->GetOffset( index ));

			// Is this null terminated?
			int i = 0;
			char *c = pVar;
			while ( *(c++) && i++ < pInfo->stringsize );

			if ( i >= pInfo->stringsize )
			{
				// Not a null terminated string, don't talk to me ever again
				pInfo->stringsize = 0;
				return NULL;
			}

			return pVar;
		}
#ifdef GAME_DLL
		case types::_STDSTRING:
			return ( (std::string*)((char*)pEnt + pInfo->GetOffset( index )) )->c_str();
#endif
		}

		return NULL;
	}

	void SetPropStringArray( HSCRIPT hEnt, const char *szProp, const char *value, int index )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return;

		varinfo_t *pInfo = CacheFetch( pEnt, szProp );
		if ( !pInfo )
		{
			pInfo = GetVarInfo( pEnt, szProp, index );

			if ( !pInfo )
				return;
		}

		if ( index < 0 || (unsigned int)index >= pInfo->arraysize )
			return;

		switch ( pInfo->datatype )
		{
		case types::_CSTRING:
		case types::_INT8:
		{
			if ( pInfo->stringsize )
			{
				V_strncpy( (char*)pEnt + pInfo->GetOffset( index ), value, pInfo->stringsize );
				NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
				break;
			}
		}
		case types::_STRING_T:
		{
			extern string_t FindPooledString( const char* );
			extern string_t AllocPooledString( const char* );

			string_t src = FindPooledString( value );
			if ( src == NULL_STRING )
				src = AllocPooledString( value );
#ifdef GAME_DLL
			*(string_t*)((char*)pEnt + pInfo->GetOffset( index )) = src;
#else
			V_strcpy( (char*)pEnt + pInfo->GetOffset( index ), src );
#endif
			NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
			break;
		}
#ifdef GAME_DLL
		case types::_STDSTRING:
		{
			( (std::string*)((char*)pEnt + pInfo->GetOffset( index )) )->assign( value, V_strlen(value) );
			NetworkStateChanged( pEnt, pInfo->GetOffset( index ) );
			break;
		}
#endif
		}
	}

#define GetProp( type, name )\
	type GetProp##name( HSCRIPT hEnt, const char* szProp )\
	{\
		return GetProp##name##Array( hEnt, szProp, 0 );\
	}

#define SetProp( type, name )\
	void SetProp##name( HSCRIPT hEnt, const char* szProp, type value )\
	{\
		SetProp##name##Array( hEnt, szProp, value, 0 );\
	}

	GetProp( int, Int );
	SetProp( int, Int );
	GetProp( float, Float );
	SetProp( float, Float );
	GetProp( HSCRIPT, Entity );
	SetProp( HSCRIPT, Entity );
	GetProp( const Vector&, Vector );
	SetProp( const Vector&, Vector );
	GetProp( const char*, String );
	SetProp( const char*, String );

#undef GetProp
#undef SetProp

#ifdef _DEBUG
private:
	CUtlBuffer m_output;
	CUtlString m_indent;
	int m_indent_level;

	void IndentStart()
	{
		m_indent = "";
		m_indent_level = 0;
	}

	void Indent1()
	{
		m_indent_level++;
		m_indent.Append("\t");
	}

	void Indent0()
	{
		m_indent_level--;
		m_indent = m_indent.Slice( 0, m_indent_level );
	}

	void PrintVec3( float *pVar )
	{
		if ( *(Vector*)pVar != vec3_invalid )
		{
			Print( "[%f %f %f]", pVar[0], pVar[1], pVar[2] );
		}
		else
		{
			Print("vec3_invalid");
		}
	}

	void PrintVec2( float *pVar )
	{
		Print( "[%f %f]", pVar[0], pVar[1] );
	}

	void PrintEntity( EHANDLE* pVar )
	{
		CBaseEntity* ent = *pVar;
		if ( ent )
		{
			Print("[%d]%s", ent->entindex(), ent->GetDebugName());
		}
		else
		{
			Print("null");
		}
	}
#ifdef GAME_DLL
	void PrintEntity( CBaseEntity* pVar )
	{
		CBaseEntity* ent = pVar;
		if ( ent )
		{
			Print("[%d]%s", ent->entindex(), ent->GetDebugName());
		}
		else
		{
			Print("null");
		}
	}

	void PrintEntity( edict_t* pVar )
	{
		CBaseEntity* ent = GetContainingEntity( pVar );
		if ( ent )
		{
			Print("[%d]%s", ent->entindex(), ent->GetDebugName());
		}
		else
		{
			Print("null");
		}
	}
#endif
#ifdef GAME_DLL
	void PrintString( string_t pVar )
	{
		if ( STRING(pVar) )
		{
			Print("\"%s\"", STRING(pVar));
		}
		else
		{
			Print("null");
		}
	}
#endif
	void PrintString( const char *pVar )
	{
		if ( pVar )
		{
			Print("\"%s\"", pVar);
		}
		else
		{
			Print("null");
		}
	}

	void PrintPropType( NetProp *pProp )
	{
		switch ( pProp->GetType() )
		{
			case DPT_Int:
				if ( IsUtlVector( pProp ) )
				{
					Print("UtlVector");
				}
				else if ( IsEHandle( pProp ) )
				{
					Print( "entity" );
				}
				else
				{
					Print( "int" );
				}
				break;
#ifdef SUPPORTS_INT64
			case DPT_Int64:
				AssertMsg( 0, "not implemented" );
				Print( "int64" );
				break;
#endif
			case DPT_Float:
				Print( "float" );
				break;
			case DPT_Vector:
				Print( "vec3" );
				break;
			case DPT_VectorXY:
				Print( "vec2" );
				break;
			case DPT_String:
			{
#ifdef GAME_DLL
				if ( pProp->GetProxyFn() == SendProxy_StringT_To_String )
				{
					Print("string_t");
				}
				else
#endif
				{
#ifdef CLIENT_DLL
					Print("string[%d]", pProp->m_StringBufferSize);
#else
					Print("string");
#endif
				}
				break;
			}
			case DPT_Array:
			case DPT_DataTable:
				break;
			default: UNREACHABLE();
		}
	}

	void PrintProp_r( char *pVar, NetProp *pProp )
	{
		switch ( pProp->GetType() )
		{
			case DPT_Int:
			{
				if ( IsUtlVector( pProp ) )
				{
				}
				else if ( IsEHandle( pProp ) )
				{
					PrintEntity( (EHANDLE*)pVar );
				}
				else
				{
#ifdef GAME_DLL
					// Is this value larger than networked size?
					AssertMsg( (*(int*)pVar & MASK_NEAREST_BYTE( pProp->m_nBits )) == 0,
							"%s(%i) %d bits doesn't fit networked %d bits",
							pProp->GetName(), *(int*)pVar & MASK_NEAREST_BYTE( pProp->m_nBits ), ALIGN_TO_NEAREST_BYTE(pProp->m_nBits), pProp->m_nBits );
#endif
					int size = GetIntPropSize( pProp );
					if ( size )
					{
						Print( "%i", *(int*)pVar & MASK_INT_SIZE( size ) );
					}
					else
					{
						Print( "<unknown size> 0x%08x", *(int*)pVar );
					}
				}
				break;
			}
#ifdef SUPPORTS_INT64
			case DPT_Int64:
			{
				Print( "%lli", *(int64*)pVar );
				break;
			}
#endif
			case DPT_Float:
			{
				Assert( pProp->GetElementStride() == sizeof(float) || pProp->GetElementStride() < 0 );
				if ( *(float*)pVar == FLT_MAX )
				{
					Print("FLT_MAX");
				}
				else
				{
					Print("%f", *(float*)pVar);
				}
				break;
			}
			case DPT_Vector:
			{
				PrintVec3( (float*)pVar );
				break;
			}
			case DPT_VectorXY:
			{
				PrintVec2( (float*)pVar );
				break;
			}
			case DPT_String:
			{
#ifdef GAME_DLL
				if ( pProp->GetProxyFn() == SendProxy_StringT_To_String )
				{
					PrintString( *(string_t*)pVar );
				}
				else
#endif
				{
					Assert( pProp->GetProxyFn() == DataTableProxy_String );
					PrintString( (char*)pVar );
				}
				break;
			}
			case DPT_DataTable:
			{
				NetTable* pArray = pProp->GetDataTable();
				Assert( pArray->GetNumProps() );

				if ( V_strcmp( pProp->GetName(), pArray->GetName() ) != 0 )
				{
					Print( " -> (%s)\n", pArray->GetName() );
					DumpNetTable_r( pVar, pArray );
					break;
				}

				// Double check that each element is the same size
				// Array indexing ints gets element size from this
				int diff1 = pArray->GetProp(1)->GetOffset() - pArray->GetProp(0)->GetOffset();
				for ( int k = 0; k < pArray->GetNumProps()-1; k++ )
				{
					int diff2 = pArray->GetProp(k+1)->GetOffset() - pArray->GetProp(k)->GetOffset();
					Assert( diff1 == diff2 );
				}

				Print(" <");
				PrintPropType( pArray->GetProp(0) );
				Print(" array> #%d", pArray->GetNumProps());
				Print("\n%s[", m_indent.Get());
				Indent1();

				for ( int j = 0; j < pArray->GetNumProps(); j++ )
				{
					Print("\n%s", m_indent.Get());
					PrintProp_r( pVar + pArray->GetProp(j)->GetOffset(), pArray->GetProp(j) );
				}

				Indent0();
				Print( "\n%s]", m_indent.Get() );

				break;
			}
			case DPT_Array:
			{
				Assert( pProp->GetArrayProp() );
				NetProp *pArray = pProp->GetArrayProp();
				pVar += pArray->GetOffset();

				int numElements = pProp->GetNumElements();
				int elementStride = pProp->GetElementStride();

				Print(" <");
				PrintPropType( pArray );
				Print(" array> #%d", numElements);
				Print("\n%s[", m_indent.Get());
				Indent1();

				for ( int j = 0; j < numElements; j++ )
				{
					Print("\n%s", m_indent.Get());
					PrintProp_r( pVar + j * elementStride, pArray );
				}

				Indent0();
				Print( "\n%s]", m_indent.Get() );

				break;
			}
			default: UNREACHABLE();
		}
	}

	void DumpNetTable_r( void *pEnt, NetTable *pTable )
	{
		Print("%s{\n", m_indent.Get());
		Indent1();

		int numProps = pTable->GetNumProps();

		for ( int i = 0; i < numProps; i++ )
		{
			NetProp* pProp = pTable->GetProp(i);
			char* pVar = (char*)pEnt + pProp->GetOffset();

			if ( pProp->IsInsideArray() )
				continue;

			Print( "%s%s", m_indent.Get(), pProp->GetName() );

			if ( pProp->GetOffset() == 0 )
				Print("<0>");

			if ( pProp->GetType() != DPT_DataTable )
				Print(" <");
			PrintPropType( pProp );
			if ( pProp->GetType() != DPT_DataTable )
				Print("> ");
			PrintProp_r( pVar, pProp );
			Print("\n");
		}

		Indent0();
		Print("%s}", m_indent.Get());
	}

	void PrintFieldType( char *pVar, typedescription_t *td )
	{
		switch ( td->fieldType )
		{
			case FIELD_INTEGER:
			case FIELD_MATERIALINDEX:
			case FIELD_MODELINDEX:
			case FIELD_TICK:
				Print( "int" );
				break;
			case FIELD_SHORT:
				Print( "short" );
				break;
			case FIELD_CHARACTER:
				Print( "char" );
				break;
			case FIELD_BOOLEAN:
				Print( "bool" );
				break;
			case FIELD_COLOR32:
				Print( "clr32" );
				break;
			case FIELD_FLOAT:
			case FIELD_TIME:
				Print( "float" );
				break;
			case FIELD_VECTOR:
			case FIELD_POSITION_VECTOR:
				Print( "vec3" );
				break;
			case FIELD_VECTOR2D:
				Print( "vec2" );
				break;
			case FIELD_STRING:
			case FIELD_MODELNAME:
			case FIELD_SOUNDNAME:
				Print( "string" );
				break;
			case FIELD_EHANDLE:
#ifdef GAME_DLL
			case FIELD_CLASSPTR:
			case FIELD_EDICT:
#endif
				Print( "entity" );
				break;
			case FIELD_VMATRIX:
				Print( "VMatrix" );
				break;
			case FIELD_VMATRIX_WORLDSPACE:
				Print( "VMatrix WORLDSPACE" );
				break;
			case FIELD_MATRIX3X4_WORLDSPACE:
				Print( "matrix3x4 WORLDSPACE" );
				break;
			case FIELD_INTERVAL:
				Print( "interval_t" );
				break;
			case FIELD_CUSTOM:
				PrintCustomFieldType( pVar, td );
				break;
			case FIELD_EMBEDDED:
				if ( td->fieldSize > 1 )
					Print( "DT" );
				break;
			default:
				Print( "unknown %d", td->fieldType );
		}
	}

	void PrintCustomFieldType( char *pVar, typedescription_t *td )
	{
		Assert( td->fieldType == FIELD_CUSTOM );

		const char *g_ppszPhysTypeNames[PIID_NUM_TYPES] =
		{
			"Unknown Phys",
			"IPhysicsObject",
			"IPhysicsFluidController",
			"IPhysicsSpring",
			"IPhysicsConstraintGroup",
			"IPhysicsConstraint",
			"IPhysicsShadowController",
			"IPhysicsPlayerController",
			"IPhysicsMotionController",
			"IPhysicsVehicleController",
		};

		for ( int i = 0; i < PIID_NUM_TYPES; i++ )
		{
			if ( td->pSaveRestoreOps == GetPhysObjSaveRestoreOps( (PhysInterfaceId_t)i ) )
			{
				Print("%s", g_ppszPhysTypeNames[i]);
				return;
			}
		}

		if ( td->pSaveRestoreOps == ActivityDataOps() )
		{
			Print("int");
		}
		else if ( td->pSaveRestoreOps == GetSoundSaveRestoreOps() )
		{
			Print("CSoundPatch");
		}
		else if ( td->pSaveRestoreOps == GetStdStringDataOps() )
		{
			Print("stdstring");
		}
#ifdef GAME_DLL
		else if ( IS_EHANDLE_UTLVECTOR( td ) )
		{
			CUtlVector< EHANDLE > &vec = *(CUtlVector< EHANDLE >*)pVar;
			if ( vec.Base() )
				Print("entity utlvector #%d", vec.Count());
			else
				Print("entity utlvector");
		}
		else if ( td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_INTEGER, int ) )
		{
			CUtlVector< int > &vec = *(CUtlVector< int >*)pVar;
			if ( vec.Base() )
				Print("int utlvector #%d", vec.Count());
			else
				Print("int utlvector");
		}
		else if ( td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_FLOAT, float ) ||
				td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_TIME, float ) )
		{
			CUtlVector< float > &vec = *(CUtlVector< float >*)pVar;
			if ( vec.Base() )
				Print("float utlvector #%d", vec.Count());
			else
				Print("float utlvector");
		}
		else if ( td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_STRING, string_t ) )
		{
			CUtlVector< string_t > &vec = *(CUtlVector< string_t >*)pVar;
			if ( vec.Base() )
				Print("string utlvector #%d", vec.Count());
			else
				Print("string utlvector");
		}
		else if ( td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_CLASSPTR, CBaseEntity* ) )
		{
			CUtlVector< CBaseEntity* > &vec = *(CUtlVector< CBaseEntity* >*)pVar;
			if ( vec.Base() )
				Print("entity utlvector #%d", vec.Count());
			else
				Print("entity utlvector");
		}
		else if ( td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_VECTOR, Vector ) )
		{
			AssertMsg( 0, "Implement me" );
			CUtlVector< Vector > &vec = *(CUtlVector< Vector >*)pVar;
			if ( vec.Base() )
				Print("Vector utlvector #%d", vec.Count());
			else
				Print("Vector utlvector");
		}
		else if ( !V_strcmp( td->fieldName, "m_pIk" ) )
		{
			Print("IK");
		}
		else if ( td->pSaveRestoreOps == thinkcontextFuncs )
		{
			Print("thinkfunc");
		}
		else if ( td->pSaveRestoreOps == (ISaveRestoreOps*)(&g_AI_MemoryListSaveRestoreOps) )
		{
			Print("AI memory map");
		}
		else if ( td->pSaveRestoreOps == (ISaveRestoreOps*)(&g_VguiScreenStringOps))
		{
			Print("string (vgui screen)");
		}
		else if ( td->pSaveRestoreOps == (ISaveRestoreOps*)(&g_ConceptHistoriesSaveDataOps) )
		{
			Print("concept histories");
		}
#endif // GAME_DLL
		else
		{
			Print("custom");
		}
	}

	void PrintCustomField( char *pVar, typedescription_t *td )
	{
		Assert( td->fieldType == FIELD_CUSTOM );

		for ( int i = 0; i < PIID_NUM_TYPES; i++ )
		{
			if ( td->pSaveRestoreOps == GetPhysObjSaveRestoreOps( (PhysInterfaceId_t)i ) )
			{
				Print("0x%x", pVar);
				return;
			}
		}

		if ( td->pSaveRestoreOps == ActivityDataOps() )
		{
			Print("%i", *(int*)pVar);
		}
		else if ( td->pSaveRestoreOps == GetSoundSaveRestoreOps() )
		{
			if ( *pVar )
			{
				CSoundPatch *pSound = *(CSoundPatch**)pVar;
				PrintString( CSoundEnvelopeController::GetController().SoundGetName( pSound ) );
			}
			else
			{
				Print( "null" );
			}
		}
		else if ( td->pSaveRestoreOps == GetStdStringDataOps() )
		{
			Print("%s", ((std::string*)pVar)->c_str());
		}
#ifdef GAME_DLL
		else if ( IS_EHANDLE_UTLVECTOR( td ) )
		{
			CUtlVector< EHANDLE > &vec = *(CUtlVector< EHANDLE >*)pVar;
			if ( !vec.Base() )
			{
				Print("null");
				return;
			}
			Print("\n%s[", m_indent.Get());
			Indent1();
			FOR_EACH_VEC( vec, i )
			{
				Print("\n%s", m_indent.Get());
				PrintEntity( vec[i] );
			}
			Indent0();
			Print("\n%s]", m_indent.Get());
		}
		else if ( td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_INTEGER, int ) )
		{
			CUtlVector< int > &vec = *(CUtlVector< int >*)pVar;
			if ( !vec.Base() )
			{
				Print("null");
				return;
			}
			Print("\n%s[", m_indent.Get());
			Indent1();
			FOR_EACH_VEC( vec, i )
			{
				Print("\n%s", m_indent.Get());
				Print( "%i", vec[i] );
			}
			Indent0();
			Print("\n%s]", m_indent.Get());
		}
		else if ( td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_FLOAT, float ) ||
				td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_TIME, float ) )
		{
			CUtlVector< float > &vec = *(CUtlVector< float >*)pVar;
			if ( !vec.Base() )
			{
				Print("null");
				return;
			}
			Print("\n%s[", m_indent.Get());
			Indent1();
			FOR_EACH_VEC( vec, i )
			{
				Print("\n%s", m_indent.Get());
				Print( "%f", vec[i] );
			}
			Indent0();
			Print("\n%s]", m_indent.Get());
		}
		else if ( td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_STRING, string_t ) )
		{
			CUtlVector< string_t > &vec = *(CUtlVector< string_t >*)pVar;
			if ( !vec.Base() )
			{
				Print("null");
				return;
			}
			Print("\n%s[", m_indent.Get());
			Indent1();
			FOR_EACH_VEC( vec, i )
			{
				Print("\n%s", m_indent.Get());
				PrintString( vec[i] );
			}
			Indent0();
			Print("\n%s]", m_indent.Get());
		}
		else if ( td->pSaveRestoreOps == UTLVECTOR_DATAOPS( FIELD_CLASSPTR, CBaseEntity* ) )
		{
			CUtlVector< CBaseEntity* > &vec = *(CUtlVector< CBaseEntity* >*)pVar;
			if ( !vec.Base() )
			{
				Print("null");
				return;
			}
			Print("\n%s[", m_indent.Get());
			Indent1();
			FOR_EACH_VEC( vec, i )
			{
				Print("\n%s", m_indent.Get());
				PrintEntity( vec[i] );
			}
			Indent0();
			Print("\n%s]", m_indent.Get());
		}
		else if ( td->pSaveRestoreOps == (ISaveRestoreOps*)(&g_VguiScreenStringOps) )
		{
			const char *pString = g_pStringTableVguiScreen->GetString( *(int*)pVar );
			PrintString( (char*)pString );
		}
#endif // GAME_DLL
		else
		{
			Print("0x%x", pVar);
		}
	}

	void PrintField_r( char *pVar, typedescription_t *td )
	{
		switch ( td->fieldType )
		{
		case FIELD_INTEGER:
		case FIELD_MATERIALINDEX:
		case FIELD_MODELINDEX:
		case FIELD_TICK:
			if ( td->flags & SPROP_UNSIGNED )
			{
				Print("%u", *(unsigned int*)pVar);
			}
			else
			{
				Print("%i", *(int*)pVar);
			}
			break;
		case FIELD_COLOR32:
			Print("0x%08x", *(int*)pVar);
			break;
		case FIELD_BOOLEAN:
			Print("%i", *(bool*)pVar & 1);
			break;
		case FIELD_CHARACTER:
			if ( *pVar < 0x20 )
			{
				Print("%i (0x%x)", *pVar, *pVar);
			}
			else
			{
				Print("%i '%c'", *pVar, *pVar);
			}
			break;
		case FIELD_SHORT:
			if ( td->flags & SPROP_UNSIGNED )
			{
				Print("%u", *(unsigned short*)pVar);
			}
			else
			{
				Print("%i", *(short*)pVar);
			}
			break;
		case FIELD_FLOAT:
		case FIELD_TIME:
			if ( *(float*)pVar == FLT_MAX )
			{
				Print("FLT_MAX");
			}
			else
			{
				Print("%f", *(float*)pVar);
			}
			break;
		case FIELD_VECTOR:
		case FIELD_POSITION_VECTOR:
			PrintVec3( (float*)pVar );
			break;
		case FIELD_VECTOR2D:
			PrintVec2( (float*)pVar );
			break;
		case FIELD_STRING:
		case FIELD_MODELNAME:
		case FIELD_SOUNDNAME:
#ifdef GAME_DLL
			PrintString( *(string_t*)pVar );
#else
			PrintString( *(char**)pVar );
#endif
			break;
		case FIELD_EHANDLE:
			PrintEntity( (EHANDLE*)pVar );
			break;
#ifdef GAME_DLL
		case FIELD_CLASSPTR:
			PrintEntity( *(CBaseEntity**)pVar );
			break;
		case FIELD_EDICT:
			PrintEntity( *(edict_t**)pVar );
			break;
#endif
		case FIELD_EMBEDDED:
			Print(" -> (%s)\n", td->td->dataClassName);
			DumpDataFields_r( pVar, td->td );
			break;
		case FIELD_CUSTOM:
			PrintCustomField( pVar, td );
			break;
		default:
			Print( "<unknown field %d>", td->fieldType );
		}
	}

	void DumpDataFields_r( void *pEnt, datamap_t *map )
	{
		Print("%s{\n", m_indent.Get());
		Indent1();

		if ( map->baseMap )
		{
			Print("%sbaseclass -> (%s)\n", m_indent.Get(), map->baseMap->dataClassName);
			DumpDataFields_r( pEnt, map->baseMap );
			Print("\n");
		}

		typedescription_t *pFields = map->dataDesc;
		int numFields = map->dataNumFields;

		for ( int i = 0; i < numFields; i++ )
		{
			typedescription_t* td = &pFields[i];

			if ( td->flags & (FTYPEDESC_FUNCTIONTABLE | FTYPEDESC_INPUT | FTYPEDESC_OUTPUT) )
				continue;

			if ( td->fieldType == FIELD_VOID || td->fieldType == FIELD_FUNCTION )
				continue;

			char *pVar = (char*)pEnt + td->fieldOffset[ TD_OFFSET_NORMAL ];

			if ( td->flags & FTYPEDESC_PTR )
			{
				AssertMsg( *(char**)pVar, "NULL ptr ref" );
				pVar = *(char**)pVar;
			}

			Print( "%s%s", m_indent.Get(), td->fieldName );

			if ( td->fieldSize == 1 )
			{
				if ( td->fieldType != FIELD_EMBEDDED )
					Print(" <");
				PrintFieldType( pVar, td );
				if ( td->fieldType != FIELD_EMBEDDED )
					Print("> ");
				PrintField_r( pVar, td );
			}
			else
			{
				Print(" <");
				PrintFieldType( pVar, td );
				Print(" array> #%d", td->fieldSize);

				Print("\n%s[", m_indent.Get());
				Indent1();

				for ( int j = 0; j < td->fieldSize; j++ )
				{
					Print("\n%s", m_indent.Get());
					PrintField_r( pVar + j * td->fieldSizeInBytes / td->fieldSize, td );
				}

				Indent0();
				Print("\n%s]", m_indent.Get());
			}

			Print("\n");
		}

		Indent0();
		Print("%s}", m_indent.Get());
	}

	void Print( const char *fmt, ... )
	{
		char buf[2048];
		va_list va;
		va_start( va, fmt );
		V_vsnprintf( buf, sizeof(buf) - 1, fmt, va );
		va_end( va );

		m_output.PutString( buf );
	}

public:
	void Dump( HSCRIPT hEnt, const char* filename )
	{
		CBaseEntity *pEnt = ToEnt( hEnt );
		if ( !pEnt )
			return;

		if ( !filename || !*filename )
			return;

		m_output.SetBufferType( true, false );
		IndentStart();

		Print( "<NetTable>\n" );
		Print( "(%s)\n", GetNetTable( GetNetworkClass(pEnt) )->GetName() );
		DumpNetTable_r( pEnt, GetNetTable( GetNetworkClass(pEnt) ) );
		Print( "\n</NetTable>\n" );

		Print( "<DataDesc>\n" );
		Print( "(%s)\n", pEnt->GetDataDescMap()->dataClassName );
		DumpDataFields_r( pEnt, pEnt->GetDataDescMap() );
		Print( "\n</DataDesc>\n" );
#ifdef CLIENT_DLL
		Print( "<PredDesc>\n" );
		Print( "(%s)\n", pEnt->GetPredDescMap()->dataClassName );
		DumpDataFields_r( pEnt, pEnt->GetPredDescMap() );
		Print( "\n</PredDesc>\n" );
#endif
		const char *pszFile = V_GetFileName( filename );
		filesystem->WriteFile( pszFile, "MOD", m_output );

		m_indent.Purge();
		m_output.Purge();
	}
#endif // _DEBUG
} g_ScriptNetPropManager;

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptNetPropManager, "CNetPropManager", SCRIPT_SINGLETON "Allows reading and updating the network properties and data fields of an entity." )
	DEFINE_SCRIPTFUNC( GetPropArraySize, "Returns the size of an array." )
	DEFINE_SCRIPTFUNC( GetPropEntity, "Reads an entity." )
	DEFINE_SCRIPTFUNC( GetPropEntityArray, "Reads an entity from an array." )
	DEFINE_SCRIPTFUNC( GetPropFloat, "Reads a float." )
	DEFINE_SCRIPTFUNC( GetPropFloatArray, "Reads a float from an array." )
	DEFINE_SCRIPTFUNC( GetPropInt, "Reads an integer." )
	DEFINE_SCRIPTFUNC( GetPropIntArray, "Reads an integer from an array." )
	DEFINE_SCRIPTFUNC( GetPropString, "Reads a string." )
	DEFINE_SCRIPTFUNC( GetPropStringArray, "Reads a string from an array." )
	DEFINE_SCRIPTFUNC( GetPropVector, "Reads a 3D vector." )
	DEFINE_SCRIPTFUNC( GetPropVectorArray, "Reads a 3D vector from an array." )
	DEFINE_SCRIPTFUNC( GetPropType, "Returns the netprop type as a string." )
	DEFINE_SCRIPTFUNC( HasProp, "Checks if netprop/datafield exists." )
	DEFINE_SCRIPTFUNC( SetPropEntity, "Sets an entity." )
	DEFINE_SCRIPTFUNC( SetPropEntityArray, "Sets an entity in an array." )
	DEFINE_SCRIPTFUNC( SetPropFloat, "Sets to the specified float." )
	DEFINE_SCRIPTFUNC( SetPropFloatArray, "Sets a float in an array." )
	DEFINE_SCRIPTFUNC( SetPropInt, "Sets to the specified integer." )
	DEFINE_SCRIPTFUNC( SetPropIntArray, "Sets an integer in an array." )
	DEFINE_SCRIPTFUNC( SetPropString, "Sets to the specified string." )
	DEFINE_SCRIPTFUNC( SetPropStringArray, "Sets a string in an array." )
	DEFINE_SCRIPTFUNC( SetPropVector, "Sets to the specified vector." )
	DEFINE_SCRIPTFUNC( SetPropVectorArray, "Sets a 3D vector in an array." )
#ifdef _DEBUG
	DEFINE_SCRIPTFUNC( Dump, "Dump all readable netprop and datafield values of this entity. Pass in file name to write into." );
#endif
END_SCRIPTDESC();

//=============================================================================
// Localization Interface
// Unique to Mapbase
//=============================================================================
class CScriptLocalize
{
public:

	const char *GetTokenAsUTF8( const char *pszToken )
	{
		const char *pText = g_pVGuiLocalize->FindAsUTF8( pszToken );
		if ( pText )
		{
			return pText;
		}

		return NULL;
	}

	void AddStringAsUTF8( const char *pszToken, const char *pszString )
	{
		wchar_t wpszString[256];
		g_pVGuiLocalize->ConvertANSIToUnicode( pszString, wpszString, sizeof(wpszString) );

		// TODO: This is a fake file name! Should "fileName" mean anything?
		g_pVGuiLocalize->AddString( pszToken, wpszString, "resource/vscript_localization.txt" );
	}

private:
} g_ScriptLocalize;

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptLocalize, "CLocalize", SCRIPT_SINGLETON "Accesses functions related to localization strings." )

	DEFINE_SCRIPTFUNC( GetTokenAsUTF8, "Gets the current language's token as a UTF-8 string (not Unicode)." )

	DEFINE_SCRIPTFUNC( AddStringAsUTF8, "Adds a new localized token as a UTF-8 string (not Unicode)." )

END_SCRIPTDESC();

//=============================================================================
// Game Event Listener
// Based on Source 2 API
//
// NOTE: In Source 2 vscript (Lua) event listener contexts are tables that are
// passed to the callback function as the call environment.
// In mapbase implementation these are string identifiers because unlike Lua,
// Squirrel has closure methods such as 'bindenv' which can bind functions to specified environments.
//=============================================================================

// Define to use the older code that loads all events manually independent from the game event manager.
// Otherwise access event descriptors directly from engine.
//#define USE_OLD_EVENT_DESCRIPTORS 1

class CScriptGameEventListener : public IGameEventListener2, public CAutoGameSystem
{
public:
	CScriptGameEventListener() : m_bActive(false)
	{
#ifdef _DEBUG
		m_nEventTick = 0;
#endif
	}

	~CScriptGameEventListener()
	{
		StopListeningForEvent();
	}

	int ListenToGameEvent( const char* szEvent, HSCRIPT hFunc, const char* szContext );
	void StopListeningForEvent();

public:
	static bool StopListeningToGameEvent( int listener );
	static void StopListeningToAllGameEvents( const char* szContext );

public:
	void FireGameEvent( IGameEvent *event );
	void LevelShutdownPreEntity();

private:
	//int m_index;
	HSCRIPT m_hCallback;
	unsigned int m_iContextHash;
	bool m_bActive;
#ifdef _DEBUG
	int m_nEventTick;
#endif

	static StringHashFunctor Hash;
	static inline unsigned int HashContext( const char* c ) { return c ? Hash(c) : 0; }

	inline int GetIndex()
	{
		Assert( sizeof(CScriptGameEventListener*) == sizeof(int) );
		return reinterpret_cast<intptr_t>(this);
	}

public:
	enum // event data types, dependant on engine definitions
	{
		TYPE_LOCAL  = 0,
		TYPE_STRING = 1,
		TYPE_FLOAT  = 2,
		TYPE_LONG   = 3,
		TYPE_SHORT  = 4,
		TYPE_BYTE   = 5,
		TYPE_BOOL   = 6
	};
	static void WriteEventData( IGameEvent *event, HSCRIPT hTable );

#ifdef USE_OLD_EVENT_DESCRIPTORS
	static void LoadAllEvents();
	static void LoadEventsFromFile( const char *filename, const char *pathID = NULL );
	static CUtlMap< unsigned int, KeyValues* > s_GameEvents;
	static CUtlVector< KeyValues* > s_LoadedFiles;
#endif

public:
	//static int g_nIndexCounter;
	static CUtlVectorAutoPurge< CScriptGameEventListener* > s_Listeners;
#if _DEBUG
	static void DumpEventListeners();
#endif

};

CUtlVectorAutoPurge< CScriptGameEventListener* > CScriptGameEventListener::s_Listeners;
StringHashFunctor CScriptGameEventListener::Hash;

#ifdef USE_OLD_EVENT_DESCRIPTORS
CUtlMap< unsigned int, KeyValues* > CScriptGameEventListener::s_GameEvents( DefLessFunc(unsigned int) );
CUtlVector< KeyValues* > CScriptGameEventListener::s_LoadedFiles;
#endif


#if _DEBUG
#ifdef CLIENT_DLL
CON_COMMAND_F( cl_dump_script_game_event_listeners, "Dump all game event listeners created from script.", FCVAR_CHEAT )
{
	CScriptGameEventListener::DumpEventListeners();
}
#else
CON_COMMAND_F( dump_script_game_event_listeners, "Dump all game event listeners created from script.", FCVAR_CHEAT )
{
	CScriptGameEventListener::DumpEventListeners();
}
#endif
#endif


#ifdef USE_OLD_EVENT_DESCRIPTORS
//-----------------------------------------------------------------------------
// Executed in LevelInitPreEntity
//-----------------------------------------------------------------------------
void CScriptGameEventListener::LoadAllEvents()
{
	// Listed in the same order they are loaded in GameEventManager
	const char *filenames[] =
	{
		"resource/serverevents.res",
		"resource/gameevents.res",
		"resource/mapbaseevents.res",
		"resource/modevents.res"
	};

	const char *pathlist[] =
	{
		"GAME",
		"MOD"
	};

	// Destroy old KeyValues
	if ( s_LoadedFiles.Count() )
	{
		for ( int i = s_LoadedFiles.Count(); i--; )
			s_LoadedFiles[i]->deleteThis();
		s_LoadedFiles.Purge();
		s_GameEvents.Purge();
	}

	for ( int j = 0; j < ARRAYSIZE(pathlist); ++j )
		for ( int i = 0; i < ARRAYSIZE(filenames); ++i )
		{
			LoadEventsFromFile( filenames[i], pathlist[j] );
		}
}

//-----------------------------------------------------------------------------
// Load event files into a lookup array to be able to return the event data to the VM.
//-----------------------------------------------------------------------------
void CScriptGameEventListener::LoadEventsFromFile( const char *filename, const char *pathID )
{
	KeyValues *pKV = new KeyValues("GameEvents");

	if ( !pKV->LoadFromFile( filesystem, filename, pathID ) )
	{
		// CGMsg( 1, CON_GROUP_VSCRIPT, "CScriptGameEventListener::LoadEventsFromFile: Failed to load file [%s]%s\n", pathID, filename );
		pKV->deleteThis();
		return;
	}

	int count = 0;

	for ( KeyValues *key = pKV->GetFirstSubKey(); key; key = key->GetNextKey() )
	{
		for ( KeyValues *sub = key->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
		{
			if ( sub->GetDataType() == KeyValues::TYPE_STRING )
			{
				const char *szVal = sub->GetString();
				if ( !V_stricmp( szVal, "string" ) )
				{
					sub->SetInt( NULL, TYPE_STRING );
				}
				else if ( !V_stricmp( szVal, "bool" ) )
				{
					sub->SetInt( NULL, TYPE_BOOL );
				}
				else if ( !V_stricmp( szVal, "byte" ) )
				{
					sub->SetInt( NULL, TYPE_BYTE );
				}
				else if ( !V_stricmp( szVal, "short" ) )
				{
					sub->SetInt( NULL, TYPE_SHORT );
				}
				else if ( !V_stricmp( szVal, "long" ) )
				{
					sub->SetInt( NULL, TYPE_LONG );
				}
				else if ( !V_stricmp( szVal, "float" ) )
				{
					sub->SetInt( NULL, TYPE_FLOAT );
				}
			}
			// none   : value is not networked
			// string : a zero terminated string
			// bool   : unsigned int, 1 bit
			// byte   : unsigned int, 8 bit
			// short  : signed int, 16 bit
			// long   : signed int, 32 bit
			// float  : float, 32 bit
		}

		// Store event subkeys
		// Replace key so modevents can overwrite gameevents.
		// It does not check for hash collisions, however.
		s_GameEvents.InsertOrReplace( Hash( key->GetName() ), key );
		++count;
	}

	// Store files (allocated KV)
	s_LoadedFiles.AddToTail( pKV );

	CGMsg( 2, CON_GROUP_VSCRIPT, "CScriptGameEventListener::LoadEventsFromFile: Loaded [%s]%s (%i)\n", pathID, filename, count );
}
#endif

#if _DEBUG
void CScriptGameEventListener::DumpEventListeners()
{
	CGMsg( 0, CON_GROUP_VSCRIPT, "--- Script game event listener dump start\n" );
	CGMsg( 0, CON_GROUP_VSCRIPT, "#    ID         CONTEXT\n" );
	FOR_EACH_VEC( s_Listeners, i )
	{
		CGMsg( 0, CON_GROUP_VSCRIPT, " %d : %d : %u\n", i,
										s_Listeners[i]->GetIndex(),
										s_Listeners[i]->m_iContextHash );
	}
	CGMsg( 0, CON_GROUP_VSCRIPT, "--- Script game event listener dump end\n" );
}
#endif

void CScriptGameEventListener::LevelShutdownPreEntity()
{
	s_Listeners.FindAndFastRemove(this);
	delete this;
}

void CScriptGameEventListener::FireGameEvent( IGameEvent *event )
{
#ifdef _DEBUG
	m_nEventTick = gpGlobals->tickcount;
#endif
	ScriptVariant_t hTable;
	g_pScriptVM->CreateTable( hTable );
	WriteEventData( event, hTable );
	g_pScriptVM->SetValue( hTable, "game_event_listener", GetIndex() );
	// g_pScriptVM->SetValue( hTable, "game_event_name", event->GetName() );
	g_pScriptVM->ExecuteFunction( m_hCallback, &hTable, 1, NULL, NULL, true );
	g_pScriptVM->ReleaseScript( hTable );
}

struct CGameEventDescriptor
{
	byte _0[36];
	KeyValues *m_pEventKeys;
	//byte _1[22];
};

class CGameEvent__// : public IGameEvent
{
public:
	virtual ~CGameEvent__();				// [0]
	CGameEventDescriptor *m_pDescriptor;	// 0x04
	//KeyValues *m_pEventData;				// 0x08
};

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CScriptGameEventListener::WriteEventData( IGameEvent *event, HSCRIPT hTable )
{
#ifdef USE_OLD_EVENT_DESCRIPTORS
	int i = s_GameEvents.Find( Hash( event->GetName() ) );
	if ( i == s_GameEvents.InvalidIndex() )
		return;
	KeyValues *pKV = s_GameEvents[i];
#endif

#if defined(_DEBUG) && !defined(USE_OLD_EVENT_DESCRIPTORS)
	try
	{
#endif

#if !defined(USE_OLD_EVENT_DESCRIPTORS)
	KeyValues *pKV = reinterpret_cast< CGameEvent__* >(event)->m_pDescriptor->m_pEventKeys;
#endif

	for ( KeyValues *sub = pKV->GetFirstSubKey(); sub; sub = sub->GetNextKey() )
	{
		const char *szKey = sub->GetName();
		switch ( sub->GetInt() )
		{
			case TYPE_LOCAL:
			case TYPE_STRING: g_pScriptVM->SetValue( hTable, szKey, event->GetString( szKey ) ); break;
			case TYPE_FLOAT:  g_pScriptVM->SetValue( hTable, szKey, event->GetFloat ( szKey ) ); break;
			case TYPE_BOOL:   g_pScriptVM->SetValue( hTable, szKey, event->GetBool  ( szKey ) ); break;
			default:          g_pScriptVM->SetValue( hTable, szKey, event->GetInt   ( szKey ) );
		}
	}

#if defined(_DEBUG) && !defined(USE_OLD_EVENT_DESCRIPTORS)
	// Access a bunch of KeyValues functions to validate it is the correct address.
	// This may not always throw an exception when it is incorrect, but eventually it will.
	}
	catch (...)
	{
		// CGameEvent or CGameEventDescriptor offsets did not match!
		// This should mean these were modified in engine.dll.
		//
		// Implement this utility yourself by adding a function to get event descriptor keys
		// either on CGameEventManager or on CGameEvent interfaces.
		// On CGameEventManager downcast IGameEvent input to CGameEvent, then return event->descriptor->keys
		// On CGameEvent return (member) descriptor->keys
		//
		// Finally assign it to pKV above.

		Warning("CScriptGameEventListener::WriteEventData internal error\n");
		Assert(0);
	}
#endif
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
int CScriptGameEventListener::ListenToGameEvent( const char* szEvent, HSCRIPT hFunc, const char* szContext )
{
	bool bValid;

	if ( gameeventmanager && hFunc )
#ifdef CLIENT_DLL
		bValid = gameeventmanager->AddListener( this, szEvent, false );
#else
		bValid = gameeventmanager->AddListener( this, szEvent, true );
#endif
	else bValid = false;

	if ( bValid )
	{
		m_iContextHash = HashContext( szContext );
		m_hCallback = hFunc;
		m_bActive = true;

		s_Listeners.AddToTail( this );

		return GetIndex();
	}
	else
	{
		delete this;
		return 0;
	}
}

//-----------------------------------------------------------------------------
// Free stuff. Called from the destructor, does not remove itself from the listener list.
//-----------------------------------------------------------------------------
void CScriptGameEventListener::StopListeningForEvent()
{
	if ( !m_bActive )
		return;

	if ( g_pScriptVM )
		g_pScriptVM->ReleaseScript( m_hCallback );

	m_hCallback = NULL;
	m_bActive = false;

	if ( gameeventmanager )
		gameeventmanager->RemoveListener( this );

#ifdef _DEBUG
	// Event listeners are iterated forwards in the game event manager,
	// removing while iterating will cause it to skip one listener.
	//
	// Fix this in engine without altering any behaviour by
	// changing event exeuction order to tail->head,
	// changing listener removal to tail->head,
	// changing listener addition to head
	if ( m_nEventTick == gpGlobals->tickcount )
	{
		Warning("CScriptGameEventListener stopped in the same frame it was fired. This will break other event listeners!\n");
	}
#endif
}

//-----------------------------------------------------------------------------
// Stop the specified event listener.
//-----------------------------------------------------------------------------
bool CScriptGameEventListener::StopListeningToGameEvent( int listener )
{
	CScriptGameEventListener *p = reinterpret_cast<CScriptGameEventListener*>(listener); // INT_TO_POINTER	

	bool bRemoved = s_Listeners.FindAndFastRemove(p);
	if ( bRemoved )
	{
		delete p;
	}

	return bRemoved;
}

//-----------------------------------------------------------------------------
// Stops listening to all events within a context.
//-----------------------------------------------------------------------------
void CScriptGameEventListener::StopListeningToAllGameEvents( const char* szContext )
{
	unsigned int hash = HashContext( szContext );
	for ( int i = s_Listeners.Count(); i--; )
	{
		CScriptGameEventListener *pCur = s_Listeners[i];
		if ( pCur->m_iContextHash == hash )
		{
			s_Listeners.FastRemove(i);
			delete pCur;
		}
	}
}

//=============================================================================
//=============================================================================

static int ListenToGameEvent( const char* szEvent, HSCRIPT hFunc, const char* szContext )
{
	CScriptGameEventListener *p = new CScriptGameEventListener();
	return p->ListenToGameEvent( szEvent, hFunc, szContext );
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
static void FireGameEvent( const char* szEvent, HSCRIPT hTable )
{
	IGameEvent *event = gameeventmanager->CreateEvent( szEvent );
	if ( event )
	{
		ScriptVariant_t key, val;
		int nIterator = -1;
		while ( ( nIterator = g_pScriptVM->GetKeyValue( hTable, nIterator, &key, &val ) ) != -1 )
		{
			switch ( val.m_type )
			{
				case FIELD_FLOAT:   event->SetFloat ( key.m_pszString, val.m_float     ); break;
				case FIELD_INTEGER: event->SetInt   ( key.m_pszString, val.m_int       ); break;
				case FIELD_BOOLEAN: event->SetBool  ( key.m_pszString, val.m_bool      ); break;
				case FIELD_CSTRING: event->SetString( key.m_pszString, val.m_pszString ); break;
			}

			g_pScriptVM->ReleaseValue(key);
			g_pScriptVM->ReleaseValue(val);
		}

#ifdef CLIENT_DLL
		gameeventmanager->FireEventClientSide(event);
#else
		gameeventmanager->FireEvent(event);
#endif
	}
}

#ifndef CLIENT_DLL
//-----------------------------------------------------------------------------
// Copy of FireGameEvent, server only with no broadcast to clients.
//-----------------------------------------------------------------------------
static void FireGameEventLocal( const char* szEvent, HSCRIPT hTable )
{
	IGameEvent *event = gameeventmanager->CreateEvent( szEvent );
	if ( event )
	{
		ScriptVariant_t key, val;
		int nIterator = -1;
		while ( ( nIterator = g_pScriptVM->GetKeyValue( hTable, nIterator, &key, &val ) ) != -1 )
		{
			switch ( val.m_type )
			{
				case FIELD_FLOAT:   event->SetFloat ( key.m_pszString, val.m_float     ); break;
				case FIELD_INTEGER: event->SetInt   ( key.m_pszString, val.m_int       ); break;
				case FIELD_BOOLEAN: event->SetBool  ( key.m_pszString, val.m_bool      ); break;
				case FIELD_CSTRING: event->SetString( key.m_pszString, val.m_pszString ); break;
			}

			g_pScriptVM->ReleaseValue(key);
			g_pScriptVM->ReleaseValue(val);
		}

		gameeventmanager->FireEvent(event,true);
	}
}
#endif // !CLIENT_DLL

static ScriptHook_t g_Hook_OnSave;
static ScriptHook_t g_Hook_OnRestore;

//=============================================================================
// Save/Restore Utility
// Based on L4D2 API
//=============================================================================
class CScriptSaveRestoreUtil : public CAutoGameSystem
{
public:
	static void SaveTable( const char *szId, HSCRIPT hTable );
	static void RestoreTable( const char *szId, HSCRIPT hTable );
	static void ClearSavedTable( const char *szId );

public: // IGameSystem

	void OnSave()
	{
		if ( g_pScriptVM )
		{
			if ( GetScriptHookManager().IsEventHooked( "OnSave" ) )
				g_Hook_OnSave.Call( NULL, NULL, NULL );

			// Legacy hook
			HSCRIPT hFunc = g_pScriptVM->LookupFunction( "OnSave" );
			if ( hFunc )
			{
				g_pScriptVM->Call( hFunc );
				g_pScriptVM->ReleaseScript( hFunc );
			}
		}
	}

#ifdef CLIENT_DLL
	// On the client, OnRestore() is called before VScript is actually restored, so this has to be called manually from VScript save/restore instead
	void OnVMRestore()
#else
	void OnRestore()
#endif
	{
		if ( g_pScriptVM )
		{
			if ( GetScriptHookManager().IsEventHooked( "OnRestore" ) )
				g_Hook_OnRestore.Call( NULL, NULL, NULL );

			// Legacy hook
			HSCRIPT hFunc = g_pScriptVM->LookupFunction( "OnRestore" );
			if ( hFunc )
			{
				g_pScriptVM->Call( hFunc );
				g_pScriptVM->ReleaseScript( hFunc );
			}
		}
	}

	void Shutdown()
	{
		FOR_EACH_MAP_FAST( m_Lookup, i )
			m_Lookup[i]->deleteThis();
		m_Lookup.Purge();
	}

private:
	static StringHashFunctor Hash;
	static CUtlMap< unsigned int, KeyValues* > m_Lookup;

} g_ScriptSaveRestoreUtil;

#ifdef CLIENT_DLL
void VScriptSaveRestoreUtil_OnVMRestore()
{
	g_ScriptSaveRestoreUtil.OnVMRestore();
}
#endif

CUtlMap< unsigned int, KeyValues* > CScriptSaveRestoreUtil::m_Lookup( DefLessFunc(unsigned int) );
StringHashFunctor CScriptSaveRestoreUtil::Hash;

//-----------------------------------------------------------------------------
// Store a table with primitive values that will persist across level transitions and save loads.
// Case sensitive
//-----------------------------------------------------------------------------
void CScriptSaveRestoreUtil::SaveTable( const char *szId, HSCRIPT hTable )
{
	KeyValues *pKV;
	unsigned int hash = Hash(szId);

	int idx = m_Lookup.Find( hash );
	if ( idx == m_Lookup.InvalidIndex() )
	{
		pKV = new KeyValues("ScriptSavedTable");
		m_Lookup.Insert( hash, pKV );
	}
	else
	{
		pKV = m_Lookup[idx];
		pKV->Clear();
	}

	ScriptVariant_t key, val;
	int nIterator = -1;
	while ( ( nIterator = g_pScriptVM->GetKeyValue( hTable, nIterator, &key, &val ) ) != -1 )
	{
		switch ( val.m_type )
		{
			case FIELD_FLOAT:   pKV->SetFloat ( key.m_pszString, val.m_float     ); break;
			case FIELD_INTEGER: pKV->SetInt   ( key.m_pszString, val.m_int       ); break;
			case FIELD_BOOLEAN: pKV->SetBool  ( key.m_pszString, val.m_bool      ); break;
			case FIELD_CSTRING: pKV->SetString( key.m_pszString, val.m_pszString ); break;
		}

		g_pScriptVM->ReleaseValue(key);
		g_pScriptVM->ReleaseValue(val);
	}
}

//-----------------------------------------------------------------------------
// Retrieves a table from storage. Write into input table.
//-----------------------------------------------------------------------------
void CScriptSaveRestoreUtil::RestoreTable( const char *szId, HSCRIPT hTable )
{
	int idx = m_Lookup.Find( Hash(szId) );
	if ( idx == m_Lookup.InvalidIndex() )
	{
		// DevWarning( 2, "RestoreTable could not find saved table with context '%s'\n", szId );
		return;
	}

	KeyValues *pKV = m_Lookup[idx];
	FOR_EACH_SUBKEY( pKV, key )
	{
		switch ( key->GetDataType() )
		{
			case KeyValues::TYPE_STRING: g_pScriptVM->SetValue( hTable, key->GetName(), key->GetString() ); break;
			case KeyValues::TYPE_INT:    g_pScriptVM->SetValue( hTable, key->GetName(), key->GetInt()    ); break;
			case KeyValues::TYPE_FLOAT:  g_pScriptVM->SetValue( hTable, key->GetName(), key->GetFloat()  ); break;
		}
	}
}

//-----------------------------------------------------------------------------
// Remove a saved table.
//-----------------------------------------------------------------------------
void CScriptSaveRestoreUtil::ClearSavedTable( const char *szId )
{
	int idx = m_Lookup.Find( Hash(szId) );
	if ( idx != m_Lookup.InvalidIndex() )
	{
		m_Lookup[idx]->deleteThis();
		m_Lookup.RemoveAt( idx );
	}
	else
	{
		// DevWarning( 2, "ClearSavedTable could not find saved table with context '%s'\n", szId );
	}
}

//=============================================================================
// Read/Write to File
// Based on L4D2/Source 2 API
//=============================================================================
#define SCRIPT_MAX_FILE_READ_SIZE  (64 * 1024 * 1024)	// 64MB
#define SCRIPT_MAX_FILE_WRITE_SIZE (64 * 1024 * 1024)	// 64MB
#define SCRIPT_RW_PATH_ID "MOD"
#define SCRIPT_RW_FULL_PATH_FMT "vscript_io/%s"

class CScriptReadWriteFile : public CAutoGameSystem
{
	// A singleton class with all static members is used to be able to free the read string on level shutdown,
	// and register script funcs directly. Same reason applies to CScriptSaveRestoreUtil
public:
	static bool FileWrite( const char *szFile, const char *szInput );
	static const char *FileRead( const char *szFile );
	static bool FileExists( const char *szFile );

	// NOTE: These two functions are new with Mapbase and have no Valve equivalent
	static bool KeyValuesWrite( const char *szFile, HSCRIPT hInput );
	static HSCRIPT KeyValuesRead( const char *szFile );

	void LevelShutdownPostEntity()
	{
		if ( m_pszReturnReadFile )
		{
			delete[] m_pszReturnReadFile;
			m_pszReturnReadFile = NULL;
		}
	}

private:
	static char *m_pszReturnReadFile;

} g_ScriptReadWrite;

char *CScriptReadWriteFile::m_pszReturnReadFile = NULL;

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CScriptReadWriteFile::FileWrite( const char *szFile, const char *szInput )
{
	size_t len = strlen(szInput);
	if ( len > SCRIPT_MAX_FILE_WRITE_SIZE )
	{
		DevWarning( 2, "Input is too large for a ScriptFileWrite ( %s / %d MB )\n", V_pretifymem(len,2,true), (SCRIPT_MAX_FILE_WRITE_SIZE >> 20) );
		return false;
	}

	char pszFullName[MAX_PATH];
	V_snprintf( pszFullName, sizeof(pszFullName), SCRIPT_RW_FULL_PATH_FMT, szFile );

	if ( !V_RemoveDotSlashes( pszFullName, CORRECT_PATH_SEPARATOR, true ) )
	{
		DevWarning( 2, "Invalid file location : %s\n", szFile );
		return false;
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	buf.PutString(szInput);

	int nSize = V_strlen(pszFullName) + 1;
	char *pszDir = (char*)stackalloc(nSize);
	V_memcpy( pszDir, pszFullName, nSize );
	V_StripFilename( pszDir );

	g_pFullFileSystem->CreateDirHierarchy( pszDir, SCRIPT_RW_PATH_ID );
	bool res = g_pFullFileSystem->WriteFile( pszFullName, SCRIPT_RW_PATH_ID, buf );
	buf.Purge();
	return res;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
const char *CScriptReadWriteFile::FileRead( const char *szFile )
{
	char pszFullName[MAX_PATH];
	V_snprintf( pszFullName, sizeof(pszFullName), SCRIPT_RW_FULL_PATH_FMT, szFile );

	if ( !CommandLine()->FindParm( "-script_dotslash_read" ) && !V_RemoveDotSlashes( pszFullName, CORRECT_PATH_SEPARATOR, true ) )
	{
		DevWarning( 2, "Invalid file location : %s\n", szFile );
		return NULL;
	}

	unsigned int size = g_pFullFileSystem->Size( pszFullName, SCRIPT_RW_PATH_ID );
	if ( size >= SCRIPT_MAX_FILE_READ_SIZE )
	{
		DevWarning( 2, "File '%s' (from '%s') is too large for a ScriptFileRead ( %s / %u bytes )\n", pszFullName, szFile, V_pretifymem(size,2,true), SCRIPT_MAX_FILE_READ_SIZE );
		return NULL;
	}

	FileHandle_t file = g_pFullFileSystem->Open( pszFullName, "rb", SCRIPT_RW_PATH_ID );
	if ( !file )
	{
		return NULL;
	}

	// Close the previous buffer
	if (m_pszReturnReadFile)
		g_pFullFileSystem->FreeOptimalReadBuffer( m_pszReturnReadFile );

	unsigned bufSize = g_pFullFileSystem->GetOptimalReadSize( file, size + 2 );
	m_pszReturnReadFile = (char*)g_pFullFileSystem->AllocOptimalReadBuffer( file, bufSize );

	bool bRetOK = ( g_pFullFileSystem->ReadEx( m_pszReturnReadFile, bufSize, size, file ) != 0 );
	g_pFullFileSystem->Close( file );	// close file after reading

	if ( bRetOK )
	{
		m_pszReturnReadFile[size] = 0; // null terminate file as EOF
		//buffer[size+1] = 0; // double NULL terminating in case this is a unicode file
		return m_pszReturnReadFile;
	}
	else
	{
		return NULL;
	}
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CScriptReadWriteFile::FileExists( const char *szFile )
{
	char pszFullName[MAX_PATH];
	V_snprintf( pszFullName, sizeof(pszFullName), SCRIPT_RW_FULL_PATH_FMT, szFile );

	if ( !CommandLine()->FindParm( "-script_dotslash_read" ) && !V_RemoveDotSlashes( pszFullName, CORRECT_PATH_SEPARATOR, true ) )
	{
		DevWarning( 2, "Invalid file location : %s\n", szFile );
		return NULL;
	}

	return g_pFullFileSystem->FileExists( pszFullName, SCRIPT_RW_PATH_ID );
}

//-----------------------------------------------------------------------------
// Get the checksum of any file. Can be used to check the existence or validity of a file.
// Returns unsigned int as hex string.
//-----------------------------------------------------------------------------
/*
const char *CScriptReadWriteFile::CRC32_Checksum( const char *szFilename )
{
	CUtlBuffer buf( 0, 0, CUtlBuffer::READ_ONLY );
	if ( !g_pFullFileSystem->ReadFile( szFilename, NULL, buf ) )
		return NULL;

	// first time calling, allocate
	if ( !m_pszReturnCRC32 )
		m_pszReturnCRC32 = new char[9]; // 'FFFFFFFF\0'

	V_snprintf( const_cast<char*>(m_pszReturnCRC32), 9, "%X", CRC32_ProcessSingleBuffer( buf.Base(), buf.Size()-1 ) );
	buf.Purge();

	return m_pszReturnCRC32;
}
*/

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
bool CScriptReadWriteFile::KeyValuesWrite( const char *szFile, HSCRIPT hInput )
{
	KeyValues *pKV = scriptmanager->GetKeyValuesFromScriptKV( g_pScriptVM, hInput );
	if (!pKV)
	{
		return false;
	}

	CUtlBuffer buf( 0, 0, CUtlBuffer::TEXT_BUFFER );
	pKV->RecursiveSaveToFile( buf, 0 );

	if ( buf.Size() > SCRIPT_MAX_FILE_WRITE_SIZE )
	{
		DevWarning( 2, "Input is too large for a ScriptKeyValuesWrite ( %s / %d MB )\n", V_pretifymem(buf.Size(),2,true), (SCRIPT_MAX_FILE_WRITE_SIZE >> 20) );
		buf.Purge();
		return false;
	}

	char pszFullName[MAX_PATH];
	V_snprintf( pszFullName, sizeof(pszFullName), SCRIPT_RW_FULL_PATH_FMT, szFile );

	if ( !V_RemoveDotSlashes( pszFullName, CORRECT_PATH_SEPARATOR, true ) )
	{
		DevWarning( 2, "Invalid file location : %s\n", szFile );
		buf.Purge();
		return false;
	}

	int nSize = V_strlen(pszFullName) + 1;
	char *pszDir = (char*)stackalloc(nSize);
	V_memcpy( pszDir, pszFullName, nSize );
	V_StripFilename( pszDir );

	g_pFullFileSystem->CreateDirHierarchy( pszDir, SCRIPT_RW_PATH_ID );
	bool res = g_pFullFileSystem->WriteFile( pszFullName, SCRIPT_RW_PATH_ID, buf );
	buf.Purge();
	return res;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
HSCRIPT CScriptReadWriteFile::KeyValuesRead( const char *szFile )
{
	char pszFullName[MAX_PATH];
	V_snprintf( pszFullName, sizeof(pszFullName), SCRIPT_RW_FULL_PATH_FMT, szFile );

	if ( !CommandLine()->FindParm( "-script_dotslash_read" ) && !V_RemoveDotSlashes( pszFullName, CORRECT_PATH_SEPARATOR, true ) )
	{
		DevWarning( 2, "Invalid file location : %s\n", szFile );
		return NULL;
	}

	unsigned int size = g_pFullFileSystem->Size( pszFullName, SCRIPT_RW_PATH_ID );
	if ( size >= SCRIPT_MAX_FILE_READ_SIZE )
	{
		DevWarning( 2, "File '%s' (from '%s') is too large for a ScriptKeyValuesRead ( %s / %u bytes )\n", pszFullName, szFile, V_pretifymem(size,2,true), SCRIPT_MAX_FILE_READ_SIZE );
		return NULL;
	}

	KeyValues *pKV = new KeyValues( szFile );
	if ( !pKV->LoadFromFile( g_pFullFileSystem, pszFullName, SCRIPT_RW_PATH_ID ) )
	{
		pKV->deleteThis();
		return NULL;
	}

	HSCRIPT hScript = scriptmanager->CreateScriptKeyValues( g_pScriptVM, pKV, true ); // bAllowDestruct is supposed to automatically remove the involved KV

	return hScript;
}


//=============================================================================
// Network message helper
// (Unique to mapbase)
//
// Uses usermessages for server to client, UserCmd for client to server communication.
// The custom message name is hashed and sent as word with the message.
//=============================================================================

static CNetMsgScriptHelper scriptnetmsg;
CNetMsgScriptHelper *g_ScriptNetMsg = &scriptnetmsg;

#ifdef GAME_DLL
#define m_MsgIn_() m_MsgIn->
#define DLL_LOC_STR "[Server]"
#else
#define m_MsgIn_() m_MsgIn.
#define DLL_LOC_STR "[Client]"
#endif

#ifdef GAME_DLL
#define SCRIPT_NETMSG_WRITE_FUNC
#else
#define SCRIPT_NETMSG_WRITE_FUNC if ( m_bWriteIgnore ) { return; }
#endif

#ifdef _DEBUG
#ifdef GAME_DLL
ConVar script_net_debug("script_net_debug", "0");
#define DebugNetMsg( l, ... ) do { if (script_net_debug.GetInt() >= l) ConColorMsg( Color(100, 225, 255, 255), __VA_ARGS__ ); } while (0);
#else
ConVar script_net_debug("script_net_debug_client", "0");
#define DebugNetMsg( l, ... ) do { if (script_net_debug.GetInt() >= l) ConColorMsg( Color(100, 225, 175, 255), __VA_ARGS__ ); } while (0);
#endif
#define DebugWarning(...) Warning( __VA_ARGS__ )
#else
#define DebugNetMsg(...) (void)(0)
#define DebugWarning(...) (void)(0)
#endif


// Keep track of message names to print on failure
#ifdef _DEBUG
struct NetMsgHook_t
{
	void Set( const char *s )
	{
		hash = CNetMsgScriptHelper::Hash( s );
		name = strdup(s);
	}

	~NetMsgHook_t()
	{
		free( name );
	}

	int hash;
	char *name;
};

CUtlVector< NetMsgHook_t > g_NetMsgHooks;

static const char *GetNetMsgName( int hash )
{
	FOR_EACH_VEC( g_NetMsgHooks, i )
	{
		if ( g_NetMsgHooks[i].hash == hash )
			return g_NetMsgHooks[i].name;
	}
	return 0;
}

static const char *HasNetMsgCollision( int hash, const char *ignore )
{
	FOR_EACH_VEC( g_NetMsgHooks, i )
	{
		if ( g_NetMsgHooks[i].hash == hash && V_strcmp( g_NetMsgHooks[i].name, ignore ) != 0 )
		{
			return g_NetMsgHooks[i].name;
		}
	}
	return 0;
}
#endif // _DEBUG



inline int CNetMsgScriptHelper::Hash( const char *key )
{
	int hash = CaselessStringHashFunctor()( key );
	return hash;
}

void CNetMsgScriptHelper::WriteToBuffer( bf_write *bf )
{
#ifdef CLIENT_DLL
	Assert( m_nQueueCount < ( 1 << SCRIPT_NETMSG_QUEUE_BITS ) );
	bf->WriteUBitLong( m_nQueueCount, SCRIPT_NETMSG_QUEUE_BITS );

	DebugNetMsg( 2, DLL_LOC_STR " CNetMsgScriptHelper::WriteToBuffer() count(%d) size(%d)\n",
		m_nQueueCount, m_MsgOut.GetNumBitsWritten() + SCRIPT_NETMSG_QUEUE_BITS );
#endif

	bf->WriteBits( m_MsgOut.GetData(), m_MsgOut.GetNumBitsWritten() );
}

//-----------------------------------------------------------------------------
// Reset the current network message buffer
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::Reset()
{
	m_MsgOut.StartWriting( m_MsgData, sizeof(m_MsgData), 0 );
#ifdef GAME_DLL
	m_filter.Reset();
#else
	m_iLastBit = 0;
#endif
}

//-----------------------------------------------------------------------------
// Create the storage for the receiver callback functions.
// Functions are handled in the VM, the storage table is here.
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::InitPostVM()
{
	ScriptVariant_t hHooks;
	g_pScriptVM->CreateTable( hHooks );
	m_Hooks = (HSCRIPT)hHooks;
}

void CNetMsgScriptHelper::LevelShutdownPreVM()
{
	Reset();
	if ( m_Hooks )
		g_pScriptVM->ReleaseScript( m_Hooks );
	m_Hooks = NULL;

#ifdef CLIENT_DLL
	m_bWriteReady = m_bWriteIgnore = false;
	m_MsgIn.Reset();
#else
	m_MsgIn = NULL;
#endif

#ifdef _DEBUG
	g_NetMsgHooks.Purge();
#endif
}

#ifdef CLIENT_DLL

bool CNetMsgScriptHelper::Init() // IGameSystem
{
	usermessages->HookMessage( "ScriptMsg", __MsgFunc_ScriptMsg );
	return true;
}

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::__MsgFunc_ScriptMsg( bf_read &msg )
{
	g_ScriptNetMsg->ReceiveMessage( msg );
}

#endif // CLIENT_DLL

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
#ifdef GAME_DLL
void CNetMsgScriptHelper::ReceiveMessage( bf_read *msg, CBaseEntity *pPlayer )
{
	m_MsgIn = msg;
#else
void CNetMsgScriptHelper::ReceiveMessage( bf_read &msg )
{
	m_MsgIn.StartReading( msg.m_pData, msg.m_nDataBytes );
#endif

	DebugNetMsg( 2, DLL_LOC_STR " %s()\n", __FUNCTION__ );

	// Don't do anything if there's no VM here. This can happen if a message from the server goes to a VM-less client, or vice versa.
	if ( !g_pScriptVM )
	{
		CGWarning( 0, CON_GROUP_VSCRIPT, DLL_LOC_STR " CNetMsgScriptHelper: No VM on receiving side\n" );
		return;
	}

#ifdef GAME_DLL
	int count = m_MsgIn_()ReadUBitLong( SCRIPT_NETMSG_QUEUE_BITS );
	DebugNetMsg( 2, "  msg count %d\n", count );
	while ( count-- )
#endif
	{
		int hash = m_MsgIn_()ReadUBitLong( SCRIPT_NETMSG_HEADER_BITS );

#ifdef _DEBUG
		const char *msgName = GetNetMsgName( hash );
		DebugNetMsg( 2, "  -- begin msg [%d]%s\n", hash, msgName );
#endif

		ScriptVariant_t hfn;
		if ( g_pScriptVM->GetValue( m_Hooks, hash, &hfn ) )
		{
#ifdef GAME_DLL
			if ( g_pScriptVM->Call( hfn, NULL, true, NULL, pPlayer->m_hScriptInstance ) == SCRIPT_ERROR )
#else
			if ( g_pScriptVM->ExecuteFunction( hfn, NULL, 0, NULL, NULL, true ) == SCRIPT_ERROR )
#endif
			{
#ifdef _DEBUG
				DevWarning( 1, DLL_LOC_STR " NetMsg: invalid callback '%s'\n", GetNetMsgName( hash ) );
#else
				DevWarning( 1, DLL_LOC_STR " NetMsg: invalid callback [%d]\n", hash );
#endif
			}
			g_pScriptVM->ReleaseValue( hfn );
		}
		else
		{
			DevWarning( 1, DLL_LOC_STR " NetMsg hook not found [%d]\n", hash );
		}

		DebugNetMsg( 2, "  -- end msg\n" );
	}
}

//-----------------------------------------------------------------------------
// Start writing new custom network message
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::Start( const char *msg )
{
	if ( !msg || !msg[0] )
	{
		g_pScriptVM->RaiseException( DLL_LOC_STR "NetMsg: invalid message name" );
		return;
	}

	DebugNetMsg( 1, DLL_LOC_STR " %s() [%d]%s\n", __FUNCTION__, Hash( msg ), msg );

#ifdef CLIENT_DLL
	// Client can write multiple messages in a frame before the usercmd is sent,
	// this queue system ensures client messages are written to the cmd all at once.
	// NOTE: All messages share the same buffer.
	if ( !m_bWriteReady )
	{
		Reset();
		m_nQueueCount = 0;
		m_bWriteIgnore = false;
	}
	else if ( m_nQueueCount == ((1<<SCRIPT_NETMSG_QUEUE_BITS)-1) )
	{
		Warning( DLL_LOC_STR " NetMsg queue is full, cannot write '%s'!\n", msg );

		m_bWriteIgnore = true;
		return;
	}

	++m_nQueueCount;
#else
	Reset();
#endif

	m_MsgOut.WriteUBitLong( Hash( msg ), SCRIPT_NETMSG_HEADER_BITS );
}

#ifdef GAME_DLL
//-----------------------------------------------------------------------------
// server -> client
//
// Sends an exclusive usermessage.
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::Send( HSCRIPT player, bool bReliable )
{
	DebugNetMsg( 1, DLL_LOC_STR " %s() size(%d)\n", __FUNCTION__, GetNumBitsWritten() );

	CBaseEntity *pPlayer = ToEnt(player);
	if ( pPlayer )
	{
		m_filter.AddRecipient( (CBasePlayer*)pPlayer );
	}

	if ( bReliable )
	{
		m_filter.MakeReliable();
	}

	Assert( usermessages->LookupUserMessage( "ScriptMsg" ) != -1 );

	DoSendUserMsg( &m_filter, usermessages->LookupUserMessage( "ScriptMsg" ) );
}
#else // CLIENT_DLL
//-----------------------------------------------------------------------------
// client -> server
//
// Mark UserCmd delta ready.
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::Send()
{
	DebugNetMsg( 1, DLL_LOC_STR " %s() size(%d)\n", __FUNCTION__, m_bWriteIgnore ? 0 : GetNumBitsWritten() );

	m_bWriteReady = true;
}
#endif

//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::Receive( const char *msg, HSCRIPT func )
{
	if ( !msg || !msg[0] )
	{
		g_pScriptVM->RaiseException( DLL_LOC_STR "NetMsg: invalid message name" );
		return;
	}

#ifdef _DEBUG
	int hash = Hash( msg );

	const char *psz = HasNetMsgCollision( hash, msg );
	AssertMsg3( !psz, DLL_LOC_STR " NetMsg hash collision! [%d] '%s', '%s'\n", hash, msg, psz );

	NetMsgHook_t &hook = g_NetMsgHooks[ g_NetMsgHooks.AddToTail() ];
	hook.Set( msg );
#endif

	if ( func )
	{
		g_pScriptVM->SetValue( m_Hooks, Hash( msg ), func );
	}
	else
	{
		g_pScriptVM->ClearValue( m_Hooks, Hash( msg ) );
	}
}

#ifdef GAME_DLL
void CNetMsgScriptHelper::DoSendUserMsg( CRecipientFilter *filter, int type )
{
	WriteToBuffer( engine->UserMessageBegin( filter, type ) );
	engine->MessageEnd();
}

void CNetMsgScriptHelper::DoSendEntityMsg( CBaseEntity *entity, bool reliable )
{
	WriteToBuffer( engine->EntityMessageBegin( entity->entindex(), entity->GetServerClass(), reliable ) );
	engine->MessageEnd();
}

//-----------------------------------------------------------------------------
// Send a usermessage from the server to the client
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::SendUserMessage( HSCRIPT hPlayer, const char *msg, bool bReliable )
{
	int msg_type = usermessages->LookupUserMessage(msg);
	if ( msg_type == -1 )
	{
		g_pScriptVM->RaiseException( UTIL_VarArgs("SendUserMessage: Unregistered message '%s'", msg) );
		return;
	}

	CBaseEntity *pPlayer = ToEnt(hPlayer);
	if ( pPlayer )
	{
		m_filter.AddRecipient( (CBasePlayer*)pPlayer );
	}

	if ( bReliable )
	{
		m_filter.MakeReliable();
	}

	DoSendUserMsg( &m_filter, msg_type );
}

//-----------------------------------------------------------------------------
// Send a message from a server side entity to its client side counterpart
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::SendEntityMessage( HSCRIPT hEnt, bool bReliable )
{
	CBaseEntity *entity = ToEnt(hEnt);
	if ( !entity )
	{
		g_pScriptVM->RaiseException("SendEntityMessage: invalid entity");
		return;
	}

	DoSendEntityMsg( entity, bReliable );
}
#else
//-----------------------------------------------------------------------------
// Dispatch a usermessage on client
//-----------------------------------------------------------------------------
void CNetMsgScriptHelper::DispatchUserMessage( const char *msg )
{
	bf_read buffer( m_MsgOut.GetData(), m_MsgOut.GetNumBytesWritten() );
	usermessages->DispatchUserMessage( usermessages->LookupUserMessage(msg), buffer );
}
#endif // GAME_DLL

void CNetMsgScriptHelper::WriteInt( int iValue, int bits )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteSBitLong( iValue, bits );
}

void CNetMsgScriptHelper::WriteUInt( int iValue, int bits )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteUBitLong( iValue, bits );
}

void CNetMsgScriptHelper::WriteByte( int iValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteByte( iValue );
}

void CNetMsgScriptHelper::WriteChar( int iValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteChar( iValue );
}

void CNetMsgScriptHelper::WriteShort( int iValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteShort( iValue );
}

void CNetMsgScriptHelper::WriteWord( int iValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteWord( iValue );
}

void CNetMsgScriptHelper::WriteLong( int iValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteLong( iValue );
}

void CNetMsgScriptHelper::WriteFloat( float flValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteFloat( flValue );
}

void CNetMsgScriptHelper::WriteNormal( float flValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteBitNormal( flValue );
}

void CNetMsgScriptHelper::WriteAngle( float flValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteBitAngle( flValue, 8 );
}

void CNetMsgScriptHelper::WriteCoord( float flValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteBitCoord( flValue );
}

void CNetMsgScriptHelper::WriteVec3Coord( const Vector& rgflValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteBitVec3Coord( rgflValue );
}

void CNetMsgScriptHelper::WriteVec3Normal( const Vector& rgflValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteBitVec3Normal( rgflValue );
}

void CNetMsgScriptHelper::WriteAngles( const QAngle& rgflValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteBitAngles( rgflValue );
}

void CNetMsgScriptHelper::WriteString( const char *sz )
{
	SCRIPT_NETMSG_WRITE_FUNC

	// Larger strings can be written but cannot be read
	Assert( V_strlen(sz) < SCRIPT_NETMSG_STRING_SIZE );

	m_MsgOut.WriteString( sz );
}

void CNetMsgScriptHelper::WriteBool( bool bValue )
{
	SCRIPT_NETMSG_WRITE_FUNC
	m_MsgOut.WriteOneBit( bValue ? 1 : 0 );
}

void CNetMsgScriptHelper::WriteEntity( HSCRIPT hEnt )
{
	SCRIPT_NETMSG_WRITE_FUNC
	CBaseEntity *p = ToEnt(hEnt);
	int i = p ? p->entindex() : 0;
	m_MsgOut.WriteUBitLong( i, MAX_EDICT_BITS );
}

void CNetMsgScriptHelper::WriteEHandle( HSCRIPT hEnt )
{
	SCRIPT_NETMSG_WRITE_FUNC
	CBaseEntity *pEnt = ToEnt( hEnt );
	long iEncodedEHandle;
	if ( pEnt )
	{
		EHANDLE hEnt = pEnt;
		int iSerialNum = hEnt.GetSerialNumber() & (1 << NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS) - 1;
		iEncodedEHandle = hEnt.GetEntryIndex() | (iSerialNum << MAX_EDICT_BITS);
	}
	else
	{
		iEncodedEHandle = INVALID_NETWORKED_EHANDLE_VALUE;
	}
	m_MsgOut.WriteLong( iEncodedEHandle );
}

int CNetMsgScriptHelper::ReadInt( int bits )
{
	return m_MsgIn_()ReadSBitLong(bits);
}

int CNetMsgScriptHelper::ReadUInt( int bits )
{
	return m_MsgIn_()ReadUBitLong(bits);
}

int CNetMsgScriptHelper::ReadByte()
{
	return m_MsgIn_()ReadByte();
}

int CNetMsgScriptHelper::ReadChar()
{
	return m_MsgIn_()ReadChar();
}

int CNetMsgScriptHelper::ReadShort()
{
	return m_MsgIn_()ReadShort();
}

int CNetMsgScriptHelper::ReadWord()
{
	return m_MsgIn_()ReadWord();
}

int CNetMsgScriptHelper::ReadLong()
{
	return m_MsgIn_()ReadLong();
}

float CNetMsgScriptHelper::ReadFloat()
{
	return m_MsgIn_()ReadFloat();
}

float CNetMsgScriptHelper::ReadNormal()
{
	return m_MsgIn_()ReadBitNormal();
}

float CNetMsgScriptHelper::ReadAngle()
{
	return m_MsgIn_()ReadBitAngle( 8 );
}

float CNetMsgScriptHelper::ReadCoord()
{
	return m_MsgIn_()ReadBitCoord();
}

const Vector& CNetMsgScriptHelper::ReadVec3Coord()
{
	static Vector vec3;
	m_MsgIn_()ReadBitVec3Coord(vec3);
	return vec3;
}

const Vector& CNetMsgScriptHelper::ReadVec3Normal()
{
	static Vector vec3;
	m_MsgIn_()ReadBitVec3Normal(vec3);
	return vec3;
}

const QAngle& CNetMsgScriptHelper::ReadAngles()
{
	static QAngle vec3;
	m_MsgIn_()ReadBitAngles(vec3);
	return vec3;
}

const char* CNetMsgScriptHelper::ReadString()
{
	static char buf[ SCRIPT_NETMSG_STRING_SIZE ];
	m_MsgIn_()ReadString( buf, sizeof(buf) );
	return buf;
}

bool CNetMsgScriptHelper::ReadBool()
{
	return m_MsgIn_()ReadOneBit();
}

HSCRIPT CNetMsgScriptHelper::ReadEntity()
{
	int index = m_MsgIn_()ReadUBitLong( MAX_EDICT_BITS );

	if ( !index )
		return NULL;

#ifdef GAME_DLL
	edict_t *e = INDEXENT(index);
	if ( e && !e->IsFree() )
	{
		return ToHScript( GetContainingEntity(e) );
	}
#else // CLIENT_DLL
	if ( index < NUM_ENT_ENTRIES )
	{
		return ToHScript( CBaseEntity::Instance(index) );
	}
#endif
	return NULL;
}

HSCRIPT CNetMsgScriptHelper::ReadEHandle()
{
	int iEncodedEHandle = m_MsgIn_()ReadLong();
	if ( iEncodedEHandle == INVALID_NETWORKED_EHANDLE_VALUE )
		return NULL;
	int iEntry = iEncodedEHandle & ( (1 << MAX_EDICT_BITS) - 1 );
	int iSerialNum = iEncodedEHandle >> MAX_EDICT_BITS;
	return ToHScript( EHANDLE( iEntry, iSerialNum ) );
}

inline int CNetMsgScriptHelper::GetNumBitsWritten()
{
#ifdef GAME_DLL
	return m_MsgOut.GetNumBitsWritten() - SCRIPT_NETMSG_HEADER_BITS;
#else
	return m_MsgOut.m_iCurBit - m_iLastBit - SCRIPT_NETMSG_HEADER_BITS;
#endif
}


BEGIN_SCRIPTDESC_ROOT_NAMED( CNetMsgScriptHelper, "CNetMsg", SCRIPT_SINGLETON "Network messages" )

#ifdef GAME_DLL
	DEFINE_SCRIPTFUNC( SendUserMessage, "Send a usermessage from the server to the client" )
	DEFINE_SCRIPTFUNC( SendEntityMessage, "Send a message from a server side entity to its client side counterpart" )

	// TODO: multiplayer
#else
	DEFINE_SCRIPTFUNC( DispatchUserMessage, "Dispatch a usermessage on client" )
#endif

	DEFINE_SCRIPTFUNC( Reset, "Reset the current network message buffer" )
	DEFINE_SCRIPTFUNC( Start, "Start writing new custom network message" )
	DEFINE_SCRIPTFUNC( Receive, "Set custom network message callback" )
	DEFINE_SCRIPTFUNC_NAMED( Receive, "Recieve", SCRIPT_HIDE ) // This was a typo until v6.3
#ifdef GAME_DLL
	DEFINE_SCRIPTFUNC( Send, "Send a custom network message from the server to the client (max 251 bytes)" )
#else
	DEFINE_SCRIPTFUNC( Send, "Send a custom network message from the client to the server (max 2044 bytes)" )
#endif

	DEFINE_SCRIPTFUNC( WriteInt, "variable bit signed int" )
	DEFINE_SCRIPTFUNC( WriteUInt, "variable bit unsigned int" )
	DEFINE_SCRIPTFUNC( WriteByte, "8 bit unsigned char" )
	DEFINE_SCRIPTFUNC( WriteChar, "8 bit char" )
	DEFINE_SCRIPTFUNC( WriteShort, "16 bit short" )
	DEFINE_SCRIPTFUNC( WriteWord, "16 bit unsigned short" )
	DEFINE_SCRIPTFUNC( WriteLong, "32 bit long" )
	DEFINE_SCRIPTFUNC( WriteFloat, "32 bit float" )
	DEFINE_SCRIPTFUNC( WriteNormal, "12 bit" )
	DEFINE_SCRIPTFUNC( WriteAngle, "8 bit unsigned char" )
	DEFINE_SCRIPTFUNC( WriteCoord, "" )
	DEFINE_SCRIPTFUNC( WriteVec3Coord, "" )
	DEFINE_SCRIPTFUNC( WriteVec3Normal, "27 bit" )
	DEFINE_SCRIPTFUNC( WriteAngles, "" )
	DEFINE_SCRIPTFUNC( WriteString, "max 512 bytes at once" )
	DEFINE_SCRIPTFUNC( WriteBool, "1 bit" )
	DEFINE_SCRIPTFUNC( WriteEntity, "11 bit (entindex)" )
	DEFINE_SCRIPTFUNC( WriteEHandle, "32 bit long" )

	DEFINE_SCRIPTFUNC( ReadInt, "" )
	DEFINE_SCRIPTFUNC( ReadUInt, "" )
	DEFINE_SCRIPTFUNC( ReadByte, "" )
	DEFINE_SCRIPTFUNC( ReadChar, "" )
	DEFINE_SCRIPTFUNC( ReadShort, "" )
	DEFINE_SCRIPTFUNC( ReadWord, "" )
	DEFINE_SCRIPTFUNC( ReadLong, "" )
	DEFINE_SCRIPTFUNC( ReadFloat, "" )
	DEFINE_SCRIPTFUNC( ReadNormal, "" )
	DEFINE_SCRIPTFUNC( ReadAngle, "" )
	DEFINE_SCRIPTFUNC( ReadCoord, "" )
	DEFINE_SCRIPTFUNC( ReadVec3Coord, "" )
	DEFINE_SCRIPTFUNC( ReadVec3Normal, "" )
	DEFINE_SCRIPTFUNC( ReadAngles, "" )
	DEFINE_SCRIPTFUNC( ReadString, "" )
	DEFINE_SCRIPTFUNC( ReadBool, "" )
	DEFINE_SCRIPTFUNC( ReadEntity, "" )
	DEFINE_SCRIPTFUNC( ReadEHandle, "" )

	DEFINE_SCRIPTFUNC( GetNumBitsWritten, "" )

END_SCRIPTDESC();



#define RETURN_IF_CANNOT_DRAW_OVERLAY\
	if (engine->IsPaused())\
		return;
class CDebugOverlayScriptHelper
{
public:

	void Box( const Vector &origin, const Vector &mins, const Vector &maxs, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddBoxOverlay(origin, mins, maxs, vec3_angle, r, g, b, a, flDuration);
	}
	void BoxDirection( const Vector &origin, const Vector &mins, const Vector &maxs, const Vector &forward, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		QAngle f_angles = vec3_angle;
		f_angles.y = UTIL_VecToYaw(forward);

		debugoverlay->AddBoxOverlay(origin, mins, maxs, f_angles, r, g, b, a, flDuration);
	}
	void BoxAngles( const Vector &origin, const Vector &mins, const Vector &maxs, const QAngle &angles, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddBoxOverlay(origin, mins, maxs, angles, r, g, b, a, flDuration);
	}
	void SweptBox( const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, const QAngle & angles, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddSweptBoxOverlay(start, end, mins, maxs, angles, r, g, b, a, flDuration);
	}
	void EntityBounds( HSCRIPT pEntity, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		CBaseEntity *pEnt = ToEnt(pEntity);
		if (!pEnt)
			return;

		const CCollisionProperty *pCollide = pEnt->CollisionProp();
		debugoverlay->AddBoxOverlay(pCollide->GetCollisionOrigin(), pCollide->OBBMins(), pCollide->OBBMaxs(), pCollide->GetCollisionAngles(), r, g, b, a, flDuration);
	}
	void Line( const Vector &origin, const Vector &target, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddLineOverlay(origin, target, r, g, b, noDepthTest, flDuration);
	}
	void Triangle( const Vector &p1, const Vector &p2, const Vector &p3, int r, int g, int b, int a, bool noDepthTest, float duration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddTriangleOverlay(p1, p2, p3, r, g, b, a, noDepthTest, duration);
	}
	void EntityText( int entityID, int text_offset, const char *text, float flDuration, int r, int g, int b, int a )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddEntityTextOverlay(entityID, text_offset, flDuration,
				(int)clamp(r * 255.f, 0.f, 255.f), (int)clamp(g * 255.f, 0.f, 255.f), (int)clamp(b * 255.f, 0.f, 255.f),
				(int)clamp(a * 255.f, 0.f, 255.f), text);
	}
	void EntityTextAtPosition( const Vector &origin, int text_offset, const char *text, float flDuration, int r, int g, int b, int a )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddTextOverlayRGB(origin, text_offset, flDuration, r, g, b, a, "%s", text);
	}
	void Grid( const Vector &vPosition )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddGridOverlay(vPosition);
	}
	void Text( const Vector &origin, const char *text, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddTextOverlay(origin, flDuration, "%s", text);
	}
	void ScreenText( float fXpos, float fYpos, const char *text, int r, int g, int b, int a, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		debugoverlay->AddScreenTextOverlay(fXpos, fYpos, flDuration, r, g, b, a, text);
	}
	void Cross3D( const Vector &position, float size, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Line( position + Vector(size,0,0), position - Vector(size,0,0), r, g, b, noDepthTest, flDuration );
		Line( position + Vector(0,size,0), position - Vector(0,size,0), r, g, b, noDepthTest, flDuration );
		Line( position + Vector(0,0,size), position - Vector(0,0,size), r, g, b, noDepthTest, flDuration );
	}
	void Cross3DOriented( const Vector &position, const QAngle &angles, float size, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector forward, right, up;
		AngleVectors( angles, &forward, &right, &up );

		forward *= size;
		right *= size;
		up *= size;

		Line( position + right, position - right, r, g, b, noDepthTest, flDuration );
		Line( position + forward, position - forward, r, g, b, noDepthTest, flDuration );
		Line( position + up, position - up, r, g, b, noDepthTest, flDuration );
	}
	void DrawTickMarkedLine( const Vector &startPos, const Vector &endPos, float tickDist, int tickTextDist, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector	lineDir = (endPos - startPos);
		float	lineDist = VectorNormalize(lineDir);
		int		numTicks = lineDist / tickDist;

		Vector  upVec = Vector(0,0,4);
		Vector	sideDir;
		Vector	tickPos = startPos;
		int		tickTextCnt = 0;

		CrossProduct(lineDir, upVec, sideDir);

		Line(startPos, endPos, r, g, b, noDepthTest, flDuration);

		for (int i = 0; i<numTicks + 1; i++)
		{
			Vector tickLeft = tickPos - sideDir;
			Vector tickRight = tickPos + sideDir;

			if (tickTextCnt == tickTextDist)
			{
				char text[25];
				Q_snprintf(text, sizeof(text), "%i", i);
				Vector textPos = tickLeft + Vector(0, 0, 8);
				Line(tickLeft, tickRight, 255, 255, 255, noDepthTest, flDuration);
				Text(textPos, text, flDuration);
				tickTextCnt = 0;
			}
			else
			{
				Line(tickLeft, tickRight, r, g, b, noDepthTest, flDuration);
			}

			tickTextCnt++;

			tickPos = tickPos + (tickDist * lineDir);
		}
	}
	void HorzArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector	lineDir		= (endPos - startPos);
		VectorNormalize( lineDir );
		Vector  upVec		= Vector( 0, 0, 1 );
		Vector	sideDir;
		float   radius		= width / 2.0;

		CrossProduct(lineDir, upVec, sideDir);

		Vector p1 =	startPos - sideDir * radius;
		Vector p2 = endPos - lineDir * width - sideDir * radius;
		Vector p3 = endPos - lineDir * width - sideDir * width;
		Vector p4 = endPos;
		Vector p5 = endPos - lineDir * width + sideDir * width;
		Vector p6 = endPos - lineDir * width + sideDir * radius;
		Vector p7 =	startPos + sideDir * radius;

		Line(p1, p2, r,g,b,noDepthTest,flDuration);
		Line(p2, p3, r,g,b,noDepthTest,flDuration);
		Line(p3, p4, r,g,b,noDepthTest,flDuration);
		Line(p4, p5, r,g,b,noDepthTest,flDuration);
		Line(p5, p6, r,g,b,noDepthTest,flDuration);
		Line(p6, p7, r,g,b,noDepthTest,flDuration);

		if ( a > 0 )
		{
			Triangle( p5, p4, p3, r, g, b, a, noDepthTest, flDuration );
			Triangle( p1, p7, p6, r, g, b, a, noDepthTest, flDuration );
			Triangle( p6, p2, p1, r, g, b, a, noDepthTest, flDuration );

			Triangle( p3, p4, p5, r, g, b, a, noDepthTest, flDuration );
			Triangle( p6, p7, p1, r, g, b, a, noDepthTest, flDuration );
			Triangle( p1, p2, p6, r, g, b, a, noDepthTest, flDuration );
		}
	}
	void YawArrow( const Vector &startPos, float yaw, float length, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector forward = UTIL_YawToVector( yaw );
		HorzArrow( startPos, startPos + forward * length, width, r, g, b, a, noDepthTest, flDuration );
	}
	void VertArrow( const Vector &startPos, const Vector &endPos, float width, int r, int g, int b, int a, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector	lineDir		= (endPos - startPos);
		VectorNormalize( lineDir );
		Vector  upVec;
		Vector	sideDir;
		float   radius		= width / 2.0;

		VectorVectors( lineDir, sideDir, upVec );

		Vector p1 =	startPos - upVec * radius;
		Vector p2 = endPos - lineDir * width - upVec * radius;
		Vector p3 = endPos - lineDir * width - upVec * width;
		Vector p4 = endPos;
		Vector p5 = endPos - lineDir * width + upVec * width;
		Vector p6 = endPos - lineDir * width + upVec * radius;
		Vector p7 =	startPos + upVec * radius;

		Line(p1, p2, r,g,b,noDepthTest,flDuration);
		Line(p2, p3, r,g,b,noDepthTest,flDuration);
		Line(p3, p4, r,g,b,noDepthTest,flDuration);
		Line(p4, p5, r,g,b,noDepthTest,flDuration);
		Line(p5, p6, r,g,b,noDepthTest,flDuration);
		Line(p6, p7, r,g,b,noDepthTest,flDuration);

		if ( a > 0 )
		{
			Triangle( p5, p4, p3, r, g, b, a, noDepthTest, flDuration );
			Triangle( p1, p7, p6, r, g, b, a, noDepthTest, flDuration );
			Triangle( p6, p2, p1, r, g, b, a, noDepthTest, flDuration );

			Triangle( p3, p4, p5, r, g, b, a, noDepthTest, flDuration );
			Triangle( p6, p7, p1, r, g, b, a, noDepthTest, flDuration );
			Triangle( p1, p2, p6, r, g, b, a, noDepthTest, flDuration );
		}
	}
	void Axis( const Vector &position, const QAngle &angles, float size, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector xvec, yvec, zvec;
		AngleVectors( angles, &xvec, &yvec, &zvec );

		xvec = position + (size * xvec);
		yvec = position - (size * yvec);
		zvec = position + (size * zvec);

		Line( position, xvec, 255, 0, 0, noDepthTest, flDuration );
		Line( position, yvec, 0, 255, 0, noDepthTest, flDuration );
		Line( position, zvec, 0, 0, 255, noDepthTest, flDuration );
	}
	void Sphere( const Vector &center, float radius, int r, int g, int b, bool noDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		Vector edge, lastEdge;

		float axisSize = radius;
		Line( center + Vector( 0, 0, -axisSize ), center + Vector( 0, 0, axisSize ), r, g, b, noDepthTest, flDuration );
		Line( center + Vector( 0, -axisSize, 0 ), center + Vector( 0, axisSize, 0 ), r, g, b, noDepthTest, flDuration );
		Line( center + Vector( -axisSize, 0, 0 ), center + Vector( axisSize, 0, 0 ), r, g, b, noDepthTest, flDuration );

		lastEdge = Vector( radius + center.x, center.y, center.z );
		float angle;
		for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
		{
			edge.x = radius * cosf( angle / 180.0f * M_PI ) + center.x;
			edge.y = center.y;
			edge.z = radius * sinf( angle / 180.0f * M_PI ) + center.z;

			Line( edge, lastEdge, r, g, b, noDepthTest, flDuration );

			lastEdge = edge;
		}

		lastEdge = Vector( center.x, radius + center.y, center.z );
		for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
		{
			edge.x = center.x;
			edge.y = radius * cosf( angle / 180.0f * M_PI ) + center.y;
			edge.z = radius * sinf( angle / 180.0f * M_PI ) + center.z;

			Line( edge, lastEdge, r, g, b, noDepthTest, flDuration );

			lastEdge = edge;
		}

		lastEdge = Vector( center.x, radius + center.y, center.z );
		for( angle=0.0f; angle <= 360.0f; angle += 22.5f )
		{
			edge.x = radius * cosf( angle / 180.0f * M_PI ) + center.x;
			edge.y = radius * sinf( angle / 180.0f * M_PI ) + center.y;
			edge.z = center.z;

			Line( edge, lastEdge, r, g, b, noDepthTest, flDuration );

			lastEdge = edge;
		}
	}
	void CircleOriented( const Vector &position, const QAngle &angles, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		matrix3x4_t xform;
		AngleMatrix(angles, position, xform);
		Vector xAxis, yAxis;
		MatrixGetColumn(xform, 2, xAxis);
		MatrixGetColumn(xform, 1, yAxis);
		Circle(position, xAxis, yAxis, radius, r, g, b, a, bNoDepthTest, flDuration);
	}
	void Circle( const Vector &position, const Vector &xAxis, const Vector &yAxis, float radius, int r, int g, int b, int a, bool bNoDepthTest, float flDuration )
	{
		RETURN_IF_CANNOT_DRAW_OVERLAY

		const unsigned int nSegments = 16;
		const float flRadStep = (M_PI*2.0f) / (float) nSegments;

		Vector vecLastPosition;
		Vector vecStart = position + xAxis * radius;
		Vector vecPosition = vecStart;

		for ( int i = 1; i <= nSegments; i++ )
		{
			vecLastPosition = vecPosition;

			float flSin, flCos;
			SinCos( flRadStep*i, &flSin, &flCos );
			vecPosition = position + (xAxis * flCos * radius) + (yAxis * flSin * radius);

			Line( vecLastPosition, vecPosition, r, g, b, bNoDepthTest, flDuration );

			if ( a && i > 1 )
			{		
				debugoverlay->AddTriangleOverlay( vecStart, vecLastPosition, vecPosition, r, g, b, a, bNoDepthTest, flDuration );
			}
		}
	}
#ifndef CLIENT_DLL
	void SetDebugBits( HSCRIPT hEntity, int bit ) // DebugOverlayBits_t
	{
		CBaseEntity *pEnt = ToEnt(hEntity);
		if (!pEnt)
			return;

		if (pEnt->m_debugOverlays & bit)
		{
			pEnt->m_debugOverlays &= ~bit;
		}
		else
		{
			pEnt->m_debugOverlays |= bit;

#ifdef AI_MONITOR_FOR_OSCILLATION
			if (pEnt->IsNPC())
			{
				pEnt->MyNPCPointer()->m_ScheduleHistory.RemoveAll();
			}
#endif//AI_MONITOR_FOR_OSCILLATION
		}
	}
#endif
	void ClearAllOverlays()
	{
#ifndef CLIENT_DLL
		// Clear all entities of their debug overlays
		for (CBaseEntity *pEntity = gEntList.FirstEnt(); pEntity; pEntity = gEntList.NextEnt(pEntity))
		{
			pEntity->m_debugOverlays = 0;
		}
#endif

		debugoverlay->ClearAllOverlays();
	}

private:
} g_ScriptDebugOverlay;

BEGIN_SCRIPTDESC_ROOT( CDebugOverlayScriptHelper, SCRIPT_SINGLETON "CDebugOverlayScriptHelper" )
	DEFINE_SCRIPTFUNC( Box, "Draws a world-space axis-aligned box. Specify bounds in world space." )
	DEFINE_SCRIPTFUNC( BoxDirection, "Draw box oriented to a Vector direction" )
	DEFINE_SCRIPTFUNC( BoxAngles, "Draws an oriented box at the origin. Specify bounds in local space." )
	DEFINE_SCRIPTFUNC( SweptBox, "Draws a swept box. Specify endpoints in world space and the bounds in local space." )
	DEFINE_SCRIPTFUNC( EntityBounds, "Draws bounds of an entity" )
	DEFINE_SCRIPTFUNC( Line, "Draws a line between two points" )
	DEFINE_SCRIPTFUNC( Triangle, "Draws a filled triangle. Specify vertices in world space." )
	DEFINE_SCRIPTFUNC( EntityText, "Draws text on an entity" )
	DEFINE_SCRIPTFUNC( EntityTextAtPosition, "Draw entity text overlay at a specific position" )
	DEFINE_SCRIPTFUNC( Grid, "Add grid overlay" )
	DEFINE_SCRIPTFUNC( Text, "Draws 2D text. Specify origin in world space." )
	DEFINE_SCRIPTFUNC( ScreenText, "Draws 2D text. Specify coordinates in screen space." )
	DEFINE_SCRIPTFUNC( Cross3D, "Draws a world-aligned cross. Specify origin in world space." )
	DEFINE_SCRIPTFUNC( Cross3DOriented, "Draws an oriented cross. Specify origin in world space." )
	DEFINE_SCRIPTFUNC( DrawTickMarkedLine, "Draws a dashed line. Specify endpoints in world space." )
	DEFINE_SCRIPTFUNC( HorzArrow, "Draws a horizontal arrow. Specify endpoints in world space." )
	DEFINE_SCRIPTFUNC( YawArrow, "Draws a arrow associated with a specific yaw. Specify endpoints in world space." )
	DEFINE_SCRIPTFUNC( VertArrow, "Draws a vertical arrow. Specify endpoints in world space." )
	DEFINE_SCRIPTFUNC( Axis, "Draws an axis. Specify origin + orientation in world space." )
	DEFINE_SCRIPTFUNC( Sphere, "Draws a wireframe sphere. Specify center in world space." )
	DEFINE_SCRIPTFUNC( CircleOriented, "Draws a circle oriented. Specify center in world space." )
	DEFINE_SCRIPTFUNC( Circle, "Draws a circle. Specify center in world space." )
#ifndef CLIENT_DLL
	DEFINE_SCRIPTFUNC( SetDebugBits, "Set debug bits on entity" )
#endif
	DEFINE_SCRIPTFUNC( ClearAllOverlays, "Clear all debug overlays at once" )
END_SCRIPTDESC();



//=============================================================================
// ConVars
//=============================================================================
class CScriptConCommand : public ConCommand, public ICommandCallback, public ICommandCompletionCallback
{
	typedef ConCommand BaseClass;

public:
	~CScriptConCommand()
	{
		Unregister();
	}

	CScriptConCommand( const char *name, HSCRIPT fn, const char *helpString, int flags, ConCommand *pLinked = NULL )
		: BaseClass( name, this, helpString, flags, 0 ),
		m_pLinked(pLinked),
		m_hCallback(fn),
		m_hCompletionCallback(NULL)
	{
		m_nCmdNameLen = V_strlen(name) + 1;
		Assert( m_nCmdNameLen - 1 <= 128 );
	}

	void CommandCallback( const CCommand &command )
	{
		int count = command.ArgC();
		ScriptVariant_t *vArgv = (ScriptVariant_t*)stackalloc( sizeof(ScriptVariant_t) * count );
		for ( int i = 0; i < count; ++i )
		{
			vArgv[i] = command[i];
		}
		ScriptVariant_t ret;
		if ( g_pScriptVM->ExecuteFunction( m_hCallback, vArgv, count, &ret, NULL, true ) == SCRIPT_ERROR )
		{
			DevWarning( 1, "CScriptConCommand: invalid callback for '%s'\n", command[0] );
		}
		if ( m_pLinked && (ret.m_type == FIELD_BOOLEAN) && ret.m_bool )
		{
			m_pLinked->Dispatch( command );
		}
	}

	int CommandCompletionCallback( const char *partial, CUtlVector< CUtlString > &commands )
	{
		Assert( g_pScriptVM );
		Assert( m_hCompletionCallback );

		ScriptVariant_t hArray;
		g_pScriptVM->CreateArray( hArray );

		// split command name from partial, pass both separately to the script function
		char *cmdname = (char*)stackalloc( m_nCmdNameLen );
		V_memcpy( cmdname, partial, m_nCmdNameLen - 1 );
		cmdname[ m_nCmdNameLen - 1 ] = 0;

		char argPartial[256];
		V_StrRight( partial, V_strlen(partial) - m_nCmdNameLen, argPartial, sizeof(argPartial) );

		ScriptVariant_t args[3] = { cmdname, argPartial, hArray };
		if ( g_pScriptVM->ExecuteFunction( m_hCompletionCallback, args, 3, NULL, NULL, true ) == SCRIPT_ERROR )
		{
			DevWarning( 1, "CScriptConCommand: invalid command completion callback for '%s'\n", cmdname );
			g_pScriptVM->ReleaseScript( hArray );
			return 0;
		}

		int count = 0;
		ScriptVariant_t val;
		int it = -1;
		while ( ( it = g_pScriptVM->GetKeyValue( hArray, it, NULL, &val ) ) != -1 )
		{
			if ( val.m_type == FIELD_CSTRING )
			{
				CUtlString &s = commands.Element( commands.AddToTail() );
				int len = V_strlen( val.m_pszString );

				if ( len <= COMMAND_COMPLETION_ITEM_LENGTH - 1 )
				{
					s.Set( val.m_pszString );
				}
				else
				{
					s.SetDirect( val.m_pszString, COMMAND_COMPLETION_ITEM_LENGTH - 1 );
				}

				++count;
			}
			g_pScriptVM->ReleaseValue(val);

			if ( count == COMMAND_COMPLETION_MAXITEMS )
				break;
		}
		g_pScriptVM->ReleaseScript( hArray );
		return count;
	}

	void SetCompletionCallback( HSCRIPT fn )
	{
		if ( m_hCompletionCallback )
			g_pScriptVM->ReleaseScript( m_hCompletionCallback );

		if (fn)
		{
			if ( !BaseClass::IsRegistered() )
				return;

			BaseClass::m_pCommandCompletionCallback = this;
			BaseClass::m_bHasCompletionCallback = true;
			m_hCompletionCallback = fn;
		}
		else
		{
			BaseClass::m_pCommandCompletionCallback = NULL;
			BaseClass::m_bHasCompletionCallback = false;
			m_hCompletionCallback = NULL;
		}
	}

	void SetCallback( HSCRIPT fn )
	{
		if (fn)
		{
			if ( !BaseClass::IsRegistered() )
				Register();

			if ( m_hCallback )
				g_pScriptVM->ReleaseScript( m_hCallback );
			m_hCallback = fn;
		}
		else
		{
			Unregister();
		}
	}

	inline void Unregister()
	{
		if ( g_pCVar && BaseClass::IsRegistered() )
			g_pCVar->UnregisterConCommand( this );

		if ( g_pScriptVM )
		{
			if ( m_hCallback )
			{
				g_pScriptVM->ReleaseScript( m_hCallback );
				m_hCallback = NULL;
			}

			SetCompletionCallback( NULL );
		}
	}

	inline void Register()
	{
		if ( g_pCVar )
			g_pCVar->RegisterConCommand( this );
	}

	HSCRIPT m_hCallback;
	ConCommand *m_pLinked;
	HSCRIPT m_hCompletionCallback;
	int m_nCmdNameLen;
};

class CScriptConVar : public ConVar
{
	typedef ConVar BaseClass;

public:
	~CScriptConVar()
	{
		Unregister();
	}

	CScriptConVar( const char *pName, const char *pDefaultValue, const char *pHelpString, int flags/*, float fMin, float fMax*/ )
		: BaseClass( pName, pDefaultValue, flags, pHelpString ),
		m_hCallback(NULL)
	{}

	void SetChangeCallback( HSCRIPT fn )
	{
		void ScriptConVarCallback( IConVar*, const char*, float );

		if ( m_hCallback )
			g_pScriptVM->ReleaseScript( m_hCallback );

		if (fn)
		{
			m_hCallback = fn;
			BaseClass::InstallChangeCallback( (FnChangeCallback_t)ScriptConVarCallback );
		}
		else
		{
			m_hCallback = NULL;
			BaseClass::InstallChangeCallback( NULL );
		}
	}

	inline void Unregister()
	{
		if ( g_pCVar && BaseClass::IsRegistered() )
			g_pCVar->UnregisterConCommand( this );

		if ( g_pScriptVM )
		{
			SetChangeCallback( NULL );
		}
	}

	HSCRIPT m_hCallback;
};

static CUtlMap< unsigned int, bool > g_ConVarsBlocked( DefLessFunc(unsigned int) );
static CUtlMap< unsigned int, bool > g_ConCommandsOverridable( DefLessFunc(unsigned int) );
static CUtlMap< unsigned int, CScriptConCommand* > g_ScriptConCommands( DefLessFunc(unsigned int) );
static CUtlMap< unsigned int, CScriptConVar* > g_ScriptConVars( DefLessFunc(unsigned int) );


class CScriptConvarAccessor : public CAutoGameSystem
{
public:
	static inline unsigned int Hash( const char*sz ){ return HashStringCaseless(sz); }

public:
	inline void AddOverridable( const char *name )
	{
		g_ConCommandsOverridable.InsertOrReplace( Hash(name), true );
	}

	inline bool IsOverridable( unsigned int hash )
	{
		int idx = g_ConCommandsOverridable.Find( hash );
		return ( idx != g_ConCommandsOverridable.InvalidIndex() );
	}

	inline void AddBlockedConVar( const char *name )
	{
		g_ConVarsBlocked.InsertOrReplace( Hash(name), true );
	}

	inline bool IsBlockedConvar( const char *name )
	{
		int idx = g_ConVarsBlocked.Find( Hash(name) );
		return ( idx != g_ConVarsBlocked.InvalidIndex() );
	}

public:
	void RegisterCommand( const char *name, HSCRIPT fn, const char *helpString, int flags );
	void SetCompletionCallback( const char *name, HSCRIPT fn );
	void UnregisterCommand( const char *name );
	void RegisterConvar( const char *name, const char *pDefaultValue, const char *helpString, int flags );
	void SetChangeCallback( const char *name, HSCRIPT fn );

	HSCRIPT GetCommandClient()
	{
#ifdef GAME_DLL
		return ToHScript( UTIL_GetCommandClient() );
#else
		return ToHScript( C_BasePlayer::GetLocalPlayer() );
#endif
	}
#ifdef GAME_DLL
	const char *GetClientConvarValue( int index, const char* cvar )
	{
		return engine->GetClientConVarValue( index, cvar );
	}
#endif
public:
	bool Init();

	void LevelShutdownPostEntity()
	{
		g_ScriptConCommands.PurgeAndDeleteElements();
		g_ScriptConVars.PurgeAndDeleteElements();
	}

public:
	float GetFloat( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		if ( cvar.IsFlagSet( FCVAR_SERVER_CANNOT_QUERY ) )
			return NULL;
		return cvar.GetFloat();
	}

	int GetInt( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		if ( cvar.IsFlagSet( FCVAR_SERVER_CANNOT_QUERY ) )
			return NULL;
		return cvar.GetInt();
	}

	bool GetBool( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		if ( cvar.IsFlagSet( FCVAR_SERVER_CANNOT_QUERY ) )
			return NULL;
		return cvar.GetBool();
	}

	const char *GetStr( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		if ( cvar.IsFlagSet( FCVAR_SERVER_CANNOT_QUERY ) )
			return NULL;
		return cvar.GetString();
	}

	const char *GetDefaultValue( const char *pszConVar )
	{
		ConVarRef cvar( pszConVar );
		return cvar.GetDefault();
	}

	bool IsFlagSet( const char *pszConVar, int nFlags )
	{
		ConVarRef cvar( pszConVar );
		return cvar.IsFlagSet( nFlags );
	}

	void SetFloat( const char *pszConVar, float value )
	{
		SetValue( pszConVar, value );
	}

	void SetInt( const char *pszConVar, int value )
	{
		SetValue( pszConVar, value );
	}

	void SetBool( const char *pszConVar, bool value )
	{
		SetValue( pszConVar, value );
	}

	void SetStr( const char *pszConVar, const char *value )
	{
		SetValue( pszConVar, value );
	}

	template <typename T>
	void SetValue( const char *pszConVar, T value )
	{
		ConVarRef cvar( pszConVar );
		if ( !cvar.IsValid() )
			return;

		if ( cvar.IsFlagSet( FCVAR_NOT_CONNECTED | FCVAR_SERVER_CANNOT_QUERY ) )
			return;

		if ( IsBlockedConvar( pszConVar ) )
			return;

		cvar.SetValue( value );
	}

} g_ScriptConvarAccessor;


void CScriptConvarAccessor::RegisterCommand( const char *name, HSCRIPT fn, const char *helpString, int flags )
{
#if CLIENT_DLL
	// FIXME: This crashes in engine when used as a hook (dispatched from CScriptConCommand::CommandCallback())
	Assert( V_stricmp( name, "load" ) != 0 );
#endif

	unsigned int hash = Hash(name);
	int idx = g_ScriptConCommands.Find(hash);
	if ( idx == g_ScriptConCommands.InvalidIndex() )
	{
		ConCommandBase *pBase = g_pCVar->FindCommandBase(name);
		if ( pBase && ( !pBase->IsCommand() || !IsOverridable(hash) ) )
		{
			DevWarning( 1, "CScriptConvarAccessor::RegisterCommand unable to register blocked ConCommand: %s\n", name );
			return;
		}

		if ( !fn )
			return;

		CScriptConCommand *p = new CScriptConCommand( name, fn, helpString, flags, static_cast< ConCommand* >(pBase) );
		g_ScriptConCommands.Insert( hash, p );
	}
	else
	{
		CScriptConCommand *pCmd = g_ScriptConCommands[idx];
		pCmd->SetCallback( fn );
		//CGMsg( 1, CON_GROUP_VSCRIPT, "CScriptConvarAccessor::RegisterCommand replacing command already registered: %s\n", name );
	}
}

void CScriptConvarAccessor::SetCompletionCallback( const char *name, HSCRIPT fn )
{
	unsigned int hash = Hash(name);
	int idx = g_ScriptConCommands.Find(hash);
	if ( idx != g_ScriptConCommands.InvalidIndex() )
	{
		g_ScriptConCommands[idx]->SetCompletionCallback( fn );
	}
}

void CScriptConvarAccessor::UnregisterCommand( const char *name )
{
	unsigned int hash = Hash(name);
	int idx = g_ScriptConCommands.Find(hash);
	if ( idx != g_ScriptConCommands.InvalidIndex() )
	{
		g_ScriptConCommands[idx]->Unregister();
	}
}

void CScriptConvarAccessor::RegisterConvar( const char *name, const char *pDefaultValue, const char *helpString, int flags )
{
	Assert( g_pCVar );
	unsigned int hash = Hash(name);
	int idx = g_ScriptConVars.Find(hash);
	if ( idx == g_ScriptConVars.InvalidIndex() )
	{
		if ( g_pCVar->FindCommandBase(name) )
		{
			DevWarning( 1, "CScriptConvarAccessor::RegisterConvar unable to register blocked ConCommand: %s\n", name );
			return;
		}

		CScriptConVar *p = new CScriptConVar( name, pDefaultValue, helpString, flags );
		g_ScriptConVars.Insert( hash, p );
	}
	else
	{
		//CGMsg( 1, CON_GROUP_VSCRIPT, "CScriptConvarAccessor::RegisterConvar convar %s already registered\n", name );
	}
}

void CScriptConvarAccessor::SetChangeCallback( const char *name, HSCRIPT fn )
{
	unsigned int hash = Hash(name);
	int idx = g_ScriptConVars.Find(hash);
	if ( idx != g_ScriptConVars.InvalidIndex() )
	{
		g_ScriptConVars[idx]->SetChangeCallback( fn );
	}
}

void ScriptConVarCallback( IConVar *var, const char* pszOldValue, float flOldValue )
{
	ConVar *cvar = (ConVar*)var;
	const char *name = cvar->GetName();
	unsigned int hash = CScriptConvarAccessor::Hash( name );
	int idx = g_ScriptConVars.Find(hash);
	if ( idx != g_ScriptConVars.InvalidIndex() )
	{
		Assert( g_ScriptConVars[idx]->m_hCallback );

		ScriptVariant_t args[5] = { name, pszOldValue, flOldValue, cvar->GetString(), cvar->GetFloat() };
		if ( g_pScriptVM->ExecuteFunction( g_ScriptConVars[idx]->m_hCallback, args, 5, NULL, NULL, true ) == SCRIPT_ERROR )
		{
			DevWarning( 1, "CScriptConVar: invalid change callback for '%s'\n", name );
		}
	}
}


bool CScriptConvarAccessor::Init()
{
	static bool bExecOnce = false;
	if ( bExecOnce )
		return true;
	bExecOnce = true;

	AddOverridable( "+attack" );
	AddOverridable( "+attack2" );
	AddOverridable( "+attack3" );
	AddOverridable( "+forward" );
	AddOverridable( "+back" );
	AddOverridable( "+moveleft" );
	AddOverridable( "+moveright" );
	AddOverridable( "+use" );
	AddOverridable( "+jump" );
	AddOverridable( "+zoom" );
	AddOverridable( "+reload" );
	AddOverridable( "+speed" );
	AddOverridable( "+walk" );
	AddOverridable( "+duck" );
	AddOverridable( "+strafe" );
	AddOverridable( "+alt1" );
	AddOverridable( "+alt2" );
	AddOverridable( "+grenade1" );
	AddOverridable( "+grenade2" );
	AddOverridable( "+showscores" );
	AddOverridable( "+voicerecord" );

	AddOverridable( "-attack" );
	AddOverridable( "-attack2" );
	AddOverridable( "-attack3" );
	AddOverridable( "-forward" );
	AddOverridable( "-back" );
	AddOverridable( "-moveleft" );
	AddOverridable( "-moveright" );
	AddOverridable( "-use" );
	AddOverridable( "-jump" );
	AddOverridable( "-zoom" );
	AddOverridable( "-reload" );
	AddOverridable( "-speed" );
	AddOverridable( "-walk" );
	AddOverridable( "-duck" );
	AddOverridable( "-strafe" );
	AddOverridable( "-alt1" );
	AddOverridable( "-alt2" );
	AddOverridable( "-grenade1" );
	AddOverridable( "-grenade2" );
	AddOverridable( "-showscores" );
	AddOverridable( "-voicerecord" );

	AddOverridable( "toggle_duck" );
	AddOverridable( "impulse" );
	AddOverridable( "use" );
	AddOverridable( "lastinv" );
	AddOverridable( "invnext" );
	AddOverridable( "invprev" );
	AddOverridable( "phys_swap" );
	AddOverridable( "slot0" );
	AddOverridable( "slot1" );
	AddOverridable( "slot2" );
	AddOverridable( "slot3" );
	AddOverridable( "slot4" );
	AddOverridable( "slot5" );
	AddOverridable( "slot6" );
	AddOverridable( "slot7" );
	AddOverridable( "slot8" );
	AddOverridable( "slot9" );
	AddOverridable( "slot10" );

	AddOverridable( "save" );
	AddOverridable( "load" );

	AddOverridable( "say" );
	AddOverridable( "say_team" );


	AddBlockedConVar( "con_enable" );
	AddBlockedConVar( "cl_allowdownload" );
	AddBlockedConVar( "cl_allowupload" );
	AddBlockedConVar( "cl_downloadfilter" );

	return true;
}

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptConvarAccessor, "CConvars", SCRIPT_SINGLETON "Provides an interface to convars." )
	DEFINE_SCRIPTFUNC( RegisterConvar, "register a new console variable." )
	DEFINE_SCRIPTFUNC( RegisterCommand, "register a console command." )
	DEFINE_SCRIPTFUNC( SetCompletionCallback, "callback is called with 3 parameters (cmd, partial, commands), user strings must be appended to 'commands' array" )
	DEFINE_SCRIPTFUNC( SetChangeCallback, "callback is called with 5 parameters (var, szOldValue, flOldValue, szNewValue, flNewValue)" )
	DEFINE_SCRIPTFUNC( UnregisterCommand, "unregister a console command." )
	DEFINE_SCRIPTFUNC( GetCommandClient, "returns the player who issued this console command." )
#ifdef GAME_DLL
	DEFINE_SCRIPTFUNC( GetClientConvarValue, "Get a convar keyvalue for a specified client" )
#endif
	DEFINE_SCRIPTFUNC( GetFloat, "Returns the convar as a float. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( GetInt, "Returns the convar as an int. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( GetBool, "Returns the convar as a bool. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( GetStr, "Returns the convar as a string. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( GetDefaultValue, "Returns the convar's default value as a string. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( IsFlagSet, "Returns the convar's flags. May return null if no such convar." )
	DEFINE_SCRIPTFUNC( SetFloat, "Sets the value of the convar as a float." )
	DEFINE_SCRIPTFUNC( SetInt, "Sets the value of the convar as an int." )
	DEFINE_SCRIPTFUNC( SetBool, "Sets the value of the convar as a bool." )
	DEFINE_SCRIPTFUNC( SetStr, "Sets the value of the convar as a string." )
END_SCRIPTDESC();


//=============================================================================
// Effects
// (Unique to mapbase)
//
// At the moment only clientside until a filtering method on server is finalised.
//
// TEs most of the time call IEffects (g_pEffects) or ITempEnts (tempents) on client,
// but they also record for tools recording mode.
//
// On client no TE is suppressed.
// TE flags are found at tempent.h
//
// TODO:
//=============================================================================
#ifdef CLIENT_DLL

class CEffectsScriptHelper
{
public:
	void DynamicLight( int index, const Vector& origin, int r, int g, int b, int exponent,
		float radius, float die, float decay, int style = 0, int flags = 0 )
	{
		//te->DynamicLight( filter, delay, &origin, r, g, b, exponent, radius, die, decay );
		dlight_t *dl = effects->CL_AllocDlight( index );
		dl->origin = origin;
		dl->color.r = r;
		dl->color.g = g;
		dl->color.b = b;
		dl->color.exponent = exponent;
		dl->radius = radius;
		dl->die = gpGlobals->curtime + die;
		dl->decay = decay;
		dl->style = style;
		dl->flags = flags;
	}

	void Explosion( const Vector& pos, float scale, int radius, int magnitude, int flags )
	{
		C_RecipientFilter filter;
		filter.AddAllPlayers();
		// framerate, modelindex, normal and materialtype are unused
		// radius for ragdolls
		extern short g_sModelIndexFireball;
		te->Explosion( filter, 0.0f, &pos, g_sModelIndexFireball, scale, 15, flags, radius, magnitude, &vec3_origin );
	}

//	void FXExplosion( const Vector& pos, const Vector& normal, int materialType = 'C' )
//	{
//		// just the particles
//		// materialtype only for debris. can be 'C','W' or anything else.
//		FX_Explosion( const_cast<Vector&>(pos), const_cast<Vector&>(normal), materialType );
//	}

//	void ConcussiveExplosion( const Vector& pos, const Vector& normal )
//	{
//		FX_ConcussiveExplosion( const_cast<Vector&>(pos), const_cast<Vector&>(normal) );
//	}

//	void MicroExplosion( const Vector& pos, const Vector& normal )
//	{
//		FX_MicroExplosion( const_cast<Vector&>(pos), const_cast<Vector&>(normal) );
//	}

//	void MuzzleFlash( int type, HSCRIPT hEntity, int attachment, bool firstPerson )
//	{
//		C_BaseEntity *p = ToEnt(hEntity);
//		ClientEntityHandle_t ent = p ? (ClientEntityList().EntIndexToHandle)( p->entindex() ) : NULL;;
//		tempents->MuzzleFlash( type, ent, attachment, firstPerson );
//	}

	void Sparks( const Vector& pos, int nMagnitude, int nTrailLength, const Vector& pDir )
	{
		//te->Sparks( filter, delay, &pos, nMagnitude, nTrailLength, &pDir );
		//g_pEffects->Sparks( pos, nMagnitude, nTrailLength, &pDir );
		FX_ElectricSpark( pos, nMagnitude, nTrailLength, &pDir );
	}

	void MetalSparks( const Vector& pos, const Vector& dir )
	{
		//g_pEffects->MetalSparks( pos, dir );
		FX_MetalSpark( pos, dir, dir );
	}

//	void Smoke( const Vector& pos, float scale, int framerate)
//	{
//		extern short g_sModelIndexSmoke;
//		//te->Smoke( filter, 0.0, &pos, g_sModelIndexSmoke, scale * 10.0f, framerate );
//		g_pEffects->Smoke( pos, g_sModelIndexSmoke, scale, framerate );
//	}

	void Dust( const Vector &pos, const Vector &dir, float size, float speed )
	{
		//te->Dust( filter, delay, pos, dir, size, speed );
		//g_pEffects->Dust( pos, dir, size, speed );
		FX_Dust( pos, dir, size, speed );
	}

	void Bubbles( const Vector &mins, const Vector &maxs, float height, int modelindex, int count, float speed )
	{
		//int bubbles = modelinfo->GetModelIndex( "sprites/bubble.vmt" );
		//te->Bubbles( filter, delay, &mins, &maxs, height, modelindex, count, speed );
		tempents->Bubbles( mins, maxs, height, modelindex, count, speed );
	}

//	void Fizz( const Vector& mins, const Vector& maxs, int modelIndex, int density, int current/*, int flags*/ )
//	{
//		//te->Fizz( filter, delay, ent, modelindex, density, current );
//		//tempents->FizzEffect( ToEnt(ent), modelindex, density, current );
//	}

	void Sprite( const Vector &pos, const Vector &dir, float scale, int modelIndex, int rendermode,
		int renderfx, int brightness, float life, int flags  )
	{
		//te->Sprite( filter, delay, &pos, modelindex, size, brightness );
		float a = (1.0 / 255.0) * brightness;
		tempents->TempSprite( pos, dir, scale, modelIndex, rendermode, renderfx, a, life, flags );
	}

//	void PhysicsProp( float delay, int modelindex, int skin, const Vector& pos, const QAngle &angles,
//		const Vector& vel, int flags, int effects )
//	{
//		//te->PhysicsProp( filter, delay, modelindex, skin, pos, angles, vel, flags, effects );
//		tempents->PhysicsProp( modelindex, skin, pos, angles, vel, flags, effects );
//	}

	void ClientProjectile( const Vector& vecOrigin, const Vector& vecVelocity, const Vector& vecAccel, int modelindex,
		int lifetime, HSCRIPT pOwner, const char *pszImpactEffect = NULL, const char *pszParticleEffect = NULL )
	{
		//te->ClientProjectile( filter, delay, &vecOrigin, &vecVelocity, modelindex, lifetime, ToEnt(pOwner) );
		if ( pszImpactEffect && !(*pszImpactEffect) )
			pszImpactEffect = NULL;
		if ( pszParticleEffect && !(*pszParticleEffect) )
			pszParticleEffect = NULL;
		tempents->ClientProjectile( vecOrigin, vecVelocity, vecAccel, modelindex, lifetime, ToEnt(pOwner), pszImpactEffect, pszParticleEffect );
	}

} g_ScriptEffectsHelper;

BEGIN_SCRIPTDESC_ROOT_NAMED( CEffectsScriptHelper, "CEffects", SCRIPT_SINGLETON "" )
	DEFINE_SCRIPTFUNC( DynamicLight, "" )
	DEFINE_SCRIPTFUNC( Explosion, "" )
	DEFINE_SCRIPTFUNC( Sparks, "" )
	DEFINE_SCRIPTFUNC( MetalSparks, "" )
	DEFINE_SCRIPTFUNC( Dust, "" )
	DEFINE_SCRIPTFUNC( Bubbles, "" )
	DEFINE_SCRIPTFUNC( Sprite, "" )
	DEFINE_SCRIPTFUNC( ClientProjectile, "" )
END_SCRIPTDESC();



//=============================================================================
//=============================================================================

extern CGlowObjectManager g_GlowObjectManager;

class CScriptGlowObjectManager : public CAutoGameSystem
{
public:
	CUtlVector<int> m_RegisteredObjects;

	void LevelShutdownPostEntity()
	{
		FOR_EACH_VEC( m_RegisteredObjects, i )
			g_GlowObjectManager.UnregisterGlowObject( m_RegisteredObjects[i] );
		m_RegisteredObjects.Purge();
	}

public:
	int Register( HSCRIPT hEntity, int r, int g, int b, int a, bool bRenderWhenOccluded, bool bRenderWhenUnoccluded )
	{
		Vector vGlowColor;
		vGlowColor.x = r * ( 1.0f / 255.0f );
		vGlowColor.y = g * ( 1.0f / 255.0f );
		vGlowColor.z = b * ( 1.0f / 255.0f );
		float flGlowAlpha = a * ( 1.0f / 255.0f );
		int idx = g_GlowObjectManager.RegisterGlowObject( ToEnt(hEntity), vGlowColor, flGlowAlpha, bRenderWhenOccluded, bRenderWhenUnoccluded, -1 );
		m_RegisteredObjects.AddToTail( idx );
		return idx;
	}

	void Unregister( int nGlowObjectHandle )
	{
		if ( (nGlowObjectHandle < 0) || (nGlowObjectHandle >= g_GlowObjectManager.m_GlowObjectDefinitions.Count()) )
			return;
		g_GlowObjectManager.UnregisterGlowObject( nGlowObjectHandle );
		m_RegisteredObjects.FindAndFastRemove( nGlowObjectHandle );
	}

	void SetEntity( int nGlowObjectHandle, HSCRIPT hEntity )
	{
		g_GlowObjectManager.SetEntity( nGlowObjectHandle, ToEnt(hEntity) );
	}

	void SetColor( int nGlowObjectHandle, int r, int g, int b )
	{
		Vector vGlowColor;
		vGlowColor.x = r * ( 1.0f / 255.0f );
		vGlowColor.y = g * ( 1.0f / 255.0f );
		vGlowColor.z = b * ( 1.0f / 255.0f );
		g_GlowObjectManager.SetColor( nGlowObjectHandle, vGlowColor );
	}

	void SetAlpha( int nGlowObjectHandle, int a )
	{
		float flGlowAlpha = a * ( 1.0f / 255.0f );
		g_GlowObjectManager.SetAlpha( nGlowObjectHandle, flGlowAlpha );
	}

	void SetRenderFlags( int nGlowObjectHandle, bool bRenderWhenOccluded, bool bRenderWhenUnoccluded )
	{
		g_GlowObjectManager.SetRenderFlags( nGlowObjectHandle, bRenderWhenOccluded, bRenderWhenUnoccluded );
	}

} g_ScriptGlowObjectManager;

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptGlowObjectManager, "CGlowObjectManager", SCRIPT_SINGLETON "" )
	DEFINE_SCRIPTFUNC( Register, "( HSCRIPT hEntity, int r, int g, int b, int a, bool bRenderWhenOccluded, bool bRenderWhenUnoccluded )" )
	DEFINE_SCRIPTFUNC( Unregister, "" )
	DEFINE_SCRIPTFUNC( SetEntity, "" )
	DEFINE_SCRIPTFUNC( SetColor, "" )
	DEFINE_SCRIPTFUNC( SetAlpha, "" )
	DEFINE_SCRIPTFUNC( SetRenderFlags, "" )
END_SCRIPTDESC();


//=============================================================================
//=============================================================================


#if !defined(NO_STEAM)
class CScriptSteamAPI
{
public:
	const char *GetSteam2ID()
	{
		if ( !steamapicontext || !steamapicontext->SteamUser() )
			return NULL;

		CSteamID id = steamapicontext->SteamUser()->GetSteamID();

		uint32 accountID = id.GetAccountID();
		uint32 steamInstanceID = 0;
		uint32 high32bits = accountID % 2;
		uint32 low32bits = accountID / 2;

		static char ret[48];
		V_snprintf( ret, sizeof(ret), "STEAM_%u:%u:%u", steamInstanceID, high32bits, low32bits );
		return ret;
	}

	int GetSecondsSinceComputerActive()
	{
		if ( !steamapicontext || !steamapicontext->SteamUtils() )
			return 0;

		return steamapicontext->SteamUtils()->GetSecondsSinceComputerActive();
	}

	int GetCurrentBatteryPower()
	{
		if ( !steamapicontext || !steamapicontext->SteamUtils() )
			return 0;

		return steamapicontext->SteamUtils()->GetCurrentBatteryPower();
	}
#if 0
	const char *GetIPCountry()
	{
		if ( !steamapicontext || !steamapicontext->SteamUtils() )
			return NULL;

		const char *get = steamapicontext->SteamUtils()->GetIPCountry();
		if ( !get )
			return NULL;

		static char ret[3];
		V_strncpy( ret, get, 3 );

		return ret;
	}
#endif
	const char *GetCurrentGameLanguage()
	{
		if ( !steamapicontext || !steamapicontext->SteamApps() )
			return NULL;

		const char *lang = steamapicontext->SteamApps()->GetCurrentGameLanguage();
		if ( !lang )
			return NULL;

		static char ret[16];
		V_strncpy( ret, lang, sizeof(ret) );

		return ret;
	}
	const char *GetCurrentBetaName()
	{
		if ( !steamapicontext || !steamapicontext->SteamApps() )
			return NULL;

		static char ret[16];
		steamapicontext->SteamApps()->GetCurrentBetaName( ret, sizeof( ret ) );
		return ret;
	}
#if 0
	bool IsSubscribedApp( int nAppID )
	{
		if ( !steamapicontext || !steamapicontext->SteamApps() )
			return false;

		return steamapicontext->SteamApps()->BIsSubscribedApp( nAppID );
	}
#endif
	bool IsAppInstalled( int nAppID )
	{
		if ( !steamapicontext || !steamapicontext->SteamApps() )
			return false;

		return steamapicontext->SteamApps()->BIsAppInstalled( nAppID );
	}

} g_ScriptSteamAPI;

BEGIN_SCRIPTDESC_ROOT_NAMED( CScriptSteamAPI, "CSteamAPI", SCRIPT_SINGLETON "" )
	DEFINE_SCRIPTFUNC( GetSteam2ID, "" )
	//DEFINE_SCRIPTFUNC( IsVACBanned, "" )
	DEFINE_SCRIPTFUNC( GetSecondsSinceComputerActive, "Returns the number of seconds since the user last moved the mouse." )
	DEFINE_SCRIPTFUNC( GetCurrentBatteryPower, "Return the amount of battery power left in the current system in % [0..100], 255 for being on AC power" )
	//DEFINE_SCRIPTFUNC( GetIPCountry, "Returns the 2 digit ISO 3166-1-alpha-2 format country code this client is running in (as looked up via an IP-to-location database)" )
	DEFINE_SCRIPTFUNC( GetCurrentGameLanguage, "Gets the current language that the user has set as API language code. This falls back to the Steam UI language if the user hasn't explicitly picked a language for the title." )
	DEFINE_SCRIPTFUNC( GetCurrentBetaName, "Gets the name of the user's current beta branch. In Source SDK Base 2013 Singleplayer, this will usually return 'upcoming'." )
	//DEFINE_SCRIPTFUNC( IsSubscribedApp, "Returns true if the user is subscribed to the specified app ID." )
	DEFINE_SCRIPTFUNC( IsAppInstalled, "Returns true if the user has the specified app ID installed on their computer." )
END_SCRIPTDESC();
#endif // !NO_STEAM

#endif // CLIENT_DLL


void RegisterScriptSingletons()
{
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptSaveRestoreUtil::SaveTable, "SaveTable", "Store a table with primitive values that will persist across level transitions and save loads." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptSaveRestoreUtil::RestoreTable, "RestoreTable", "Retrieves a table from storage. Write into input table." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptSaveRestoreUtil::ClearSavedTable, "ClearSavedTable", "Removes the table with the given context." );
	ScriptRegisterSimpleHook( g_pScriptVM, g_Hook_OnSave, "OnSave", FIELD_VOID, "Called when the game is saved." );
	ScriptRegisterSimpleHook( g_pScriptVM, g_Hook_OnRestore, "OnRestore", FIELD_VOID, "Called when the game is restored." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::FileWrite, "StringToFile", "Stores the string into the file" );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::FileRead, "FileToString", "Returns the string from the file, null if no file or file is too big." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::FileExists, "FileExists", "Returns true if the file exists." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::KeyValuesWrite, "KeyValuesToFile", "Stores the CScriptKeyValues into the file" );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptReadWriteFile::KeyValuesRead, "FileToKeyValues", "Returns the CScriptKeyValues from the file, null if no file or file is too big." );

	ScriptRegisterFunction( g_pScriptVM, ListenToGameEvent, "Register as a listener for a game event from script." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptGameEventListener::StopListeningToGameEvent, "StopListeningToGameEvent", "Stop the specified event listener." );
	ScriptRegisterFunctionNamed( g_pScriptVM, CScriptGameEventListener::StopListeningToAllGameEvents, "StopListeningToAllGameEvents", "Stop listening to all game events within a specific context." );
	ScriptRegisterFunction( g_pScriptVM, FireGameEvent, "Fire a game event." );
#ifndef CLIENT_DLL
	ScriptRegisterFunction( g_pScriptVM, FireGameEventLocal, "Fire a game event without broadcasting to the client." );
#endif

	g_pScriptVM->RegisterInstance( &g_ScriptNetPropManager, "NetProps" );
	g_pScriptVM->RegisterInstance( &g_ScriptLocalize, "Localize" );
	g_pScriptVM->RegisterInstance( g_ScriptNetMsg, "NetMsg" );
	g_pScriptVM->RegisterInstance( &g_ScriptDebugOverlay, "debugoverlay" );
	g_pScriptVM->RegisterInstance( &g_ScriptConvarAccessor, "Convars" );
#ifdef CLIENT_DLL
	g_pScriptVM->RegisterInstance( &g_ScriptEffectsHelper, "effects" );
	g_pScriptVM->RegisterInstance( &g_ScriptGlowObjectManager, "GlowObjectManager" );

#if !defined(NO_STEAM)
	g_pScriptVM->RegisterInstance( &g_ScriptSteamAPI, "steam" );
#endif
#endif

	// Singletons not unique to VScript (not declared or defined here)
	g_pScriptVM->RegisterInstance( GameRules(), "GameRules" );
	g_pScriptVM->RegisterInstance( GetAmmoDef(), "AmmoDef" );
#ifndef CLIENT_DLL
	g_pScriptVM->RegisterInstance( &g_AI_SquadManager, "Squads" );
#endif

#ifdef USE_OLD_EVENT_DESCRIPTORS
	CScriptGameEventListener::LoadAllEvents();
#endif

	g_ScriptNetMsg->InitPostVM();
}
