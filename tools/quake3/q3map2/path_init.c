/* -------------------------------------------------------------------------------

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

   ----------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */

/* marker */
#define PATH_INIT_C

/* dependencies */
#include "q3map2.h"

/* path support */
#define MAX_BASE_PATHS  10
#define MAX_GAME_PATHS  10
#define MAX_PAK_PATHS  200

qboolean				customHomePath = qfalse;
char                    *homePath;

#if GDEF_OS_MACOS
char					*macLibraryApplicationSupportPath;
#elif GDEF_OS_XDG
char                    *xdgDataHomePath;
#endif // GDEF_OS_XDG

char installPath[ MAX_OS_PATH ];

int numBasePaths;
char                    *basePaths[ MAX_BASE_PATHS ];
int numGamePaths;
char                    *gamePaths[ MAX_GAME_PATHS ];
int numPakPaths;
char                    *pakPaths[ MAX_PAK_PATHS ];
char                    *homeBasePath = NULL;


/*
   some of this code is based off the original q3map port from loki
   and finds various paths. moved here from bsp.c for clarity.
 */

/*
   PathLokiGetHomeDir()
   gets the user's home dir (for ~/.q3a)
 */

char *LokiGetHomeDir( void ){
	#if GDEF_OS_WINDOWS
	return NULL;
	#else // !GDEF_OS_WINDOWS
	static char	buf[ 4096 ];
	struct passwd   pw, *pwp;
	char            *home;

	/* get the home environment variable */
	home = getenv( "HOME" );

	/* look up home dir in password database */
	if( home == NULL )
	{
		if ( getpwuid_r( getuid(), &pw, buf, sizeof( buf ), &pwp ) == 0 ) {
			return pw.pw_dir;
		}
	}

	/* return it */
	return home;
	#endif // !GDEF_OS_WINDOWS
}



/*
   PathLokiInitPaths()
   initializes some paths on linux/os x
 */

void LokiInitPaths( char *argv0 ){
	char *home;

	if ( homePath == NULL ) {
		/* get home dir */
		home = LokiGetHomeDir();
		if ( home == NULL ) {
			home = ".";
		}

		/* set home path */
		homePath = home;
	}
	else{
		home = homePath;
	}

	#if GDEF_OS_MACOS
	char *subPath = "/Library/Application Support";
	macLibraryApplicationSupportPath = safe_malloc( sizeof( char ) * ( strlen( home ) + strlen( subPath ) ) + 1 );
	sprintf( macLibraryApplicationSupportPath, "%s%s", home, subPath );
	#elif GDEF_OS_XDG
	xdgDataHomePath = getenv( "XDG_DATA_HOME" );

	if ( xdgDataHomePath == NULL ) {
		char *subPath = "/.local/share";
		xdgDataHomePath = safe_malloc( sizeof( char ) * ( strlen( home ) + strlen( subPath ) ) + 1 );
		sprintf( xdgDataHomePath, "%s%s", home, subPath );
	}
	#endif // GDEF_OS_XDG

	#if GDEF_OS_WINDOWS
	/* this is kinda crap, but hey */
	strcpy( installPath, "../" );
	#else // !GDEF_OS_WINDOWS

	char temp[ MAX_OS_PATH ];
	char *path;
	char *last;
	qboolean found;


	path = getenv( "PATH" );

	/* do some path divining */
	Q_strncpyz( temp, argv0, sizeof( temp ) );
	if ( strrchr( temp, '/' ) ) {
		argv0 = strrchr( argv0, '/' ) + 1;
	}
	else if ( path != NULL ) {

		/*
		   This code has a special behavior when q3map2 is a symbolic link.

		   For each dir in ${PATH} (example: "/usr/bin", "/usr/local/bin" if ${PATH} == "/usr/bin:/usr/local/bin"),
		   it looks for "${dir}/q3map2" (file exists and is executable),
		   then it uses "dirname(realpath("${dir}/q3map2"))/../" as installPath.

		   So, if "/usr/bin/q3map2" is a symbolic link to "/opt/radiant/tools/q3map2",
		   it will find the installPath "/usr/share/radiant/",
		   so q3map2 will look for "/opt/radiant/baseq3" to find paks.

		   More precisely, it looks for "${dir}/${argv[0]}",
		   so if "/usr/bin/q3map2" is a symbolic link to "/opt/radiant/tools/q3map2",
		   and if "/opt/radiant/tools/q3ma2" is a symbolic link to "/opt/radiant/tools/q3map2.x86_64",
		   it will use "dirname("/opt/radiant/tools/q3map2.x86_64")/../" as path,
		   so it will use "/opt/radiant/" as installPath, which will be expanded later to "/opt/radiant/baseq3" to find paks.
		*/

		found = qfalse;
		last = path;

		/* go through each : segment of path */
		while ( last[ 0 ] != '\0' && found == qfalse )
		{
			/* null out temp */
			temp[ 0 ] = '\0';

			/* find next chunk */
			last = strchr( path, ':' );
			if ( last == NULL ) {
				last = path + strlen( path );
			}

			/* found home dir candidate */
			if ( *path == '~' ) {
				Q_strncpyz( temp, home, sizeof( temp ) );
				path++;
			}


			/* concatenate */
			if ( last > ( path + 1 ) ) {
				// +1 hack: Q_strncat calls Q_strncpyz that expects a len including '\0'
				// so that extraneous char will be rewritten by '\0', so it's ok.
				// Also, in this case this extraneous char is always ':' or '\0', so it's ok.
				Q_strncat( temp, sizeof( temp ), path, ( last - path + 1) );
				Q_strcat( temp, sizeof( temp ), "/" );
			}
			Q_strcat( temp, sizeof( temp ), argv0 );

			/* verify the path */
			if ( access( temp, X_OK ) == 0 ) {
				found = qtrue;
			}
			path = last + 1;
		}
	}

	/* flake */
	if ( realpath( temp, installPath ) ) {
		/*
		   if "q3map2" is "/opt/radiant/tools/q3map2",
		   installPath is "/opt/radiant"
		*/
		*( strrchr( installPath, '/' ) ) = '\0';
		*( strrchr( installPath, '/' ) ) = '\0';
	}
	#endif // !GDEF_OS_WINDOWS
}



/*
   CleanPath() - ydnar
   cleans a dos path \ -> /
 */

void CleanPath( char *path ){
	while ( *path )
	{
		if ( *path == '\\' ) {
			*path = '/';
		}
		path++;
	}
}



/*
   GetGame() - ydnar
   gets the game_t based on a -game argument
   returns NULL if no match found
 */

game_t *GetGame( char *arg ){
	int i;


	/* dummy check */
	if ( arg == NULL || arg[ 0 ] == '\0' ) {
		return NULL;
	}

	/* joke */
	if ( !Q_stricmp( arg, "quake1" ) ||
		 !Q_stricmp( arg, "quake2" ) ||
		 !Q_stricmp( arg, "unreal" ) ||
		 !Q_stricmp( arg, "ut2k3" ) ||
		 !Q_stricmp( arg, "dn3d" ) ||
		 !Q_stricmp( arg, "dnf" ) ||
		 !Q_stricmp( arg, "hl" ) ) {
		Sys_Printf( "April fools, silly rabbit!\n" );
		exit( 0 );
	}

	/* test it */
	i = 0;
	while ( games[ i ].arg != NULL )
	{
		if ( Q_stricmp( arg, games[ i ].arg ) == 0 ) {
			return &games[ i ];
		}
		i++;
	}

	/* no matching game */
	return NULL;
}



/*
   AddBasePath() - ydnar
   adds a base path to the list
 */

void AddBasePath( char *path ){
	/* dummy check */
	if ( path == NULL || path[ 0 ] == '\0' || numBasePaths >= MAX_BASE_PATHS ) {
		return;
	}

	/* add it to the list */
	basePaths[ numBasePaths ] = safe_malloc( strlen( path ) + 1 );
	strcpy( basePaths[ numBasePaths ], path );
	CleanPath( basePaths[ numBasePaths ] );
	if ( EnginePath[0] == '\0' ) strcpy( EnginePath, basePaths[ numBasePaths ] );
	numBasePaths++;
}



/*
   AddHomeBasePath() - ydnar
   adds a base path to the beginning of the list
 */

void AddHomeBasePath( char *path ){
	int i;
	char temp[ MAX_OS_PATH ];

	if ( homePath == NULL ) {
		return;
	}

	/* dummy check */
	if ( path == NULL || path[ 0 ] == '\0' ) {
		return;
	}

	if ( strcmp( path, "." ) == 0 ) {
		/* -fs_homebase . means that -fs_home is to be used as is */
		strcpy( temp, homePath );
	}
	else {
		char *tempHomePath;
		tempHomePath = homePath;

		/* homePath is . on Windows if not user supplied */

		#if GDEF_OS_MACOS
		/*
		   use ${HOME}/Library/Application as ${HOME}
		   if home path is not user supplied
		   and strip the leading dot from prefix in any case
		  
		   basically it produces
		   ${HOME}/Library/Application/unvanquished
		   /user/supplied/home/path/unvanquished
		*/
		tempHomePath = macLibraryApplicationSupportPath;
		path = path + 1;
		#elif GDEF_OS_XDG
		/*
		   on Linux, check if game uses ${XDG_DATA_HOME}/prefix instead of ${HOME}/.prefix
		   if yes and home path is not user supplied
		   use XDG_DATA_HOME instead of HOME
		   and strip the leading dot

		   basically it produces
		   ${XDG_DATA_HOME}/unvanquished
		   /user/supplied/home/path/unvanquished

		   or
		   ${HOME}/.q3a
		   /user/supplied/home/path/.q3a
		 */

		sprintf( temp, "%s/%s", xdgDataHomePath, ( path + 1 ) );
		if ( access( temp, X_OK ) == 0 ) {
			if ( customHomePath == qfalse ) {
				tempHomePath = xdgDataHomePath;
	}
			path = path + 1;
		}
		#endif // GDEF_OS_XDG

		/* concatenate home dir and path */
		sprintf( temp, "%s/%s", tempHomePath, path );
	}

	/* make a hole */
	for ( i = ( MAX_BASE_PATHS - 2 ); i >= 0; i-- )
		basePaths[ i + 1 ] = basePaths[ i ];

	/* add it to the list */
	basePaths[ 0 ] = safe_malloc( strlen( temp ) + 1 );
	strcpy( basePaths[ 0 ], temp );
	CleanPath( basePaths[ 0 ] );
	numBasePaths++;
}



/*
   AddGamePath() - ydnar
   adds a game path to the list
 */

void AddGamePath( char *path ){
	int i;

	/* dummy check */
	if ( path == NULL || path[ 0 ] == '\0' || numGamePaths >= MAX_GAME_PATHS ) {
		return;
	}

	/* add it to the list */
	gamePaths[ numGamePaths ] = safe_malloc( strlen( path ) + 1 );
	strcpy( gamePaths[ numGamePaths ], path );
	CleanPath( gamePaths[ numGamePaths ] );
	numGamePaths++;

	/* don't add it if it's already there */
	for ( i = 0; i < numGamePaths - 1; i++ )
	{
		if ( strcmp( gamePaths[i], gamePaths[numGamePaths - 1] ) == 0 ) {
			free( gamePaths[numGamePaths - 1] );
			gamePaths[numGamePaths - 1] = NULL;
			numGamePaths--;
			break;
		}
	}

}


/*
   AddPakPath()
   adds a pak path to the list
 */

void AddPakPath( char *path ){
	/* dummy check */
	if ( path == NULL || path[ 0 ] == '\0' || numPakPaths >= MAX_PAK_PATHS ) {
		return;
	}

	/* add it to the list */
	pakPaths[ numPakPaths ] = safe_malloc( strlen( path ) + 1 );
	strcpy( pakPaths[ numPakPaths ], path );
	CleanPath( pakPaths[ numPakPaths ] );
	numPakPaths++;
}



/*
   InitPaths() - ydnar
   cleaned up some of the path initialization code from bsp.c
   will remove any arguments it uses
 */

void InitPaths( int *argc, char **argv ){
	int i, j, k, len, len2;
	char temp[ MAX_OS_PATH ];

	int noBasePath = 0;
	int noHomePath = 0;
	int noMagicPath = 0;

	/* note it */
	Sys_FPrintf( SYS_VRB, "--- InitPaths ---\n" );

	/* get the install path for backup */
	LokiInitPaths( argv[ 0 ] );

	/* set game to default (q3a) */
	game = &games[ 0 ];
	numBasePaths = 0;
	numGamePaths = 0;

	EnginePath[0] = '\0';

	/* parse through the arguments and extract those relevant to paths */
	for ( i = 0; i < *argc; i++ )
	{
		/* check for null */
		if ( argv[ i ] == NULL ) {
			continue;
		}

		/* -game */
		if ( strcmp( argv[ i ], "-game" ) == 0 ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No game specified after %s", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			game = GetGame( argv[ i ] );
			if ( game == NULL ) {
				game = &games[ 0 ];
			}
			argv[ i ] = NULL;
		}

		/* -fs_forbiddenpath */
		else if ( strcmp( argv[ i ], "-fs_forbiddenpath" ) == 0 ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			if ( g_numForbiddenDirs < VFS_MAXDIRS ) {
				strncpy( g_strForbiddenDirs[g_numForbiddenDirs], argv[i], PATH_MAX );
				g_strForbiddenDirs[g_numForbiddenDirs][PATH_MAX] = 0;
				++g_numForbiddenDirs;
			}
			argv[ i ] = NULL;
		}

		/* -fs_nobasepath */
		else if ( strcmp( argv[ i ], "-fs_nobasepath" ) == 0 ) {
			noBasePath = 1;
			// we don't want any basepath, neither guessed ones
			noMagicPath = 1;
			argv[ i ] = NULL;
		}		

		/* -fs_nomagicpath */
		else if ( strcmp( argv[ i ], "-fs_nomagicpath") == 0) {
			noMagicPath = 1;
			argv[ i ] = NULL;
		}

		/* -fs_basepath */
		else if ( strcmp( argv[ i ], "-fs_basepath" ) == 0 ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			AddBasePath( argv[ i ] );
			argv[ i ] = NULL;
		}

		/* -fs_game */
		else if ( strcmp( argv[ i ], "-fs_game" ) == 0 ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			AddGamePath( argv[ i ] );
			argv[ i ] = NULL;
		}

		/* -fs_home */
		else if ( strcmp( argv[ i ], "-fs_home" ) == 0 ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			customHomePath = qtrue;
			homePath = argv[i];
			argv[ i ] = NULL;
		}

		/* -fs_nohomepath */
		else if ( strcmp( argv[ i ], "-fs_nohomepath" ) == 0 ) {
			noHomePath = 1;
			argv[ i ] = NULL;
		}		

		/* -fs_homebase */
		else if ( strcmp( argv[ i ], "-fs_homebase" ) == 0 ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			homeBasePath = argv[i];
			argv[ i ] = NULL;
		}

		/* -fs_homepath - sets both of them */
		else if ( strcmp( argv[ i ], "-fs_homepath" ) == 0 ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			homePath = argv[i];
			homeBasePath = ".";
			argv[ i ] = NULL;
		}

		/* -fs_pakpath */
		else if ( strcmp( argv[ i ], "-fs_pakpath" ) == 0 ) {
			if ( ++i >= *argc || !argv[ i ] ) {
				Error( "Out of arguments: No path specified after %s.", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			AddPakPath( argv[ i ] );
			argv[ i ] = NULL;
		}
	}

	/* remove processed arguments */
	for ( i = 0, j = 0, k = 0; i < *argc && j < *argc; i++, j++ )
	{
		for ( ; j < *argc && argv[ j ] == NULL; j++ ) ;
		argv[ i ] = argv[ j ];
		if ( argv[ i ] != NULL ) {
			k++;
		}
	}
	*argc = k;

	/* add standard game path */
	AddGamePath( game->gamePath );

	/* if there is no base path set, figure it out unless fs_nomagicpath is set */
	if ( numBasePaths == 0 && noBasePath == 0 && noMagicPath == 0 ) {
		/* this is another crappy replacement for SetQdirFromPath() */
		len2 = strlen( game->magic );
		for ( i = 0; i < *argc && numBasePaths == 0; i++ )
		{
			/* extract the arg */
			strcpy( temp, argv[ i ] );
			CleanPath( temp );
			len = strlen( temp );
			Sys_FPrintf( SYS_VRB, "Searching for \"%s\" in \"%s\" (%d)...\n", game->magic, temp, i );

			/* this is slow, but only done once */
			for ( j = 0; j < ( len - len2 ); j++ )
			{
				/* check for the game's magic word */
				if ( Q_strncasecmp( &temp[ j ], game->magic, len2 ) == 0 ) {
					/* now find the next slash and nuke everything after it */
					while ( temp[ ++j ] != '/' && temp[ j ] != '\0' ) ;
					temp[ j ] = '\0';

					/* add this as a base path */
					AddBasePath( temp );
					break;
				}
			}
		}

		/* add install path */
		if ( numBasePaths == 0 ) {
			AddBasePath( installPath );
		}

		/* check again */
		if ( numBasePaths == 0 ) {
			Error( "Failed to find a valid base path." );
		}
	}

	if ( noBasePath == 1 ) {
		numBasePaths = 0;
	}

	if ( noHomePath == 0 ) {
		/* this only affects unix */
		if ( homeBasePath ) {
			AddHomeBasePath( homeBasePath );
		}
		else{
			AddHomeBasePath( game->homeBasePath );
		}
	}

	/* initialize vfs paths */
	if ( numBasePaths > MAX_BASE_PATHS ) {
		numBasePaths = MAX_BASE_PATHS;
	}
	if ( numGamePaths > MAX_GAME_PATHS ) {
		numGamePaths = MAX_GAME_PATHS;
	}

	/* walk the list of game paths */
	for ( j = 0; j < numGamePaths; j++ )
	{
		/* walk the list of base paths */
		for ( i = 0; i < numBasePaths; i++ )
		{
			/* create a full path and initialize it */
			sprintf( temp, "%s/%s/", basePaths[ i ], gamePaths[ j ] );
			vfsInitDirectory( temp );
		}
	}

	/* initialize vfs paths */
	if ( numPakPaths > MAX_PAK_PATHS ) {
		numPakPaths = MAX_PAK_PATHS;
	}

	/* walk the list of pak paths */
	for ( i = 0; i < numPakPaths; i++ )
	{
		/* initialize this pak path */
		vfsInitDirectory( pakPaths[ i ] );
	}

	/* done */
	Sys_Printf( "\n" );
}
