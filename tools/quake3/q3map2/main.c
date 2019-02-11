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

   -------------------------------------------------------------------------------

   This code has been altered significantly from its original form, to support
   several games based on the Quake III Arena engine, in the form of "Q3Map2."

   ------------------------------------------------------------------------------- */



/* marker */
#define MAIN_C



/* dependencies */
#include "q3map2.h"
#include <glib.h>

/*
   Random()
   returns a pseudorandom number between 0 and 1
 */

vec_t Random( void ){
	return (vec_t) rand() / RAND_MAX;
}


char *Q_strncpyz( char *dst, const char *src, size_t len ) {
	if ( len == 0 ) {
		abort();
	}

	strncpy( dst, src, len );
	dst[ len - 1 ] = '\0';
	return dst;
}


char *Q_strcat( char *dst, size_t dlen, const char *src ) {
	size_t n = strlen( dst );

	if ( n > dlen ) {
		abort(); /* buffer overflow */
	}

	return Q_strncpyz( dst + n, src, dlen - n );
}


char *Q_strncat( char *dst, size_t dlen, const char *src, size_t slen ) {
	size_t n = strlen( dst );

	if ( n > dlen ) {
		abort(); /* buffer overflow */
	}

	return Q_strncpyz( dst + n, src, MIN( slen, dlen - n ) );
}


/*
   ExitQ3Map()
   cleanup routine
 */

static void ExitQ3Map( void ){
	BSPFilesCleanup();
	if ( mapDrawSurfs != NULL ) {
		free( mapDrawSurfs );
	}
}


/*
   ShiftBSPMain()
   shifts a map: for testing physics with huge coordinates
 */

int ShiftBSPMain( int argc, char **argv ){
	int i, j;
	float f, a;
	vec3_t scale;
	vec3_t vec;
	char str[ 1024 ];
	int uniform, axis;
	qboolean texscale;
	float *old_xyzst = NULL;
	float spawn_ref = 0;


	/* arg checking */
	if ( argc < 3 ) {
		Sys_Printf( "Usage: q3map [-v] -shift [-tex] [-spawn_ref <value>] <value> <mapname>\n" );
		return 0;
	}

	texscale = qfalse;
	for ( i = 1; i < argc - 2; ++i )
	{
		if ( !strcmp( argv[i], "-tex" ) ) {
			texscale = qtrue;
		}
		else if ( !strcmp( argv[i], "-spawn_ref" ) ) {
			spawn_ref = atof( argv[i + 1] );
			++i;
		}
		else{
			break;
		}
	}

	/* get shift */
	// if(argc-2 >= i) // always true
	scale[2] = scale[1] = scale[0] = atof( argv[ argc - 2 ] );
	if ( argc - 3 >= i ) {
		scale[1] = scale[0] = atof( argv[ argc - 3 ] );
	}
	if ( argc - 4 >= i ) {
		scale[0] = atof( argv[ argc - 4 ] );
	}

	uniform = ( ( scale[0] == scale[1] ) && ( scale[1] == scale[2] ) );


	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();

	/* note it */
	Sys_Printf( "--- ShiftBSP ---\n" );
	Sys_FPrintf( SYS_VRB, "%9d entities\n", numEntities );

	/* shift entity keys */
	for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		/* shift origin */
		GetVectorForKey( &entities[ i ], "origin", vec );
		if ( ( vec[ 0 ] || vec[ 1 ] || vec[ 2 ] ) ) {
			if ( !strncmp( ValueForKey( &entities[i], "classname" ), "info_player_", 12 ) ) {
				vec[2] += spawn_ref;
			}
			vec[0] += scale[0];
			vec[1] += scale[1];
			vec[2] += scale[2];
			if ( !strncmp( ValueForKey( &entities[i], "classname" ), "info_player_", 12 ) ) {
				vec[2] -= spawn_ref;
			}
			sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
			SetKeyValue( &entities[ i ], "origin", str );
		}

	}

	/* shift models */
	for ( i = 0; i < numBSPModels; i++ )
	{
		bspModels[ i ].mins[0] += scale[0];
		bspModels[ i ].mins[1] += scale[1];
		bspModels[ i ].mins[2] += scale[2];
		bspModels[ i ].maxs[0] += scale[0];
		bspModels[ i ].maxs[1] += scale[1];
		bspModels[ i ].maxs[2] += scale[2];
	}

	/* shift nodes */
	for ( i = 0; i < numBSPNodes; i++ )
	{
		bspNodes[ i ].mins[0] += scale[0];
		bspNodes[ i ].mins[1] += scale[1];
		bspNodes[ i ].mins[2] += scale[2];
		bspNodes[ i ].maxs[0] += scale[0];
		bspNodes[ i ].maxs[1] += scale[1];
		bspNodes[ i ].maxs[2] += scale[2];
	}

	/* shift leafs */
	for ( i = 0; i < numBSPLeafs; i++ )
	{
		bspLeafs[ i ].mins[0] += scale[0];
		bspLeafs[ i ].mins[1] += scale[1];
		bspLeafs[ i ].mins[2] += scale[2];
		bspLeafs[ i ].maxs[0] += scale[0];
		bspLeafs[ i ].maxs[1] += scale[1];
		bspLeafs[ i ].maxs[2] += scale[2];
	}

	/* shift drawverts */
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[i].xyz[0] += scale[0];
		bspDrawVerts[i].xyz[1] += scale[1];
		bspDrawVerts[i].xyz[2] += scale[2];
	}

	/* shift planes */

	vec3_t point;

	for ( i = 0; i < numBSPPlanes; i++ )
	{
		//find point on plane
		for ( j=0; j<3; j++ ){
			if ( fabs( bspPlanes[ i ].normal[j] ) > 0.5 ){
				point[j] = bspPlanes[ i ].dist / bspPlanes[ i ].normal[j];
				point[(j+1)%3] = point[(j+2)%3] = 0;
				break;
			}
		}
		//shift point
		for ( j=0; j<3; j++ ){
			point[j] += scale[j];
		}
		//calc new plane dist
		bspPlanes[ i ].dist = DotProduct( point, bspPlanes[ i ].normal );
	}

	/* scale gridsize */
	/*
	GetVectorForKey( &entities[ 0 ], "gridsize", vec );
	if ( ( vec[ 0 ] + vec[ 1 ] + vec[ 2 ] ) == 0.0f ) {
		VectorCopy( gridSize, vec );
	}
	vec[0] *= scale[0];
	vec[1] *= scale[1];
	vec[2] *= scale[2];
	sprintf( str, "%f %f %f", vec[ 0 ], vec[ 1 ], vec[ 2 ] );
	SetKeyValue( &entities[ 0 ], "gridsize", str );
*/
	/* inject command line parameters */
	InjectCommandLine( argv, 0, argc - 1 );

	/* write the bsp */
	UnparseEntities();
	StripExtension( source );
	DefaultExtension( source, "_sh.bsp" );
	Sys_Printf( "Writing %s\n", source );
	WriteBSPFile( source );

	/* return to sender */
	return 0;
}


void FixDOSName( char *src ){
	if ( src == NULL ) {
		return;
	}

	while ( *src )
	{
		if ( *src == '\\' ) {
			*src = '/';
		}
		src++;
	}
}

/*
	Check if newcoming texture is unique and not excluded
*/
void tex2list( char* texlist, int *texnum, char* EXtex, int *EXtexnum ){
	int i;
	if ( token[0] == '\0') return;
	StripExtension( token );
	FixDOSName( token );
	for ( i = 0; i < *texnum; i++ ){
		if ( !Q_stricmp( texlist[i], token ) ) return;
	}
	for ( i = 0; i < *EXtexnum; i++ ){
		if ( !Q_stricmp( EXtex[i], token ) ) return;
	}
	strcpy ( texlist + (*texnum)*65, token );
	(*texnum)++;
	return;
}


/*
	Check if newcoming res is unique
*/
void res2list( char* data, int *num ){
	int i;
	if ( *( data + (*num)*65 ) == '\0') return;
	for ( i = 0; i < *num; i++ ){
		if ( !Q_stricmp( data[i], data[*num] ) ) return;
	}
	(*num)++;
	return;
}

void parseEXblock ( char* data, int *num, const char *exName ){
	if ( !GetToken( qtrue ) || strcmp( token, "{" ) ) {
		Error( "ReadExclusionsFile: %s, line %d: { not found", exName, scriptline );
	}
	while ( 1 )
	{
		if ( !GetToken( qtrue ) ) {
			break;
		}
		if ( !strcmp( token, "}" ) ) {
			break;
		}
		if ( token[0] == '{' ) {
			Error( "ReadExclusionsFile: %s, line %d: brace, opening twice in a row.", exName, scriptline );
		}

		/* add to list */
		strcpy( data + (*num)*65, token );
		(*num)++;
	}
	return;
}

char q3map2path[1024];
/*
   pk3BSPMain()
   map autopackager, works for Q3 type of shaders and ents
 */

int pk3BSPMain( int argc, char **argv ){
	int i, j, len;
	qboolean dbg = qfalse, png = qfalse;

	/* process arguments */
	for ( i = 1; i < ( argc - 1 ); i++ ){
		if ( !strcmp( argv[ i ],  "-dbg" ) ) {
			dbg = qtrue;
		}
		else if ( !strcmp( argv[ i ],  "-png" ) ) {
			png = qtrue;
		}
	}

	/* do some path mangling */
	strcpy( source, ExpandArg( argv[ argc - 1 ] ) );
	StripExtension( source );
	DefaultExtension( source, ".bsp" );

	/* load the bsp */
	Sys_Printf( "Loading %s\n", source );
	LoadBSPFile( source );
	ParseEntities();


	char packname[ 1024 ], base[ 1024 ], nameOFmap[ 1024 ], temp[ 1024 ];

	/* copy map name */
	strcpy( base, source );
	StripExtension( base );

	/* extract map name */
	len = strlen( base ) - 1;
	while ( len > 0 && base[ len ] != '/' && base[ len ] != '\\' )
		len--;
	strcpy( nameOFmap, &base[ len + 1 ] );


	qboolean drawsurfSHs[1024] = { qfalse };

	for ( i = 0; i < numBSPDrawSurfaces; i++ ){
		/* can't exclude nodraw patches here (they want shaders :0!) */
		//if ( !( bspDrawSurfaces[i].surfaceType == 2 && bspDrawSurfaces[i].numIndexes == 0 ) ) drawsurfSHs[bspDrawSurfaces[i].shaderNum] = qtrue;
		drawsurfSHs[ bspDrawSurfaces[i].shaderNum ] = qtrue;
		//Sys_Printf( "%s\n", bspShaders[bspDrawSurfaces[i].shaderNum].shader );
	}

	int pk3ShadersN = 0;
	char* pk3Shaders;
	pk3Shaders = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3SoundsN = 0;
	char* pk3Sounds;
	pk3Sounds = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3ShaderfilesN = 0;
	char* pk3Shaderfiles;
	pk3Shaderfiles = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3TexturesN = 0;
	char* pk3Textures;
	pk3Textures = (char *)calloc( 1024*65, sizeof( char ) );
	int pk3VideosN = 0;
	char* pk3Videos;
	pk3Videos = (char *)calloc( 1024*65, sizeof( char ) );



	for ( i = 0; i < numBSPShaders; i++ ){
		if ( drawsurfSHs[i] ){
			strcpy( pk3Shaders + pk3ShadersN*65, bspShaders[i].shader );
			res2list( pk3Shaders, &pk3ShadersN );
			//pk3ShadersN++;
			//Sys_Printf( "%s\n", bspShaders[i].shader );
		}
	}

	/* Ent keys */
	epair_t *ep;
	for ( ep = entities[0].epairs; ep != NULL; ep = ep->next )
	{
		if ( !Q_strncasecmp( ep->key, "vertexremapshader", 17 ) ) {
			sscanf( ep->value, "%*[^;] %*[;] %s", pk3Shaders + pk3ShadersN*65 );
			res2list( pk3Shaders, &pk3ShadersN );
		}
	}
	strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[0], "music" ) );
	if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' ){
		FixDOSName( pk3Sounds + pk3SoundsN*65 );
		DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
		res2list( pk3Sounds, &pk3SoundsN );
	}

	for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		strcpy( pk3Sounds + pk3SoundsN*65, ValueForKey( &entities[i], "noise" ) );
		if ( *( pk3Sounds + pk3SoundsN*65 ) != '\0' && *( pk3Sounds + pk3SoundsN*65 ) != '*' ){
			FixDOSName( pk3Sounds + pk3SoundsN*65 );
			DefaultExtension( pk3Sounds + pk3SoundsN*65, ".wav" );
			res2list( pk3Sounds, &pk3SoundsN );
		}

		if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "func_plat" ) ){
			strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_strt.wav");
			res2list( pk3Sounds, &pk3SoundsN );
			strcpy( pk3Sounds + pk3SoundsN*65, "sound/movers/plats/pt1_end.wav");
			res2list( pk3Sounds, &pk3SoundsN );
		}
		if ( !Q_stricmp( ValueForKey( &entities[i], "classname" ), "target_push" ) ){
			if ( !(IntForKey( &entities[i], "spawnflags") & 1) ){
				strcpy( pk3Sounds + pk3SoundsN*65, "sound/misc/windfly.wav");
				res2list( pk3Sounds, &pk3SoundsN );
			}
		}
		strcpy( pk3Shaders + pk3ShadersN*65, ValueForKey( &entities[i], "targetShaderNewName" ) );
		res2list( pk3Shaders, &pk3ShadersN );
	}

	//levelshot
	sprintf( pk3Shaders + pk3ShadersN*65, "levelshots/%s", nameOFmap );
	res2list( pk3Shaders, &pk3ShadersN );


	if( dbg ){
		Sys_Printf( "\tDrawsurface+ent calls....%i\n", pk3ShadersN );
		for ( i = 0; i < pk3ShadersN; i++ ){
			Sys_Printf( "%s\n", pk3Shaders + i*65 );
		}
		Sys_Printf( "\tSounds....%i\n", pk3SoundsN );
		for ( i = 0; i < pk3SoundsN; i++ ){
			Sys_Printf( "%s\n", pk3Sounds + i*65 );
		}
	}

	vfsListShaderFiles( pk3Shaderfiles, &pk3ShaderfilesN );

	if( dbg ){
		Sys_Printf( "\tSchroider fileses.....%i\n", pk3ShaderfilesN );
		for ( i = 0; i < pk3ShaderfilesN; i++ ){
			Sys_Printf( "%s\n", pk3Shaderfiles + i*65 );
		}
	}


	/* load exclusions file */
	int EXpk3TexturesN = 0;
	char* EXpk3Textures;
	EXpk3Textures = (char *)calloc( 4096*65, sizeof( char ) );
	int EXpk3ShadersN = 0;
	char* EXpk3Shaders;
	EXpk3Shaders = (char *)calloc( 4096*65, sizeof( char ) );
	int EXpk3SoundsN = 0;
	char* EXpk3Sounds;
	EXpk3Sounds = (char *)calloc( 4096*65, sizeof( char ) );
	int EXpk3ShaderfilesN = 0;
	char* EXpk3Shaderfiles;
	EXpk3Shaderfiles = (char *)calloc( 4096*65, sizeof( char ) );
	int EXpk3VideosN = 0;
	char* EXpk3Videos;
	EXpk3Videos = (char *)calloc( 4096*65, sizeof( char ) );

	char exName[ 1024 ];
	byte *buffer;
	int size;

	strcpy( exName, q3map2path );
	char *cut = strrchr( exName, '\\' );
	char *cut2 = strrchr( exName, '/' );
	if ( cut == NULL && cut2 == NULL ){
		Sys_Printf( "WARNING: Unable to load exclusions file.\n" );
		goto skipEXfile;
	}
	if ( cut2 > cut ) cut = cut2;
	cut[1] = '\0';
	strcat( exName, game->arg );
	strcat( exName, ".exclude" );

	Sys_Printf( "Loading %s\n", exName );
	size = TryLoadFile( exName, (void**) &buffer );
	if ( size <= 0 ) {
		Sys_Printf( "WARNING: Unable to find exclusions file %s.\n", exName );
		goto skipEXfile;
	}

	/* parse the file */
	ParseFromMemory( (char *) buffer, size );

	/* tokenize it */
	while ( 1 )
	{
		/* test for end of file */
		if ( !GetToken( qtrue ) ) {
			break;
		}

		/* blocks */
		if ( !Q_stricmp( token, "textures" ) ){
			parseEXblock ( EXpk3Textures, &EXpk3TexturesN, exName );
		}
		else if ( !Q_stricmp( token, "shaders" ) ){
			parseEXblock ( EXpk3Shaders, &EXpk3ShadersN, exName );
		}
		else if ( !Q_stricmp( token, "shaderfiles" ) ){
			parseEXblock ( EXpk3Shaderfiles, &EXpk3ShaderfilesN, exName );
		}
		else if ( !Q_stricmp( token, "sounds" ) ){
			parseEXblock ( EXpk3Sounds, &EXpk3SoundsN, exName );
		}
		else if ( !Q_stricmp( token, "videos" ) ){
			parseEXblock ( EXpk3Videos, &EXpk3VideosN, exName );
		}
		else{
			Error( "ReadExclusionsFile: %s, line %d: unknown block name!\nValid ones are: textures, shaders, shaderfiles, sounds, videos.", exName, scriptline );
		}
	}

	/* free the buffer */
	free( buffer );

skipEXfile:

	if( dbg ){
		Sys_Printf( "\tEXpk3Textures....%i\n", EXpk3TexturesN );
		for ( i = 0; i < EXpk3TexturesN; i++ ) Sys_Printf( "%s\n", EXpk3Textures + i*65 );
		Sys_Printf( "\tEXpk3Shaders....%i\n", EXpk3ShadersN );
		for ( i = 0; i < EXpk3ShadersN; i++ ) Sys_Printf( "%s\n", EXpk3Shaders + i*65 );
		Sys_Printf( "\tEXpk3Shaderfiles....%i\n", EXpk3ShaderfilesN );
		for ( i = 0; i < EXpk3ShaderfilesN; i++ ) Sys_Printf( "%s\n", EXpk3Shaderfiles + i*65 );
		Sys_Printf( "\tEXpk3Sounds....%i\n", EXpk3SoundsN );
		for ( i = 0; i < EXpk3SoundsN; i++ ) Sys_Printf( "%s\n", EXpk3Sounds + i*65 );
		Sys_Printf( "\tEXpk3Videos....%i\n", EXpk3VideosN );
		for ( i = 0; i < EXpk3VideosN; i++ ) Sys_Printf( "%s\n", EXpk3Videos + i*65 );
	}


	//Parse Shader Files
	for ( i = 0; i < pk3ShaderfilesN; i++ ){
		qboolean wantShader = qfalse, wantShaderFile = qfalse;
		char shadername[ 1024 ], lastwantedShader[ 1024 ];

		/* load the shader */
		sprintf( temp, "%s/%s", game->shaderPath, pk3Shaderfiles + i*65 );
		LoadScriptFile( temp, 0 );

		/* tokenize it */
		while ( 1 )
		{
			/* test for end of file */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			//dump shader names
			if( dbg ) Sys_Printf( "%s\n", token );

			/* do wanna le shader? */
			wantShader = qfalse;
			for ( j = 0; j < pk3ShadersN; j++ ){
				if ( !Q_stricmp( pk3Shaders + j*65, token) ){
					strcpy ( shadername, pk3Shaders + j*65 );
					*(pk3Shaders + j*65) = '\0';
					wantShader = qtrue;
					break;
				}
			}

			/* handle { } section */
			if ( !GetToken( qtrue ) ) {
				break;
			}
			if ( strcmp( token, "{" ) ) {
					Error( "ParseShaderFile: %s, line %d: { not found!\nFound instead: %s",
						temp, scriptline, token );
			}

			while ( 1 )
			{
				/* get the next token */
				if ( !GetToken( qtrue ) ) {
					break;
				}
				if ( !strcmp( token, "}" ) ) {
					break;
				}


				/* -----------------------------------------------------------------
				shader stages (passes)
				----------------------------------------------------------------- */

				/* parse stage directives */
				if ( !strcmp( token, "{" ) ) {
					while ( 1 )
					{
						if ( !GetToken( qtrue ) ) {
							break;
						}
						if ( !strcmp( token, "}" ) ) {
							break;
						}
						/* skip the shader */
						if ( !wantShader ) continue;

						/* digest any images */
						if ( !Q_stricmp( token, "map" ) ||
							!Q_stricmp( token, "clampMap" ) ) {

							/* get an image */
							GetToken( qfalse );
							if ( token[ 0 ] != '*' && token[ 0 ] != '$' ) {
								tex2list( pk3Textures, &pk3TexturesN, EXpk3Textures, &EXpk3TexturesN );
							}
						}
						else if ( !Q_stricmp( token, "animMap" ) ||
							!Q_stricmp( token, "clampAnimMap" ) ) {
							GetToken( qfalse );// skip num
							while ( TokenAvailable() ){
								GetToken( qfalse );
								tex2list( pk3Textures, &pk3TexturesN, EXpk3Textures, &EXpk3TexturesN );
							}
						}
						else if ( !Q_stricmp( token, "videoMap" ) ){
							GetToken( qfalse );
							FixDOSName( token );
							if ( strchr( token, "/" ) == NULL ){
								sprintf( temp, "video/%s", token );
								strcpy( token, temp );
							}
							for ( j = 0; j < pk3VideosN; j++ ){
								if ( !Q_stricmp( pk3Videos + j*65, token ) ){
									goto away;
								}
							}
							for ( j = 0; j < EXpk3VideosN; j++ ){
								if ( !Q_stricmp( EXpk3Videos + j*65, token ) ){
									goto away;
								}
							}
							strcpy ( pk3Videos + pk3VideosN*65, token );
							pk3VideosN++;
							away:
							j = 0;
						}
					}
				}
				/* skip the shader */
				else if ( !wantShader ) continue;

				/* -----------------------------------------------------------------
				surfaceparm * directives
				----------------------------------------------------------------- */

				/* match surfaceparm */
				else if ( !Q_stricmp( token, "surfaceparm" ) ) {
					GetToken( qfalse );
					if ( !Q_stricmp( token, "nodraw" ) ) {
						wantShader = qfalse;
					}
				}

				/* skyparms <outer image> <cloud height> <inner image> */
				else if ( !Q_stricmp( token, "skyParms" ) ) {
					/* get image base */
					GetToken( qfalse );

					/* ignore bogus paths */
					if ( Q_stricmp( token, "-" ) && Q_stricmp( token, "full" ) ) {
						strcpy ( temp, token );
						sprintf( token, "%s_up", temp );
						tex2list( pk3Textures, &pk3TexturesN, EXpk3Textures, &EXpk3TexturesN );
						sprintf( token, "%s_dn", temp );
						tex2list( pk3Textures, &pk3TexturesN, EXpk3Textures, &EXpk3TexturesN );
						sprintf( token, "%s_lf", temp );
						tex2list( pk3Textures, &pk3TexturesN, EXpk3Textures, &EXpk3TexturesN );
						sprintf( token, "%s_rt", temp );
						tex2list( pk3Textures, &pk3TexturesN, EXpk3Textures, &EXpk3TexturesN );
						sprintf( token, "%s_bk", temp );
						tex2list( pk3Textures, &pk3TexturesN, EXpk3Textures, &EXpk3TexturesN );
						sprintf( token, "%s_ft", temp );
						tex2list( pk3Textures, &pk3TexturesN, EXpk3Textures, &EXpk3TexturesN );
					}
					/* skip rest of line */
					GetToken( qfalse );
					GetToken( qfalse );
				}
			}
			//exclude shader
			if ( wantShader ){
				for ( j = 0; j < EXpk3ShadersN; j++ ){
					if ( !Q_stricmp( EXpk3Shaders + j*65, shadername ) ){
						wantShader = qfalse;
						break;
					}
				}
				/* shouldnt make shaders for shipped with the game textures aswell */
				if ( wantShader ){
					for ( j = 0; j < EXpk3TexturesN; j++ ){
						if ( !Q_stricmp( EXpk3Textures + j*65, shadername ) ){
							wantShader = qfalse;
							break;
						}
					}
				}
				if ( wantShader ){
					wantShaderFile = qtrue;
					strcpy( lastwantedShader, shadername );
				}
			}
		}
		//exclude shader file
		if ( wantShaderFile ){
			for ( j = 0; j < EXpk3ShaderfilesN; j++ ){
				if ( !Q_stricmp( EXpk3Shaderfiles + j*65, pk3Shaderfiles + i*65 ) ){
					Sys_Printf( "WARNING: excluded shader %s, since it was located in restricted shader file: %s\n", lastwantedShader, pk3Shaderfiles + i*65 );
					*( pk3Shaderfiles + i*65 ) = '\0';
					break;
				}
			}
		}
		else {
			*( pk3Shaderfiles + i*65 ) = '\0';
		}

	}



/* exclude stuff */
//pure textures (shader ones are done)
	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			FixDOSName( pk3Shaders + i*65 );
			for ( j = 0; j < pk3TexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, pk3Textures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
			if ( *( pk3Shaders + i*65 ) == '\0' ) continue;
			for ( j = 0; j < EXpk3TexturesN; j++ ){
				if ( !Q_stricmp( pk3Shaders + i*65, EXpk3Textures + j*65 ) ){
					*( pk3Shaders + i*65 ) = '\0';
					break;
				}
			}
		}
	}

//snds
	for ( i = 0; i < pk3SoundsN; i++ ){
		for ( j = 0; j < EXpk3SoundsN; j++ ){
			if ( !Q_stricmp( pk3Sounds + i*65, EXpk3Sounds + j*65 ) ){
				*( pk3Sounds + i*65 ) = '\0';
				break;
			}
		}
	}

	if( dbg ){
		Sys_Printf( "\tShader referenced textures....%i\n", pk3TexturesN );
		for ( i = 0; i < pk3TexturesN; i++ ){
			Sys_Printf( "%s\n", pk3Textures + i*65 );
		}
		Sys_Printf( "\tShader files....\n" );
		for ( i = 0; i < pk3ShaderfilesN; i++ ){
			if ( *( pk3Shaderfiles + i*65 ) != '\0' ) Sys_Printf( "%s\n", pk3Shaderfiles + i*65 );
		}
		Sys_Printf( "\tPure textures....\n" );
		for ( i = 0; i < pk3ShadersN; i++ ){
			if ( *( pk3Shaders + i*65 ) != '\0' ) Sys_Printf( "%s\n", pk3Shaders + i*65 );
		}
	}


	sprintf( packname, "%s/%s_autopacked.pk3", EnginePath, nameOFmap );
	remove( packname );

	Sys_Printf( "--- ZipZip ---\n" );

	Sys_Printf( "\n\tShader referenced textures....\n" );

	for ( i = 0; i < pk3TexturesN; i++ ){
		if ( png ){
			sprintf( temp, "%s.png", pk3Textures + i*65 );
			if ( vfsPackFile( temp, packname ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
		}
		sprintf( temp, "%s.tga", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		sprintf( temp, "%s.jpg", pk3Textures + i*65 );
		if ( vfsPackFile( temp, packname ) ){
			Sys_Printf( "++%s\n", temp );
			continue;
		}
		Sys_Printf( "  !FAIL! %s\n", pk3Textures + i*65 );
	}

	Sys_Printf( "\n\tPure textures....\n" );

	for ( i = 0; i < pk3ShadersN; i++ ){
		if ( *( pk3Shaders + i*65 ) != '\0' ){
			if ( png ){
				sprintf( temp, "%s.png", pk3Shaders + i*65 );
				if ( vfsPackFile( temp, packname ) ){
					Sys_Printf( "++%s\n", temp );
					continue;
				}
			}
			sprintf( temp, "%s.tga", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			sprintf( temp, "%s.jpg", pk3Shaders + i*65 );
			if ( vfsPackFile( temp, packname ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			Sys_Printf( "  !FAIL! %s\n", pk3Shaders + i*65 );
		}
	}

	Sys_Printf( "\n\tShaizers....\n" );

	for ( i = 0; i < pk3ShaderfilesN; i++ ){
		if ( *( pk3Shaderfiles + i*65 ) != '\0' ){
			sprintf( temp, "%s/%s", game->shaderPath, pk3Shaderfiles + i*65 );
			if ( vfsPackFile( temp, packname ) ){
				Sys_Printf( "++%s\n", temp );
				continue;
			}
			Sys_Printf( "  !FAIL! %s\n", pk3Shaders + i*65 );
		}
	}

	Sys_Printf( "\n\tSounds....\n" );

	for ( i = 0; i < pk3SoundsN; i++ ){
		if ( *( pk3Sounds + i*65 ) != '\0' ){
			if ( vfsPackFile( pk3Sounds + i*65, packname ) ){
				Sys_Printf( "++%s\n", pk3Sounds + i*65 );
				continue;
			}
			Sys_Printf( "  !FAIL! %s\n", pk3Sounds + i*65 );
		}
	}

	Sys_Printf( "\n\tVideos....\n" );

	for ( i = 0; i < pk3VideosN; i++ ){
		if ( vfsPackFile( pk3Videos + i*65, packname ) ){
			Sys_Printf( "++%s\n", pk3Videos + i*65 );
			continue;
		}
		Sys_Printf( "  !FAIL! %s\n", pk3Videos + i*65 );
	}

	Sys_Printf( "\n\t.\n" );

	sprintf( temp, "maps/%s.bsp", nameOFmap );
	if ( vfsPackFile( temp, packname ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  !FAIL! %s\n", temp );
	}

	sprintf( temp, "maps/%s.aas", nameOFmap );
	if ( vfsPackFile( temp, packname ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  !FAIL! %s\n", temp );
	}

	sprintf( temp, "scripts/%s.arena", nameOFmap );
	if ( vfsPackFile( temp, packname ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  !FAIL! %s\n", temp );
	}

	sprintf( temp, "scripts/%s.defi", nameOFmap );
	if ( vfsPackFile( temp, packname ) ){
			Sys_Printf( "++%s\n", temp );
		}
	else{
		Sys_Printf( "  !FAIL! %s\n", temp );
	}

	Sys_Printf( "\nSaved to %s\n", packname );
	/* return to sender */
	return 0;
}


/*
   main()
   q3map mojo...
 */

int main( int argc, char **argv ){
	int i, r;
	double start, end;
	extern qboolean werror;


	/* we want consistent 'randomness' */
	srand( 0 );

	/* start timer */
	start = I_FloatTime();

	/* this was changed to emit version number over the network */
	printf( Q3MAP_VERSION "\n" );

	/* set exit call */
	atexit( ExitQ3Map );

	/* read general options first */
	for ( i = 1; i < argc; i++ )
	{
		/* -help */
		if ( !strcmp( argv[ i ], "-h" ) || !strcmp( argv[ i ], "--help" )
			|| !strcmp( argv[ i ], "-help" ) ) {
			HelpMain(argv[i+1]);
			return 0;
		}

		/* -connect */
		if ( !strcmp( argv[ i ], "-connect" ) ) {
			argv[ i ] = NULL;
			i++;
			Broadcast_Setup( argv[ i ] );
			argv[ i ] = NULL;
		}

		/* verbose */
		else if ( !strcmp( argv[ i ], "-v" ) ) {
			if ( !verbose ) {
				verbose = qtrue;
				argv[ i ] = NULL;
			}
		}

		/* force */
		else if ( !strcmp( argv[ i ], "-force" ) ) {
			force = qtrue;
			argv[ i ] = NULL;
		}

		/* make all warnings into errors */
		else if ( !strcmp( argv[ i ], "-werror" ) ) {
			werror = qtrue;
			argv[ i ] = NULL;
		}

		/* patch subdivisions */
		else if ( !strcmp( argv[ i ], "-subdivisions" ) ) {
			argv[ i ] = NULL;
			i++;
			patchSubdivisions = atoi( argv[ i ] );
			argv[ i ] = NULL;
			if ( patchSubdivisions <= 0 ) {
				patchSubdivisions = 1;
			}
		}

		/* threads */
		else if ( !strcmp( argv[ i ], "-threads" ) ) {
			argv[ i ] = NULL;
			i++;
			numthreads = atoi( argv[ i ] );
			argv[ i ] = NULL;
		}

		else if( !strcmp( argv[ i ], "-nocmdline" ) )
		{
			Sys_Printf( "noCmdLine\n" );
			nocmdline = qtrue;
			argv[ i ] = NULL;
		}

	}

	/* init model library */
	PicoInit();
	PicoSetMallocFunc( safe_malloc );
	PicoSetFreeFunc( free );
	PicoSetPrintFunc( PicoPrintFunc );
	PicoSetLoadFileFunc( PicoLoadFileFunc );
	PicoSetFreeFileFunc( free );

	/* set number of threads */
	ThreadSetDefault();

	/* generate sinusoid jitter table */
	for ( i = 0; i < MAX_JITTERS; i++ )
	{
		jitters[ i ] = sin( i * 139.54152147 );
		//%	Sys_Printf( "Jitter %4d: %f\n", i, jitters[ i ] );
	}

	/* we print out two versions, q3map's main version (since it evolves a bit out of GtkRadiant)
	   and we put the GtkRadiant version to make it easy to track with what version of Radiant it was built with */

	Sys_Printf( "Q3Map         - v1.0r (c) 1999 Id Software Inc.\n" );
	Sys_Printf( "Q3Map (ydnar) - v" Q3MAP_VERSION "\n" );
	Sys_Printf( "NetRadiant    - v" RADIANT_VERSION " " __DATE__ " " __TIME__ "\n" );
	Sys_Printf( "%s\n", Q3MAP_MOTD );
	Sys_Printf( "%s\n", argv[0] );

	strcpy( q3map2path, argv[0] );//fuer autoPack func

	/* ydnar: new path initialization */
	InitPaths( &argc, argv );

	/* set game options */
	if ( !patchSubdivisions ) {
		patchSubdivisions = game->patchSubdivisions;
	}

	/* check if we have enough options left to attempt something */
	if ( argc < 2 ) {
		Error( "Usage: %s [general options] [options] mapfile", argv[ 0 ] );
	}

	/* fixaas */
	if ( !strcmp( argv[ 1 ], "-fixaas" ) ) {
		r = FixAASMain( argc - 1, argv + 1 );
	}

	/* analyze */
	else if ( !strcmp( argv[ 1 ], "-analyze" ) ) {
		r = AnalyzeBSPMain( argc - 1, argv + 1 );
	}

	/* info */
	else if ( !strcmp( argv[ 1 ], "-info" ) ) {
		r = BSPInfoMain( argc - 2, argv + 2 );
	}

	/* vis */
	else if ( !strcmp( argv[ 1 ], "-vis" ) ) {
		r = VisMain( argc - 1, argv + 1 );
	}

	/* light */
	else if ( !strcmp( argv[ 1 ], "-light" ) ) {
		r = LightMain( argc - 1, argv + 1 );
	}

	/* vlight */
	else if ( !strcmp( argv[ 1 ], "-vlight" ) ) {
		Sys_FPrintf( SYS_WRN, "WARNING: VLight is no longer supported, defaulting to -light -fast instead\n\n" );
		argv[ 1 ] = "-fast";    /* eek a hack */
		r = LightMain( argc, argv );
	}

	/* QBall: export entities */
	else if ( !strcmp( argv[ 1 ], "-exportents" ) ) {
		r = ExportEntitiesMain( argc - 1, argv + 1 );
	}

	/* ydnar: lightmap export */
	else if ( !strcmp( argv[ 1 ], "-export" ) ) {
		r = ExportLightmapsMain( argc - 1, argv + 1 );
	}

	/* ydnar: lightmap import */
	else if ( !strcmp( argv[ 1 ], "-import" ) ) {
		r = ImportLightmapsMain( argc - 1, argv + 1 );
	}

	/* ydnar: bsp scaling */
	else if ( !strcmp( argv[ 1 ], "-scale" ) ) {
		r = ScaleBSPMain( argc - 1, argv + 1 );
	}

	/* bsp shifting */
	else if ( !strcmp( argv[ 1 ], "-shift" ) ) {
		r = ShiftBSPMain( argc - 1, argv + 1 );
	}

	/* autopacking */
	else if ( !strcmp( argv[ 1 ], "-pk3" ) ) {
		r = pk3BSPMain( argc - 1, argv + 1 );
	}

	/* ydnar: bsp conversion */
	else if ( !strcmp( argv[ 1 ], "-convert" ) ) {
		r = ConvertBSPMain( argc - 1, argv + 1 );
	}

	/* div0: minimap */
	else if ( !strcmp( argv[ 1 ], "-minimap" ) ) {
		r = MiniMapBSPMain( argc - 1, argv + 1 );
	}

	/* ydnar: otherwise create a bsp */
	else{
		r = BSPMain( argc, argv );
	}

	/* emit time */
	end = I_FloatTime();
	Sys_Printf( "%9.0f seconds elapsed\n", end - start );

	/* shut down connection */
	Broadcast_Shutdown();

	/* return any error code */
	return r;
}
