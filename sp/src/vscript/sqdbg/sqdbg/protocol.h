//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//

#ifndef SQDBG_DAP_H
#define SQDBG_DAP_H

#include "json.h"

#define DAP_HEADER_START "Content-Length: "
#define DAP_HEADER_END "\r\n\r\n"

#define STRCMP( s, StrLiteral )\
	memcmp( (s), (StrLiteral), sizeof(StrLiteral)-1 )

inline void DAP_Serialise( json_table_t *table, char **jsonptr, int *jsonlen )
{
	int contentSize = GetJSONStringSize( table );
	int idx = countdigits( contentSize );
	int size = STRLEN( DAP_HEADER_START DAP_HEADER_END ) + idx + contentSize + 1;

	char *mem = (char*)sqdbg_malloc( ( size + 3 ) & ~3 );

	memcpy( mem, DAP_HEADER_START, STRLEN( DAP_HEADER_START ) );
	idx += STRLEN( DAP_HEADER_START );

	for ( int i = idx - 1; contentSize; )
	{
		char c = contentSize % 10;
		contentSize /= 10;
		mem[i--] = '0' + c;
	}

	memcpy( mem + idx, DAP_HEADER_END, STRLEN( DAP_HEADER_END ) );
	idx += STRLEN( DAP_HEADER_END );

	idx = JSONStringify( table, mem, size, idx );
	mem[idx] = 0;

	Assert( idx == size-1 );

	*jsonptr = mem;
	*jsonlen = idx;
}

inline void DAP_Free( char *jsonptr, int jsonlen )
{
	sqdbg_free( jsonptr, ( ( jsonlen + 1 ) + 3 ) & ~3 );
}

inline bool DAP_ReadHeader( char **ppMsg, int *pLength )
{
	char *pMsg = *ppMsg;
	const char *pMemEnd = *ppMsg + *pLength;
	char *pEnd;
	int nContentLength = 0;

	if ( pMsg + STRLEN( DAP_HEADER_START ) >= pMemEnd )
		return false;

	if ( STRCMP( pMsg, DAP_HEADER_START ) != 0 )
		goto invalid;

	pMsg += STRLEN( DAP_HEADER_START );
	pEnd = pMsg;

	for (;;)
	{
		if ( pEnd + 4 >= pMemEnd )
			return false;

		if ( IN_RANGE_CHAR( *(unsigned char*)pEnd, '0', '9' ) )
		{
			nContentLength = nContentLength * 10 + *pEnd - '0';
		}
		else if ( *(unsigned int*)pEnd == htonl(0x0d0a0d0a) )
		{
			if ( nContentLength <= 0 )
				goto invalid;

			Assert( pEnd < pMemEnd && pEnd - pMsg < (int)FMT_UINT32_LEN && nContentLength > 0 );

			*pLength = nContentLength;
			*ppMsg = pEnd + 4;

			return true;
		}

		pEnd++;

		// Header end can't be further than maximum content size
		// This is most likely a malformed message
		if ( pEnd - pMsg >= (int)FMT_UINT32_LEN )
			goto invalid;
	}

invalid:
	// Signal that the client needs to be dropped
	*pLength = 0x7fffffff;
	*ppMsg = pMsg;
	return true;
}

#undef DAP_HEADER_START
#undef DAP_HEADER_END

#undef STRCMP


#define DAP_START_REQUEST( _seq, _cmd ) \
	{ \
		json_table_t packet(4); \
		packet.SetInt( "seq", _seq ); \
		packet.SetString( "type", "request" ); \
		packet.SetString( "command", _cmd );

#define _DAP_START_RESPONSE( _seq, _cmd, _suc, _elemcount ) \
	{ \
		json_table_t packet( 4 + _elemcount ); \
		packet.SetInt( "request_seq", _seq ); \
		packet.SetString( "type", "response" ); \
		packet.SetString( "command", _cmd ); \
		packet.SetBool( "success", _suc );

#define DAP_START_RESPONSE( _seq, _cmd ) \
		_DAP_START_RESPONSE( _seq, _cmd, true, 1 );

#define DAP_ERROR_RESPONSE( _seq, _cmd ) \
		_DAP_START_RESPONSE( _seq, _cmd, false, 1 );

#define DAP_ERROR_BODY( _id, _fmt, _elemcount ) \
		json_table_t body(1); \
		json_table_t error( 2 + _elemcount ); \
		packet.SetTable( "body", body ); \
		body.SetTable( "error", error ); \
		error.SetInt( "id", _id ); \
		error.SetString( "format", _fmt ); \

#define DAP_START_EVENT( _seq, _ev ) \
	{ \
		json_table_t packet(4); \
		packet.SetInt( "seq", _seq ); \
		packet.SetString( "type", "event" ); \
		packet.SetString( "event", _ev );

#define DAP_SET( _key, _val ) \
		packet.Set( _key, _val );

#define DAP_SET_TABLE( _val, _elemcount ) \
		json_table_t _val( _elemcount ); \
		packet.SetTable( #_val, _val );

#define DAP_SEND() \
		{ \
			char *jsonptr; \
			int jsonlen; \
			DAP_Serialise( &packet, &jsonptr, &jsonlen ); \
			Send( jsonptr, jsonlen ); \
			DAP_Free( jsonptr, jsonlen ); \
		} \
	}

#endif // SQDBG_DAP_H
