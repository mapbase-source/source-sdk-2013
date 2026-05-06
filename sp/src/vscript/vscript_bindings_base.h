//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 =================
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================

#ifndef VSCRIPT_BINDINGS_BASE
#define VSCRIPT_BINDINGS_BASE
#ifdef _WIN32
#pragma once
#endif

#include "vscript/ivscript.h"
#include "tier1/KeyValues.h"

// ----------------------------------------------------------------------------
// KeyValues access
// ----------------------------------------------------------------------------
class CScriptKeyValues
{
public:
	CScriptKeyValues( KeyValues *pKeyValues, bool bBorrow );
	~CScriptKeyValues( );

	HSCRIPT_RC ScriptFindKey( const char *pszName );
	HSCRIPT_RC ScriptGetFirstSubKey( void );
	HSCRIPT_RC ScriptGetNextKey( void );
	int ScriptGetKeyValueInt( const char *pszName );
	float ScriptGetKeyValueFloat( const char *pszName );
	const char *ScriptGetKeyValueString( const char *pszName );
	bool ScriptIsKeyValueEmpty( const char *pszName );
	bool ScriptGetKeyValueBool( const char *pszName );
	void ScriptReleaseKeyValues( );

	// Functions below are new with Mapbase
	void TableToSubKeys( HSCRIPT hTable );
	void SubKeysToTable( HSCRIPT hTable );

	HSCRIPT_RC ScriptFindOrCreateKey( const char *pszName );

	const char *ScriptGetName();
	int ScriptGetInt();
	float ScriptGetFloat();
	const char *ScriptGetString();
	bool ScriptGetBool();

	void ScriptSetKeyValueInt( const char *pszName, int iValue );
	void ScriptSetKeyValueFloat( const char *pszName, float flValue );
	void ScriptSetKeyValueString( const char *pszName, const char *pszValue );
	void ScriptSetKeyValueBool( const char *pszName, bool bValue );
	void ScriptSetName( const char *pszValue );
	void ScriptSetInt( int iValue );
	void ScriptSetFloat( float flValue );
	void ScriptSetString( const char *pszValue );
	void ScriptSetBool( bool bValue );

	KeyValues *GetKeyValues() { return m_pSelf->ptr; }

	// The lifetime of the KeyValues pointer needs to be decoupled from refcounted script objects
	// because base kv script objects can be released while their children live: kv = kv.GetFirstSubKey()
	// Refcounting externally allows children to extend the lifetime of KeyValues while
	// being able to automatically dispose of CScriptKeyValues and HSCRIPT objects with
	// script refcounted HSCRIPT_RC

	struct KeyValues_RC
	{
		KeyValues *ptr;
		unsigned int refs;
		// Wheter KeyValues memory is borrowed or owned by CScriptKeyValues
		// if not borrowed, it is deleted on release
		bool borrow;

		KeyValues_RC( KeyValues *pKeyValues, bool bBorrow ) :
			ptr( pKeyValues ),
			refs( 1 ),
			borrow( bBorrow )
		{
		}

		void AddRef()
		{
			Assert( refs < (unsigned int)-1 );
			refs++;
		}

		void Release()
		{
			Assert( refs > 0 );
			refs--;

			if ( refs == 0 )
			{
				if ( !borrow )
				{
					ptr->deleteThis();
				}

				delete this;
			}
		}
	};

	KeyValues_RC *m_pSelf;
	KeyValues_RC *m_pBase;
};

//-----------------------------------------------------------------------------
// Exposes Color to VScript
//-----------------------------------------------------------------------------
class CScriptColorInstanceHelper : public IScriptInstanceHelper
{
	bool ToString( void *p, char *pBuf, int bufSize );

	bool Get( void *p, const char *pszKey, ScriptVariant_t &variant );
	bool Set( void *p, const char *pszKey, ScriptVariant_t &variant );
};

void RegisterBaseBindings( IScriptVM *pVM );

#endif
