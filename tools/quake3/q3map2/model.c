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
#define MODEL_C



/* dependencies */
#include "q3map2.h"



/*
   PicoPrintFunc()
   callback for picomodel.lib
 */

void PicoPrintFunc( int level, const char *str ){
	if ( str == NULL ) {
		return;
	}
	switch ( level )
	{
	case PICO_NORMAL:
		Sys_Printf( "%s\n", str );
		break;

	case PICO_VERBOSE:
		Sys_FPrintf( SYS_VRB, "%s\n", str );
		break;

	case PICO_WARNING:
		Sys_FPrintf( SYS_WRN, "WARNING: %s\n", str );
		break;

	case PICO_ERROR:
		Sys_FPrintf( SYS_ERR, "ERROR: %s\n", str );
		break;

	case PICO_FATAL:
		Error( "ERROR: %s\n", str );
		break;
	}
}



/*
   PicoLoadFileFunc()
   callback for picomodel.lib
 */

void PicoLoadFileFunc( const char *name, byte **buffer, int *bufSize ){
	*bufSize = vfsLoadFile( name, (void**) buffer, 0 );
}



/*
   FindModel() - ydnar
   finds an existing picoModel and returns a pointer to the picoModel_t struct or NULL if not found
 */

picoModel_t *FindModel( const char *name, int frame ){
	int i;


	/* init */
	if ( numPicoModels <= 0 ) {
		memset( picoModels, 0, sizeof( picoModels ) );
	}

	/* dummy check */
	if ( name == NULL || name[ 0 ] == '\0' ) {
		return NULL;
	}

	/* search list */
	for ( i = 0; i < MAX_MODELS; i++ )
	{
		if ( picoModels[ i ] != NULL &&
			 !strcmp( PicoGetModelName( picoModels[ i ] ), name ) &&
			 PicoGetModelFrameNum( picoModels[ i ] ) == frame ) {
			return picoModels[ i ];
		}
	}

	/* no matching picoModel found */
	return NULL;
}



/*
   LoadModel() - ydnar
   loads a picoModel and returns a pointer to the picoModel_t struct or NULL if not found
 */

picoModel_t *LoadModel( const char *name, int frame ){
	int i;
	picoModel_t     *model, **pm;


	/* init */
	if ( numPicoModels <= 0 ) {
		memset( picoModels, 0, sizeof( picoModels ) );
	}

	/* dummy check */
	if ( name == NULL || name[ 0 ] == '\0' ) {
		return NULL;
	}

	/* try to find existing picoModel */
	model = FindModel( name, frame );
	if ( model != NULL ) {
		return model;
	}

	/* none found, so find first non-null picoModel */
	pm = NULL;
	for ( i = 0; i < MAX_MODELS; i++ )
	{
		if ( picoModels[ i ] == NULL ) {
			pm = &picoModels[ i ];
			break;
		}
	}

	/* too many picoModels? */
	if ( pm == NULL ) {
		Error( "MAX_MODELS (%d) exceeded, there are too many model files referenced by the map.", MAX_MODELS );
	}

	/* attempt to parse model */
	*pm = PicoLoadModel( name, frame );

	/* if loading failed, make a bogus model to silence the rest of the warnings */
	if ( *pm == NULL ) {
		/* allocate a new model */
		*pm = PicoNewModel();
		if ( *pm == NULL ) {
			return NULL;
		}

		/* set data */
		PicoSetModelName( *pm, name );
		PicoSetModelFrameNum( *pm, frame );
	}

	/* debug code */
	#if 0
	{
		int numSurfaces, numVertexes;
		picoSurface_t   *ps;


		Sys_Printf( "Model %s\n", name );
		numSurfaces = PicoGetModelNumSurfaces( *pm );
		for ( i = 0; i < numSurfaces; i++ )
		{
			ps = PicoGetModelSurface( *pm, i );
			numVertexes = PicoGetSurfaceNumVertexes( ps );
			Sys_Printf( "Surface %d has %d vertexes\n", i, numVertexes );
		}
	}
	#endif

	/* set count */
	if ( *pm != NULL ) {
		numPicoModels++;
	}

	/* return the picoModel */
	return *pm;
}



/*
   InsertModel() - ydnar
   adds a picomodel into the bsp
 */

void InsertModel( const char *name, int skin, int frame, m4x4_t transform, remap_t *remap, shaderInfo_t *celShader, int eNum, int castShadows, int recvShadows, int spawnFlags, float lightmapScale, int lightmapSampleSize, float shadeAngle, float clipDepth ){
	int i, j, s, k, numSurfaces;
	m4x4_t identity, nTransform;
	picoModel_t         *model;
	picoShader_t        *shader;
	picoSurface_t       *surface;
	shaderInfo_t        *si;
	mapDrawSurface_t    *ds;
	bspDrawVert_t       *dv;
	char                *picoShaderName;
	char shaderName[ MAX_QPATH ];
	picoVec_t           *xyz, *normal, *st;
	byte                *color;
	picoIndex_t         *indexes;
	remap_t             *rm, *glob;
	skinfile_t          *sf, *sf2;
	char skinfilename[ MAX_QPATH ];
	char                *skinfilecontent;
	int skinfilesize;
	char                *skinfileptr, *skinfilenextptr;
	//int ok=0, notok=0;
	int spf = ( spawnFlags & 8088 );
	float limDepth=0;


	if ( clipDepth < 0 ){
		limDepth = -clipDepth;
		clipDepth = 2.0;
	}


	/* get model */
	model = LoadModel( name, frame );
	if ( model == NULL ) {
		return;
	}

	/* load skin file */
	snprintf( skinfilename, sizeof( skinfilename ), "%s_%d.skin", name, skin );
	skinfilename[sizeof( skinfilename ) - 1] = 0;
	skinfilesize = vfsLoadFile( skinfilename, (void**) &skinfilecontent, 0 );
	if ( skinfilesize < 0 && skin != 0 ) {
		/* fallback to skin 0 if invalid */
		snprintf( skinfilename, sizeof( skinfilename ), "%s_0.skin", name );
		skinfilename[sizeof( skinfilename ) - 1] = 0;
		skinfilesize = vfsLoadFile( skinfilename, (void**) &skinfilecontent, 0 );
		if ( skinfilesize >= 0 ) {
			Sys_Printf( "Skin %d of %s does not exist, using 0 instead\n", skin, name );
		}
	}
	sf = NULL;
	if ( skinfilesize >= 0 ) {
		Sys_Printf( "Using skin %d of %s\n", skin, name );
		int pos;
		for ( skinfileptr = skinfilecontent; *skinfileptr; skinfileptr = skinfilenextptr )
		{
			// for fscanf
			char format[64];

			skinfilenextptr = strchr( skinfileptr, '\r' );
			if ( skinfilenextptr ) {
				*skinfilenextptr++ = 0;
			}
			else
			{
				skinfilenextptr = strchr( skinfileptr, '\n' );
				if ( skinfilenextptr ) {
					*skinfilenextptr++ = 0;
				}
				else{
					skinfilenextptr = skinfileptr + strlen( skinfileptr );
				}
			}

			/* create new item */
			sf2 = sf;
			sf = safe_malloc( sizeof( *sf ) );
			sf->next = sf2;

			sprintf( format, "replace %%%ds %%%ds", (int)sizeof( sf->name ) - 1, (int)sizeof( sf->to ) - 1 );
			if ( sscanf( skinfileptr, format, sf->name, sf->to ) == 2 ) {
				continue;
			}
			sprintf( format, " %%%d[^,  ] ,%%%ds", (int)sizeof( sf->name ) - 1, (int)sizeof( sf->to ) - 1 );
			if ( ( pos = sscanf( skinfileptr, format, sf->name, sf->to ) ) == 2 ) {
				continue;
			}

			/* invalid input line -> discard sf struct */
			Sys_Printf( "Discarding skin directive in %s: %s\n", skinfilename, skinfileptr );
			free( sf );
			sf = sf2;
		}
		free( skinfilecontent );
	}

	/* handle null matrix */
	if ( transform == NULL ) {
		m4x4_identity( identity );
		transform = identity;
	}

	/* hack: Stable-1_2 and trunk have differing row/column major matrix order
	   this transpose is necessary with Stable-1_2
	   uncomment the following line with old m4x4_t (non 1.3/spog_branch) code */
	//%	m4x4_transpose( transform );

	/* create transform matrix for normals */
	memcpy( nTransform, transform, sizeof( m4x4_t ) );
	if ( m4x4_invert( nTransform ) ) {
		Sys_FPrintf( SYS_VRB, "WARNING: Can't invert model transform matrix, using transpose instead\n" );
	}
	m4x4_transpose( nTransform );

	/* fix bogus lightmap scale */
	if ( lightmapScale <= 0.0f ) {
		lightmapScale = 1.0f;
	}

	/* fix bogus shade angle */
	if ( shadeAngle <= 0.0f ) {
		shadeAngle = 0.0f;
	}

	/* each surface on the model will become a new map drawsurface */
	numSurfaces = PicoGetModelNumSurfaces( model );
	//%	Sys_FPrintf( SYS_VRB, "Model %s has %d surfaces\n", name, numSurfaces );
	for ( s = 0; s < numSurfaces; s++ )
	{
		/* get surface */
		surface = PicoGetModelSurface( model, s );
		if ( surface == NULL ) {
			continue;
		}

		/* only handle triangle surfaces initially (fixme: support patches) */
		if ( PicoGetSurfaceType( surface ) != PICO_TRIANGLES ) {
			continue;
		}

		/* get shader name */
		shader = PicoGetSurfaceShader( surface );
		if ( shader == NULL ) {
			picoShaderName = "";
		}
		else{
			picoShaderName = PicoGetShaderName( shader );
		}

		/* handle .skin file */
		if ( sf ) {
			picoShaderName = NULL;
			for ( sf2 = sf; sf2 != NULL; sf2 = sf2->next )
			{
				if ( !Q_stricmp( surface->name, sf2->name ) ) {
					Sys_FPrintf( SYS_VRB, "Skin file: mapping %s to %s\n", surface->name, sf2->to );
					picoShaderName = sf2->to;
					break;
				}
			}
			if ( !picoShaderName ) {
				Sys_FPrintf( SYS_VRB, "Skin file: not mapping %s\n", surface->name );
				continue;
			}
		}

		/* handle shader remapping */
		glob = NULL;
		for ( rm = remap; rm != NULL; rm = rm->next )
		{
			if ( rm->from[ 0 ] == '*' && rm->from[ 1 ] == '\0' ) {
				glob = rm;
			}
			else if ( !Q_stricmp( picoShaderName, rm->from ) ) {
				Sys_FPrintf( SYS_VRB, "Remapping %s to %s\n", picoShaderName, rm->to );
				picoShaderName = rm->to;
				glob = NULL;
				break;
			}
		}

		if ( glob != NULL ) {
			Sys_FPrintf( SYS_VRB, "Globbing %s to %s\n", picoShaderName, glob->to );
			picoShaderName = glob->to;
		}

		/* shader renaming for sof2 */
		if ( renameModelShaders ) {
			strcpy( shaderName, picoShaderName );
			StripExtension( shaderName );
			if ( spawnFlags & 1 ) {
				strcat( shaderName, "_RMG_BSP" );
			}
			else{
				strcat( shaderName, "_BSP" );
			}
			si = ShaderInfoForShader( shaderName );
		}
		else{
			si = ShaderInfoForShader( picoShaderName );
		}

		/* allocate a surface (ydnar: gs mods) */
		ds = AllocDrawSurface( SURFACE_TRIANGLES );
		ds->entityNum = eNum;
		ds->castShadows = castShadows;
		ds->recvShadows = recvShadows;

		/* set shader */
		ds->shaderInfo = si;

		/* force to meta? */
		if ( ( si != NULL && si->forceMeta ) || ( spawnFlags & 4 ) ) { /* 3rd bit */
			ds->type = SURFACE_FORCED_META;
		}

		/* fix the surface's normals (jal: conditioned by shader info) */
		if ( !( spawnFlags & 64 ) && ( shadeAngle == 0.0f || ds->type != SURFACE_FORCED_META ) ) {
			PicoFixSurfaceNormals( surface );
		}

		/* set sample size */
		if ( lightmapSampleSize > 0.0f ) {
			ds->sampleSize = lightmapSampleSize;
		}

		/* set lightmap scale */
		if ( lightmapScale > 0.0f ) {
			ds->lightmapScale = lightmapScale;
		}

		/* set shading angle */
		if ( shadeAngle > 0.0f ) {
			ds->shadeAngleDegrees = shadeAngle;
		}

		/* set particulars */
		ds->numVerts = PicoGetSurfaceNumVertexes( surface );
		ds->verts = safe_malloc0( ds->numVerts * sizeof( ds->verts[ 0 ] ) );

		ds->numIndexes = PicoGetSurfaceNumIndexes( surface );
		ds->indexes = safe_malloc0( ds->numIndexes * sizeof( ds->indexes[ 0 ] ) );

		/* copy vertexes */
		for ( i = 0; i < ds->numVerts; i++ )
		{
			/* get vertex */
			dv = &ds->verts[ i ];

			/* xyz and normal */
			xyz = PicoGetSurfaceXYZ( surface, i );
			VectorCopy( xyz, dv->xyz );
			m4x4_transform_point( transform, dv->xyz );

			normal = PicoGetSurfaceNormal( surface, i );
			VectorCopy( normal, dv->normal );
			m4x4_transform_normal( nTransform, dv->normal );
			VectorNormalize( dv->normal, dv->normal );

			/* ydnar: tek-fu celshading support for flat shaded shit */
			if ( flat ) {
				dv->st[ 0 ] = si->stFlat[ 0 ];
				dv->st[ 1 ] = si->stFlat[ 1 ];
			}

			/* ydnar: gs mods: added support for explicit shader texcoord generation */
			else if ( si->tcGen ) {
				/* project the texture */
				dv->st[ 0 ] = DotProduct( si->vecs[ 0 ], dv->xyz );
				dv->st[ 1 ] = DotProduct( si->vecs[ 1 ], dv->xyz );
			}

			/* normal texture coordinates */
			else
			{
				st = PicoGetSurfaceST( surface, 0, i );
				dv->st[ 0 ] = st[ 0 ];
				dv->st[ 1 ] = st[ 1 ];
			}

			/* set lightmap/color bits */
			color = PicoGetSurfaceColor( surface, 0, i );
			for ( j = 0; j < MAX_LIGHTMAPS; j++ )
			{
				dv->lightmap[ j ][ 0 ] = 0.0f;
				dv->lightmap[ j ][ 1 ] = 0.0f;
				if ( spawnFlags & 32 ) { // spawnflag 32: model color -> alpha hack
					dv->color[ j ][ 0 ] = 255.0f;
					dv->color[ j ][ 1 ] = 255.0f;
					dv->color[ j ][ 2 ] = 255.0f;
					dv->color[ j ][ 3 ] = RGBTOGRAY( color );
				}
				else
				{
					dv->color[ j ][ 0 ] = color[ 0 ];
					dv->color[ j ][ 1 ] = color[ 1 ];
					dv->color[ j ][ 2 ] = color[ 2 ];
					dv->color[ j ][ 3 ] = color[ 3 ];
				}
			}
		}

		/* copy indexes */
		indexes = PicoGetSurfaceIndexes( surface, 0 );
		for ( i = 0; i < ds->numIndexes; i++ )
			ds->indexes[ i ] = indexes[ i ];

		/* set cel shader */
		ds->celShader = celShader;

		/* ydnar: giant hack land: generate clipping brushes for model triangles */
		if ( ( si->clipModel && !( spf ) ) ||	//default CLIPMODEL
			( ( spawnFlags & 8090 ) == 2 ) ||	//default CLIPMODEL
			( spf == 8 ) ||		//EXTRUDE_FACE_NORMALS
			( spf == 16 )   ||	//EXTRUDE_TERRAIN
			( spf == 128 )  ||	//EXTRUDE_VERTEX_NORMALS
			( spf == 256 )  ||	//PYRAMIDAL_CLIP
			( spf == 512 )  ||	//EXTRUDE_DOWNWARDS
			( spf == 1024 ) ||	//EXTRUDE_UPWARDS
			( spf == 4096 ) ||	//default CLIPMODEL + AXIAL_BACKPLANE
			( spf == 264 )  ||	//EXTRUDE_FACE_NORMALS+PYRAMIDAL_CLIP (extrude 45)
			( spf == 2064 ) ||	//EXTRUDE_TERRAIN+MAX_EXTRUDE
			( spf == 4112 ) ||	//EXTRUDE_TERRAIN+AXIAL_BACKPLANE
			( spf == 384 )  ||	//EXTRUDE_VERTEX_NORMALS + PYRAMIDAL_CLIP - vertex normals + don't check for sides, sticking outwards
			( spf == 4352 ) ||	//PYRAMIDAL_CLIP+AXIAL_BACKPLANE
			( spf == 1536 ) ||	//EXTRUDE_DOWNWARDS+EXTRUDE_UPWARDS
			( spf == 2560 ) ||	//EXTRUDE_DOWNWARDS+MAX_EXTRUDE
			( spf == 4608 ) ||	//EXTRUDE_DOWNWARDS+AXIAL_BACKPLANE
			( spf == 3584 ) ||	//EXTRUDE_DOWNWARDS+EXTRUDE_UPWARDS+MAX_EXTRUDE
			( spf == 5632 ) ||	//EXTRUDE_DOWNWARDS+EXTRUDE_UPWARDS+AXIAL_BACKPLANE
			( spf == 3072 ) ||	//EXTRUDE_UPWARDS+MAX_EXTRUDE
			( spf == 5120 ) ){	//EXTRUDE_UPWARDS+AXIAL_BACKPLANE
			vec3_t points[ 4 ], cnt, bestNormal, nrm, Vnorm[3], Enorm[3];
			vec4_t plane, reverse, p[3];
			double normalEpsilon_save;
			qboolean snpd;
			vec3_t min = { 999999, 999999, 999999 }, max = { -999999, -999999, -999999 };
			vec3_t avgDirection = { 0, 0, 0 };
			int axis = 0;
			#define nonax_clip_dbg 0

			/* temp hack */
			if ( !si->clipModel && !( si->compileFlags & C_SOLID ) ) {
				continue;
			}

			//wont snap these in normal way, or will explode
			normalEpsilon_save = normalEpsilon;
			//normalEpsilon = 0.000001;


			//MAX_EXTRUDE or EXTRUDE_TERRAIN
			if ( ( spawnFlags & 2048 ) || ( spawnFlags & 16 ) ){

				for ( i = 0; i < ds->numIndexes; i += 3 )
				{
					for ( j = 0; j < 3; j++ )
					{
						dv = &ds->verts[ ds->indexes[ i + j ] ];
						VectorCopy( dv->xyz, points[ j ] );
					}
					if ( PlaneFromPoints( plane, points[ 0 ], points[ 1 ], points[ 2 ] ) ){
						if ( spawnFlags & 16 ) VectorAdd( avgDirection, plane, avgDirection );	//calculate average mesh facing direction

						//get min/max
						for ( k = 2; k > -1; k-- ){
							if ( plane[k] > 0 ){
								for ( j = 0; j < 3; j++ ){ if ( points[j][k] < min[k] ) min[k] = points[j][k]; }
							}
							else if ( plane[k] < 0 ){
								for ( j = 0; j < 3; j++ ){ if ( points[j][k] > max[k] ) max[k] = points[j][k]; }
							}
							//if EXTRUDE_DOWNWARDS or EXTRUDE_UPWARDS
							if ( ( spawnFlags & 512 ) || ( spawnFlags & 1024 ) ){
								break;
							}
						}
					}
				}
				//unify avg direction
				if ( spawnFlags & 16 ){
					for ( j = 0; j < 3; j++ ){
						if ( fabs(avgDirection[j]) > fabs(avgDirection[(j+1)%3]) ){
							avgDirection[(j+1)%3] = 0.0;
							axis = j;
						}
						else {
							avgDirection[j] = 0.0;
						}
					}
					if ( VectorNormalize( avgDirection, avgDirection ) == 0 ){
						axis = 2;
						VectorSet( avgDirection, 0, 0, 1 );
					}
				}
			}

			/* walk triangle list */
			for ( i = 0; i < ds->numIndexes; i += 3 )
			{
				/* overflow hack */
				AUTOEXPAND_BY_REALLOC( mapplanes, ( nummapplanes + 64 ) << 1, allocatedmapplanes, 1024 );

				/* make points */
				for ( j = 0; j < 3; j++ )
				{
					/* get vertex */
					dv = &ds->verts[ ds->indexes[ i + j ] ];

					/* copy xyz */
					VectorCopy( dv->xyz, points[ j ] );
				}

				/* make plane for triangle */
				if ( PlaneFromPoints( plane, points[ 0 ], points[ 1 ], points[ 2 ] ) ) {

					/* build a brush */
					buildBrush = AllocBrush( 48 );
					buildBrush->entityNum = mapEntityNum;
					buildBrush->original = buildBrush;
					buildBrush->contentShader = si;
					buildBrush->compileFlags = si->compileFlags;
					buildBrush->contentFlags = si->contentFlags;
					buildBrush->detail = qtrue;

					//snap points before using them for further calculations
					//precision suffers a lot, when two of normal values are under .00025 (often no collision, knocking up effect in ioq3)
					//also broken drawsurfs in case of normal brushes
					snpd = qfalse;
					for ( j=0; j<3; j++ )
					{
						if ( fabs(plane[j]) < 0.00025 && fabs(plane[(j+1)%3]) < 0.00025 && ( plane[j] != 0.0 || plane[(j+1)%3] != 0.0 ) ){
							VectorAdd( points[ 0 ], points[ 1 ], cnt );
							VectorAdd( cnt, points[ 2 ], cnt );
							VectorScale( cnt, 0.3333333333333f, cnt );
							points[0][(j+2)%3]=points[1][(j+2)%3]=points[2][(j+2)%3]=cnt[(j+2)%3];
							snpd = qtrue;
							break;
						}
					}

					//snap pairs of points to prevent bad side planes
					for ( j=0; j<3; j++ )
					{
						VectorSubtract( points[j], points[(j+1)%3], nrm );
						VectorNormalize( nrm, nrm );
						for ( k=0; k<3; k++ )
						{
							if ( nrm[k] != 0.0 && fabs(nrm[k]) < 0.00025 ){
								//Sys_Printf( "b4(%6.6f %6.6f %6.6f)(%6.6f %6.6f %6.6f)\n", points[j][0], points[j][1], points[j][2], points[(j+1)%3][0], points[(j+1)%3][1], points[(j+1)%3][2] );
								points[j][k]=points[(j+1)%3][k] = ( points[j][k] + points[(j+1)%3][k] ) / 2.0;
								//Sys_Printf( "sn(%6.6f %6.6f %6.6f)(%6.6f %6.6f %6.6f)\n", points[j][0], points[j][1], points[j][2], points[(j+1)%3][0], points[(j+1)%3][1], points[(j+1)%3][2] );
								snpd = qtrue;
							}
						}
					}

					if ( snpd ) {
						PlaneFromPoints( plane, points[ 0 ], points[ 1 ], points[ 2 ] );
						snpd = qfalse;
					}

					//vector-is-close-to-be-on-axis check again, happens after previous code sometimes
					for ( j=0; j<3; j++ )
					{
						if ( fabs(plane[j]) < 0.00025 && fabs(plane[(j+1)%3]) < 0.00025 && ( plane[j] != 0.0 || plane[(j+1)%3] != 0.0 ) ){
							VectorAdd( points[ 0 ], points[ 1 ], cnt );
							VectorAdd( cnt, points[ 2 ], cnt );
							VectorScale( cnt, 0.3333333333333f, cnt );
							points[0][(j+2)%3]=points[1][(j+2)%3]=points[2][(j+2)%3]=cnt[(j+2)%3];
							PlaneFromPoints( plane, points[ 0 ], points[ 1 ], points[ 2 ] );
							break;
						}
					}

					//snap single snappable normal components
					for ( j=0; j<3; j++ )
					{
						if ( plane[j] != 0.0 && fabs(plane[j]) < 0.00005 ){
							plane[j]=0.0;
							snpd = qtrue;
						}
					}

					//adjust plane dist
					if ( snpd ) {
						VectorAdd( points[ 0 ], points[ 1 ], cnt );
						VectorAdd( cnt, points[ 2 ], cnt );
						VectorScale( cnt, 0.3333333333333f, cnt );
						VectorNormalize( plane, plane );
						plane[3] = DotProduct( plane, cnt );

						//project points to resulting plane to keep intersections precision
						for ( j=0; j<3; j++ )
						{
							//Sys_Printf( "b4 %i (%6.7f %6.7f %6.7f)\n", j, points[j][0], points[j][1], points[j][2] );
							VectorMA( points[j], plane[3] - DotProduct( plane, points[j]), plane, points[j] );
							//Sys_Printf( "sn %i (%6.7f %6.7f %6.7f)\n", j, points[j][0], points[j][1], points[j][2] );
						}
						//Sys_Printf( "sn pln (%6.7f %6.7f %6.7f %6.7f)\n", plane[0], plane[1], plane[2], plane[3] );
						//PlaneFromPoints( plane, points[ 0 ], points[ 1 ], points[ 2 ] );
						//Sys_Printf( "pts pln (%6.7f %6.7f %6.7f %6.7f)\n", plane[0], plane[1], plane[2], plane[3] );
					}


					if ( spf == 4352 ){	//PYRAMIDAL_CLIP+AXIAL_BACKPLANE

						for ( j=0; j<3; j++ )
						{
							if ( fabs(plane[j]) < 0.05 && fabs(plane[(j+1)%3]) < 0.05 ){ //no way, close to lay on two axises
								goto default_CLIPMODEL;
							}
						}

						// best axial normal
						VectorCopy( plane, bestNormal );
						for ( j = 0; j < 3; j++ ){
							if ( fabs(bestNormal[j]) > fabs(bestNormal[(j+1)%3]) ){
								bestNormal[(j+1)%3] = 0.0;
								axis = j;
							}
							else {
								bestNormal[j] = 0.0;
							}
						}
						VectorNormalize( bestNormal, bestNormal );


						float bestdist, currdist, bestangle, currangle, mindist = 999999;

						for ( j = 0; j < 3; j++ ){//planes
							bestdist = 999999;
							bestangle = 1;
							for ( k = 0; k < 3; k++ ){//axises
								VectorSubtract( points[ (j+1)%3 ], points[ j ], nrm );
								if ( k == axis ){
									CrossProduct( bestNormal, nrm, reverse );
								}
								else{
									VectorClear( Vnorm[0] );
									if ( (k+1)%3 == axis ){
										if ( nrm[ (k+2)%3 ] == 0 ) continue;
										Vnorm[0][ (k+2)%3 ] = nrm[ (k+2)%3 ];
									}
									else{
										if ( nrm[ (k+1)%3 ] == 0 ) continue;
										Vnorm[0][ (k+1)%3 ] = nrm[ (k+1)%3 ];
									}
									CrossProduct( bestNormal, Vnorm[0], Enorm[0] );
									CrossProduct( Enorm[0], nrm, reverse );
								}
								VectorNormalize( reverse, reverse );
								reverse[3] = DotProduct( points[ j ], reverse );
								//check facing, thickness
								currdist = reverse[3] - DotProduct( reverse, points[ (j+2)%3 ] );
								currangle = DotProduct( reverse, plane );
								if ( ( ( currdist > 0.1 ) && ( currdist < bestdist ) && ( currangle < 0 ) ) ||
									( ( currangle >= 0 ) && ( currangle <= bestangle ) ) ){
									bestangle = currangle;
									if ( currangle < 0 ) bestdist = currdist;
									VectorCopy( reverse, p[j] );
									p[j][3] = reverse[3];
								}
							}
							//if ( bestdist == 999999 && bestangle == 1 ) Sys_Printf("default_CLIPMODEL\n");
							if ( bestdist == 999999 && bestangle == 1 ) goto default_CLIPMODEL;
							if ( bestdist < mindist ) mindist = bestdist;
						}
						if ( (limDepth != 0.0) && (mindist > limDepth) ) goto default_CLIPMODEL;


#if nonax_clip_dbg
						for ( j = 0; j < 3; j++ )
						{
							for ( k = 0; k < 3; k++ )
							{
								if ( fabs(p[j][k]) < 0.00025 && p[j][k] != 0.0 ){
									Sys_Printf( "nonax nrm %6.17f %6.17f %6.17f\n", p[j][0], p[j][1], p[j][2] );
								}
							}
						}
#endif
						/* set up brush sides */
						buildBrush->numsides = 4;
						buildBrush->sides[ 0 ].shaderInfo = si;
						for ( j = 1; j < buildBrush->numsides; j++ ) {
							if ( debugClip ) {
								buildBrush->sides[ 0 ].shaderInfo = ShaderInfoForShader( "debugclip2" );
								buildBrush->sides[ j ].shaderInfo = ShaderInfoForShader( "debugclip" );
							}
							else {
								buildBrush->sides[ j ].shaderInfo = NULL;  // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
							}
						}
						VectorCopy( points[0], points[3] ); // for cyclic usage

						buildBrush->sides[ 0 ].planenum = FindFloatPlane( plane, plane[ 3 ], 3, points );
						buildBrush->sides[ 1 ].planenum = FindFloatPlane( p[0], p[0][ 3 ], 2, &points[ 0 ] ); // p[0] contains points[0] and points[1]
						buildBrush->sides[ 2 ].planenum = FindFloatPlane( p[1], p[1][ 3 ], 2, &points[ 1 ] ); // p[1] contains points[1] and points[2]
						buildBrush->sides[ 3 ].planenum = FindFloatPlane( p[2], p[2][ 3 ], 2, &points[ 2 ] ); // p[2] contains points[2] and points[0] (copied to points[3]
					}


					else if ( ( spf == 16 ) ||	//EXTRUDE_TERRAIN
						( spf == 512 ) ||	//EXTRUDE_DOWNWARDS
						( spf == 1024 ) ||	//EXTRUDE_UPWARDS
						( spf == 4096 ) ||	//default CLIPMODEL + AXIAL_BACKPLANE
						( spf == 2064 ) ||	//EXTRUDE_TERRAIN+MAX_EXTRUDE
						( spf == 4112 ) ||	//EXTRUDE_TERRAIN+AXIAL_BACKPLANE
						( spf == 1536 ) ||	//EXTRUDE_DOWNWARDS+EXTRUDE_UPWARDS
						( spf == 2560 ) ||	//EXTRUDE_DOWNWARDS+MAX_EXTRUDE
						( spf == 4608 ) ||	//EXTRUDE_DOWNWARDS+AXIAL_BACKPLANE
						( spf == 3584 ) ||	//EXTRUDE_DOWNWARDS+EXTRUDE_UPWARDS+MAX_EXTRUDE
						( spf == 5632 ) ||	//EXTRUDE_DOWNWARDS+EXTRUDE_UPWARDS+AXIAL_BACKPLANE
						( spf == 3072 ) ||	//EXTRUDE_UPWARDS+MAX_EXTRUDE
						( spf == 5120 ) ){	//EXTRUDE_UPWARDS+AXIAL_BACKPLANE

						if ( spawnFlags & 16 ){ //autodirection
							VectorCopy( avgDirection, bestNormal );
						}
						else{
							axis = 2;
							if ( ( spawnFlags & 1536 ) == 1536 ){ //up+down
								VectorSet( bestNormal, 0, 0, ( plane[2] >= 0 ? 1 : -1 ) );
							}
							else if ( spawnFlags & 512 ){ //down
								VectorSet( bestNormal, 0, 0, 1 );

							}
							else if ( spawnFlags & 1024 ){ //up
								VectorSet( bestNormal, 0, 0, -1 );
							}
							else{ // best axial normal
								VectorCopy( plane, bestNormal );
								for ( j = 0; j < 3; j++ ){
									if ( fabs(bestNormal[j]) > fabs(bestNormal[(j+1)%3]) ){
										bestNormal[(j+1)%3] = 0.0;
										axis = j;
									}
									else {
										bestNormal[j] = 0.0;
									}
								}
								VectorNormalize( bestNormal, bestNormal );
							}
						}

						if ( DotProduct( plane, bestNormal ) < 0.05 ){
							goto default_CLIPMODEL;
						}


						/* make side planes */
						for ( j = 0; j < 3; j++ )
						{
							VectorSubtract( points[(j+1)%3], points[ j ], nrm );
							CrossProduct( bestNormal, nrm, p[ j ] );
							VectorNormalize( p[ j ], p[ j ] );
							p[j][3] = DotProduct( points[j], p[j] );
						}

						/* make back plane */
						if ( spawnFlags & 2048 ){ //max extrude
							VectorScale( bestNormal, -1.0f, reverse );
							if ( bestNormal[axis] > 0 ){
								reverse[3] = -min[axis] + clipDepth;
							}
							else{
								reverse[3] = max[axis] + clipDepth;
							}
						}
						else if ( spawnFlags & 4096 ){ //axial backplane
							VectorScale( bestNormal, -1.0f, reverse );
							reverse[3] = points[0][axis];
							if ( bestNormal[axis] > 0 ){
								for ( j = 1; j < 3; j++ ){
									if ( points[j][axis] < reverse[3] ){
										reverse[3] = points[j][axis];
									}
								}
								reverse[3] = -reverse[3] + clipDepth;
							}
							else{
								for ( j = 1; j < 3; j++ ){
									if ( points[j][axis] > reverse[3] ){
										reverse[3] = points[j][axis];
									}
								}
								reverse[3] += clipDepth;
							}
							if (limDepth != 0.0){
								VectorCopy( points[0], cnt );
								if ( bestNormal[axis] > 0 ){
									for ( j = 1; j < 3; j++ ){
										if ( points[j][axis] > cnt[axis] ){
											VectorCopy( points[j], cnt );
										}
									}
								}
								else {
									for ( j = 1; j < 3; j++ ){
										if ( points[j][axis] < cnt[axis] ){
											VectorCopy( points[j], cnt );
										}
									}
								}
								VectorMA( cnt, reverse[3] - DotProduct( reverse, cnt ), reverse, cnt );
								if ( ( plane[3] - DotProduct( plane, cnt ) ) > limDepth ){
									VectorScale( plane, -1.0f, reverse );
									reverse[ 3 ] = -plane[ 3 ];
									reverse[3] += clipDepth;
								}
							}
						}
						else{	//normal backplane
							VectorScale( plane, -1.0f, reverse );
							reverse[ 3 ] = -plane[ 3 ];
							reverse[3] += clipDepth;
						}
#if nonax_clip_dbg
						for ( j = 0; j < 3; j++ )
						{
							for ( k = 0; k < 3; k++ )
							{
								if ( fabs(p[j][k]) < 0.00025 && p[j][k] != 0.0 ){
									Sys_Printf( "nonax nrm %6.17f %6.17f %6.17f\n", p[j][0], p[j][1], p[j][2] );
								}
							}
						}
#endif
						/* set up brush sides */
						buildBrush->numsides = 5;
						buildBrush->sides[ 0 ].shaderInfo = si;
						for ( j = 1; j < buildBrush->numsides; j++ ) {
							if ( debugClip ) {
								buildBrush->sides[ 0 ].shaderInfo = ShaderInfoForShader( "debugclip2" );
								buildBrush->sides[ j ].shaderInfo = ShaderInfoForShader( "debugclip" );
							}
							else {
								buildBrush->sides[ j ].shaderInfo = NULL;  // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
							}
						}
						VectorCopy( points[0], points[3] ); // for cyclic usage

						buildBrush->sides[ 0 ].planenum = FindFloatPlane( plane, plane[ 3 ], 3, points );
						buildBrush->sides[ 1 ].planenum = FindFloatPlane( p[0], p[0][ 3 ], 2, &points[ 0 ] ); // p[0] contains points[0] and points[1]
						buildBrush->sides[ 2 ].planenum = FindFloatPlane( p[1], p[1][ 3 ], 2, &points[ 1 ] ); // p[1] contains points[1] and points[2]
						buildBrush->sides[ 3 ].planenum = FindFloatPlane( p[2], p[2][ 3 ], 2, &points[ 2 ] ); // p[2] contains points[2] and points[0] (copied to points[3]
						buildBrush->sides[ 4 ].planenum = FindFloatPlane( reverse, reverse[ 3 ], 0, NULL );
					}


					else if ( spf == 264 ){	//EXTRUDE_FACE_NORMALS+PYRAMIDAL_CLIP (extrude 45)

						//45 degrees normals for side planes
						for ( j = 0; j < 3; j++ )
						{
							VectorSubtract( points[(j+1)%3], points[ j ], nrm );
							CrossProduct( plane, nrm, Enorm[ j ] );
							VectorNormalize( Enorm[ j ], Enorm[ j ] );
							VectorAdd( plane, Enorm[ j ], Enorm[ j ] );
							VectorNormalize( Enorm[ j ], Enorm[ j ] );
							/* make side planes */
							CrossProduct( Enorm[ j ], nrm, p[ j ] );
							VectorNormalize( p[ j ], p[ j ] );
							p[j][3] = DotProduct( points[j], p[j] );
							//snap nearly axial side planes
							snpd = qfalse;
							for ( k = 0; k < 3; k++ )
							{
								if ( fabs(p[j][k]) < 0.00025 && p[j][k] != 0.0 ){
									p[j][k] = 0.0;
									snpd = qtrue;
								}
							}
							if ( snpd ){
								VectorNormalize( p[j], p[j] );
								VectorAdd( points[j], points[(j+1)%3], cnt );
								VectorScale( cnt, 0.5f, cnt );
								p[j][3] = DotProduct( cnt, p[j] );
							}
						}

						/* make back plane */
						VectorScale( plane, -1.0f, reverse );
						reverse[ 3 ] = -plane[ 3 ];
						reverse[3] += clipDepth;

						/* set up brush sides */
						buildBrush->numsides = 5;
						buildBrush->sides[ 0 ].shaderInfo = si;
						for ( j = 1; j < buildBrush->numsides; j++ ) {
							if ( debugClip ) {
								buildBrush->sides[ 0 ].shaderInfo = ShaderInfoForShader( "debugclip2" );
								buildBrush->sides[ j ].shaderInfo = ShaderInfoForShader( "debugclip" );
							}
							else {
								buildBrush->sides[ j ].shaderInfo = NULL;  // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
							}
						}
						VectorCopy( points[0], points[3] ); // for cyclic usage

						buildBrush->sides[ 0 ].planenum = FindFloatPlane( plane, plane[ 3 ], 3, points );
						buildBrush->sides[ 1 ].planenum = FindFloatPlane( p[0], p[0][ 3 ], 2, &points[ 0 ] ); // p[0] contains points[0] and points[1]
						buildBrush->sides[ 2 ].planenum = FindFloatPlane( p[1], p[1][ 3 ], 2, &points[ 1 ] ); // p[1] contains points[1] and points[2]
						buildBrush->sides[ 3 ].planenum = FindFloatPlane( p[2], p[2][ 3 ], 2, &points[ 2 ] ); // p[2] contains points[2] and points[0] (copied to points[3]
						buildBrush->sides[ 4 ].planenum = FindFloatPlane( reverse, reverse[ 3 ], 0, NULL );
					}


					else if ( ( spf == 128 ) ||	//EXTRUDE_VERTEX_NORMALS
						( spf == 384 ) ){ 		//EXTRUDE_VERTEX_NORMALS + PYRAMIDAL_CLIP - vertex normals + don't check for sides, sticking outwards
						/* get vertex normals */
						for ( j = 0; j < 3; j++ )
						{
							/* get vertex */
							dv = &ds->verts[ ds->indexes[ i + j ] ];
							/* copy normal */
							VectorCopy( dv->normal, Vnorm[ j ] );
						}

						//avg normals for side planes
						for ( j = 0; j < 3; j++ )
						{
							VectorAdd( Vnorm[ j ], Vnorm[ (j+1)%3 ], Enorm[ j ] );
							VectorNormalize( Enorm[ j ], Enorm[ j ] );
							//check fuer bad ones
							VectorSubtract( points[(j+1)%3], points[ j ], cnt );
							CrossProduct( plane, cnt, nrm );
							VectorNormalize( nrm, nrm );
							//check for negative or outside direction
							if ( DotProduct( Enorm[ j ], plane ) > 0.1 ){
								if ( ( DotProduct( Enorm[ j ], nrm ) > -0.2 ) || ( spawnFlags & 256 ) ){
									//ok++;
									continue;
								}
							}
							//notok++;
							//Sys_Printf( "faulty Enormal %i/%i\n", notok, ok );
							//use 45 normal
							VectorAdd( plane, nrm, Enorm[ j ] );
							VectorNormalize( Enorm[ j ], Enorm[ j ] );
						}

						/* make side planes */
						for ( j = 0; j < 3; j++ )
						{
							VectorSubtract( points[(j+1)%3], points[ j ], nrm );
							CrossProduct( Enorm[ j ], nrm, p[ j ] );
							VectorNormalize( p[ j ], p[ j ] );
							p[j][3] = DotProduct( points[j], p[j] );
							//snap nearly axial side planes
							snpd = qfalse;
							for ( k = 0; k < 3; k++ )
							{
								if ( fabs(p[j][k]) < 0.00025 && p[j][k] != 0.0 ){
									//Sys_Printf( "init plane %6.8f %6.8f %6.8f %6.8f\n", p[j][0], p[j][1], p[j][2], p[j][3]);
									p[j][k] = 0.0;
									snpd = qtrue;
								}
							}
							if ( snpd ){
								VectorNormalize( p[j], p[j] );
								//Sys_Printf( "nrm plane %6.8f %6.8f %6.8f %6.8f\n", p[j][0], p[j][1], p[j][2], p[j][3]);
								VectorAdd( points[j], points[(j+1)%3], cnt );
								VectorScale( cnt, 0.5f, cnt );
								p[j][3] = DotProduct( cnt, p[j] );
								//Sys_Printf( "dst plane %6.8f %6.8f %6.8f %6.8f\n", p[j][0], p[j][1], p[j][2], p[j][3]);
							}
						}

						/* make back plane */
						VectorScale( plane, -1.0f, reverse );
						reverse[ 3 ] = -plane[ 3 ];
						reverse[3] += clipDepth;

						/* set up brush sides */
						buildBrush->numsides = 5;
						buildBrush->sides[ 0 ].shaderInfo = si;
						for ( j = 1; j < buildBrush->numsides; j++ ) {
							if ( debugClip ) {
								buildBrush->sides[ 0 ].shaderInfo = ShaderInfoForShader( "debugclip2" );
								buildBrush->sides[ j ].shaderInfo = ShaderInfoForShader( "debugclip" );
							}
							else {
								buildBrush->sides[ j ].shaderInfo = NULL;  // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
							}
						}
						VectorCopy( points[0], points[3] ); // for cyclic usage

						buildBrush->sides[ 0 ].planenum = FindFloatPlane( plane, plane[ 3 ], 3, points );
						buildBrush->sides[ 1 ].planenum = FindFloatPlane( p[0], p[0][ 3 ], 2, &points[ 0 ] ); // p[0] contains points[0] and points[1]
						buildBrush->sides[ 2 ].planenum = FindFloatPlane( p[1], p[1][ 3 ], 2, &points[ 1 ] ); // p[1] contains points[1] and points[2]
						buildBrush->sides[ 3 ].planenum = FindFloatPlane( p[2], p[2][ 3 ], 2, &points[ 2 ] ); // p[2] contains points[2] and points[0] (copied to points[3]
						buildBrush->sides[ 4 ].planenum = FindFloatPlane( reverse, reverse[ 3 ], 0, NULL );
					}


					else if ( spf == 8 ){	//EXTRUDE_FACE_NORMALS

						/* make side planes */
						for ( j = 0; j < 3; j++ )
						{
							VectorSubtract( points[(j+1)%3], points[ j ], nrm );
							CrossProduct( plane, nrm, p[ j ] );
							VectorNormalize( p[ j ], p[ j ] );
							p[j][3] = DotProduct( points[j], p[j] );
							//snap nearly axial side planes
							snpd = qfalse;
							for ( k = 0; k < 3; k++ )
							{
								if ( fabs(p[j][k]) < 0.00025 && p[j][k] != 0.0 ){
									//Sys_Printf( "init plane %6.8f %6.8f %6.8f %6.8f\n", p[j][0], p[j][1], p[j][2], p[j][3]);
									p[j][k] = 0.0;
									snpd = qtrue;
								}
							}
							if ( snpd ){
								VectorNormalize( p[j], p[j] );
								//Sys_Printf( "nrm plane %6.8f %6.8f %6.8f %6.8f\n", p[j][0], p[j][1], p[j][2], p[j][3]);
								VectorAdd( points[j], points[(j+1)%3], cnt );
								VectorScale( cnt, 0.5f, cnt );
								p[j][3] = DotProduct( cnt, p[j] );
								//Sys_Printf( "dst plane %6.8f %6.8f %6.8f %6.8f\n", p[j][0], p[j][1], p[j][2], p[j][3]);
							}
						}

						/* make back plane */
						VectorScale( plane, -1.0f, reverse );
						reverse[ 3 ] = -plane[ 3 ];
						reverse[3] += clipDepth;
#if nonax_clip_dbg
						for ( j = 0; j < 3; j++ )
						{
							for ( k = 0; k < 3; k++ )
							{
								if ( fabs(p[j][k]) < 0.00005 && p[j][k] != 0.0 ){
									Sys_Printf( "nonax nrm %6.17f %6.17f %6.17f\n", p[j][0], p[j][1], p[j][2] );
									Sys_Printf( "frm src nrm %6.17f %6.17f %6.17f\n", plane[0], plane[1], plane[2]);
								}
							}
						}
#endif
						/* set up brush sides */
						buildBrush->numsides = 5;
						buildBrush->sides[ 0 ].shaderInfo = si;
						for ( j = 1; j < buildBrush->numsides; j++ ) {
							if ( debugClip ) {
								buildBrush->sides[ 0 ].shaderInfo = ShaderInfoForShader( "debugclip2" );
								buildBrush->sides[ j ].shaderInfo = ShaderInfoForShader( "debugclip" );
							}
							else {
								buildBrush->sides[ j ].shaderInfo = NULL;  // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
							}
						}
						VectorCopy( points[0], points[3] ); // for cyclic usage

						buildBrush->sides[ 0 ].planenum = FindFloatPlane( plane, plane[ 3 ], 3, points );
						buildBrush->sides[ 1 ].planenum = FindFloatPlane( p[0], p[0][ 3 ], 2, &points[ 0 ] ); // p[0] contains points[0] and points[1]
						buildBrush->sides[ 2 ].planenum = FindFloatPlane( p[1], p[1][ 3 ], 2, &points[ 1 ] ); // p[1] contains points[1] and points[2]
						buildBrush->sides[ 3 ].planenum = FindFloatPlane( p[2], p[2][ 3 ], 2, &points[ 2 ] ); // p[2] contains points[2] and points[0] (copied to points[3]
						buildBrush->sides[ 4 ].planenum = FindFloatPlane( reverse, reverse[ 3 ], 0, NULL );
					}


					else if ( spf == 256 ){	//PYRAMIDAL_CLIP

						/* calculate center */
						VectorAdd( points[ 0 ], points[ 1 ], cnt );
						VectorAdd( cnt, points[ 2 ], cnt );
						VectorScale( cnt, 0.3333333333333f, cnt );

						/* make back pyramid point */
						VectorMA( cnt, -clipDepth, plane, cnt );

						/* make 3 more planes */
						if( PlaneFromPoints( p[0], points[ 2 ], points[ 1 ], cnt ) &&
							PlaneFromPoints( p[1], points[ 1 ], points[ 0 ], cnt ) &&
							PlaneFromPoints( p[2], points[ 0 ], points[ 2 ], cnt ) ) {

							//check for dangerous planes
							while( (( p[0][0] != 0.0 || p[0][1] != 0.0 ) && fabs(p[0][0]) < 0.00025 && fabs(p[0][1]) < 0.00025) ||
								(( p[0][0] != 0.0 || p[0][2] != 0.0 ) && fabs(p[0][0]) < 0.00025 && fabs(p[0][2]) < 0.00025) ||
								(( p[0][2] != 0.0 || p[0][1] != 0.0 ) && fabs(p[0][2]) < 0.00025 && fabs(p[0][1]) < 0.00025) ||
								(( p[1][0] != 0.0 || p[1][1] != 0.0 ) && fabs(p[1][0]) < 0.00025 && fabs(p[1][1]) < 0.00025) ||
								(( p[1][0] != 0.0 || p[1][2] != 0.0 ) && fabs(p[1][0]) < 0.00025 && fabs(p[1][2]) < 0.00025) ||
								(( p[1][2] != 0.0 || p[1][1] != 0.0 ) && fabs(p[1][2]) < 0.00025 && fabs(p[1][1]) < 0.00025) ||
								(( p[2][0] != 0.0 || p[2][1] != 0.0 ) && fabs(p[2][0]) < 0.00025 && fabs(p[2][1]) < 0.00025) ||
								(( p[2][0] != 0.0 || p[2][2] != 0.0 ) && fabs(p[2][0]) < 0.00025 && fabs(p[2][2]) < 0.00025) ||
								(( p[2][2] != 0.0 || p[2][1] != 0.0 ) && fabs(p[2][2]) < 0.00025 && fabs(p[2][1]) < 0.00025) ) {
								VectorMA( cnt, -0.1f, plane, cnt );
								//	Sys_Printf( "shifting pyramid point\n" );
								PlaneFromPoints( p[0], points[ 2 ], points[ 1 ], cnt );
								PlaneFromPoints( p[1], points[ 1 ], points[ 0 ], cnt );
								PlaneFromPoints( p[2], points[ 0 ], points[ 2 ], cnt );
							}
#if nonax_clip_dbg
							for ( j = 0; j < 3; j++ )
							{
								for ( k = 0; k < 3; k++ )
								{
									if ( fabs(p[j][k]) < 0.00005 && p[j][k] != 0.0 ){
										Sys_Printf( "nonax nrm %6.17f %6.17f %6.17f\n (%6.8f %6.8f %6.8f)\n (%6.8f %6.8f %6.8f)\n (%6.8f %6.8f %6.8f)\n", p[j][0], p[j][1], p[j][2], points[j][0], points[j][1], points[j][2], points[(j+1)%3][0], points[(j+1)%3][1], points[(j+1)%3][2], cnt[0], cnt[1], cnt[2] );
									}
								}
							}
#endif
							/* set up brush sides */
							buildBrush->numsides = 4;
							buildBrush->sides[ 0 ].shaderInfo = si;
							for ( j = 1; j < buildBrush->numsides; j++ ) {
								if ( debugClip ) {
									buildBrush->sides[ 0 ].shaderInfo = ShaderInfoForShader( "debugclip2" );
									buildBrush->sides[ j ].shaderInfo = ShaderInfoForShader( "debugclip" );
								}
								else {
									buildBrush->sides[ j ].shaderInfo = NULL;  // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
								}
							}
							VectorCopy( points[0], points[3] ); // for cyclic usage

							buildBrush->sides[ 0 ].planenum = FindFloatPlane( plane, plane[ 3 ], 3, points );
							buildBrush->sides[ 1 ].planenum = FindFloatPlane( p[0], p[0][ 3 ], 2, &points[ 1 ] ); // p[0] contains points[1] and points[2]
							buildBrush->sides[ 2 ].planenum = FindFloatPlane( p[1], p[1][ 3 ], 2, &points[ 0 ] ); // p[1] contains points[0] and points[1]
							buildBrush->sides[ 3 ].planenum = FindFloatPlane( p[2], p[2][ 3 ], 2, &points[ 2 ] ); // p[2] contains points[2] and points[0] (copied to points[3]
						}
						else
						{
							Sys_Printf( "WARNING: triangle (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) of %s was not autoclipped\n", points[0][0], points[0][1], points[0][2], points[1][0], points[1][1], points[1][2], points[2][0], points[2][1], points[2][2], name );
							free( buildBrush );
							continue;
						}
					}


					else if ( ( si->clipModel && !( spf ) ) || ( ( spawnFlags & 8090 ) == 2 ) ){	//default CLIPMODEL

						default_CLIPMODEL:
						// axial normal
						VectorCopy( plane, bestNormal );
						for ( j = 0; j < 3; j++ ){
							if ( fabs(bestNormal[j]) > fabs(bestNormal[(j+1)%3]) ){
								bestNormal[(j+1)%3] = 0.0;
							}
							else {
								bestNormal[j] = 0.0;
							}
						}
						VectorNormalize( bestNormal, bestNormal );

						/* make side planes */
						for ( j = 0; j < 3; j++ )
						{
							VectorSubtract( points[(j+1)%3], points[ j ], nrm );
							CrossProduct( bestNormal, nrm, p[ j ] );
							VectorNormalize( p[ j ], p[ j ] );
							p[j][3] = DotProduct( points[j], p[j] );
						}

						/* make back plane */
						VectorScale( plane, -1.0f, reverse );
						reverse[ 3 ] = -plane[ 3 ];
						reverse[3] += DotProduct( bestNormal, plane ) * clipDepth;
#if nonax_clip_dbg
						for ( j = 0; j < 3; j++ )
						{
							for ( k = 0; k < 3; k++ )
							{
								if ( fabs(p[j][k]) < 0.00025 && p[j][k] != 0.0 ){
									Sys_Printf( "nonax nrm %6.17f %6.17f %6.17f\n", p[j][0], p[j][1], p[j][2] );
								}
							}
						}
#endif
						/* set up brush sides */
						buildBrush->numsides = 5;
						buildBrush->sides[ 0 ].shaderInfo = si;
						for ( j = 1; j < buildBrush->numsides; j++ ) {
							if ( debugClip ) {
								buildBrush->sides[ 0 ].shaderInfo = ShaderInfoForShader( "debugclip2" );
								buildBrush->sides[ j ].shaderInfo = ShaderInfoForShader( "debugclip" );
							}
							else {
								buildBrush->sides[ j ].shaderInfo = NULL;  // don't emit these faces as draw surfaces, should make smaller BSPs; hope this works
							}
						}
						VectorCopy( points[0], points[3] ); // for cyclic usage

						buildBrush->sides[ 0 ].planenum = FindFloatPlane( plane, plane[ 3 ], 3, points );
						buildBrush->sides[ 1 ].planenum = FindFloatPlane( p[0], p[0][ 3 ], 2, &points[ 0 ] ); // p[0] contains points[0] and points[1]
						buildBrush->sides[ 2 ].planenum = FindFloatPlane( p[1], p[1][ 3 ], 2, &points[ 1 ] ); // p[1] contains points[1] and points[2]
						buildBrush->sides[ 3 ].planenum = FindFloatPlane( p[2], p[2][ 3 ], 2, &points[ 2 ] ); // p[2] contains points[2] and points[0] (copied to points[3]
						buildBrush->sides[ 4 ].planenum = FindFloatPlane( reverse, reverse[ 3 ], 0, NULL );
					}


					/* add to entity */
					if ( CreateBrushWindings( buildBrush ) ) {
						AddBrushBevels();
						//%	EmitBrushes( buildBrush, NULL, NULL );
						buildBrush->next = entities[ mapEntityNum ].brushes;
						entities[ mapEntityNum ].brushes = buildBrush;
						entities[ mapEntityNum ].numBrushes++;
					}
					else{
						Sys_Printf( "WARNING: triangle (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) (%6.0f %6.0f %6.0f) of %s was not autoclipped\n", points[0][0], points[0][1], points[0][2], points[1][0], points[1][1], points[1][2], points[2][0], points[2][1], points[2][2], name );
						free( buildBrush );
					}
				}
			}
			normalEpsilon = normalEpsilon_save;
		}
		else if ( spawnFlags & 8090 ){
			Sys_Printf( "WARNING: nonexistent clipping mode selected\n" );
		}
	}
}



/*
   AddTriangleModels()
   adds misc_model surfaces to the bsp
 */

void AddTriangleModels( entity_t *e ){
	int num, frame, skin, castShadows, recvShadows, spawnFlags;
	entity_t        *e2;
	const char      *targetName;
	const char      *target, *model, *value;
	char shader[ MAX_QPATH ];
	shaderInfo_t    *celShader;
	float temp, baseLightmapScale, lightmapScale, clipDepth;
	float shadeAngle;
	int lightmapSampleSize;
	vec3_t origin, scale, angles;
	m4x4_t transform;
	epair_t         *ep;
	remap_t         *remap, *remap2;
	char            *split;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- AddTriangleModels ---\n" );


	/* get current brush entity targetname */
	if ( e == entities ) {
		targetName = "";
	}
	else
	{
		targetName = ValueForKey( e, "targetname" );

		/* misc_model entities target non-worldspawn brush model entities */
		if ( targetName[ 0 ] == '\0' ) {
			return;
		}
	}

	/* get lightmap scale */
	/* vortex: added _ls key (short name of lightmapscale) */
	baseLightmapScale = 0.0f;
	if ( strcmp( "", ValueForKey( e, "lightmapscale" ) ) ||
		 strcmp( "", ValueForKey( e, "_lightmapscale" ) ) ||
		 strcmp( "", ValueForKey( e, "_ls" ) ) ) {
		baseLightmapScale = FloatForKey( e, "lightmapscale" );
		if ( baseLightmapScale <= 0.0f ) {
			baseLightmapScale = FloatForKey( e, "_lightmapscale" );
		}
		if ( baseLightmapScale <= 0.0f ) {
			baseLightmapScale = FloatForKey( e, "_ls" );
		}
		if ( baseLightmapScale < 0.0f ) {
			baseLightmapScale = 0.0f;
		}
		if ( baseLightmapScale > 0.0f ) {
			Sys_Printf( "World Entity has lightmap scale of %.4f\n", baseLightmapScale );
		}
	}

	/* walk the entity list */
	for ( num = 1; num < numEntities; num++ )
	{
		/* get e2 */
		e2 = &entities[ num ];

		/* convert misc_models into raw geometry */
		if ( Q_stricmp( "misc_model", ValueForKey( e2, "classname" ) ) ) {
			continue;
		}

		/* ydnar: added support for md3 models on non-worldspawn models */
		target = ValueForKey( e2, "target" );
		if ( strcmp( target, targetName ) ) {
			continue;
		}

		/* get model name */
		model = ValueForKey( e2, "model" );
		if ( model[ 0 ] == '\0' ) {
			Sys_FPrintf( SYS_WRN, "WARNING: misc_model at %i %i %i without a model key\n",
						(int) origin[ 0 ], (int) origin[ 1 ], (int) origin[ 2 ] );
			continue;
		}

		/* get model frame */
		frame = 0;
		if ( strcmp( "", ValueForKey( e2, "_frame" ) ) ) {
			frame = IntForKey( e2, "_frame" );
		}
		else if ( strcmp( "", ValueForKey( e2, "frame" ) ) ) {
			frame = IntForKey( e2, "frame" );
		}

		/* worldspawn (and func_groups) default to cast/recv shadows in worldspawn group */
		if ( e == entities ) {
			castShadows = WORLDSPAWN_CAST_SHADOWS;
			recvShadows = WORLDSPAWN_RECV_SHADOWS;
		}

		/* other entities don't cast any shadows, but recv worldspawn shadows */
		else
		{
			castShadows = ENTITY_CAST_SHADOWS;
			recvShadows = ENTITY_RECV_SHADOWS;
		}

		/* get explicit shadow flags */
		GetEntityShadowFlags( e2, e, &castShadows, &recvShadows );

		/* get spawnflags */
		spawnFlags = IntForKey( e2, "spawnflags" );

		/* get origin */
		GetVectorForKey( e2, "origin", origin );
		VectorSubtract( origin, e->origin, origin );    /* offset by parent */

		/* get scale */
		scale[ 0 ] = scale[ 1 ] = scale[ 2 ] = 1.0f;
		temp = FloatForKey( e2, "modelscale" );
		if ( temp != 0.0f ) {
			scale[ 0 ] = scale[ 1 ] = scale[ 2 ] = temp;
		}
		value = ValueForKey( e2, "modelscale_vec" );
		if ( value[ 0 ] != '\0' ) {
			sscanf( value, "%f %f %f", &scale[ 0 ], &scale[ 1 ], &scale[ 2 ] );
		}

		/* get "angle" (yaw) or "angles" (pitch yaw roll) */
		angles[ 0 ] = angles[ 1 ] = angles[ 2 ] = 0.0f;
		angles[ 2 ] = FloatForKey( e2, "angle" );
		value = ValueForKey( e2, "angles" );
		if ( value[ 0 ] != '\0' ) {
			sscanf( value, "%f %f %f", &angles[ 1 ], &angles[ 2 ], &angles[ 0 ] );
		}

		/* set transform matrix (thanks spog) */
		m4x4_identity( transform );
		m4x4_pivoted_transform_by_vec3( transform, origin, angles, eXYZ, scale, vec3_origin );

		/* get shader remappings */
		remap = NULL;
		for ( ep = e2->epairs; ep != NULL; ep = ep->next )
		{
			/* look for keys prefixed with "_remap" */
			if ( ep->key != NULL && ep->value != NULL &&
				 ep->key[ 0 ] != '\0' && ep->value[ 0 ] != '\0' &&
				 !Q_strncasecmp( ep->key, "_remap", 6 ) ) {
				/* create new remapping */
				remap2 = remap;
				remap = safe_malloc( sizeof( *remap ) );
				remap->next = remap2;
				strcpy( remap->from, ep->value );

				/* split the string */
				split = strchr( remap->from, ';' );
				if ( split == NULL ) {
					Sys_FPrintf( SYS_WRN, "WARNING: Shader _remap key found in misc_model without a ; character\n" );
					free( remap );
					remap = remap2;
					continue;
				}

				/* store the split */
				*split = '\0';
				strcpy( remap->to, ( split + 1 ) );

				/* note it */
				//%	Sys_FPrintf( SYS_VRB, "Remapping %s to %s\n", remap->from, remap->to );
			}
		}

		/* ydnar: cel shader support */
		value = ValueForKey( e2, "_celshader" );
		if ( value[ 0 ] == '\0' ) {
			value = ValueForKey( &entities[ 0 ], "_celshader" );
		}
		if ( value[ 0 ] != '\0' ) {
			sprintf( shader, "textures/%s", value );
			celShader = ShaderInfoForShader( shader );
		}
		else{
			celShader = *globalCelShader ? ShaderInfoForShader( globalCelShader ) : NULL;
		}

		/* jal : entity based _samplesize */
		lightmapSampleSize = 0;
		if ( strcmp( "", ValueForKey( e2, "_lightmapsamplesize" ) ) ) {
			lightmapSampleSize = IntForKey( e2, "_lightmapsamplesize" );
		}
		else if ( strcmp( "", ValueForKey( e2, "_samplesize" ) ) ) {
			lightmapSampleSize = IntForKey( e2, "_samplesize" );
		}
		else if ( strcmp( "", ValueForKey( e2, "_ss" ) ) ) {
			lightmapSampleSize = IntForKey( e2, "_ss" );
		}

		if ( lightmapSampleSize < 0 ) {
			lightmapSampleSize = 0;
		}

		if ( lightmapSampleSize > 0.0f ) {
			Sys_Printf( "misc_model has lightmap sample size of %.d\n", lightmapSampleSize );
		}

		/* get lightmap scale */
		/* vortex: added _ls key (short name of lightmapscale) */
		lightmapScale = 0.0f;
		if ( strcmp( "", ValueForKey( e2, "lightmapscale" ) ) ||
			 strcmp( "", ValueForKey( e2, "_lightmapscale" ) ) ||
			 strcmp( "", ValueForKey( e2, "_ls" ) ) ) {
			lightmapScale = FloatForKey( e2, "lightmapscale" );
			if ( lightmapScale <= 0.0f ) {
				lightmapScale = FloatForKey( e2, "_lightmapscale" );
			}
			if ( lightmapScale <= 0.0f ) {
				lightmapScale = FloatForKey( e2, "_ls" );
			}
			if ( lightmapScale < 0.0f ) {
				lightmapScale = 0.0f;
			}
			if ( lightmapScale > 0.0f ) {
				Sys_Printf( "misc_model has lightmap scale of %.4f\n", lightmapScale );
			}
		}

		/* jal : entity based _shadeangle */
		shadeAngle = 0.0f;
		if ( strcmp( "", ValueForKey( e2, "_shadeangle" ) ) ) {
			shadeAngle = FloatForKey( e2, "_shadeangle" );
		}
		/* vortex' aliases */
		else if ( strcmp( "", ValueForKey( e2, "_smoothnormals" ) ) ) {
			shadeAngle = FloatForKey( e2, "_smoothnormals" );
		}
		else if ( strcmp( "", ValueForKey( e2, "_sn" ) ) ) {
			shadeAngle = FloatForKey( e2, "_sn" );
		}
		else if ( strcmp( "", ValueForKey( e2, "_sa" ) ) ) {
			shadeAngle = FloatForKey( e2, "_sa" );
		}
		else if ( strcmp( "", ValueForKey( e2, "_smooth" ) ) ) {
			shadeAngle = FloatForKey( e2, "_smooth" );
		}

		if ( shadeAngle < 0.0f ) {
			shadeAngle = 0.0f;
		}

		if ( shadeAngle > 0.0f ) {
			Sys_Printf( "misc_model has shading angle of %.4f\n", shadeAngle );
		}

		skin = 0;
		if ( strcmp( "", ValueForKey( e2, "_skin" ) ) ) {
			skin = IntForKey( e2, "_skin" );
		}
		else if ( strcmp( "", ValueForKey( e2, "skin" ) ) ) {
			skin = IntForKey( e2, "skin" );
		}

		clipDepth = clipDepthGlobal;
		if ( strcmp( "", ValueForKey( e2, "_clipdepth" ) ) ) {
			clipDepth = FloatForKey( e2, "_clipdepth" );
			Sys_Printf( "misc_model has autoclip depth of %.3f\n", clipDepth );
		}


		/* insert the model */
		InsertModel( model, skin, frame, transform, remap, celShader, mapEntityNum, castShadows, recvShadows, spawnFlags, lightmapScale, lightmapSampleSize, shadeAngle, clipDepth );

		/* free shader remappings */
		while ( remap != NULL )
		{
			remap2 = remap->next;
			free( remap );
			remap = remap2;
		}
	}

}
