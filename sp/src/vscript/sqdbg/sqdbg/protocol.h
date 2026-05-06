//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//

#ifndef SQDBG_DAP_H
#define SQDBG_DAP_H

#define DAP_HEADER_CONTENTLENGTH "Content-Length"
#define DAP_HEADER_END "\r\n\r\n"
#define DAP_HEADER_MAXSIZE ( STRLEN(DAP_HEADER_CONTENTLENGTH ": ") + STRLEN(DAP_HEADER_END) + FMT_UINT32_LEN )

inline void DAP_Serialise( CBuffer *buffer )
{
	Assert( buffer->Size() > 0 && buffer->Size() < INT_MAX );

	char *mem = buffer->Base();
	int contentSize = buffer->Size() - DAP_HEADER_MAXSIZE;
	int digits = countdigits( contentSize );
	int padding = FMT_UINT32_LEN - digits;

	int nearest = 10;
	while ( contentSize >= nearest )
		nearest *= 10;

	contentSize += padding;

	if ( contentSize >= nearest )
	{
		// Padding between header and content increased content size digits,
		// add padding in the end to match
		padding--;
		digits++;
		buffer->base.Ensure( buffer->Size() + 1 );
		mem[buffer->size++] = ' ';
	}

	memcpy( mem, DAP_HEADER_CONTENTLENGTH ": ", STRLEN(DAP_HEADER_CONTENTLENGTH ": ") );

	int idx = STRLEN(DAP_HEADER_CONTENTLENGTH ": ") + digits;

	for ( int i = idx - 1; contentSize; )
	{
		char c = contentSize % 10;
		contentSize /= 10;
		mem[i--] = '0' + c;
	}

	memcpy( mem + idx, DAP_HEADER_END, STRLEN(DAP_HEADER_END) );
	idx += STRLEN(DAP_HEADER_END);
	memset( mem + idx, ' ', padding );
}

inline void DAP_Free( CBuffer *buffer )
{
	buffer->size = 0;
}

static inline int ParseFieldName( const char *pMemEnd, char *pStart )
{
	char *c = pStart;

	for (;;)
	{
		if ( IN_RANGE_CHAR( ((unsigned char*)c)[0], 0x20, 0x7E ) )
		{
			if ( c + 1 >= pMemEnd )
				return -1;

			if ( c[0] == ':' )
			{
				if ( c[1] == ' ' )
					return c - pStart;

				return 0;
			}

			c++;
		}
		else
		{
			return 0;
		}
	}
}

static inline int ParseFieldValue( const char *pMemEnd, char *pStart )
{
	char *c = pStart;

	for (;;)
	{
		if ( c + 1 >= pMemEnd )
			return -1;

		if ( c[0] == '\n' )
			return 0;

		if ( c[0] == '\r' && c[1] == '\n' )
			return c - pStart;

		c++;
	}
}

inline bool DAP_ReadHeader( char **ppMsg, int *pLength )
{
	char *pMsg = *ppMsg;
	const char *pMemEnd = pMsg + *pLength;
	int nContentLength = 0;

	for (;;)
	{
		int len = ParseFieldName( pMemEnd, pMsg );

		if ( len == 0 )
			goto invalid;

		if ( len == -1 )
			return false;

		if ( len == (int)STRLEN(DAP_HEADER_CONTENTLENGTH) &&
				!memcmp( pMsg, DAP_HEADER_CONTENTLENGTH, STRLEN(DAP_HEADER_CONTENTLENGTH) ) )
		{
			// Duplicate length field
			if ( nContentLength )
				goto ignore;

			pMsg += len + 2;

			for ( char *pStart = pMsg;; )
			{
				if ( pMsg >= pMemEnd )
					return false;

				if ( IN_RANGE_CHAR( *pMsg, '0', '9' ) )
				{
					nContentLength = nContentLength * 10 + *pMsg - '0';
					pMsg++;

					if ( pMsg - pStart > (int)FMT_UINT32_LEN )
						goto invalid;
				}
				// Strict - no whitespace allowed
				else
				{
					if ( pMsg + 1 >= pMemEnd )
						return false;

					if ( pMsg[0] == '\r' && pMsg[1] == '\n' )
					{
						if ( nContentLength <= 0 )
							goto invalid;

						*pLength = nContentLength;
						pMsg += 2;
						break;
					}
					else
					{
						goto invalid;
					}
				}
			}
		}
		// Ignore unknown header fields
		else
		{
ignore:
			pMsg += len + 2;

			len = ParseFieldValue( pMemEnd, pMsg );

			if ( len == 0 )
				goto invalid;

			if ( len == -1 )
				return false;

			pMsg += len + 2;
		}

		if ( pMsg + 1 >= pMemEnd )
			return false;

		if ( pMsg[0] == '\r' && pMsg[1] == '\n' )
		{
			*ppMsg = pMsg + 2;
			return true;
		}
	}

invalid:
	// Signal that the client needs to be dropped
	*pLength = -1;
	*ppMsg = pMsg;
	return true;
}

#ifdef SQDBG_VALIDATE_SENT_MSG
inline void DAP_Test( CScratch< true, JSON_SCRATCH_CHUNK_SIZE > *scratch, CBuffer *buffer )
{
	char *pMsg = buffer->Base();
	int nLength = buffer->Size();

	bool res = DAP_ReadHeader( &pMsg, &nLength );
	Assert( res && nLength < buffer->Size() );

	if ( res )
	{
		json_table_t table;
		JSONParser parser( scratch, pMsg, nLength, &table );

		AssertMsg1( !parser.GetError(), "%s", parser.GetError() );
	}
}
#else
#define DAP_Test(...) (void)0
#endif

#define _DAP_INIT_BUF( _buf ) \
	CBufTmpCache _bufcache( (_buf) ); \
	(_buf)->size = DAP_HEADER_MAXSIZE; \
	(void)0

#define DAP_START_REQUEST( _seq, _cmd ) \
{ \
	_DAP_INIT_BUF( &m_SendBuf ); \
	{ \
		wjson_table_t packet( m_SendBuf ); \
		packet.SetInt( "seq", _seq ); \
		packet.SetString( "type", "request" ); \
		packet.SetString( "command", _cmd );

#define _DAP_START_RESPONSE( _seq, _cmd, _suc ) \
if ( IsClientConnected() ) \
{ \
	_DAP_INIT_BUF( &m_SendBuf ); \
	{ \
		wjson_table_t packet( m_SendBuf ); \
		packet.SetInt( "request_seq", _seq ); \
		packet.SetString( "type", "response" ); \
		packet.SetString( "command", _cmd ); \
		packet.SetBool( "success", _suc );

#define DAP_START_RESPONSE( _seq, _cmd ) \
		_DAP_START_RESPONSE( _seq, _cmd, true )

#define DAP_ERROR_RESPONSE( _seq, _cmd ) \
		_DAP_START_RESPONSE( _seq, _cmd, false )

#define DAP_ERROR_BODY( _id, _fmt ) \
		wjson_table_t body = packet.SetTable( "body" ); \
		wjson_table_t error = body.SetTable( "error" ); \
		error.SetInt( "id", _id ); \
		error.SetString( "format", _fmt ); \

#define DAP_START_EVENT( _seq, _ev ) \
{ \
	_DAP_INIT_BUF( &m_SendBuf ); \
	{ \
		wjson_table_t packet( m_SendBuf ); \
		packet.SetInt( "seq", _seq ); \
		packet.SetString( "type", "event" ); \
		packet.SetString( "event", _ev );

#define DAP_SET( _key, _val ) \
		packet.Set( _key, _val );

#define DAP_SET_TABLE( _val ) \
		wjson_table_t _val = packet.SetTable( #_val );

#define DAP_SEND() \
	} \
\
	DAP_Serialise( &m_SendBuf ); \
	Send( m_SendBuf.Base(), m_SendBuf.Size() ); \
	DAP_Test( &m_ReadBuf, &m_SendBuf ); \
	DAP_Free( &m_SendBuf ); \
}

#endif // SQDBG_DAP_H
