//========= Mapbase - https://github.com/mapbase-source/source-sdk-2013 ============//
//
// Purpose: Implementation of BSPZIP
// 
// Author: Nooodles
//
//=============================================================================//
#include "../common/cmdlib.h"
#include "../common/bsplib.h"
#include "tier2/tier2.h"
#include "filesystem.h"
#include "const.h"

void Usage( const char *argument )
{
#define PRINT_BSPZIP_USAGE( arg, params, desc ) if( !argument || !V_stricmp(arg, argument ) ) Msg( "bspzip " arg " " params "\n" desc );

	if ( argument )
		Msg( "Invalid syntax!\n" );

	PRINT_BSPZIP_USAGE( "-extract", "<bspfile> <blah.zip>", "" );
	PRINT_BSPZIP_USAGE( "-extractfiles", "<bspfile> <targetpath>", "" );
	PRINT_BSPZIP_USAGE( "-dir", "<bspfile>", "" );
	PRINT_BSPZIP_USAGE( "-addfile", "<bspfile> <relativepathname> <fullpathname> <newbspfile>", "" );
	PRINT_BSPZIP_USAGE( "-addlist", "<bspfile> <listfile> <newbspfile>", "" );
	PRINT_BSPZIP_USAGE( "-addorupdatelist", "<bspfile> <listfile> <newbspfile>", "" );
	PRINT_BSPZIP_USAGE( "-extractcubemaps", "<bspfile> <targetPath>",
		"  Extracts the cubemaps to <targetPath>.\n" );
	PRINT_BSPZIP_USAGE( "-deletecubemaps", "<bspfile>",
		"  Deletes the cubemaps from <bspFile>.\n" );
	PRINT_BSPZIP_USAGE( "-addfiles", "<bspfile> <relativePathPrefix> <listfile> <newbspfile>",
		"  Adds files to <newbspfile>.\n" );
	PRINT_BSPZIP_USAGE( "-repack", "-repack [ -compress ] <bspfile>",
		"  Optimally repacks a BSP file, optionally using compressed BSP format.\n"
		"  Using on a compressed BSP without -compress will effectively decompress\n"
		"  a compressed BSP.\n" );
	exit( -1 );
}

int main( int argc, char **argv )
{
	Msg( "\nMapbase - bspzip.exe (" __DATE__ ")\n" );

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
	InitDefaultFileSystem();
	g_pFileSystem = g_pFullFileSystem;

	if ( argc < 2 )
	{
		Msg( "No arguments provided.\n" );
		Usage( nullptr );
	}

	// current argv index
	int argi = 1;
	// how many potential arguments a command has
	int argCount = argc - 2;
	// skip over -game
	if ( !V_stricmp( argv[argi], "-game" ) && argc >= 3 )
	{
		argi += 2;
		argCount -= 2;
	}

	// Extract all files from the pak file into a specified directory
	// Arguments: <bspfile> <targetpath>
	if ( !V_stricmp( argv[argi], "-extractfiles" ) )
	{
		if ( argCount < 2 )
			Usage( argv[argi] );

		const char *bspfile = argv[argi + 1];
		const char *targetPath = argv[argi + 2];

		Msg( "Opening bsp file: %s\n", bspfile );
		::LoadBSPFile_FileSystemOnly( bspfile );

		char targetPathFixed[MAX_PATH];
		V_FixupPathName( targetPathFixed, sizeof( targetPathFixed ), targetPath );

		int id = 0;
		char filePath[MAX_PATH];
		int fileSize = 0;
		CUtlBuffer buf;
		while ( GetPakFile()->GetNextFilename( id++, filePath, sizeof( filePath ), fileSize ) != -1 )
		{
			char outPath[MAX_PATH];
			V_sprintf_safe( outPath, "%s/%s", targetPathFixed, filePath );
			V_FixSlashes( outPath );

			Msg( "Writing file: %s\n", outPath );
			GetPakFile()->ReadFileFromZip( filePath, false, buf );
			g_pFullFileSystem->WriteFile( outPath, nullptr, buf );
			buf.Clear();
		}
		return 0;
	}
	// Print out contents of the pak file
	// Arguments: <bspfile>
	else if ( !V_stricmp( argv[argi], "-dir" ) )
	{
		if ( argCount < 1 )
			Usage( argv[argi] );

		const char *bspfile = argv[argi + 1];

		::LoadBSPFile_FileSystemOnly( bspfile );
		::PrintBSPPackDirectory();
		return 0;
	}
	// Add a list of files to a specific path in the pak file
	// Arguments: <bspfile> <relativePathPrefix> <listfile> <newbspfile>
	else if ( !V_stricmp( argv[argi], "-addfiles" ) )
	{
		if ( argCount < 4 )
			Usage( argv[argi] );

		const char *bspfile = argv[argi + 1];
		const char *relativePathPrefix = argv[argi + 2];
		const char *listfile = argv[argi + 3];
		const char *newbspfile = argv[argi + 4];

		Msg( "Opening bsp file: %s\n", bspfile );
		::LoadBSPFile_FileSystemOnly( bspfile );

		CUtlBuffer bufListFile;
		bufListFile.SetBufferType( true, false );
		if ( !g_pFullFileSystem->ReadFile( listfile, NULL, bufListFile ) )
			Error( "Unable to load file list" );

		char szAbsolutePath[MAX_PATH];
		char szRelativePath[MAX_PATH];
		while ( bufListFile.GetBytesRemaining() )
		{
			bufListFile.GetLine( szAbsolutePath, sizeof( szAbsolutePath ) );
			if ( !szAbsolutePath[0] )
				Error( "Error: Missing paired relative/full path names\n" );

			V_StripTrailingWhitespace( szAbsolutePath );

			const char *fileName = V_GetFileName( szAbsolutePath );
			V_sprintf_safe( szRelativePath, "%s/%s", relativePathPrefix, fileName );
			Msg( "Adding file : \"%s\" as \"%s\"\n", szAbsolutePath, szRelativePath );

			GetPakFile()->AddFileToZip( szRelativePath, szAbsolutePath );
		}

		Msg( "Writing new bsp file : %s\n", newbspfile );

		CUtlBuffer buf;
		GetPakFile()->SaveToBuffer( buf );
		::SetPakFileLump( bspfile, newbspfile, buf.Base(), buf.Size() );
		return 0;
	}
	// Add a single file to the pak file
	// Arguments: <bspfile> <relativepathname> <fullpathname> <newbspfile>
	else if ( !V_stricmp( argv[argi], "-addfile" ) )
	{
		if ( argCount < 4 )
			Usage( argv[argi] );

		const char *bspfile = argv[argi + 1];
		const char *relativepathname = argv[argi + 2];
		const char *fullpathname = argv[argi + 3];
		const char *newbspfile = argv[argi + 4];

		Msg( "Opening bsp file: %s\n", bspfile );
		::LoadBSPFile_FileSystemOnly( bspfile );

		Msg( "Adding file : \"%s\" as \"%s\"\n", fullpathname, relativepathname );
		GetPakFile()->AddFileToZip( relativepathname, fullpathname );

		Msg( "Writing new bsp file : %s\n", newbspfile );
		CUtlBuffer buf;
		GetPakFile()->SaveToBuffer( buf );
		::SetPakFileLump( bspfile, newbspfile, buf.Base(), buf.Size() );
		return 0;
	}
	// Add (or add and update) a list of files to the pak file
	// Arguments: <bspfile> <listfile> <newbspfile>
	else if ( !V_stricmp( argv[argi], "-addlist" ) || !V_stricmp( argv[argi], "-addorupdatelist" ) )
	{
		if ( argCount < 3 )
			Usage( argv[argi] );

		bool updateFiles = !V_stricmp( argv[argi], "-addorupdatelist" );

		const char *bspfile = argv[argi + 1];
		const char *listfile = argv[argi + 2];
		const char *newbspfile = argv[argi + 3];

		// TODO validate file
		Msg( "Opening bsp file: %s\n", bspfile );
		::LoadBSPFile_FileSystemOnly( bspfile );
		CUtlBuffer bufListFile;
		bufListFile.SetBufferType( true, false );
		if ( !g_pFullFileSystem->ReadFile( listfile, NULL, bufListFile ) )
			Error( "Unable to load file list" );

		char szRelativePath[MAX_PATH];
		char szAbsolutePath[MAX_PATH];
		while ( bufListFile.GetBytesRemaining() )
		{
			bufListFile.GetLine( szRelativePath, sizeof( szRelativePath ) );
			bufListFile.GetLine( szAbsolutePath, sizeof( szAbsolutePath ) );
			if ( !szAbsolutePath[0] )
				Error( "Error: Missing paired relative/full path names\n" );

			V_StripTrailingWhitespace( szRelativePath );
			V_StripTrailingWhitespace( szAbsolutePath );

			bool existingFile = false;
			if ( updateFiles )
			{
				// remove the existing file so it can be updated
				existingFile = GetPakFile()->FileExistsInZip( szRelativePath );
				if ( existingFile )
					GetPakFile()->RemoveFileFromZip( szRelativePath );
			}

			if ( existingFile )
				Msg( "Updating file : \"%s\" as \"%s\"\n", szAbsolutePath, szRelativePath );
			else
				Msg( "Adding file : \"%s\" as \"%s\"\n", szAbsolutePath, szRelativePath );

			GetPakFile()->AddFileToZip( szRelativePath, szAbsolutePath );
		}

		Msg( "Writing new bsp file : %s\n", newbspfile );

		CUtlBuffer buf;
		GetPakFile()->SaveToBuffer( buf );
		::SetPakFileLump( bspfile, newbspfile, buf.Base(), buf.Size() );
		return 0;
	}
	// extract all cubemaps from the pak file to a specified directory
	// Arguments: <bspfile> <targetPath>
	else if ( !V_stricmp( argv[argi], "-extractcubemaps" ) )
	{
		if ( argCount < 2 )
			Usage( argv[argi] );

		const char *bspfile = argv[argi + 1];
		const char *targetPath = argv[argi + 2];

		Msg( "Opening bsp file: %s\n", bspfile );
		::LoadBSPFile_FileSystemOnly( bspfile );

		char targetPathFixed[MAX_PATH];
		V_FixupPathName( targetPathFixed, sizeof( targetPathFixed ), targetPath );

		char szMapName[MAX_MAP_NAME];
		V_FileBase( bspfile, szMapName, sizeof( szMapName ) );

		// path to where cubemaps would normally be
		char mapMatPath[MAX_PATH];
		V_sprintf_safe( mapMatPath, "materials/maps/%s/", szMapName );
		int mapMatLen = V_strlen( mapMatPath );

		// find our cubemap textures and extract them
		int id = 0;
		char filePath[MAX_PATH];
		int fileSize = 0;
		CUtlBuffer buf;
		int cubemapTextureCount = 0;
		while ( GetPakFile()->GetNextFilename( id++, filePath, sizeof( filePath ), fileSize ) != -1 )
		{
			// cubemaps are in the material/maps path, don't have any following /'s and start with c
			char *matPath = V_stristr( filePath, mapMatPath );
			if ( matPath )
			{
				matPath = filePath + mapMatLen;
				char *slash = V_stristr( matPath, "/" );
				if ( !slash && matPath[0] && matPath[0] == 'c' )
				{
					char outPath[MAX_PATH];
					V_sprintf_safe( outPath, "%s/%s", targetPathFixed, filePath );
					V_FixSlashes( outPath );

					Msg( "Writing vtf file: %s\n", outPath );
					GetPakFile()->ReadFileFromZip( filePath, false, buf );
					g_pFullFileSystem->WriteFile( outPath, nullptr, buf );
					buf.Clear();
					cubemapTextureCount++;
				}
			}
		}

		Msg( "%d cubemaps extracted.", cubemapTextureCount );
		return 0;
	}
	// Delete all cubemap textures the pak file
	// Arguments: <bspfile>
	else if ( !V_stricmp( argv[argi], "-deletecubemaps" ) )
	{
		if ( argCount < 2 )
			Usage( argv[argi] );

		const char *bspfile = argv[argi + 1];

		Msg( "Opening bsp file: %s\n", bspfile );
		::LoadBSPFile_FileSystemOnly( bspfile );

		char szMapName[MAX_MAP_NAME];
		V_FileBase( bspfile, szMapName, sizeof( szMapName ) );

		// path to where cubemaps would normally be
		char mapMatPath[MAX_PATH];
		V_sprintf_safe( mapMatPath, "materials/maps/%s/", szMapName );
		int mapMatLen = V_strlen( mapMatPath );

		// find our cubemap textures and extract them
		int id = 0;
		char filePath[MAX_PATH];
		int fileSize = 0;
		CUtlBuffer buf;
		int cubemapTextureCount = 0;
		while ( GetPakFile()->GetNextFilename( id++, filePath, sizeof( filePath ), fileSize ) != -1 )
		{
			// cubemaps are in the material/maps path, don't have any following /'s and start with c
			char *matPath = V_stristr( filePath, mapMatPath );
			if ( matPath )
			{
				matPath = filePath + mapMatLen;
				char *slash = V_stristr( matPath, "/" );
				if ( !slash && matPath[0] && matPath[0] == 'c' )
				{
					GetPakFile()->RemoveFileFromZip( filePath );
					cubemapTextureCount++;
				}
			}
		}

		CUtlBuffer pakBuf;
		GetPakFile()->SaveToBuffer( pakBuf );
		::SetPakFileLump( bspfile, bspfile, pakBuf.Base(), pakBuf.Size() );
		return 0;
	}
	// Extract the pak file to a specified directory
	// Arguments: <bspfile> <blah.zip>
	else if ( !V_stricmp( argv[argi], "-extract" ) )
	{
		if ( argCount < 2 )
			Usage( argv[argi] );

		const char *bspfile = argv[argi + 1];
		const char *zipfile = argv[argi + 2];

		::ExtractZipFileFromBSP( (char *) bspfile, (char *) zipfile );
		return 0;
	}
	// Repack a BSP with the option to compress it
	// Arguments: [ -compress ] <bspfile>
	else if ( !V_stricmp( argv[argi], "-repack" ) )
	{
		if ( argCount < 1 )
			Usage( argv[argi] );

		// check if our first argument is -compress
		bool doCompression = !V_stricmp( argv[argi + 1], "-compress" );
		if ( doCompression && argCount < 2 )
			Usage( argv[argi] );

		const char *bspfile = doCompression ? argv[argi + 2] : argv[argi + 1];
		if ( !g_pFullFileSystem->FileExists( bspfile ) )
		{
			Warning( "Couldn't open input file %s - BSP recompress failed\n", bspfile );
			return false;
		}

		CUtlBuffer inputBuffer;
		if ( !g_pFullFileSystem->ReadFile( bspfile, nullptr, inputBuffer ) )
		{
			Warning( "Couldn't read file %s - BSP compression failed\n", bspfile );
			return false;
		}

		CUtlBuffer outputBuffer;
		IZip::eCompressionType compressionType = doCompression ? IZip::eCompressionType_LZMA : IZip::eCompressionType_None;
		CompressFunc_t funcCompress = doCompression ? RepackBSPCallback_LZMA : nullptr;

		if ( doCompression )
			Msg( "Compressing %s\n", bspfile );
		else
			Msg( "Repacking %s\n", bspfile );

		if ( !::RepackBSP( inputBuffer, outputBuffer, funcCompress, compressionType ) )
		{
			Warning( "Internal error compressing BSP\n" );
			return false;
		}

		if ( doCompression )
			Msg( "Successfully compressed %s -- %d -> %d\n", bspfile, inputBuffer.Size(), outputBuffer.Size() );
		else
			Msg( "Successfully repacked %s -- %d -> %d\n", bspfile, inputBuffer.Size(), outputBuffer.Size() );

		g_pFullFileSystem->WriteFile( bspfile, NULL, outputBuffer );
		return 0;
	}

	// print all usage
	Msg( "Unknown argument or invalid usage of \"%s\"\n", argv[argi] );
	Usage( nullptr );
	return 1;
}
