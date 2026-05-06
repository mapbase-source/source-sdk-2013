//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: VCD-based sentences.
//
//=============================================================================//

#ifndef CHOREOSENTENCE_H
#define CHOREOSENTENCE_H

#include "cbase.h"


#define MAX_CHOREO_SENTENCE_PREFIX_LEN 64
#define MAX_CHOREO_SENTENCE_WORD_LEN 32
#define MAX_CHOREO_SENTENCE_CAPTION_LEN 64

#define MAX_CHOREO_SENTENCE_VIRTUAL_WORD_LEN 32

struct ChoreoSentenceWord_t
{
	ChoreoSentenceWord_t() {}
	ChoreoSentenceWord_t( const ChoreoSentenceWord_t &src )
	{
		pszWord = src.pszWord;
		nPitch = src.nPitch; nVol = src.nVol;
		bVirtual = src.bVirtual;
	}

	const char *pszWord;
	int nPitch, nVol = 100;
	bool bVirtual = false;
};

struct ChoreoSentence_t 
{
	ChoreoSentence_t() {}
	ChoreoSentence_t( const ChoreoSentence_t &src )
	{
		pszName = src.pszName;
		pszPrefix = src.pszPrefix;
		pszCaption = src.pszCaption;
		m_Words.RemoveAll();
		m_Words.AddVectorToTail( src.m_Words );
	}

	const char *GetWordString( CBaseEntity *pSpeaker, int i ) const;

	CUtlVector< ChoreoSentenceWord_t > m_Words;
	const char *pszName;
	const char *pszPrefix;
	const char *pszCaption;
};

//----------------------------------------------------------------------------

extern void LoadChoreoSentenceFile( const char *pszFile );

extern const ChoreoSentence_t *LookupChoreoSentence( CBaseEntity *pSpeaker, const char *pszSentenceName );

extern void PrecacheChoreoSentence( const ChoreoSentence_t &sentence );

bool ParseChoreoSentence( CBaseEntity *pSpeaker, const char *pszRawSentence, ChoreoSentence_t &sentence );
bool ParseChoreoSentenceContents( CBaseEntity *pSpeaker, char *pszSentence, ChoreoSentence_t &sentence );

extern float GetChoreoSentenceDuration( CBaseFlex *pSpeaker, const ChoreoSentence_t &sentence );

#endif // CHOREOSENTENCE_H
