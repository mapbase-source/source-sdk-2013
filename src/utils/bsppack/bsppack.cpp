// Created by ficool2 and posted in the Valve Modding Community Discord on 4/5/2025 
#include "../common/cmdlib.h"
#include "../common/bsplib.h"
#include "ibsppack.h"

static void SetFileSystem( IFileSystem* pFileSystem )
{
    g_pFullFileSystem = pFileSystem;
    g_pFileSystem = pFileSystem;
}

class CBSPPack : public IBSPPack
{
public:
    void LoadBSPFile( IFileSystem *pFileSystem, char *filename ) override
    {
        SetFileSystem( pFileSystem );
        ::LoadBSPFile( filename );
    }
    void WriteBSPFile( char *filename ) override
    {
        ::WriteBSPFile( filename );
    }
    void ClearPackFile( void ) override
    {
        ::ClearPakFile( GetPakFile() );
    }
    void AddFileToPack( const char *relativename, const char *fullpath ) override
    {
        ::AddFileToPak( GetPakFile(), relativename, fullpath, IZip::eCompressionType_None );
    }
    void AddBufferToPack( const char *relativename, void *data, int length, bool bTextMode ) override
    {
        ::AddBufferToPak( GetPakFile(), relativename, data, length, bTextMode, IZip::eCompressionType_None );
    }
    void SetHDRMode( bool bHDR ) override
    {
        ::SetHDRMode( bHDR );
    }
    bool SwapBSPFile( IFileSystem *pFileSystem, const char *filename, const char *swapFilename, bool bSwapOnLoad, VTFConvertFunc_t pVTFConvertFunc, VHVFixupFunc_t pVHVFixupFunc, CompressFunc_t pCompressFunc ) override
    {
        SetFileSystem( pFileSystem );
        return ::SwapBSPFile( filename, swapFilename, bSwapOnLoad, pVTFConvertFunc, pVHVFixupFunc, pCompressFunc );
    }
    bool RepackBSP( CUtlBuffer &inputBuffer, CUtlBuffer &outputBuffer, eRepackBSPFlags repackFlags ) override
    {
        CompressFunc_t funcCompress = NULL;
        if ( repackFlags & eRepackBSP_CompressLumps )
            funcCompress = RepackBSPCallback_LZMA;

        IZip::eCompressionType eCompressType = IZip::eCompressionType_None;
        if ( repackFlags & eRepackBSP_CompressPackfile )
            eCompressType = IZip::eCompressionType_LZMA;

        return ::RepackBSP( inputBuffer, outputBuffer, funcCompress, eCompressType );
    }
    bool GetPakFileLump( IFileSystem *pFileSystem, const char *pBSPFilename, void **pPakData, int *pPakSize ) override
    {
        SetFileSystem( pFileSystem );
        return ::GetPakFileLump( pBSPFilename, pPakData, pPakSize );
    }
    bool SetPakFileLump( IFileSystem *pFileSystem, const char *pBSPFilename, const char *pNewFilename, void *pPakData, int pakSize ) override
    {
        SetFileSystem( pFileSystem );
        return ::SetPakFileLump( pBSPFilename, pNewFilename, pPakData, pakSize );
    }
    bool GetBSPDependants( IFileSystem *pFileSystem, const char *pBSPFilename, CUtlVector< CUtlString > *pList ) override
    {
        SetFileSystem( pFileSystem );
        return ::GetBSPDependants( pBSPFilename, pList );
    }
};

EXPOSE_SINGLE_INTERFACE( CBSPPack, IBSPPack, IBSPPACK_VERSION_STRING );