//-----------------------------------------------------------------------
//                       github.com/samisalreadytaken/sqdbg
//-----------------------------------------------------------------------
//

#ifndef SQDBG_VEC_H
#define SQDBG_VEC_H

void *sqdbg_malloc( unsigned int size );
void *sqdbg_realloc( void *p, unsigned int oldsize, unsigned int size );
void sqdbg_free( void *p, unsigned int size );

class CMemory
{
public:
#pragma pack(push, 4)
	struct memory_t
	{
		char *ptr;
		unsigned int size;
	};
#pragma pack(pop)

	memory_t memory;

	char &operator[]( unsigned int i ) const
	{
		Assert( memory.size > 0 );
		return *( memory.ptr + i );
	}

	char *Base()
	{
		return memory.ptr;
	}

	unsigned int Size() const
	{
		return memory.size;
	}

	void Free()
	{
		if ( memory.ptr )
		{
			sqdbg_free( memory.ptr, memory.size );
			memory.ptr = 0;
			memory.size = 0;
		}
	}

	void Alloc( unsigned int count )
	{
		Assert( count > 0 || memory.size == 0 );

		if ( count == memory.size )
			return;

		const int size = ( count + sizeof(void*) - 1 ) & ~( sizeof(void*) - 1 );

		if ( memory.ptr )
		{
			memory.ptr = (char*)sqdbg_realloc( memory.ptr, memory.size, size );
		}
		else
		{
			memory.ptr = (char*)sqdbg_malloc( size );
		}

		AssertOOM( memory.ptr, size );

		if ( memory.ptr )
		{
			memory.size = size;
		}
		else
		{
			memory.size = 0;
		}
	}

	void Ensure( unsigned int newcount )
	{
		unsigned int oldcount = memory.size;

		if ( newcount <= oldcount )
			return;

		if ( oldcount != 0 )
		{
			unsigned int i = (unsigned int)0x7fffffffu;

			while ( ( i >> 1 ) > newcount )
			{
				i >>= 1;
			}

			Assert( i > 0 );
			Assert( i > oldcount );
			Assert( i >= newcount );

			newcount = i;
		}
		else
		{
			if ( newcount < 4 )
				newcount = 4;
		}

		Alloc( newcount );
	}
};

// GCC requires this to be outside of the class
template < bool S >
struct _CScratch_members;

template <>
struct _CScratch_members< true >
{
	int m_LastFreeChunk;
	int m_LastFreeIndex;
	int m_PrevChunk;
	int m_PrevIndex;

	int LastFreeChunk() { return m_LastFreeChunk; }
	int LastFreeIndex() { return m_LastFreeIndex; }
	int PrevChunk() { return m_PrevChunk; }
	int PrevIndex() { return m_PrevIndex; }

	void SetLastFreeChunk( int i ) { m_LastFreeChunk = i; }
	void SetLastFreeIndex( int i ) { m_LastFreeIndex = i; }
	void SetPrevChunk( int i ) { m_PrevChunk = i; }
	void SetPrevIndex( int i ) { m_PrevIndex = i; }
};

template <>
struct _CScratch_members< false >
{
	int LastFreeChunk() { return 0; }
	int LastFreeIndex() { return 0; }
	int PrevChunk() { return 0; }
	int PrevIndex() { return 0; }

	void SetLastFreeChunk( int ) {}
	void SetLastFreeIndex( int ) {}
	void SetPrevChunk( int ) {}
	void SetPrevIndex( int ) {}
};

template< bool SEQUENTIAL, int MEM_CACHE_CHUNKS_ALIGN = 2048 >
class CScratch
{
public:
	static const int MEM_CACHE_CHUNKSIZE = 4;
	static const int INVALID_INDEX = 0x80000000;

	struct chunk_t
	{
		char *ptr;
		int count;
	};

	chunk_t *m_Memory;
	int m_MemChunkCount;
	_CScratch_members< SEQUENTIAL > m;

	char *Get( int index )
	{
		Assert( index != INVALID_INDEX );

		int msgIdx = index & 0x0000ffff;
		int chunkIdx = index >> 16;

		Assert( m_Memory );
		Assert( chunkIdx < m_MemChunkCount );

		chunk_t *chunk = &m_Memory[ chunkIdx ];
		Assert( msgIdx < chunk->count );

		return &chunk->ptr[ msgIdx * MEM_CACHE_CHUNKSIZE ];
	}

	char *Alloc( int size, int *index = NULL )
	{
		if ( !m_Memory )
		{
			m_MemChunkCount = 4;
			m_Memory = (chunk_t*)sqdbg_malloc( m_MemChunkCount * sizeof(chunk_t) );
			AssertOOM( m_Memory, m_MemChunkCount * sizeof(chunk_t) );
			memset( (char*)m_Memory, 0, m_MemChunkCount * sizeof(chunk_t) );

			chunk_t *chunk = &m_Memory[0];
			chunk->count = MEM_CACHE_CHUNKS_ALIGN;
			chunk->ptr = (char*)sqdbg_malloc( chunk->count * MEM_CACHE_CHUNKSIZE );
			AssertOOM( chunk->ptr, chunk->count * MEM_CACHE_CHUNKSIZE );
			memset( chunk->ptr, 0, chunk->count * MEM_CACHE_CHUNKSIZE );
		}

		int requiredChunks;
		int msgIdx;
		int chunkIdx;
		int matchedChunks = 0;

		if ( SEQUENTIAL )
		{
			requiredChunks = ( size - 1 ) / MEM_CACHE_CHUNKSIZE + 1;
			msgIdx = m.LastFreeIndex();
			chunkIdx = m.LastFreeChunk();
		}
		else
		{
			requiredChunks = ( size + sizeof(int) - 1 ) / MEM_CACHE_CHUNKSIZE + 1;
			msgIdx = 0;
			chunkIdx = 0;
		}

		for (;;)
		{
			chunk_t *chunk = &m_Memory[ chunkIdx ];
			Assert( chunk->count && chunk->ptr );

			if ( SEQUENTIAL )
			{
				int remainingChunks = chunk->count - msgIdx;

				if ( remainingChunks >= requiredChunks )
				{
					m.SetPrevChunk( m.LastFreeChunk() );
					m.SetPrevIndex( m.LastFreeIndex() );

					m.SetLastFreeIndex( msgIdx + requiredChunks );
					m.SetLastFreeChunk( chunkIdx );

					if ( index )
					{
						Assert( msgIdx < 0x0000ffff );
						Assert( chunkIdx < 0x00007fff );
						*index = ( chunkIdx << 16 ) | msgIdx;
					}

					return &chunk->ptr[ msgIdx * MEM_CACHE_CHUNKSIZE ];
				}
			}
			else
			{
				char *ptr = &chunk->ptr[ msgIdx * MEM_CACHE_CHUNKSIZE ];
				Assert( *(int*)ptr >= 0 && *(int*)ptr < ( chunk->count - msgIdx ) * MEM_CACHE_CHUNKSIZE );

				if ( *(int*)ptr == 0 )
				{
					if ( ++matchedChunks == requiredChunks )
					{
						msgIdx = msgIdx - matchedChunks + 1;
						Assert( !index );
						ptr = &chunk->ptr[ msgIdx * MEM_CACHE_CHUNKSIZE ];
						*(int*)ptr = size;
						return ptr + sizeof(int);
					}
				}
				else
				{
					matchedChunks = 0;
				}

				msgIdx += ( *(int*)ptr + sizeof(int) - 1 ) / MEM_CACHE_CHUNKSIZE + 1;

				Assert( msgIdx < 0x0000ffff );

				if ( msgIdx < chunk->count )
					continue;

				matchedChunks = 0;
			}

			msgIdx = 0;

			if ( ++chunkIdx >= m_MemChunkCount )
			{
				int oldcount = m_MemChunkCount;
				m_MemChunkCount <<= 1;
				m_Memory = (chunk_t*)sqdbg_realloc( m_Memory,
						oldcount * sizeof(chunk_t),
						m_MemChunkCount * sizeof(chunk_t) );
				AssertOOM( m_Memory, m_MemChunkCount * sizeof(chunk_t) );
				memset( (char*)m_Memory + oldcount * sizeof(chunk_t),
						0,
						(m_MemChunkCount - oldcount) * sizeof(chunk_t) );
			}

			chunk = &m_Memory[ chunkIdx ];

			if ( chunk->count == 0 )
			{
				Assert( chunk->ptr == NULL );

				chunk->count = ( requiredChunks + ( MEM_CACHE_CHUNKS_ALIGN - 1 ) ) & ~( MEM_CACHE_CHUNKS_ALIGN - 1 );
				chunk->ptr = (char*)sqdbg_malloc( chunk->count * MEM_CACHE_CHUNKSIZE );
				AssertOOM( chunk->ptr, chunk->count * MEM_CACHE_CHUNKSIZE );
				memset( chunk->ptr, 0, chunk->count * MEM_CACHE_CHUNKSIZE );
			}

			Assert( chunkIdx < 0x00007fff );
		}
	}

	void Free( void *ptr )
	{
		STATIC_ASSERT( !SEQUENTIAL );
		Assert( m_Memory );
		Assert( ptr );

		*(char**)&ptr -= sizeof(int);

#ifdef _DEBUG
		bool found = false;

		for ( int chunkIdx = 0; chunkIdx < m_MemChunkCount; chunkIdx++ )
		{
			chunk_t *chunk = &m_Memory[ chunkIdx ];

			if ( chunk->count && ptr >= chunk->ptr && ptr < chunk->ptr + chunk->count * MEM_CACHE_CHUNKSIZE )
			{
				Assert( *(int*)ptr >= 0 );
				Assert( (char*)ptr + sizeof(int) + *(int*)ptr <= chunk->ptr + chunk->count * MEM_CACHE_CHUNKSIZE );
				found = true;
				break;
			}
		}

		Assert( found );

		if ( *(int*)ptr )
			(*(unsigned char**)&ptr)[ *(int*)ptr + sizeof(int) - 1 ] = 0xdd;
#endif

		memset( (char*)ptr, 0, *(int*)ptr + sizeof(int) );
	}

	void Free()
	{
		if ( !m_Memory )
			return;

		for ( int chunkIdx = 0; chunkIdx < m_MemChunkCount; chunkIdx++ )
		{
			chunk_t *chunk = &m_Memory[ chunkIdx ];

			if ( chunk->count )
			{
				sqdbg_free( chunk->ptr, chunk->count * MEM_CACHE_CHUNKSIZE );
			}
		}

		sqdbg_free( m_Memory, m_MemChunkCount * sizeof(chunk_t) );

		m_Memory = NULL;
		m_MemChunkCount = 4;
		m.SetLastFreeChunk( 0 );
		m.SetLastFreeIndex( 0 );
		m.SetPrevChunk( 0 );
		m.SetPrevIndex( 0 );
	}

	void ReleaseShrink()
	{
		STATIC_ASSERT( SEQUENTIAL );

		if ( !m_Memory )
			return;

		for ( int chunkIdx = 4; chunkIdx < m_MemChunkCount; chunkIdx++ )
		{
			chunk_t *chunk = &m_Memory[ chunkIdx ];

			if ( chunk->count )
			{
				sqdbg_free( chunk->ptr, chunk->count * MEM_CACHE_CHUNKSIZE );

				chunk->count = 0;
				chunk->ptr = NULL;
			}
		}

#ifdef _DEBUG
		for ( int chunkIdx = 0; chunkIdx < 4; chunkIdx++ )
		{
			chunk_t *chunk = &m_Memory[ chunkIdx ];

			if ( chunk->count )
			{
				memset( chunk->ptr, 0xdd, chunk->count * MEM_CACHE_CHUNKSIZE );
			}
		}
#endif

		if ( m_MemChunkCount > 8 )
		{
			int oldcount = m_MemChunkCount;
			m_MemChunkCount = 8;
			m_Memory = (chunk_t*)sqdbg_realloc( m_Memory,
					oldcount * sizeof(chunk_t),
					m_MemChunkCount * sizeof(chunk_t) );
			AssertOOM( m_Memory, m_MemChunkCount * sizeof(chunk_t) );
		}

		m.SetLastFreeChunk( 0 );
		m.SetLastFreeIndex( 0 );
		m.SetPrevChunk( 0 );
		m.SetPrevIndex( 0 );
	}

	void Release()
	{
		STATIC_ASSERT( SEQUENTIAL );

		if ( !m_Memory || ( !m.LastFreeChunk() && !m.LastFreeIndex() ) )
			return;

#ifdef _DEBUG
		for ( int chunkIdx = 0; chunkIdx < m_MemChunkCount; chunkIdx++ )
		{
			chunk_t *chunk = &m_Memory[ chunkIdx ];

			if ( chunk->count )
			{
				memset( chunk->ptr, 0xdd, chunk->count * MEM_CACHE_CHUNKSIZE );
			}
		}
#endif

		m.SetLastFreeChunk( 0 );
		m.SetLastFreeIndex( 0 );
		m.SetPrevChunk( 0 );
		m.SetPrevIndex( 0 );
	}

	void ReleaseTop()
	{
		STATIC_ASSERT( SEQUENTIAL );

		m.SetLastFreeChunk( m.PrevChunk() );
		m.SetLastFreeIndex( m.PrevIndex() );
	}
};

template < typename T, bool bExternalMem = false, class CAllocator = CMemory >
class vector
{
public:
	typedef unsigned int I;

	CAllocator base;
	I size;

	vector() : base(), size(0)
	{
		STATIC_ASSERT( !bExternalMem );
	}

	vector( CAllocator &a ) : base(a), size(0)
	{
		STATIC_ASSERT( bExternalMem );
	}

	vector( I count ) : base(), size(0)
	{
		STATIC_ASSERT( !bExternalMem );
		base.Alloc( count * sizeof(T) );
	}

	vector( const vector< T > &src ) : base()
	{
		STATIC_ASSERT( !bExternalMem );
		base.Alloc( src.base.Size() );
		size = src.size;

		for ( I i = 0; i < size; i++ )
			new( &base[ i * sizeof(T) ] ) T( (T&)src.base[ i * sizeof(T) ] );
	}

	~vector()
	{
		Assert( (unsigned int)size <= base.Size() );

		for ( I i = 0; i < size; i++ )
			((T&)(base[ i * sizeof(T) ])).~T();

		if ( !bExternalMem )
			base.Free();
	}

	T &operator[]( I i ) const
	{
		Assert( size > 0 );
		Assert( i >= 0 && i < size );
		Assert( size * sizeof(T) <= base.Size() );
		return (T&)base[ i * sizeof(T) ];
	}

	T *Base()
	{
		return base.Base();
	}

	I Size() const
	{
		return size;
	}

	I Capacity() const
	{
		return base.Size() / sizeof(T);
	}

	T &Top() const
	{
		Assert( size > 0 );
		return (T&)base[ ( size - 1 ) * sizeof(T) ];
	}

	void Pop()
	{
		Assert( size > 0 );
		((T&)base[ --size * sizeof(T) ]).~T();
	}

	T &Append()
	{
		base.Ensure( ++size * sizeof(T) );
		Assert( size * sizeof(T) <= base.Size() );
		return *( new( &base[ ( size - 1 ) * sizeof(T) ] ) T() );
	}

	void Append( const T &src )
	{
		base.Ensure( ++size * sizeof(T) );
		Assert( size * sizeof(T) <= base.Size() );
		new( &base[ ( size - 1 ) * sizeof(T) ] ) T( src );
	}

	T &Insert( I i )
	{
		Assert( i >= 0 && i <= size );

		base.Ensure( ++size * sizeof(T) );
		Assert( size * sizeof(T) <= base.Size() );

		if ( i != size - 1 )
		{
			memmove( &base[ ( i + 1 ) * sizeof(T) ],
					&base[ i * sizeof(T) ],
					( size - ( i + 1 ) ) * sizeof(T) );
		}

		return *( new( &base[ i * sizeof(T) ] ) T() );
	}

	void Remove( I i )
	{
		Assert( size > 0 );
		Assert( i >= 0 && i < size );

		((T&)base[ i * sizeof(T) ]).~T();

		if ( i != size - 1 )
		{
			memmove( &base[ i * sizeof(T) ],
					&base[ ( i + 1 ) * sizeof(T) ],
					( size - ( i + 1 ) ) * sizeof(T) );
		}

		size--;
	}

	void Clear()
	{
		for ( I i = 0; i < size; i++ )
			((T&)base[ i * sizeof(T) ]).~T();

		size = 0;
	}

	void Sort( int (*fn)(const T *, const T *) )
	{
		Assert( size * sizeof(T) <= base.Size() );

		if ( size > 1 )
		{
			qsort( base.Base(), size, sizeof(T), (int (*)(const void *, const void *))fn );
		}
	}

	void Reserve( I count )
	{
		Assert( (unsigned int)size <= base.Size() );

		if ( count == 0 )
			count = 4;

		if ( (unsigned int)count == base.Size() )
			return;

		for ( I i = count; i < size; i++ )
			((T&)base[ i * sizeof(T) ]).~T();

		base.Alloc( count * sizeof(T) );
	}

	void Purge()
	{
		Assert( size * sizeof(T) <= base.Size() );

		for ( I i = 0; i < size; i++ )
			((T&)base[ i * sizeof(T) ]).~T();

		base.Free();
		size = 0;
	}
};

class CBuffer
{
public:
	CMemory base;
	int size;
	int offset;

	char *Base()
	{
		return base.Base() + offset;
	}

	int Size() const
	{
		return size;
	}

	int Capacity() const
	{
		return base.Size();
	}

	void Reserve( int count )
	{
		Assert( (unsigned int)size <= base.Size() );

		if ( (unsigned int)count == base.Size() )
			return;

		base.Alloc( count );
	}

	void Purge()
	{
		base.Free();
		size = 0;
		offset = 0;
	}
};

class CBufTmpCache
{
public:
	CBuffer *buffer;
	int size;

	CBufTmpCache( CBuffer *b ) :
		buffer(b),
		size(buffer->size)
	{
		buffer->offset += buffer->size;
		buffer->size = 0;
	}

	~CBufTmpCache()
	{
		buffer->offset -= size;
		buffer->size = size;
	}
};

#endif // SQDBG_VEC_H
