/*
   Copyright (C) 1999-2007 id Software, Inc. and contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of GtkRadiant.

   GtkRadiant is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   GtkRadiant is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GtkRadiant; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

// Nurail: Swiped from quake3/common

#include "cmdlib.h"
#include "globaldefs.h"
#include "mathlib.h"
#include "inout.h"
#include <sys/types.h>
#include <sys/stat.h>

#if GDEF_OS_WINDOWS
#include <direct.h>
#include <windows.h>
#elif GDEF_OS_NEXT
#include <libc.h>
#else // OTHER OSES
#include <unistd.h>
#endif // OTHER OSES

#if !GDEF_OS_WINDOWS
#define strlwr strlower
#endif // !GDEF_OS_WINDOWS

#define BASEDIRNAME "h"
#define PATHSEPERATOR '/'

extern qboolean verbose;

qboolean g_dokeypress = false;

qboolean g_nomkdir = false;

void *safe_malloc( size_t size ){
	void *p;

	p = malloc( size );
	if ( !p ) {
		Error( "safe_malloc failed on allocation of %i bytes", size );
	}

	return p;
}

void *safe_malloc_info( size_t size, char* info ){
	void *p;

	p = malloc( size );
	if ( !p ) {
		Error( "%s: safe_malloc failed on allocation of %i bytes", info, size );
	}

	return p;
}

void *SafeMalloc( size_t n, char *desc ){
	void *p;

	if ( ( p = malloc( n ) ) == NULL ) {
		Error( "Failed to allocate %d bytes for '%s'.\n", n, desc );
	}
	memset( p, 0, n );
	return p;
}

// set these before calling CheckParm
int myargc;
char **myargv;

char com_token[1024];
qboolean com_eof;

qboolean archive;
char archivedir[1024];


/*
   ===================
   ExpandWildcards

   Mimic unix command line expansion
   ===================
 */
#define MAX_EX_ARGC 1024

int ex_argc;
char    *ex_argv[MAX_EX_ARGC];

#if GDEF_OS_WINDOWS
#include "io.h"
void ExpandWildcards( int *argc, char ***argv ){
	struct _finddata_t fileinfo;
	int handle;
	int i;
	char filename[1024];
	char filebase[1024];
	char    *path;

	ex_argc = 0;
	for ( i = 0 ; i < *argc ; i++ )
	{
		path = ( *argv )[i];
		if ( path[0] == '-'
			 || ( !strstr( path, "*" ) && !strstr( path, "?" ) ) ) {
			ex_argv[ex_argc++] = path;
			continue;
		}

		handle = _findfirst( path, &fileinfo );
		if ( handle == -1 ) {
			return;
		}

		ExtractFilePath( path, filebase );

		do
		{
			sprintf( filename, "%s%s", filebase, fileinfo.name );
			ex_argv[ex_argc++] = copystring( filename );
		} while ( _findnext( handle, &fileinfo ) != -1 );

		_findclose( handle );
	}

	*argc = ex_argc;
	*argv = ex_argv;
}
#else // !GDEF_OS_WINDOWS
void ExpandWildcards( int *argc, char ***argv ){
}
#endif // !GDEF_OS_WINDOWS

/*

   qdir will hold the path up to the quake directory, including the slash

   f:\quake\
   /raid/quake/

   gamedir will hold qdir + the game directory (id1, id2, etc)

 */

char qdir[1024];
char gamedir[1024];
char writedir[1024];

void SetQdirFromPath( const char *path ){
	char temp[1024];
	const char  *c;
	const char *sep;
	int len, count;

	if ( !( path[0] == '/' || path[0] == '\\' || path[1] == ':' ) ) { // path is partial
		Q_getwd( temp );
		strcat( temp, path );
		path = temp;
	}

	// search for "quake2" in path

	len = strlen( BASEDIRNAME );
	for ( c = path + strlen( path ) - 1 ; c != path ; c-- )
	{
		int i;

		if ( !Q_strncasecmp( c, BASEDIRNAME, len ) ) {
			//
			//strncpy (qdir, path, c+len+2-path);
			// the +2 assumes a 2 or 3 following quake which is not the
			// case with a retail install
			// so we need to add up how much to the next separator
			sep = c + len;
			count = 1;
			while ( *sep && *sep != '/' && *sep != '\\' )
			{
				sep++;
				count++;
			}
			strncpy( qdir, path, c + len + count - path );
			Sys_Printf( "qdir: %s\n", qdir );
			for ( i = 0; i < strlen( qdir ); i++ )
			{
				if ( qdir[i] == '\\' ) {
					qdir[i] = '/';
				}
			}

			c += len + count;
			while ( *c )
			{
				if ( *c == '/' || *c == '\\' ) {
					strncpy( gamedir, path, c + 1 - path );

					for ( i = 0; i < strlen( gamedir ); i++ )
					{
						if ( gamedir[i] == '\\' ) {
							gamedir[i] = '/';
						}
					}

					Sys_Printf( "gamedir: %s\n", gamedir );

					if ( !writedir[0] ) {
						strcpy( writedir, gamedir );
					}
					else if ( writedir[strlen( writedir ) - 1] != '/' ) {
						writedir[strlen( writedir )] = '/';
						writedir[strlen( writedir ) + 1] = 0;
					}

					return;
				}
				c++;
			}
			Error( "No gamedir in %s", path );
			return;
		}
	}
	Error( "SetQdirFromPath: no '%s' in %s", BASEDIRNAME, path );
}

char *ExpandArg( const char *path ){
	static char full[1024];

	if ( path[0] != '/' && path[0] != '\\' && path[1] != ':' ) {
		Q_getwd( full );
		strcat( full, path );
	}
	else{
		strcpy( full, path );
	}
	return full;
}

char *ExpandPath( const char *path ){
	static char full[1024];
	if ( !qdir ) {
		Error( "ExpandPath called without qdir set" );
	}
	if ( path[0] == '/' || path[0] == '\\' || path[1] == ':' ) {
		strcpy( full, path );
		return full;
	}
	sprintf( full, "%s%s", qdir, path );
	return full;
}

char *ExpandGamePath( const char *path ){
	static char full[1024];
	if ( !qdir ) {
		Error( "ExpandGamePath called without qdir set" );
	}
	if ( path[0] == '/' || path[0] == '\\' || path[1] == ':' ) {
		strcpy( full, path );
		return full;
	}
	sprintf( full, "%s%s", gamedir, path );
	return full;
}

char *ExpandPathAndArchive( const char *path ){
	char    *expanded;
	char archivename[1024];

	expanded = ExpandPath( path );

	if ( archive ) {
		sprintf( archivename, "%s/%s", archivedir, path );
		QCopyFile( expanded, archivename );
	}
	return expanded;
}


char *copystring( const char *s ){
	char    *b;
	b = safe_malloc( strlen( s ) + 1 );
	strcpy( b, s );
	return b;
}



/*
   ================
   I_FloatTime
   ================
 */
double I_FloatTime( void ){
	time_t t;

	time( &t );

	return t;
#if 0
// more precise, less portable
	struct timeval tp;
	struct timezone tzp;
	static int secbase;

	gettimeofday( &tp, &tzp );

	if ( !secbase ) {
		secbase = tp.tv_sec;
		return tp.tv_usec / 1000000.0;
	}

	return ( tp.tv_sec - secbase ) + tp.tv_usec / 1000000.0;
#endif
}

void Q_getwd( char *out ){
	int i = 0;

#if GDEF_OS_WINDOWS
	_getcwd( out, 256 );
	strcat( out, "\\" );
#else // !GDEF_OS_WINDOWS
	// Gef: Changed from getwd() to getcwd() to avoid potential buffer overflow
	getcwd( out, 256 );
	strcat( out, "/" );
#endif // !GDEF_OS_WINDOWS
	while ( out[i] != 0 )
	{
		if ( out[i] == '\\' ) {
			out[i] = '/';
		}
		i++;
	}
}


void Q_mkdir( const char *path ){
#if GDEF_OS_WINDOWS
	if ( _mkdir( path ) != -1 ) {
		return;
	}
#else // !GDEF_OS_WINDOWS
	if ( mkdir( path, 0777 ) != -1 ) {
		return;
	}
#endif // !GDEF_OS_WINDOWS
	if ( errno != EEXIST ) {
		Error( "mkdir %s: %s",path, strerror( errno ) );
	}
}

/*
   ============
   FileTime

   returns -1 if not present
   ============
 */
int FileTime( const char *path ){
	struct  stat buf;

	if ( stat( path,&buf ) == -1 ) {
		return -1;
	}

	return buf.st_mtime;
}



/*
   ==============
   COM_Parse

   Parse a token out of a string
   ==============
 */
char *COM_Parse( char *data ){
	int c;
	int len;

	len = 0;
	com_token[0] = 0;

	if ( !data ) {
		return NULL;
	}

// skip whitespace
skipwhite:
	while ( ( c = *data ) <= ' ' )
	{
		if ( c == 0 ) {
			com_eof = true;
			return NULL;            // end of file;
		}
		data++;
	}

// skip // comments
	if ( c == '/' && data[1] == '/' ) {
		while ( *data && *data != '\n' )
			data++;
		goto skipwhite;
	}


// handle quoted strings specially
	if ( c == '\"' ) {
		data++;
		do
		{
			c = *data++;
			if ( c == '\"' ) {
				com_token[len] = 0;
				return data;
			}
			com_token[len] = c;
			len++;
		} while ( 1 );
	}

// parse single characters
	if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' ) {
		com_token[len] = c;
		len++;
		com_token[len] = 0;
		return data + 1;
	}

// parse a regular word
	do
	{
		com_token[len] = c;
		data++;
		len++;
		c = *data;
		if ( c == '{' || c == '}' || c == ')' || c == '(' || c == '\'' || c == ':' ) {
			break;
		}
	} while ( c > 32 );

	com_token[len] = 0;
	return data;
}

int Q_strncasecmp( const char *s1, const char *s2, int n ){
	int c1, c2;

	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- ) {
			return 0;       // strings are equal until end point

		}
		if ( c1 != c2 ) {
			if ( c1 >= 'a' && c1 <= 'z' ) {
				c1 -= ( 'a' - 'A' );
			}
			if ( c2 >= 'a' && c2 <= 'z' ) {
				c2 -= ( 'a' - 'A' );
			}
			if ( c1 != c2 ) {
				return -1;      // strings not equal
			}
		}
	} while ( c1 );

	return 0;       // strings are equal
}

int Q_stricmp( const char *s1, const char *s2 ){
	return Q_strncasecmp( s1, s2, 99999 );
}

int Q_strcasecmp( const char *s1, const char *s2 ){
	return Q_strncasecmp( s1, s2, 99999 );
}

// May be already defined with some compilers on Windows
#ifndef strupr
char *strupr (char *start)
{
	char	*in;
	in = start;
	while (*in)
	{
		*in = toupper(*in);
		in++;
	}
	return start;
}
#endif

char *strlower( char *start ){
	char    *in;
	in = start;
	while ( *in )
	{
		*in = tolower( *in );
		in++;
	}
	return start;
}


/*
   =============================================================================

                        MISC FUNCTIONS

   =============================================================================
 */


/*
   =================
   CheckParm

   Checks for the given parameter in the program's command line arguments
   Returns the argument number (1 to argc-1) or 0 if not present
   =================
 */
int CheckParm( const char *check ){
	int i;

	for ( i = 1; i < myargc; i++ )
	{
		if ( !Q_stricmp( check, myargv[i] ) ) {
			return i;
		}
	}

	return 0;
}



/*
   ================
   Q_filelength
   ================
 */
int Q_filelength( FILE *f ){
	int pos;
	int end;

	pos = ftell( f );
	fseek( f, 0, SEEK_END );
	end = ftell( f );
	fseek( f, pos, SEEK_SET );

	return end;
}


FILE *SafeOpenWrite( const char *filename ){
	FILE    *f;

	f = fopen( filename, "wb" );

	if ( !f ) {
		Error( "Error opening %s: %s",filename,strerror( errno ) );
	}

	return f;
}

FILE *SafeOpenRead( const char *filename ){
	FILE    *f;

	f = fopen( filename, "rb" );

	if ( !f ) {
		Error( "Error opening %s: %s",filename,strerror( errno ) );
	}

	return f;
}


void SafeRead( FILE *f, void *buffer, int count ){
	if ( fread( buffer, 1, count, f ) != (size_t)count ) {
		Error( "File read failure" );
	}
}


void SafeWrite( FILE *f, const void *buffer, int count ){
	if ( fwrite( buffer, 1, count, f ) != (size_t)count ) {
		Error( "File write failure" );
	}
}


/*
   ==============
   FileExists
   ==============
 */
qboolean    FileExists( const char *filename ){
	FILE    *f;

	f = fopen( filename, "r" );
	if ( !f ) {
		return false;
	}
	fclose( f );
	return true;
}

/*
   ==============
   LoadFile
   ==============
 */
int    LoadFile( const char *filename, void **bufferptr ){
	FILE    *f;
	int length;
	void    *buffer;

	f = SafeOpenRead( filename );
	length = Q_filelength( f );
	buffer = safe_malloc( length + 1 );
	( (char *)buffer )[length] = 0;
	SafeRead( f, buffer, length );
	fclose( f );

	*bufferptr = buffer;
	return length;
}


/*
   ==============
   LoadFileBlock
   -
   rounds up memory allocation to 4K boundry
   -
   ==============
 */
int    LoadFileBlock( const char *filename, void **bufferptr ){
	FILE    *f;
	int length, nBlock, nAllocSize;
	void    *buffer;

	f = SafeOpenRead( filename );
	length = Q_filelength( f );
	nAllocSize = length;
	nBlock = nAllocSize % MEM_BLOCKSIZE;
	if ( nBlock > 0 ) {
		nAllocSize += MEM_BLOCKSIZE - nBlock;
	}
	buffer = safe_malloc( nAllocSize + 1 );
	memset( buffer, 0, nAllocSize + 1 );
	SafeRead( f, buffer, length );
	fclose( f );

	*bufferptr = buffer;
	return length;
}


/*
   ==============
   TryLoadFile

   Allows failure
   ==============
 */
int    TryLoadFile( const char *filename, void **bufferptr ){
	FILE    *f;
	int length;
	void    *buffer;

	*bufferptr = NULL;

	f = fopen( filename, "rb" );
	if ( !f ) {
		return -1;
	}
	length = Q_filelength( f );
	buffer = safe_malloc( length + 1 );
	( (char *)buffer )[length] = 0;
	SafeRead( f, buffer, length );
	fclose( f );

	*bufferptr = buffer;
	return length;
}


/*
   ==============
   SaveFile
   ==============
 */
void    SaveFile( const char *filename, const void *buffer, int count ){
	FILE    *f;

	f = SafeOpenWrite( filename );
	SafeWrite( f, buffer, count );
	fclose( f );
}



void DefaultExtension( char *path, const char *extension ){
	char    *src;
//
// if path doesnt have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen( path ) - 1;

	while ( *src != '/' && *src != '\\' && src != path )
	{
		if ( *src == '.' ) {
			return;                 // it has an extension
		}
		src--;
	}

	strcat( path, extension );
}


void DefaultPath( char *path, const char *basepath ){
	char temp[128];

	if ( path[ 0 ] == '/' || path[ 0 ] == '\\' ) {
		return;                   // absolute path location
	}
	strcpy( temp,path );
	strcpy( path,basepath );
	strcat( path,temp );
}


void    StripFilename( char *path ){
	int length;

	length = strlen( path ) - 1;
	while ( length > 0 && path[length] != '/' && path[ length ] != '\\' )
		length--;
	path[length] = 0;
}

void    StripExtension( char *path ){
	int length;

	length = strlen( path ) - 1;
	while ( length > 0 && path[length] != '.' )
	{
		length--;
		if ( path[length] == '/' || path[ length ] == '\\' ) {
			return;     // no extension
		}
	}
	if ( length ) {
		path[length] = 0;
	}
}


/*
   ====================
   Extract file parts
   ====================
 */

// FIXME: should include the slash, otherwise
// backing to an empty path will be wrong when appending a slash
void ExtractFilePath( const char *path, char *dest ){
	const char    *src;

	src = path + strlen( path ) - 1;

//
// back up until a \ or the start
//
	while ( src != path && *( src - 1 ) != '\\' && *( src - 1 ) != '/' )
		src--;

	memcpy( dest, path, src - path );
	dest[src - path] = 0;
}

void ExtractFileBase( const char *path, char *dest ){
	const char    *src;

	src = path + strlen( path ) - 1;

//
// back up until a \ or the start
//
	while ( src != path && *( src - 1 ) != '/' && *( src - 1 ) != '\\' )
		src--;

	while ( *src && *src != '.' )
	{
		*dest++ = *src++;
	}
	*dest = 0;
}

void ExtractFileExtension( const char *path, char *dest ){
	const char    *src;

	src = path + strlen( path ) - 1;

//
// back up until a . or the start
//
	while ( src != path && *( src - 1 ) != '.' )
		src--;
	if ( src == path ) {
		*dest = 0;  // no extension
		return;
	}

	strcpy( dest,src );
}


/*
   ==============
   ParseNum / ParseHex
   ==============
 */
int ParseHex( const char *hex ){
	const char    *str;
	int num;

	num = 0;
	str = hex;

	while ( *str )
	{
		num <<= 4;
		if ( *str >= '0' && *str <= '9' ) {
			num += *str - '0';
		}
		else if ( *str >= 'a' && *str <= 'f' ) {
			num += 10 + *str - 'a';
		}
		else if ( *str >= 'A' && *str <= 'F' ) {
			num += 10 + *str - 'A';
		}
		else{
			Error( "Bad hex number: %s",hex );
		}
		str++;
	}

	return num;
}


int ParseNum( const char *str ){
	if ( str[0] == '$' ) {
		return ParseHex( str + 1 );
	}
	if ( str[0] == '0' && str[1] == 'x' ) {
		return ParseHex( str + 2 );
	}
	return atol( str );
}
/*
   // all output ends up through here
   void FPrintf (int flag, char *buf)
   {
   printf( "%s", buf );

   }

   void Sys_FPrintf (int flag, const char *format, ...)
   {
   char out_buffer[4096];
    va_list argptr;

   if ((flag == SYS_VRB) && (verbose == false))
    return;

   va_start (argptr, format);
    vsprintf (out_buffer, format, argptr);
    va_end (argptr);

   FPrintf (flag, out_buffer);
   }

   void Sys_Printf (const char *format, ...)
   {
   char out_buffer[4096];
    va_list argptr;

   va_start (argptr, format);
    vsprintf (out_buffer, format, argptr);
    va_end (argptr);

   FPrintf (SYS_STD, out_buffer);
   }

   //=================
   //Error
   //
   //For abnormal program terminations
   //=================

   void Error( const char *error, ...)
   {
   char out_buffer[4096];
   char tmp[4096];
    va_list argptr;

    va_start (argptr,error);
    vsprintf (tmp, error, argptr);
    va_end (argptr);

   sprintf( out_buffer, "************ ERROR ************\n%s\n", tmp );

   FPrintf( SYS_ERR, out_buffer );

    exit (1);
   }

 */

/*
   ============================================================================

                    BYTE ORDER FUNCTIONS

   ============================================================================
 */

#if GDEF_ARCH_ENDIAN_BIG

short   LittleShort( short l ){
	byte b1,b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

short   BigShort( short l ){
	return l;
}


int    LittleLong( int l ){
	byte b1,b2,b3,b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (int)b1 << 24 ) + ( (int)b2 << 16 ) + ( (int)b3 << 8 ) + b4;
}

int    BigLong( int l ){
	return l;
}


float   LittleFloat( float l ){
	union {byte b[4]; float f; } in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

float   BigFloat( float l ){
	return l;
}


#else // !GDEF_ARCH_ENDIAN_BIG


short   BigShort( short l ){
	byte b1,b2;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;

	return ( b1 << 8 ) + b2;
}

short   LittleShort( short l ){
	return l;
}


int    BigLong( int l ){
	byte b1,b2,b3,b4;

	b1 = l & 255;
	b2 = ( l >> 8 ) & 255;
	b3 = ( l >> 16 ) & 255;
	b4 = ( l >> 24 ) & 255;

	return ( (int)b1 << 24 ) + ( (int)b2 << 16 ) + ( (int)b3 << 8 ) + b4;
}

int    LittleLong( int l ){
	return l;
}

float   BigFloat( float l ){
	union {byte b[4]; float f; } in, out;

	in.f = l;
	out.b[0] = in.b[3];
	out.b[1] = in.b[2];
	out.b[2] = in.b[1];
	out.b[3] = in.b[0];

	return out.f;
}

float   LittleFloat( float l ){
	return l;
}

#endif // ! GDEF_ARCH_ENDIAN_BIG


//=============================================================================

/*
   ============
   CreatePath
   ============
 */
void    CreatePath( const char *path ){
	const char  *ofs;
	char c;
	char dir[1024];

#if GDEF_OS_WINDOWS
	int olddrive = -1;

	if ( path[1] == ':' ) {
		olddrive = _getdrive();
		_chdrive( toupper( path[0] ) - 'A' + 1 );
	}
#endif // !GDEF_OS_WINDOWS

	if ( path[1] == ':' ) {
		path += 2;
	}

	for ( ofs = path + 1 ; *ofs ; ofs++ )
	{
		c = *ofs;
		if ( c == '/' || c == '\\' ) { // create the directory
			memcpy( dir, path, ofs - path );
			dir[ ofs - path ] = 0;
			Q_mkdir( dir );
		}
	}

#if GDEF_OS_WINDOWS
	if ( olddrive != -1 ) {
		_chdrive( olddrive );
	}
#endif // !GDEF_OS_WINDOWS
}


/*
   ============
   QCopyFile

   Used to archive source files
   ============
 */
void QCopyFile( const char *from, const char *to ){
	void    *buffer;
	int length;

	length = LoadFile( from, &buffer );
	CreatePath( to );
	SaveFile( to, buffer, length );
	free( buffer );
}

void Sys_Sleep( int n ){
#if GDEF_OS_WINDOWS
	Sleep( n );
#else // !GDEF_OS_WINDOWS
	usleep( n * 1000 );
#endif // !GDEF_OS_WINDOWS
}
