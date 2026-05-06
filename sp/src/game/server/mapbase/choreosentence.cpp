//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: VCD-based sentences.
//
//=============================================================================//

#include "cbase.h"
#include "utlhashtable.h"
#include "engine/IEngineSound.h"
#include "soundchars.h"
#include "choreosentence.h"
#include "mapbase/matchers.h"
#include "ai_speech_new.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

//----------------------------------------------------------------------------

#define IsVirtualWord(pszWord) (pszWord[0] == 'V' && pszWord[1] == '_')
#define RemoveVirtualPrefix(pszWord) IsVirtualWord(pszWord) ? pszWord + 2 : pszWord

//----------------------------------------------------------------------------

class CChoreoSentenceSystem : public CAutoGameSystem
{
public:

	struct VirtualWord_t;
	struct VirtualWordDefinition_t;
	struct VirtualWordDefinitionList_t;
	struct SentenceContextPrerequisite_t;

	//----------------------------------------------------------------------------

	CChoreoSentenceSystem() : m_Strings( 256, 0, &StringLessThan ) { }
	~CChoreoSentenceSystem() { PurgeSentences(); }

	bool Init()
	{
		// Load the default choreo sentences files
		LoadChoreoSentenceManifest( "scripts/choreosentences_manifest.txt" );
		return true;
	}

	void Shutdown()
	{
		PurgeSentences();
	}

	//----------------------------------------------------------------------------

	void LoadChoreoSentenceManifest( const char *pszFile )
	{
		KeyValues *pManifest = new KeyValues( "ChoreoSentencesManifest" );
		if (pManifest->LoadFromFile( filesystem, pszFile ))
		{
			FOR_EACH_SUBKEY( pManifest, pSubKey )
			{
				LoadChoreoSentenceFile( pSubKey->GetString() );
			}
		}
		pManifest->deleteThis();

		//m_Strings.Compact( true );
	}

	void LoadChoreoSentenceFile( const char *pszFile )
	{
		KeyValues *pManifest = new KeyValues( "ChoreoSentences" );
		if (pManifest->LoadFromFile( filesystem, pszFile ))
		{
			//----------------------------------------------------------------------------
			// Context Prerequisites
			//----------------------------------------------------------------------------
			KeyValues *pSentenceContextPrerequisites = pManifest->FindKey( "SentenceContextPrerequisites" );
			FOR_EACH_SUBKEY( pSentenceContextPrerequisites, pSubKey )
			{
				KeyValues *pCondition = pSubKey->GetFirstSubKey();
				if (pCondition)
				{
					//
					// Only add if there is a subkey for condition
					//
					int i = m_ContextPrerequisites.AddToTail();
					m_ContextPrerequisites[i].nConditionType = DeduceCondition( pCondition->GetName(), V_strlen( pCondition->GetName() ) );

					m_ContextPrerequisites[i].pszContextName = AllocateString( pSubKey->GetName() );
					m_ContextPrerequisites[i].pszCondition = AllocateString( pCondition->GetString() );
				}
			}

			//----------------------------------------------------------------------------
			// Virtual Words
			//----------------------------------------------------------------------------
			KeyValues *pVirtualWords = pManifest->FindKey( "VirtualWords" );
			FOR_EACH_SUBKEY( pVirtualWords, pSubKey )
			{
				//
				// Determine if this has nested virtual words or not
				//
				KeyValues *pMatches = pSubKey->FindKey( "matches" );
				if (pMatches)
				{
					//
					// It has nested virtual words
					//
					int i = m_VirtualWordLists.Insert( RemoveVirtualPrefix( pSubKey->GetName() ) );
					m_VirtualWordLists[i].pszCriterion = AllocateString( pSubKey->GetString( "criterion" ) );

					bool bFirst = true;
					FOR_EACH_SUBKEY( pMatches, pWordList )
					{
						if (bFirst)
						{
							// First one is always default
							if (LoadVirtualWordDefinitionFromKV( pWordList, m_VirtualWordLists[i].m_DefaultWordDefinition, pSubKey->GetName() ))
								bFirst = false;
						}
						else
						{
							int i2 = m_VirtualWordLists[i].m_WordDefinitions.Insert( pWordList->GetName() );
							if (!LoadVirtualWordDefinitionFromKV( pWordList, m_VirtualWordLists[i].m_WordDefinitions[i2], pSubKey->GetName() ))
								m_VirtualWordLists[i].m_WordDefinitions.RemoveAt( i2 );
						}
					}
				}
				else
				{
					//
					// It is a virtual word
					//
					int i = m_VirtualWords.Insert( RemoveVirtualPrefix( pSubKey->GetName() ) );
					if (!LoadVirtualWordDefinitionFromKV( pSubKey, m_VirtualWords[i], pSubKey->GetName() ))
						m_VirtualWords.RemoveAt( i );
				}
			}

			//----------------------------------------------------------------------------
			// Sentences
			//----------------------------------------------------------------------------
			KeyValues *pSentences = pManifest->FindKey( "Sentences" );
			FOR_EACH_SUBKEY( pSentences, pSetKey )
			{
				FOR_EACH_SUBKEY( pSetKey, pSubKey )
				{
					if (pSubKey->GetName() && *pSubKey->GetString())
					{
						int i = m_Sentences.Insert( pSubKey->GetName() );

						char szPathName[MAX_PATH];
						V_strncpy( szPathName, pSetKey->GetName(), sizeof( szPathName ) );
						V_FixSlashes( szPathName );
						m_Sentences[i].pszPrefix = AllocateString( szPathName );

						//m_Sentences[i].pszPrefix = AllocateString( pSetKey->GetName() );

						ParseChoreoSentence( NULL, pSubKey->GetString(), m_Sentences[i] );
						m_Sentences[i].pszName = m_Sentences.GetElementName( i );
					}
				}
			}
		}
		pManifest->deleteThis();
	}

	bool LoadVirtualWordDefinitionFromKV( KeyValues *pWordList, VirtualWordDefinition_t &wordDefinition, const char *pszWordName )
	{
		//
		// Get the condition for this virtual word
		//
		KeyValues *pCondition = pWordList->FindKey( "condition" );
		if (pCondition)
		{
			int nStrLen = V_strlen( pCondition->GetString() );
			const char *pColon = V_strnchr( pCondition->GetString(), ':', nStrLen );
			if (pColon)
			{
				wordDefinition.pszCondition = AllocateString( pColon + 1 );
				wordDefinition.nConditionType = DeduceCondition( pCondition->GetString(), pColon - pCondition->GetString() );
			}
			else
			{
				wordDefinition.nConditionType = DeduceCondition( pCondition->GetString(), nStrLen );
			}
		}
		else
		{
			wordDefinition.nConditionType = 0;
			wordDefinition.pszCondition = "";
		}

		//
		// Get the words themselves
		//
		KeyValues *pWords = pWordList->FindKey( "words" );
		if (pWords)
		{
			FOR_EACH_SUBKEY( pWords, pWord )
			{
				int i = wordDefinition.m_Words.AddToTail();
				V_strncpy( wordDefinition.m_Words[i].szWord, pWord->GetString(), sizeof( wordDefinition.m_Words[i].szWord ) );
			}
		}
		else
		{
			pWords = pWordList->FindKey( "words_from" );
			if (pWords)
			{
				const char *pszWordsTarget = RemoveVirtualPrefix( pWords->GetString() );

				//
				// Get from another virtual word
				//
				FOR_EACH_DICT_FAST( m_VirtualWords, i )
				{
					if (FStrEq( m_VirtualWords.GetElementName( i ), pszWordsTarget ))
					{
						wordDefinition.m_Words.AddVectorToTail( m_VirtualWords[i].m_Words );
					}
				}
			}
		}

		if (wordDefinition.m_Words.Count() <= 0)
		{
			Warning( "WARNING: No words found on virtual word %s\n", pszWordName );
			return false;
		}

		return true;
	}

	//----------------------------------------------------------------------------

	void PurgeSentences()
	{
		m_ContextPrerequisites.RemoveAll();
		m_VirtualWordLists.RemoveAll();
		m_VirtualWords.RemoveAll();
		m_Sentences.RemoveAll();

		for (unsigned int i = 0; i < m_Strings.Count(); i++)
		{
			delete m_Strings[i];
		}
		m_Strings.Purge();
	}

	//----------------------------------------------------------------------------
	
	void PrintEverything()
	{
		Msg( "CONTEXT PREREQUISITES\n\n" );
		
		FOR_EACH_VEC( m_ContextPrerequisites, i )
		{
			Msg( "\t\"%s\"\n\t\tCondition: %i (\"%s\")\n\n", m_ContextPrerequisites[i].pszContextName, m_ContextPrerequisites[i].nConditionType, m_ContextPrerequisites[i].pszCondition );
		}

		Msg( "VIRTUAL WORDS\n\n" );
		
		FOR_EACH_DICT_FAST( m_VirtualWords, i )
		{
			char szWords[256] = { 0 };
			FOR_EACH_VEC( m_VirtualWords[i].m_Words, nWord )
			{
				V_strncat( szWords, "\t\t\t\"", sizeof( szWords ) );
				V_strncat( szWords, m_VirtualWords[i].m_Words[nWord].szWord, sizeof( szWords ) );
				V_strncat( szWords, "\"\n", sizeof( szWords ) );
			}

			Msg( "\t\"%s\"\n\t\tCondition: %i (\"%s\")\n\t\tWords:\n%s\n\n", m_VirtualWords.GetElementName( i ), m_VirtualWords[i].nConditionType, m_VirtualWords[i].pszCondition, szWords );
		}

		Msg( "SENTENCES\n\n" );

		FOR_EACH_DICT_FAST( m_Sentences, i )
		{
			char szWords[128] = { 0 };
			FOR_EACH_VEC( m_Sentences[i].m_Words, nWord )
			{
				if (m_Sentences[i].m_Words[nWord].nPitch != 100)
				{
					V_snprintf( szWords, sizeof( szWords ), "%s(p%i) ", szWords, m_Sentences[i].m_Words[nWord].nPitch );
				}

				if (m_Sentences[i].m_Words[nWord].nVol != 100)
				{
					V_snprintf( szWords, sizeof( szWords ), "%s(v%i) ", szWords, m_Sentences[i].m_Words[nWord].nVol );
				}

				V_strncat( szWords, "\"", sizeof( szWords ) );
				V_strncat( szWords, m_Sentences[i].m_Words[nWord].pszWord, sizeof( szWords ) );
				V_strncat( szWords, "\" ", sizeof( szWords ) );
			}

			Msg( "\t\"%s\"\n\t\tPrefix: \"%s\"\n\t\tCaption: \"%s\"\n\t\tWords: %s\n\n", m_Sentences.GetElementName( i ), m_Sentences[i].pszPrefix, m_Sentences[i].pszCaption ? m_Sentences[i].pszCaption : "N/A", szWords);
		}
	}

	void PrintStrings()
	{
		CUtlVector<const char*> strings( 0, m_Strings.Count() );
		for ( UtlHashHandle_t i = m_Strings.FirstInorder(); i != m_Strings.InvalidIndex(); i = m_Strings.NextInorder(i) )
		{
			strings.AddToTail( m_Strings[i] );
		}

		struct _Local {
			static int __cdecl F(const char * const *a, const char * const *b) { return strcmp(*a, *b); }
		};
		strings.Sort( _Local::F );

		for ( int i = 0; i < strings.Count(); ++i )
		{
			Msg( "  %d (0x%p) : %s\n", i, strings[i], strings[i] );
		}
		Msg( "\n" );
		Msg( "Size:  %d items\n", strings.Count() );
	}
	
	//----------------------------------------------------------------------------

	const ChoreoSentence_t *LookupChoreoSentence( CBaseEntity *pSpeaker, const char *pszSentenceName )
	{
		FOR_EACH_DICT_FAST( m_Sentences, i )
		{
			if (FStrEq( m_Sentences.GetElementName( i ), pszSentenceName ))
			{
				return &m_Sentences[i];
			}
		}

		return NULL;
	}

	//----------------------------------------------------------------------------

	void PrecacheVirtualWord( const char *pszWord, const char *pszPrefix, const char *pszSuffix )
	{
		pszWord = RemoveVirtualPrefix( pszWord );

		FOR_EACH_DICT_FAST( m_VirtualWords, i )
		{
			if (FStrEq( m_VirtualWords.GetElementName( i ), pszWord ))
			{
				// Precache all words
				FOR_EACH_VEC( m_VirtualWords[i].m_Words, i2 )
				{
					CBaseEntity::PrecacheScriptSound( UTIL_VarArgs( "%s%s%s", pszPrefix, m_VirtualWords[i].m_Words[i2].szWord, pszSuffix ) );
				}
				return;
			}
		}

		FOR_EACH_DICT_FAST( m_VirtualWordLists, i )
		{
			if (FStrEq( m_VirtualWordLists.GetElementName( i ), pszWord ))
			{
				// Precache all words in default definition
				FOR_EACH_VEC( m_VirtualWordLists[i].m_DefaultWordDefinition.m_Words, i2 )
				{
					CBaseEntity::PrecacheScriptSound( UTIL_VarArgs( "%s%s%s", pszPrefix, m_VirtualWordLists[i].m_DefaultWordDefinition.m_Words[i2].szWord, pszSuffix));
				}

				// Precache all words in nested definitions
				FOR_EACH_DICT( m_VirtualWordLists[i].m_WordDefinitions, i2 )
				{
					FOR_EACH_VEC( m_VirtualWordLists[i].m_WordDefinitions[i2].m_Words, i3 )
					{
						CBaseEntity::PrecacheScriptSound( UTIL_VarArgs( "%s%s%s", pszPrefix, m_VirtualWordLists[i].m_WordDefinitions[i2].m_Words[i3].szWord, pszSuffix ) );
					}
				}
				return;
			}
		}
	}

	inline const VirtualWord_t &ResolveVirtualWordDefinition( CBaseEntity *pSpeaker, const VirtualWordDefinition_t &wordDefinition )
	{
		// Resolve this condition
		int nIndex = ResolveVirtualCondition( pSpeaker, wordDefinition.nConditionType, wordDefinition.pszCondition, wordDefinition.m_Words.Count() );
		Assert( nIndex >= 0 && nIndex < wordDefinition.m_Words.Count() );

		return wordDefinition.m_Words[nIndex];
	}

	const char *ResolveVirtualWord( CBaseEntity *pSpeaker, const char *pszWord )
	{
		pszWord = RemoveVirtualPrefix( pszWord );

		FOR_EACH_DICT_FAST( m_VirtualWords, i )
		{
			if (FStrEq( m_VirtualWords.GetElementName( i ), pszWord ))
			{
				return ResolveVirtualWordDefinition( pSpeaker, m_VirtualWords[i] ).szWord;
			}
		}

		FOR_EACH_DICT_FAST( m_VirtualWordLists, i )
		{
			if (FStrEq( m_VirtualWordLists.GetElementName( i ), pszWord ))
			{
				AI_CriteriaSet set;
				if (pSpeaker)
					pSpeaker->ModifyOrAppendCriteria( set );

				int nCriterion = set.FindCriterionIndex( m_VirtualWordLists[i].pszCriterion );
				if (set.IsValidIndex( nCriterion ))
				{
					const char *pszValue = set.GetValue( nCriterion );

					// Find the set of virtual words that matches the criterion
					FOR_EACH_DICT( m_VirtualWordLists[i].m_WordDefinitions, i2 )
					{
						if (Matcher_Match( m_VirtualWordLists[i].m_WordDefinitions.GetElementName(i2), pszValue ))
						{
							return ResolveVirtualWordDefinition( pSpeaker, m_VirtualWordLists[i].m_WordDefinitions[i2] ).szWord;
						}
					}

					// Return the default
					return ResolveVirtualWordDefinition( pSpeaker, m_VirtualWordLists[i].m_DefaultWordDefinition ).szWord;
				}
			}
		}

		CGWarning( 0, CON_GROUP_CHOREO, "Choreo sentence can't find virtual word \"%s\"\n", pszWord );
		return "";
	}

	int ResolveVirtualCondition( CBaseEntity *pSpeaker, int nConditionType, const char *pszCondition, int nWordCount )
	{
		switch (nConditionType)
		{
			default:
			case ConditionType_None:
				// Randomize between each word
				return RandomInt( 0, nWordCount - 1 );
				break;

			case ConditionType_Context:
				// Return context as integer
				if (pSpeaker)
				{
					if (pSpeaker->FindContextByName( pszCondition ) == -1)
					{
						// Check if this is a prerequisite, and if it is, then apply it
						FOR_EACH_VEC( m_ContextPrerequisites, i )
						{
							if (FStrEq( m_ContextPrerequisites[i].pszContextName, pszCondition ))
							{
								pSpeaker->AddContext( pszCondition, UTIL_VarArgs("%i", ResolveVirtualCondition(pSpeaker, m_ContextPrerequisites[i].nConditionType, m_ContextPrerequisites[i].pszCondition, nWordCount)));
							}
						}
					}

					int nContext = clamp( atoi( pSpeaker->GetContextValue( pszCondition ) ), 0, nWordCount - 1 );
					return nContext;
				}
				break;

			case ConditionType_VScript:
				{
					// Return VScript code result
					g_pScriptVM->SetValue( "_choreo_val", "" );

					HSCRIPT hCreateChainScript = g_pScriptVM->CompileScript( UTIL_VarArgs( "_choreo_val = (%s)", pszCondition ) );
					g_pScriptVM->Run( hCreateChainScript, pSpeaker ? pSpeaker->GetOrCreatePrivateScriptScope() : NULL );

					ScriptVariant_t scriptVar;
					g_pScriptVM->GetValue( "_choreo_val", &scriptVar );

					g_pScriptVM->ClearValue( "_choreo_val" );

					if (scriptVar.m_int < 0 || scriptVar.m_int >= nWordCount)
					{
						Warning("Choreo sentence script var %i from '%s' out of range\n", scriptVar.m_int, pszCondition );
						scriptVar.m_int = 0;
					}

					return scriptVar.m_int;
				}
				break;

			case ConditionType_DistTo:
			case ConditionType_DirTo:
			case ConditionType_GridX:
			case ConditionType_GridY:
				{
					CBaseEntity *pTarget = gEntList.FindEntityByName( NULL, pszCondition, pSpeaker );
					if (pSpeaker && pTarget)
					{
						if (nConditionType == ConditionType_DistTo)
						{
							// Convert to meters
							return (pSpeaker->GetAbsOrigin() - pTarget->GetAbsOrigin()).Length() / 52.49344;
						}
						else if (nConditionType == ConditionType_DirTo)
						{
							Vector vecDir = (pSpeaker->GetAbsOrigin() - pTarget->GetAbsOrigin());
							return vecDir.y;
						}
						else if (nConditionType == ConditionType_GridX)
						{
							return (int)(pTarget->GetAbsOrigin().x / 10.0f) % 20;
						}
						else if (nConditionType == ConditionType_GridY)
						{
							return (int)(pTarget->GetAbsOrigin().y / 10.0f) % 20;
						}
					}
				}
				break;
		}

		return 0;
	}

	//----------------------------------------------------------------------------

	enum
	{
		ConditionType_None,		// No condition (random)
		ConditionType_Context,	// Word should match value from specified context
		ConditionType_VScript,	// Word should match value returned from specified VScript code

		// HL2 sentence parameters
		ConditionType_DistTo,	// Word should match distance to specified target
		ConditionType_DirTo,	// Word should match direction to specified target
		ConditionType_GridX,	// Word should match world X axis grid of specified target
		ConditionType_GridY,	// Word should match world Y axis grid of specified target
	};

	int DeduceCondition( const char *pszCondition, int nNumChars )
	{
		if (V_strncmp( pszCondition, "context", nNumChars ) == 0)
			return ConditionType_Context;
		else if (V_strncmp( pszCondition, "vscript", nNumChars ) == 0)
			return ConditionType_VScript;
		else if (V_strncmp( pszCondition, "dist_to", nNumChars ) == 0)
			return ConditionType_DistTo;
		else if (V_strncmp( pszCondition, "dir_to", nNumChars ) == 0)
			return ConditionType_DirTo;
		else if (V_strncmp( pszCondition, "gridx", nNumChars ) == 0)
			return ConditionType_GridX;
		else if (V_strncmp( pszCondition, "gridy", nNumChars ) == 0)
			return ConditionType_GridY;

		return ConditionType_None;
	}

	struct VirtualWord_t
	{
		char szWord[MAX_CHOREO_SENTENCE_VIRTUAL_WORD_LEN];
	};

	struct VirtualWordDefinition_t
	{
		VirtualWordDefinition_t() {}
		VirtualWordDefinition_t( const VirtualWordDefinition_t &src )
		{
			pszCondition = src.pszCondition;
			nConditionType = src.nConditionType;
			m_Words.RemoveAll();
			m_Words.AddVectorToTail( src.m_Words );
		}

		const char *pszCondition;
		int nConditionType;

		CUtlVector< VirtualWord_t > m_Words;
	};

	struct VirtualWordDefinitionList_t
	{
		VirtualWordDefinitionList_t() {}
		VirtualWordDefinitionList_t( const VirtualWordDefinitionList_t &src )
		{
			pszCriterion = src.pszCriterion;
			m_DefaultWordDefinition = src.m_DefaultWordDefinition;

			m_WordDefinitions.RemoveAll();
			m_WordDefinitions.EnsureCapacity( src.m_WordDefinitions.Count() );
			FOR_EACH_DICT_FAST( src.m_WordDefinitions, i )
				m_WordDefinitions.Insert( src.m_WordDefinitions.GetElementName( i ), src.m_WordDefinitions[i] );
		}

		const char *pszCriterion;

		VirtualWordDefinition_t m_DefaultWordDefinition;
		CUtlDict< VirtualWordDefinition_t, int > m_WordDefinitions;
	};

	struct SentenceContextPrerequisite_t
	{
		SentenceContextPrerequisite_t() {}
		SentenceContextPrerequisite_t( const SentenceContextPrerequisite_t &src )
		{
			pszContextName = src.pszContextName;
			pszCondition = src.pszCondition;
			nConditionType = src.nConditionType;
		}

		const char *pszContextName;
		const char *pszCondition;
		int nConditionType;
	};

	//----------------------------------------------------------------------------

	const char *FindString( const char *string )
	{
		unsigned short i = m_Strings.Find( string );
		return i == m_Strings.InvalidIndex() ? NULL : m_Strings[i];
	}

	const char *AllocateString( const char *string )
	{
		int i = m_Strings.Find( string );
		if ( i != m_Strings.InvalidIndex() )
		{
			return m_Strings[i];
		}

		int len = Q_strlen( string );
		char *out = new char[ len + 1 ];
		Q_memcpy( out, string, len );
		out[ len ] = 0;

		return m_Strings[ m_Strings.Insert( out ) ];
	}

	//----------------------------------------------------------------------------

	const CUtlDict< ChoreoSentence_t, int > &GetSentences() { return m_Sentences; }

private:

	// Context prerequisites to be applied if not already applied
	CUtlVector< SentenceContextPrerequisite_t > m_ContextPrerequisites;

	// Embedded lists of virtual words based on a criterion
	CUtlDict< VirtualWordDefinitionList_t, int > m_VirtualWordLists;

	// Lists of virtual words (does not include nested ones)
	CUtlDict< VirtualWordDefinition_t, int > m_VirtualWords;

	// Sentences themselves
	CUtlDict< ChoreoSentence_t, int > m_Sentences;

	// Dedicated strings, copied from game string pool
	CUtlRBTree<const char *> m_Strings;

};

static CChoreoSentenceSystem g_ChoreoSentenceSystem;

//-----------------------------------------------------------------------------

CON_COMMAND( choreosentence_reload, "Reloads choreo sentences" )
{
	//int nStringCount = g_ChoreoSentenceSystem.GetStringCount();
	g_ChoreoSentenceSystem.PurgeSentences();

	//g_ChoreoSentenceSystem.ReserveStrings( nStringCount );
	g_ChoreoSentenceSystem.LoadChoreoSentenceManifest( "scripts/choreosentences_manifest.txt" );
}

CON_COMMAND( choreosentence_dump, "Prints all choreo sentence stuff" )
{
	g_ChoreoSentenceSystem.PrintEverything();
}

CON_COMMAND( choreosentence_dump_strings, "Prints all strings allocated by choreo sentences" )
{
	g_ChoreoSentenceSystem.PrintStrings();
}

//------------------------------------------------------------------------------
// Purpose : 
//------------------------------------------------------------------------------
static int AutoCompleteChoreoSentences(const char *cmdname, CUtlVector< CUtlString > &commands, CUtlRBTree< CUtlString > &symbols, char *substring, int checklen = 0)
{
	FOR_EACH_DICT_FAST( g_ChoreoSentenceSystem.GetSentences(), i )
	{
		CUtlString sym = g_ChoreoSentenceSystem.GetSentences().GetElementName( i );

		if (Q_strnicmp( sym, substring, checklen ) != 0)
			continue;

		int idx = symbols.Find( sym );
		if (idx == symbols.InvalidIndex())
		{
			symbols.Insert( sym );
		}

		// Too many
		if (symbols.Count() >= COMMAND_COMPLETION_MAXITEMS)
			break;
	}

	// Now fill in the results
	for (int i = symbols.FirstInorder(); i != symbols.InvalidIndex(); i = symbols.NextInorder(i))
	{
		const char *name = symbols[i].String();

		char buf[512];
		Q_snprintf( buf, sizeof( buf ), "%s %s", cmdname, name );
		Q_strlower( buf );

		CUtlString command;
		command = buf;
		commands.AddToTail(command);
	}

	return symbols.Count();
}

class CChoreoSentenceAutoCompletionFunctor : public ICommandCallback, public ICommandCompletionCallback
{
public:
	virtual void CommandCallback( const CCommand &command )
	{
		if (command.ArgC() != 2)
		{
			Msg( "Format: choreosentence_play <sentence name>\n" );
			return;
		}

		CBasePlayer *pPlayer = UTIL_GetLocalPlayer();
		if (pPlayer)
		{
			const ChoreoSentence_t *pSentence = LookupChoreoSentence( pPlayer, command.Arg( 1 ) );
			if (pSentence)
				PrecacheChoreoSentence( *pSentence );

			pPlayer->PlayChoreoSentenceScene( command.Arg( 1 ) );
		}
	}

	virtual int CommandCompletionCallback( const char *partial, CUtlVector< CUtlString > &commands )
	{
		if ( !g_pGameRules )
		{
			return 0;
		}

		const char *cmdname = "choreosentence_play";

		char *substring = (char *)partial;
		if ( Q_strstr( partial, cmdname ) )
		{
			substring = (char *)partial + strlen( cmdname ) + 1;
		}

		int checklen = Q_strlen( substring );

		extern bool UtlStringLessFunc( const CUtlString & lhs, const CUtlString & rhs );
		CUtlRBTree< CUtlString > symbols( 0, 0, UtlStringLessFunc );

		return AutoCompleteChoreoSentences(cmdname, commands, symbols, substring, checklen);
	}
};

static CChoreoSentenceAutoCompletionFunctor g_ChoreoSentenceAutoComplete;
static ConCommand choreosentence_play("choreosentence_play", &g_ChoreoSentenceAutoComplete, "Plays the specified choreo sentence on the player", FCVAR_CHEAT, &g_ChoreoSentenceAutoComplete );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const char *ChoreoSentence_t::GetWordString( CBaseEntity *pSpeaker, int i ) const
{
	const char *pszExtension = "";
	if (V_strrchr( pszPrefix, CORRECT_PATH_SEPARATOR ))
	{
		// Use waves if our prefix is a path
		pszExtension = ".wav";
	}

	// TODO: Something more elaborate than UTIL_VarArgs?
	if (m_Words[i].bVirtual)
	{
		const char *pszVirtualWord = g_ChoreoSentenceSystem.ResolveVirtualWord( pSpeaker, m_Words[i].pszWord );
		return UTIL_VarArgs( "%s%s%s", pszPrefix, pszVirtualWord, pszExtension );
	}

	return UTIL_VarArgs( "%s%s%s", pszPrefix, m_Words[i].pszWord, pszExtension );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void LoadChoreoSentenceFile( const char *pszFile )
{
	g_ChoreoSentenceSystem.LoadChoreoSentenceFile( pszFile );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
const ChoreoSentence_t *LookupChoreoSentence( CBaseEntity *pSpeaker, const char *pszSentenceName )
{
	return g_ChoreoSentenceSystem.LookupChoreoSentence( pSpeaker, pszSentenceName );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void PrecacheChoreoSentence( const ChoreoSentence_t &sentence )
{
	FOR_EACH_VEC( sentence.m_Words, i )
	{
		if (sentence.m_Words[i].bVirtual)
		{
			// Precache all virtual words
			const char *pszExtension = "";
			if (V_strrchr( sentence.pszPrefix, CORRECT_PATH_SEPARATOR ))
				pszExtension = ".wav";

			g_ChoreoSentenceSystem.PrecacheVirtualWord( sentence.m_Words[i].pszWord, sentence.pszPrefix, pszExtension );
		}
		else
		{
			CBaseEntity::PrecacheScriptSound( sentence.GetWordString( NULL, i ) );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ParseChoreoSentence( CBaseEntity *pSpeaker, const char *pszRawSentence, ChoreoSentence_t &sentence )
{
	if (pszRawSentence == NULL || *pszRawSentence == NULL)
		return false;

	char szSentence[256];

	// First, try getting the prefix
	const char *pColon = V_strnchr( pszRawSentence, ':', V_strlen( pszRawSentence ) );
	if (pColon)
	{
		// Sentence is everything after colon
		Q_strncpy( szSentence, pColon + 1, sizeof( szSentence ) );

		// Copy everything before colon for prefix
		char szPathName[MAX_PATH];
		V_strncpy( szPathName, pszRawSentence, pColon - pszRawSentence + 1 );
		V_FixSlashes( szPathName );
		sentence.pszPrefix = g_ChoreoSentenceSystem.AllocateString( szPathName );
	}
	else
	{
		// It's all one sentence
		Q_strncpy( szSentence, pszRawSentence, sizeof( szSentence ) );
	}

	// Now get any parameters
	const char *pSemiColon = V_strnchr( szSentence, ';', sizeof( szSentence ) );
	if (pSemiColon)
	{
		// Caption is whatever's after the semicolon
		const char *pszCaption = pSemiColon+1;
		if (pszCaption[0] == ' ')
			pszCaption++;

		sentence.pszCaption = g_ChoreoSentenceSystem.AllocateString( pszCaption );

		// Replace semicolon with null terminator
		szSentence[pSemiColon - szSentence] = '\0';
	}

	// Next, split up the sentence itself
	bool success = ParseChoreoSentenceContents( pSpeaker, szSentence, sentence );

	return success;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ParseChoreoSentenceContents( CBaseEntity *pSpeaker, char *pszSentence, ChoreoSentence_t &sentence )
{
	int nCurVol = 100, nCurPitch = 100;

	char *pszToken = strtok( pszSentence, " " );
	for (; pszToken != NULL; pszToken = strtok( NULL, " " ))
	{
		if (!pszToken || !*pszToken)
			continue;

		// Check if this is a command number
		if (pszToken[0] == '(')
		{
			pszToken++;

			// Remove closing parentheses
			//int end = V_strlen( pszToken )-1;
			//if (pszToken[end] == ')')
			//	pszToken[end] = '\0';

			int nNum = atoi( pszToken + 1 );
			if (nNum > 0)
			{
				switch (pszToken[0])
				{
					// TODO: Recognize e, t, etc.?
					case 'v':	nCurVol = nNum; break;
					case 'p':	nCurPitch = nNum; break;
				}
				continue;
			}
			else
			{
				Msg( "Zero command number in %s\n", pszSentence );
			}
		}

		int nWord = sentence.m_Words.AddToTail();

		sentence.m_Words[nWord].nVol = nCurVol;
		sentence.m_Words[nWord].nPitch = nCurPitch;

		// Check if this is virtual
		if (IsVirtualWord( pszToken ))
			sentence.m_Words[nWord].bVirtual = true;
		
		// Check for periods or commas
		int end = V_strlen( pszToken )-1;
		if (pszToken[end] == ',')
		{
			int nWord2 = sentence.m_Words.AddToTail();
			sentence.m_Words[nWord2].pszWord = g_ChoreoSentenceSystem.AllocateString( "_comma" );
			sentence.m_Words[nWord2].nVol = nCurVol;
			sentence.m_Words[nWord2].nPitch = nCurPitch;
			pszToken[end] = '\0';
		}
		else if (pszToken[end] == '.')
		{
			int nWord2 = sentence.m_Words.AddToTail();
			sentence.m_Words[nWord2].pszWord = g_ChoreoSentenceSystem.AllocateString( "_period" );
			sentence.m_Words[nWord2].nVol = nCurVol;
			sentence.m_Words[nWord2].nPitch = nCurPitch;
			pszToken[end] = '\0';
		}

		sentence.m_Words[nWord].pszWord = g_ChoreoSentenceSystem.AllocateString( pszToken );
	}

	return sentence.m_Words.Count() > 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float GetChoreoSentenceDuration( CBaseFlex *pSpeaker, const ChoreoSentence_t &sentence )
{
	const char *actormodel = (pSpeaker ? STRING( pSpeaker->GetModelName() ) : NULL);
	float flLength = 0.0f;

	FOR_EACH_VEC( sentence.m_Words, i )
	{
		//float duration = CBaseEntity::GetSoundDuration( sentence.GetWordString(pSpeaker, i), actormodel );

		float duration;
		const char *pszWord = sentence.GetWordString( pSpeaker, i );

		// For now, call the engine functions manually instead of using CBaseEntity::GetSoundDuration so that we could get around the WaveTrace warning
		if ( V_stristr( pszWord, ".wav" ) || V_stristr( pszWord, ".mp3" ) )
		{
			duration = enginesound->GetSoundDuration( PSkipSoundChars( pszWord ) );
		}
		else
		{
			extern ISoundEmitterSystemBase *soundemitterbase;
			duration = enginesound->GetSoundDuration( PSkipSoundChars( soundemitterbase->GetWavFileForSound( pszWord, actormodel ) ) );
		}

		flLength += duration;
	}

	return flLength;
}
