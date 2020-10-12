#include "shaderlib/cshader.h"
class flow_watercheap_ps20_Static_Index
{
private:
	int m_nMULTITEXTURE;
#ifdef _DEBUG
	bool m_bMULTITEXTURE;
#endif
public:
	void SetMULTITEXTURE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nMULTITEXTURE = i;
#ifdef _DEBUG
		m_bMULTITEXTURE = true;
#endif
	}
	void SetMULTITEXTURE( bool i )
	{
		m_nMULTITEXTURE = i ? 1 : 0;
#ifdef _DEBUG
		m_bMULTITEXTURE = true;
#endif
	}
private:
	int m_nFRESNEL;
#ifdef _DEBUG
	bool m_bFRESNEL;
#endif
public:
	void SetFRESNEL( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFRESNEL = i;
#ifdef _DEBUG
		m_bFRESNEL = true;
#endif
	}
	void SetFRESNEL( bool i )
	{
		m_nFRESNEL = i ? 1 : 0;
#ifdef _DEBUG
		m_bFRESNEL = true;
#endif
	}
private:
	int m_nBLEND;
#ifdef _DEBUG
	bool m_bBLEND;
#endif
public:
	void SetBLEND( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nBLEND = i;
#ifdef _DEBUG
		m_bBLEND = true;
#endif
	}
	void SetBLEND( bool i )
	{
		m_nBLEND = i ? 1 : 0;
#ifdef _DEBUG
		m_bBLEND = true;
#endif
	}
private:
	int m_nREFRACTALPHA;
#ifdef _DEBUG
	bool m_bREFRACTALPHA;
#endif
public:
	void SetREFRACTALPHA( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nREFRACTALPHA = i;
#ifdef _DEBUG
		m_bREFRACTALPHA = true;
#endif
	}
	void SetREFRACTALPHA( bool i )
	{
		m_nREFRACTALPHA = i ? 1 : 0;
#ifdef _DEBUG
		m_bREFRACTALPHA = true;
#endif
	}
private:
	int m_nHDRTYPE;
#ifdef _DEBUG
	bool m_bHDRTYPE;
#endif
public:
	void SetHDRTYPE( int i )
	{
		Assert( i >= 0 && i <= 2 );
		m_nHDRTYPE = i;
#ifdef _DEBUG
		m_bHDRTYPE = true;
#endif
	}
	void SetHDRTYPE( bool i )
	{
		m_nHDRTYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bHDRTYPE = true;
#endif
	}
private:
	int m_nFLOWMAP;
#ifdef _DEBUG
	bool m_bFLOWMAP;
#endif
public:
	void SetFLOWMAP( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLOWMAP = i;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif
	}
	void SetFLOWMAP( bool i )
	{
		m_nFLOWMAP = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLOWMAP = true;
#endif
	}
private:
	int m_nFLOW_DEBUG;
#ifdef _DEBUG
	bool m_bFLOW_DEBUG;
#endif
public:
	void SetFLOW_DEBUG( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nFLOW_DEBUG = i;
#ifdef _DEBUG
		m_bFLOW_DEBUG = true;
#endif
	}
	void SetFLOW_DEBUG( bool i )
	{
		m_nFLOW_DEBUG = i ? 1 : 0;
#ifdef _DEBUG
		m_bFLOW_DEBUG = true;
#endif
	}
public:
	flow_watercheap_ps20_Static_Index( )
	{
#ifdef _DEBUG
		m_bMULTITEXTURE = false;
#endif // _DEBUG
		m_nMULTITEXTURE = 0;
#ifdef _DEBUG
		m_bFRESNEL = false;
#endif // _DEBUG
		m_nFRESNEL = 0;
#ifdef _DEBUG
		m_bBLEND = false;
#endif // _DEBUG
		m_nBLEND = 0;
#ifdef _DEBUG
		m_bREFRACTALPHA = false;
#endif // _DEBUG
		m_nREFRACTALPHA = 0;
#ifdef _DEBUG
		m_bHDRTYPE = false;
#endif // _DEBUG
		m_nHDRTYPE = 0;
#ifdef _DEBUG
		m_bFLOWMAP = false;
#endif // _DEBUG
		m_nFLOWMAP = 0;
#ifdef _DEBUG
		m_bFLOW_DEBUG = false;
#endif // _DEBUG
		m_nFLOW_DEBUG = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllStaticVarsDefined = m_bMULTITEXTURE && m_bFRESNEL && m_bBLEND && m_bREFRACTALPHA && m_bHDRTYPE && m_bFLOWMAP && m_bFLOW_DEBUG;
		Assert( bAllStaticVarsDefined );
#endif // _DEBUG
		return ( 4 * m_nMULTITEXTURE ) + ( 8 * m_nFRESNEL ) + ( 16 * m_nBLEND ) + ( 32 * m_nREFRACTALPHA ) + ( 64 * m_nHDRTYPE ) + ( 192 * m_nFLOWMAP ) + ( 384 * m_nFLOW_DEBUG ) + 0;
	}
};
#define shaderStaticTest_flow_watercheap_ps20 psh_forgot_to_set_static_MULTITEXTURE + psh_forgot_to_set_static_FRESNEL + psh_forgot_to_set_static_BLEND + psh_forgot_to_set_static_REFRACTALPHA + psh_forgot_to_set_static_HDRTYPE + psh_forgot_to_set_static_FLOWMAP + psh_forgot_to_set_static_FLOW_DEBUG + 0
class flow_watercheap_ps20_Dynamic_Index
{
private:
	int m_nHDRENABLED;
#ifdef _DEBUG
	bool m_bHDRENABLED;
#endif
public:
	void SetHDRENABLED( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nHDRENABLED = i;
#ifdef _DEBUG
		m_bHDRENABLED = true;
#endif
	}
	void SetHDRENABLED( bool i )
	{
		m_nHDRENABLED = i ? 1 : 0;
#ifdef _DEBUG
		m_bHDRENABLED = true;
#endif
	}
private:
	int m_nPIXELFOGTYPE;
#ifdef _DEBUG
	bool m_bPIXELFOGTYPE;
#endif
public:
	void SetPIXELFOGTYPE( int i )
	{
		Assert( i >= 0 && i <= 1 );
		m_nPIXELFOGTYPE = i;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
	void SetPIXELFOGTYPE( bool i )
	{
		m_nPIXELFOGTYPE = i ? 1 : 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = true;
#endif
	}
public:
	flow_watercheap_ps20_Dynamic_Index()
	{
#ifdef _DEBUG
		m_bHDRENABLED = false;
#endif // _DEBUG
		m_nHDRENABLED = 0;
#ifdef _DEBUG
		m_bPIXELFOGTYPE = false;
#endif // _DEBUG
		m_nPIXELFOGTYPE = 0;
	}
	int GetIndex()
	{
		// Asserts to make sure that we aren't using any skipped combinations.
		// Asserts to make sure that we are setting all of the combination vars.
#ifdef _DEBUG
		bool bAllDynamicVarsDefined = m_bHDRENABLED && m_bPIXELFOGTYPE;
		Assert( bAllDynamicVarsDefined );
#endif // _DEBUG
		return ( 1 * m_nHDRENABLED ) + ( 2 * m_nPIXELFOGTYPE ) + 0;
	}
};
#define shaderDynamicTest_flow_watercheap_ps20 psh_forgot_to_set_dynamic_HDRENABLED + psh_forgot_to_set_dynamic_PIXELFOGTYPE + 0
