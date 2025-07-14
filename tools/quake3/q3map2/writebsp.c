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
#define WRITEBSP_C



/* dependencies */
#include "q3map2.h"



//prefixInfo-stats
typedef struct {
	char	*name;
	int		surfaceFlags;
} prefixInfo_t;

static prefixInfo_t prefixInfo[] = {
	{ "metal",	TEX_SURF_METAL},
	{ "wood",	TEX_SURF_WOOD},
	{ "cloth",	TEX_SURF_CLOTH},
	{ "dirt",	TEX_SURF_DIRT},
	{ "glass",	TEX_SURF_GLASS},
	{ "plant",	TEX_SURF_PLANT},
	{ "sand",	TEX_SURF_SAND},
	{ "snow",	TEX_SURF_SNOW},
	{ "stone",	TEX_SURF_STONE},
	{ "water",	TEX_SURF_WATER},
	{ "grass",	TEX_SURF_GRASS},
};

#define NUM_PREFIXINFO 11 /* very important */

//Added by Spoon to recognize surfaceparms by shadernames
int GetSurfaceParm(const char *tex){
	char surf[MAX_QPATH], tex2[MAX_QPATH];
	int	i, j = 0;

	strcpy(tex2, tex);

	/* find last dir */
	for(i = 0; i < 64 && tex2[i] != '\0'; i++){
		if(tex2[i] == '\\' || tex2[i] == '/')
			j=i+1;
	}

	strcpy(surf, tex2+j);

	for(i=0; i<10; i++){
		if(surf[i] == '_')
			break;
	}
	surf[i] = '\0';

	/* Sys_Printf("%s\n", surf); */

	for(i=0; i < NUM_PREFIXINFO; i++){
		if(!Q_stricmp(surf, prefixInfo[i].name)){
			return prefixInfo[i].surfaceFlags;
		}
	}
	return 0;
}



/*
   EmitShader()
   emits a bsp shader entry
 */

int EmitShader( const char *shader, int *contentFlags, int *surfaceFlags ){
	int i;
	shaderInfo_t    *si;


	/* handle special cases */
	if ( shader == NULL ) {
		shader = "noshader";
	}

	/* try to find an existing shader */
	for ( i = 0; i < numBSPShaders; i++ )
	{
		/* if not Smokin'Guns like tex file */
		if ( !game->texFile )
		{
		/* ydnar: handle custom surface/content flags */
		if ( surfaceFlags != NULL && bspShaders[ i ].surfaceFlags != *surfaceFlags ) {
			continue;
		}
		if ( contentFlags != NULL && bspShaders[ i ].contentFlags != *contentFlags ) {
			continue;
		}
		}
		if ( !doingBSP ){
			si = ShaderInfoForShader( shader );
			if ( si->remapShader && si->remapShader[ 0 ] ) {
				shader = si->remapShader;
			}
		}
		/* compare name */
		if ( !Q_stricmp( shader, bspShaders[ i ].shader ) ) {
			return i;
		}
	}

	/* get shaderinfo */
	si = ShaderInfoForShader( shader );

	/* emit a new shader */
	AUTOEXPAND_BY_REALLOC0_BSP( Shaders, 1024 );

	numBSPShaders++;
	strcpy( bspShaders[ i ].shader, shader );
	bspShaders[ i ].surfaceFlags = ( surfaceFlags != NULL ) ? *surfaceFlags : si->surfaceFlags;
	bspShaders[ i ].contentFlags = ( contentFlags != NULL ) ? *contentFlags : si->contentFlags;

	if ( game->texFile )
	{
		/* Smokin'Guns like tex file */
		bspShaders[ i ].surfaceFlags |= GetSurfaceParm(si->shader);
	}

	/* recursively emit any damage shaders */
	if ( si->damageShader != NULL && si->damageShader[ 0 ] != '\0' ) {
		Sys_FPrintf( SYS_VRB, "Shader %s has damage shader %s\n", si->shader, si->damageShader );
		EmitShader( si->damageShader, NULL, NULL );
	}

	/* return it */
	return i;
}



/*
   EmitPlanes()
   there is no oportunity to discard planes, because all of the original
   brushes will be saved in the map
 */

void EmitPlanes( void ){
	int i;
	bspPlane_t  *bp;
	plane_t     *mp;


	/* walk plane list */
	mp = mapplanes;
	for ( i = 0; i < nummapplanes; i++, mp++ )
	{
		AUTOEXPAND_BY_REALLOC_BSP( Planes, 1024 );
		bp = &bspPlanes[ numBSPPlanes ];
		VectorCopy( mp->normal, bp->normal );
		bp->dist = mp->dist;
		numBSPPlanes++;
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d BSP planes\n", numBSPPlanes );
}



/*
   EmitLeaf()
   emits a leafnode to the bsp file
 */

void EmitLeaf( node_t *node ){
	bspLeaf_t       *leaf_p;
	brush_t         *b;
	drawSurfRef_t   *dsr;


	/* check limits */
	if ( numBSPLeafs >= MAX_MAP_LEAFS ) {
		Error( "MAX_MAP_LEAFS" );
	}

	leaf_p = &bspLeafs[numBSPLeafs];
	numBSPLeafs++;

	leaf_p->cluster = node->cluster;
	leaf_p->area = node->area;

	/* emit bounding box */
	VectorCopy( node->mins, leaf_p->mins );
	VectorCopy( node->maxs, leaf_p->maxs );

	/* emit leaf brushes */
	leaf_p->firstBSPLeafBrush = numBSPLeafBrushes;
	for ( b = node->brushlist; b; b = b->next )
	{
		/* something is corrupting brushes */
		if ( (size_t) b < 256 ) {
			Sys_FPrintf( SYS_WRN, "WARNING: Node brush list corrupted (0x%08X)\n", b );
			break;
		}
		//%	if( b->guard != 0xDEADBEEF )
		//%		Sys_Printf( "Brush %6d: 0x%08X Guard: 0x%08X Next: 0x%08X Original: 0x%08X Sides: %d\n", b->brushNum, b, b, b->next, b->original, b->numsides );

		AUTOEXPAND_BY_REALLOC_BSP( LeafBrushes, 1024 );
		bspLeafBrushes[ numBSPLeafBrushes ] = b->original->outputNum;
		numBSPLeafBrushes++;
	}

	leaf_p->numBSPLeafBrushes = numBSPLeafBrushes - leaf_p->firstBSPLeafBrush;

	/* emit leaf surfaces */
	if ( node->opaque ) {
		return;
	}

	/* add the drawSurfRef_t drawsurfs */
	leaf_p->firstBSPLeafSurface = numBSPLeafSurfaces;
	for ( dsr = node->drawSurfReferences; dsr; dsr = dsr->nextRef )
	{
		AUTOEXPAND_BY_REALLOC_BSP( LeafSurfaces, 1024 );
		bspLeafSurfaces[ numBSPLeafSurfaces ] = dsr->outputNum;
		numBSPLeafSurfaces++;
	}

	leaf_p->numBSPLeafSurfaces = numBSPLeafSurfaces - leaf_p->firstBSPLeafSurface;
}


/*
   EmitDrawNode_r()
   recursively emit the bsp nodes
 */

int EmitDrawNode_r( node_t *node ){
	bspNode_t   *n;
	int i, n0;


	/* check for leafnode */
	if ( node->planenum == PLANENUM_LEAF ) {
		EmitLeaf( node );
		return -numBSPLeafs;
	}

	/* emit a node */
	AUTOEXPAND_BY_REALLOC_BSP( Nodes, 1024 );
	n0 = numBSPNodes;
	n = &bspNodes[ n0 ];
	numBSPNodes++;

	VectorCopy( node->mins, n->mins );
	VectorCopy( node->maxs, n->maxs );

	if ( node->planenum & 1 ) {
		Error( "WriteDrawNodes_r: odd planenum" );
	}
	n->planeNum = node->planenum;

	//
	// recursively output the other nodes
	//
	for ( i = 0 ; i < 2 ; i++ )
	{
		if ( node->children[i]->planenum == PLANENUM_LEAF ) {
			n->children[i] = -( numBSPLeafs + 1 );
			EmitLeaf( node->children[i] );
		}
		else
		{
			n->children[i] = numBSPNodes;
			EmitDrawNode_r( node->children[i] );
			// n may have become invalid here, so...
			n = &bspNodes[ n0 ];
		}
	}

	return n - bspNodes;
}



/*
   ============
   SetModelNumbers
   ============
 */
void SetModelNumbers( void ){
	int i;
	int models;
	char value[10];

	models = 1;
	for ( i = 1 ; i < numEntities ; i++ ) {
		if ( entities[i].brushes || entities[i].patches ) {
			sprintf( value, "*%i", models );
			models++;
			SetKeyValue( &entities[i], "model", value );
		}
	}

}




/*
   SetLightStyles()
   sets style keys for entity lights
 */

void SetLightStyles( void ){
	int i, j, style, numStyles;
	const char  *t;
	entity_t    *e;
	epair_t     *ep, *next;
	char value[ 10 ];
	char lightTargets[ MAX_SWITCHED_LIGHTS ][ 64 ];
	int lightStyles[ MAX_SWITCHED_LIGHTS ];

	/* -keeplights option: force lights to be kept and ignore what the map file says */
	if ( keepLights ) {
		SetKeyValue( &entities[0], "_keepLights", "1" );
	}

	/* ydnar: determine if we keep lights in the bsp */
	if ( KeyExists( &entities[ 0 ], "_keepLights" ) == qtrue ) {
		t = ValueForKey( &entities[ 0 ], "_keepLights" );
		keepLights = ( t[ 0 ] == '1' ) ? qtrue : qfalse;
	}

	/* any light that is controlled (has a targetname) must have a unique style number generated for it */
	numStyles = 0;
	for ( i = 1; i < numEntities; i++ )
	{
		e = &entities[ i ];

		t = ValueForKey( e, "classname" );
		if ( Q_strncasecmp( t, "light", 5 ) ) {
			continue;
		}
		t = ValueForKey( e, "targetname" );
		if ( t[ 0 ] == '\0' ) {
			/* ydnar: strip the light from the BSP file */
			if ( keepLights == qfalse ) {
				ep = e->epairs;
				while ( ep != NULL )
				{
					next = ep->next;
					free( ep->key );
					free( ep->value );
					free( ep );
					ep = next;
				}
				e->epairs = NULL;
				numStrippedLights++;
			}

			/* next light */
			continue;
		}

		/* get existing style */
		style = IntForKey( e, "style" );
		if ( style < LS_NORMAL || style > LS_NONE ) {
			Error( "Invalid lightstyle (%d) on entity %d", style, i );
		}

		/* find this targetname */
		for ( j = 0; j < numStyles; j++ )
			if ( lightStyles[ j ] == style && !strcmp( lightTargets[ j ], t ) ) {
				break;
			}

		/* add a new style */
		if ( j >= numStyles ) {
			if ( numStyles == MAX_SWITCHED_LIGHTS ) {
				Error( "MAX_SWITCHED_LIGHTS (%d) exceeded, reduce the number of lights with targetnames", MAX_SWITCHED_LIGHTS );
			}
			strcpy( lightTargets[ j ], t );
			lightStyles[ j ] = style;
			numStyles++;
		}

		/* set explicit style */
		sprintf( value, "%d", 32 + j );
		SetKeyValue( e, "style", value );

		/* set old style */
		if ( style != LS_NORMAL ) {
			sprintf( value, "%d", style );
			SetKeyValue( e, "switch_style", value );
		}
	}

	/* emit some statistics */
	Sys_FPrintf( SYS_VRB, "%9d light entities stripped\n", numStrippedLights );
}



/*
   BeginBSPFile()
   starts a new bsp file
 */

void BeginBSPFile( void ){
	/* these values may actually be initialized if the file existed when loaded, so clear them explicitly */
	numBSPModels = 0;
	numBSPNodes = 0;
	numBSPBrushSides = 0;
	numBSPLeafSurfaces = 0;
	numBSPLeafBrushes = 0;

	/* leave leaf 0 as an error, because leafs are referenced as negative number nodes */
	numBSPLeafs = 1;


	/* ydnar: gs mods: set the first 6 drawindexes to 0 1 2 2 1 3 for triangles and quads */
	numBSPDrawIndexes = 6;
	AUTOEXPAND_BY_REALLOC_BSP( DrawIndexes, 1024 );
	bspDrawIndexes[ 0 ] = 0;
	bspDrawIndexes[ 1 ] = 1;
	bspDrawIndexes[ 2 ] = 2;
	bspDrawIndexes[ 3 ] = 0;
	bspDrawIndexes[ 4 ] = 2;
	bspDrawIndexes[ 5 ] = 3;
}



/*
   RestoreSurfaceFlags()
   read Smokin'Guns like tex file
   added by spoon to get back the changed surfaceflags
 */

void RestoreSurfaceFlags( char *filename ) {
	int i;
	FILE *texfile;
	int surfaceFlags[ MAX_MAP_DRAW_SURFS ];
	int numTexInfos;

	/* first parse the tex file */
	texfile = fopen( filename, "r" );

	if ( texfile ) {
		fscanf( texfile, "TEXFILE\n%i\n", &numTexInfos );

		/* Sys_Printf( "%i\n", numTexInfos ); */

		for ( i = 0; i < numTexInfos; i++ ) {
			vec3_t color;

			fscanf( texfile, "%i %f %f %f\n", &surfaceFlags[ i ],
				&color[ 0 ], &color[ 1 ], &color[ 2 ]);

			bspShaders[ i ].surfaceFlags = surfaceFlags[ i ];

			/* Sys_Printf( "%i\n", surfaceFlags[ i ] ); */
		}
	} else {
		Sys_Printf("couldn't find %s not tex-file is now writed without surfaceFlags!\n", filename);
	}
}



/*
   WriteTexFile()
   write Smokin'Guns like tex file
   added by spoon
 */

void WriteTexFile( char* filename ) {
	FILE *texfile;
	int i;

	if ( !compile_map ) {
		RestoreSurfaceFlags( filename );
	}

	Sys_Printf( "Writing %s ...\n", filename );

	texfile = fopen ( filename, "w" );

	fprintf( texfile, "TEXFILE\n" );

	fprintf( texfile, "%i\n", numBSPShaders );

	for ( i = 0 ; i < numBSPShaders ; i++ ) {
		shaderInfo_t *se = ShaderInfoForShader( bspShaders[ i ].shader );

		fprintf( texfile, "\n%i %f %f %f", bspShaders[ i ].surfaceFlags,
			se->color[ 0 ], se->color[ 1 ], se->color[ 2 ] );

		bspShaders[ i ].surfaceFlags = i;
	}

	fclose( texfile );
}



/*
   EndBSPFile()
   finishes a new bsp and writes to disk
 */

void EndBSPFile( qboolean do_write, const char *BSPFilePath, const char *surfaceFilePath ){

	Sys_FPrintf( SYS_VRB, "--- EndBSPFile ---\n" );

	EmitPlanes();

	numBSPEntities = numEntities;
	UnparseEntities();

	if ( do_write ) {
		/* write the surface extra file */
		WriteSurfaceExtraFile( surfaceFilePath );

		if ( game->texFile )
		{
			char basename[ 1024 ];	
			char filename[ 1024 ];
			strncpy( basename, BSPFilePath, sizeof(basename) );
			StripExtension( basename );
			sprintf( filename, "%s.tex", basename );

			/* only create tex file if it is the first compile */
			WriteTexFile( filename );
		}

		/* write the bsp */
		Sys_Printf( "Writing %s\n", BSPFilePath );
		WriteBSPFile( BSPFilePath );
	}
}



/*
   EmitBrushes()
   writes the brush list to the bsp
 */

void EmitBrushes( brush_t *brushes, int *firstBrush, int *numBrushes ){
	int j;
	brush_t         *b;
	bspBrush_t      *db;
	bspBrushSide_t  *cp;


	/* set initial brush */
	if ( firstBrush != NULL ) {
		*firstBrush = numBSPBrushes;
	}
	if ( numBrushes != NULL ) {
		*numBrushes = 0;
	}

	/* walk list of brushes */
	for ( b = brushes; b != NULL; b = b->next )
	{
		/* check limits */
		AUTOEXPAND_BY_REALLOC_BSP( Brushes, 1024 );

		/* get bsp brush */
		b->outputNum = numBSPBrushes;
		db = &bspBrushes[ numBSPBrushes ];
		numBSPBrushes++;
		if ( numBrushes != NULL ) {
			( *numBrushes )++;
		}

		db->shaderNum = EmitShader( b->contentShader->shader, &b->contentShader->contentFlags, &b->contentShader->surfaceFlags );
		db->firstSide = numBSPBrushSides;

		/* walk sides */
		db->numSides = 0;
		for ( j = 0; j < b->numsides; j++ )
		{
			/* set output number to bogus initially */
			b->sides[ j ].outputNum = -1;

			/* check count */
			AUTOEXPAND_BY_REALLOC_BSP( BrushSides, 1024 );

			/* emit side */
			b->sides[ j ].outputNum = numBSPBrushSides;
			cp = &bspBrushSides[ numBSPBrushSides ];
			db->numSides++;
			numBSPBrushSides++;
			cp->planeNum = b->sides[ j ].planenum;

			/* emit shader */
			if ( b->sides[ j ].shaderInfo ) {
				cp->shaderNum = EmitShader( b->sides[ j ].shaderInfo->shader, &b->sides[ j ].shaderInfo->contentFlags, &b->sides[ j ].shaderInfo->surfaceFlags );
			}
			else{
				cp->shaderNum = EmitShader( NULL, NULL, NULL );
			}
		}
	}
}



/*
   EmitFogs() - ydnar
   turns map fogs into bsp fogs
 */

void EmitFogs( void ){
	int i, j;


	/* setup */
	numBSPFogs = numMapFogs;

	/* walk list */
	for ( i = 0; i < numMapFogs; i++ )
	{
		/* set shader */
		strcpy( bspFogs[ i ].shader, mapFogs[ i ].si->shader );

		/* global fog doesn't have an associated brush */
		if ( mapFogs[ i ].brush == NULL ) {
			bspFogs[ i ].brushNum = -1;
			bspFogs[ i ].visibleSide = -1;
		}
		else
		{
			/* set brush */
			bspFogs[ i ].brushNum = mapFogs[ i ].brush->outputNum;

			/* try to use forced visible side */
			if ( mapFogs[ i ].visibleSide >= 0 ) {
				bspFogs[ i ].visibleSide = mapFogs[ i ].visibleSide;
				continue;
			}

			/* find visible side */
			for ( j = 0; j < 6; j++ )
			{
				if ( mapFogs[ i ].brush->sides[ j ].visibleHull != NULL ) {
					Sys_Printf( "Fog %d has visible side %d\n", i, j );
					bspFogs[ i ].visibleSide = j;
					break;
				}
			}
		}
	}
}



/*
   BeginModel()
   sets up a new brush model
 */

void BeginModel( void ){
	bspModel_t  *mod;
	brush_t     *b;
	entity_t    *e;
	vec3_t mins, maxs;
	vec3_t lgMins, lgMaxs;          /* ydnar: lightgrid mins/maxs */
	parseMesh_t *p;
	int i;


	/* test limits */
	AUTOEXPAND_BY_REALLOC_BSP( Models, 256 );

	/* get model and entity */
	mod = &bspModels[ numBSPModels ];
	e = &entities[ mapEntityNum ];

	/* ydnar: lightgrid mins/maxs */
	ClearBounds( lgMins, lgMaxs );

	/* bound the brushes */
	ClearBounds( mins, maxs );
	for ( b = e->brushes; b; b = b->next )
	{
		/* ignore non-real brushes (origin, etc) */
		if ( b->numsides == 0 ) {
			continue;
		}
		AddPointToBounds( b->mins, mins, maxs );
		AddPointToBounds( b->maxs, mins, maxs );

		/* ydnar: lightgrid bounds */
		if ( b->compileFlags & C_LIGHTGRID ) {
			AddPointToBounds( b->mins, lgMins, lgMaxs );
			AddPointToBounds( b->maxs, lgMins, lgMaxs );
		}
	}

	/* bound patches */
	for ( p = e->patches; p; p = p->next )
	{
		for ( i = 0; i < ( p->mesh.width * p->mesh.height ); i++ )
			AddPointToBounds( p->mesh.verts[i].xyz, mins, maxs );
	}

	/* ydnar: lightgrid mins/maxs */
	if ( lgMins[ 0 ] < 99999 ) {
		/* use lightgrid bounds */
		VectorCopy( lgMins, mod->mins );
		VectorCopy( lgMaxs, mod->maxs );
	}
	else
	{
		/* use brush/patch bounds */
		VectorCopy( mins, mod->mins );
		VectorCopy( maxs, mod->maxs );
	}

	/* note size */
	Sys_FPrintf( SYS_VRB, "BSP bounds: { %f %f %f } { %f %f %f }\n", mins[ 0 ], mins[ 1 ], mins[ 2 ], maxs[ 0 ], maxs[ 1 ], maxs[ 2 ] );
	Sys_FPrintf( SYS_VRB, "Lightgrid bounds: { %f %f %f } { %f %f %f }\n", lgMins[ 0 ], lgMins[ 1 ], lgMins[ 2 ], lgMaxs[ 0 ], lgMaxs[ 1 ], lgMaxs[ 2 ] );

	/* set firsts */
	mod->firstBSPSurface = numBSPDrawSurfaces;
	mod->firstBSPBrush = numBSPBrushes;
}




/*
   EndModel()
   finish a model's processing
 */

void EndModel( entity_t *e, node_t *headnode ){
	bspModel_t  *mod;


	/* note it */
	Sys_FPrintf( SYS_VRB, "--- EndModel ---\n" );

	/* emit the bsp */
	mod = &bspModels[ numBSPModels ];
	EmitDrawNode_r( headnode );

	/* set surfaces and brushes */
	mod->numBSPSurfaces = numBSPDrawSurfaces - mod->firstBSPSurface;
	mod->firstBSPBrush = e->firstBrush;
	mod->numBSPBrushes = e->numBrushes;

	/* increment model count */
	numBSPModels++;
}
