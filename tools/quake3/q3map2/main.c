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
	size_t n = strlen( dst  );

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
   shifts a map: works correctly only with axial faces, placed in positive half of axis
   for testing physics with huge coordinates
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

	/* get scale */
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

	/* scale entity keys */
	for ( i = 0; i < numBSPEntities && i < numEntities; i++ )
	{
		/* scale origin */
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

	/* scale models */
	for ( i = 0; i < numBSPModels; i++ )
	{
		bspModels[ i ].mins[0] += scale[0];
		bspModels[ i ].mins[1] += scale[1];
		bspModels[ i ].mins[2] += scale[2];
		bspModels[ i ].maxs[0] += scale[0];
		bspModels[ i ].maxs[1] += scale[1];
		bspModels[ i ].maxs[2] += scale[2];
	}

	/* scale nodes */
	for ( i = 0; i < numBSPNodes; i++ )
	{
		bspNodes[ i ].mins[0] += scale[0];
		bspNodes[ i ].mins[1] += scale[1];
		bspNodes[ i ].mins[2] += scale[2];
		bspNodes[ i ].maxs[0] += scale[0];
		bspNodes[ i ].maxs[1] += scale[1];
		bspNodes[ i ].maxs[2] += scale[2];
	}

	/* scale leafs */
	for ( i = 0; i < numBSPLeafs; i++ )
	{
		bspLeafs[ i ].mins[0] += scale[0];
		bspLeafs[ i ].mins[1] += scale[1];
		bspLeafs[ i ].mins[2] += scale[2];
		bspLeafs[ i ].maxs[0] += scale[0];
		bspLeafs[ i ].maxs[1] += scale[1];
		bspLeafs[ i ].maxs[2] += scale[2];
	}
/*
	if ( texscale ) {
		Sys_Printf( "Using texture unlocking (and probably breaking texture alignment a lot)\n" );
		old_xyzst = safe_malloc( sizeof( *old_xyzst ) * numBSPDrawVerts * 5 );
		for ( i = 0; i < numBSPDrawVerts; i++ )
		{
			old_xyzst[5 * i + 0] = bspDrawVerts[i].xyz[0];
			old_xyzst[5 * i + 1] = bspDrawVerts[i].xyz[1];
			old_xyzst[5 * i + 2] = bspDrawVerts[i].xyz[2];
			old_xyzst[5 * i + 3] = bspDrawVerts[i].st[0];
			old_xyzst[5 * i + 4] = bspDrawVerts[i].st[1];
		}
	}
*/
	/* scale drawverts */
	for ( i = 0; i < numBSPDrawVerts; i++ )
	{
		bspDrawVerts[i].xyz[0] += scale[0];
		bspDrawVerts[i].xyz[1] += scale[1];
		bspDrawVerts[i].xyz[2] += scale[2];
//		bspDrawVerts[i].normal[0] /= scale[0];
//		bspDrawVerts[i].normal[1] /= scale[1];
//		bspDrawVerts[i].normal[2] /= scale[2];
//		VectorNormalize( bspDrawVerts[i].normal, bspDrawVerts[i].normal );
	}
/*
	if ( texscale ) {
		for ( i = 0; i < numBSPDrawSurfaces; i++ )
		{
			switch ( bspDrawSurfaces[i].surfaceType )
			{
			case SURFACE_FACE:
			case SURFACE_META:
				if ( bspDrawSurfaces[i].numIndexes % 3 ) {
					Error( "Not a triangulation!" );
				}
				for ( j = bspDrawSurfaces[i].firstIndex; j < bspDrawSurfaces[i].firstIndex + bspDrawSurfaces[i].numIndexes; j += 3 )
				{
					int ia = bspDrawIndexes[j] + bspDrawSurfaces[i].firstVert, ib = bspDrawIndexes[j + 1] + bspDrawSurfaces[i].firstVert, ic = bspDrawIndexes[j + 2] + bspDrawSurfaces[i].firstVert;
					bspDrawVert_t *a = &bspDrawVerts[ia], *b = &bspDrawVerts[ib], *c = &bspDrawVerts[ic];
					float *oa = &old_xyzst[ia * 5], *ob = &old_xyzst[ib * 5], *oc = &old_xyzst[ic * 5];
					// extrapolate:
					//   a->xyz -> oa
					//   b->xyz -> ob
					//   c->xyz -> oc
					ExtrapolateTexcoords(
						&oa[0], &oa[3],
						&ob[0], &ob[3],
						&oc[0], &oc[3],
						a->xyz, a->st,
						b->xyz, b->st,
						c->xyz, c->st );
				}
				break;
			}
		}
	}
*/
	/* scale planes */

	for ( i = 0; i < numBSPPlanes; i++ )
	{
		if ( bspPlanes[ i ].dist > 0 ){
				if ( bspPlanes[ i ].normal[0] ){
					bspPlanes[ i ].dist += scale[0];
					continue;
				}
				else if ( bspPlanes[ i ].normal[1] ){
					bspPlanes[ i ].dist += scale[1];
					continue;
				}
				else if ( bspPlanes[ i ].normal[2] ){
					bspPlanes[ i ].dist += scale[2];
					continue;
				}
		}
		else{
				if ( bspPlanes[ i ].normal[0] ){
					bspPlanes[ i ].dist -= scale[0];
					continue;
				}
				else if ( bspPlanes[ i ].normal[1] ){
					bspPlanes[ i ].dist -= scale[1];
					continue;
				}
				else if ( bspPlanes[ i ].normal[2] ){
					bspPlanes[ i ].dist -= scale[2];
					continue;
				}
		}
	}


/*	if ( uniform ) {
		for ( i = 0; i < numBSPPlanes; i++ )
		{
			bspPlanes[ i ].dist += scale[0];
		}
	}
	else
	{
		for ( i = 0; i < numBSPPlanes; i++ )
		{
//			bspPlanes[ i ].normal[0] /= scale[0];
//			bspPlanes[ i ].normal[1] /= scale[1];
//			bspPlanes[ i ].normal[2] /= scale[2];
			f = 1 / VectorLength( bspPlanes[i].normal );
			VectorScale( bspPlanes[i].normal, f, bspPlanes[i].normal );
			bspPlanes[ i ].dist *= f;
		}
	}*/

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
			HelpMain( ( i + 1 < argc ) ? argv[ i + 1 ] : NULL );
			return 0;
		}

		/* -connect */
		if ( !strcmp( argv[ i ], "-connect" ) ) {
			if ( ++i >= argc || !argv[ i ] ) {
				Error( "Out of arguments: No address specified after %s", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
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
			if ( ++i >= argc || !argv[ i ] ) {
				Error( "Out of arguments: No value specified after %s", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
			patchSubdivisions = atoi( argv[ i ] );
			argv[ i ] = NULL;
			if ( patchSubdivisions <= 0 ) {
				patchSubdivisions = 1;
			}
		}

		/* threads */
		else if ( !strcmp( argv[ i ], "-threads" ) ) {
			if ( ++i >= argc || !argv[ i ] ) {
				Error( "Out of arguments: No value specified after %s", argv[ i - 1 ] );
			}
			argv[ i - 1 ] = NULL;
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
	Sys_Printf( RADIANT_NAME "    - v" RADIANT_VERSION " " __DATE__ " " __TIME__ "\n" );
	Sys_Printf( "%s\n", Q3MAP_MOTD );

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

	/* ydnar: bsp conversion */
	else if ( !strcmp( argv[ 1 ], "-convert" ) ) {
		r = ConvertBSPMain( argc - 1, argv + 1 );
	}

	/* div0: minimap */
	else if ( !strcmp( argv[ 1 ], "-minimap" ) ) {
		r = MiniMapBSPMain( argc - 1, argv + 1 );
	}

	/* ydnar: otherwise create a bsp */
	else {
		/* used to write Smokin'Guns like tex file */
		compile_map = qtrue;

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
