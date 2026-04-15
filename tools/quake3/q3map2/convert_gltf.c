/* -------------------------------------------------------------------------------

   Copyright (C) 2026 Netradiant contributors.
   For a list of contributors, see the accompanying CONTRIBUTORS file.

   This file is part of NetRadiant.

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

   ------------------------------------------------------------------------------- */



/* marker */
#define CONVERT_GLTF_C



/* dependencies */
#include "q3map2.h"

#include "cgltf.h"
#define CGLTF_WRITE_IMPLEMENTATION
#include "cgltf_write.h"

#include <float.h>
#include <ctype.h>
#include "png.h"


/*
   stristr()
   case-insensitive substring search
 */
static const char *stristr( const char *haystack, const char *needle ){
	if ( !haystack || !needle ) {
		return NULL;
	}
	for ( ; *haystack; haystack++ ) {
		const char *h = haystack;
		const char *n = needle;
		while ( *h && *n && tolower( (unsigned char)*h ) == tolower( (unsigned char)*n ) ) {
			h++;
			n++;
		}
		if ( !*n ) {
			return haystack;
		}
	}
	return NULL;
}

/*
   PNGWriteCallback()
   libpng write callback that appends to a dynamically growing buffer
 */

typedef struct pngWriteBuffer_s {
	unsigned char *data;
	cgltf_size size;
	cgltf_size capacity;
} pngWriteBuffer_t;

static void PNGWriteCallback( png_structp png, png_bytep data, png_size_t length ){
	pngWriteBuffer_t *buf = (pngWriteBuffer_t *)png_get_io_ptr( png );
	while ( buf->size + length > buf->capacity ) {
		buf->capacity = buf->capacity ? buf->capacity * 2 : 4096;
		buf->data = realloc( buf->data, buf->capacity );
	}
	memcpy( buf->data + buf->size, data, length );
	buf->size += length;
}

static void PNGFlushCallback( png_structp png ){
	(void)png;
}


/*
   EncodePNG()
   encodes RGBA pixel data into a PNG in memory
   returns allocated buffer and sets outSize; caller must free()
 */

static unsigned char *EncodePNG( const byte *pixels, int width, int height, cgltf_size *outSize ){
	png_structp png;
	png_infop info;
	png_bytep *rowPointers;
	pngWriteBuffer_t buf;
	int i;

	*outSize = 0;

	png = png_create_write_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if ( !png ) {
		return NULL;
	}
	info = png_create_info_struct( png );
	if ( !info ) {
		png_destroy_write_struct( &png, NULL );
		return NULL;
	}
	if ( setjmp( png_jmpbuf( png ) ) ) {
		png_destroy_write_struct( &png, &info );
		return NULL;
	}

	memset( &buf, 0, sizeof( buf ) );
	png_set_write_fn( png, &buf, PNGWriteCallback, PNGFlushCallback );

	png_set_IHDR( png, info, width, height, 8, PNG_COLOR_TYPE_RGBA,
	              PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT );

	rowPointers = safe_malloc( sizeof( png_bytep ) * height );
	for ( i = 0; i < height; i++ ) {
		rowPointers[ i ] = (png_bytep)( pixels + i * width * 4 );
	}

	png_set_rows( png, info, rowPointers );
	png_write_png( png, info, PNG_TRANSFORM_IDENTITY, NULL );

	png_destroy_write_struct( &png, &info );
	free( rowPointers );

	*outSize = buf.size;
	return buf.data;
}



/*
   ConvertBSPToGLTF()
   exports a glTF 2.0 GLB file from the bsp
 */

/* blend type classification for shader-to-gltf material mapping */
#define GLTF_BLEND_OPAQUE    0  /* fully opaque */
#define GLTF_BLEND_ALPHA     1  /* uses texture alpha (blendfunc blend) */
#define GLTF_BLEND_ADDITIVE  2  /* additive: black = transparent (blendfunc add) */
#define GLTF_BLEND_MASK      3  /* alpha test (alphaFunc) */

static int IsSurfaceExportable( int surfaceType ){
	return surfaceType == MST_PLANAR || surfaceType == MST_TRIANGLE_SOUP || surfaceType == MST_PATCH;
}

int ConvertBSPToGLTF( char *bspName ){
	int i, j, k, s, v, x, y, modelNum;
	char name[ 1024 ], base[ 1024 ];
	bspModel_t *model;
	bspDrawSurface_t *ds;
	bspDrawVert_t *dv;
	entity_t *e;
	vec3_t origin;
	const char *key;

	int totalSurfaces, totalVertices, totalIndices, numMeshes, numMaterials;
	int *shaderToMaterial;
	int *primitivesPerMesh;
	int meshIdx, surfIdx, primOffset;
	int hasSurfaces, primCount;
	int numVerts, numIndexes;

	/* patch tessellation cache */
	mesh_t **patchMeshes;
	int numPatchMeshes, patchMeshIdx;
	mesh_t *mesh;

	cgltf_size bufferSize, bufferOffset;
	cgltf_size posOffset, normOffset, uvOffset, idxOffset;
	unsigned char *bufferData;
	float *posPtr, *normPtr, *uvPtr;
	unsigned int *idxPtr;

	int bvBase, acBase;
	float posMin[ 3 ], posMax[ 3 ];
	float gltfCoord[ 3 ];

	cgltf_buffer *gltfBuffers;
	cgltf_buffer_view *bufferViews;
	cgltf_accessor *gltfAccessors;
	cgltf_material *gltfMaterials;
	cgltf_mesh *gltfMeshes;
	cgltf_node *gltfNodes;
	cgltf_node **nodePointers;
	cgltf_scene *gltfScenes;
	cgltf_primitive *allPrimitives;
	cgltf_attribute *allAttributes;
	cgltf_image *gltfImages;
	cgltf_texture *gltfTextures;
	cgltf_sampler *gltfSamplers;

	/* texture embedding */
	int numImages;
	unsigned char **pngDataArray;
	cgltf_size *pngSizeArray;
	cgltf_size totalImageSize;
	int numBufferViews;
	int *blendTypes;

	cgltf_data data;
	cgltf_options options;
	cgltf_result result;


	/* note it */
	Sys_Printf( "--- Convert BSP to GLTF ---\n" );

	/* create output filename */
	strcpy( name, bspName );
	StripExtension( name );
	strcat( name, ".glb" );
	Sys_Printf( "writing %s\n", name );
	ExtractFileBase( bspName, base );


	/* first pass: count surfaces, vertices, indices, meshes, tessellate patches */
	totalSurfaces = 0;
	totalVertices = 0;
	totalIndices = 0;
	numMeshes = 0;
	numPatchMeshes = 0;

	/* count patch surfaces first to allocate cache */
	for ( i = 0; i < numEntities; i++ )
	{
		e = &entities[ i ];
		if ( i == 0 ) {
			modelNum = 0;
		}
		else
		{
			key = ValueForKey( e, "model" );
			if ( key[ 0 ] != '*' ) {
				continue;
			}
			modelNum = atoi( key + 1 );
		}
		model = &bspModels[ modelNum ];
		for ( j = 0; j < model->numBSPSurfaces; j++ )
		{
			s = j + model->firstBSPSurface;
			ds = &bspDrawSurfaces[ s ];
			if ( ds->surfaceType == MST_PATCH ) {
				numPatchMeshes++;
			}
		}
	}
	patchMeshes = safe_malloc0( sizeof( mesh_t * ) * ( numPatchMeshes > 0 ? numPatchMeshes : 1 ) );
	patchMeshIdx = 0;

	for ( i = 0; i < numEntities; i++ )
	{
		e = &entities[ i ];
		if ( i == 0 ) {
			modelNum = 0;
		}
		else
		{
			key = ValueForKey( e, "model" );
			if ( key[ 0 ] != '*' ) {
				continue;
			}
			modelNum = atoi( key + 1 );
		}
		model = &bspModels[ modelNum ];

		hasSurfaces = 0;
		for ( j = 0; j < model->numBSPSurfaces; j++ )
		{
			s = j + model->firstBSPSurface;
			ds = &bspDrawSurfaces[ s ];
			if ( !IsSurfaceExportable( ds->surfaceType ) ) {
				continue;
			}

			if ( ds->surfaceType == MST_PATCH ) {
				mesh = TessellatePatch( ds );
				patchMeshes[ patchMeshIdx++ ] = mesh;
				numVerts = mesh->width * mesh->height;
				numIndexes = ( mesh->width - 1 ) * ( mesh->height - 1 ) * 6;
			}
			else {
				numVerts = ds->numVerts;
				numIndexes = ds->numIndexes;
			}

			totalSurfaces++;
			totalVertices += numVerts;
			totalIndices += numIndexes;
			hasSurfaces = 1;
		}
		if ( hasSurfaces ) {
			numMeshes++;
		}
	}

	if ( totalSurfaces == 0 ) {
		Sys_Printf( "No surfaces to export\n" );
		return 0;
	}


	/* count unique materials (shader indices actually used) */
	shaderToMaterial = safe_malloc0( sizeof( int ) * numBSPShaders );
	numMaterials = 0;
	for ( i = 0; i < numBSPShaders; i++ )
	{
		shaderToMaterial[ i ] = -1;
	}

	for ( i = 0; i < numEntities; i++ )
	{
		e = &entities[ i ];
		if ( i == 0 ) {
			modelNum = 0;
		}
		else
		{
			key = ValueForKey( e, "model" );
			if ( key[ 0 ] != '*' ) {
				continue;
			}
			modelNum = atoi( key + 1 );
		}
		model = &bspModels[ modelNum ];
		for ( j = 0; j < model->numBSPSurfaces; j++ )
		{
			s = j + model->firstBSPSurface;
			ds = &bspDrawSurfaces[ s ];
			if ( !IsSurfaceExportable( ds->surfaceType ) ) {
				continue;
			}
			if ( shaderToMaterial[ ds->shaderNum ] == -1 ) {
				shaderToMaterial[ ds->shaderNum ] = numMaterials++;
			}
		}
	}


	/* count primitives per mesh */
	primitivesPerMesh = safe_malloc0( sizeof( int ) * numMeshes );
	meshIdx = 0;


	/* determine blend type for each material from shader properties */
	blendTypes = safe_malloc0( sizeof( int ) * numMaterials );
	for ( i = 0; i < numBSPShaders; i++ )
	{
		if ( shaderToMaterial[ i ] >= 0 ) {
			shaderInfo_t *si;
			k = shaderToMaterial[ i ];
			si = ShaderInfoForShader( bspShaders[ i ].shader );
			blendTypes[ k ] = GLTF_BLEND_OPAQUE;

			if ( si ) {
				if ( si->implicitMap == IM_MASKED ) {
					blendTypes[ k ] = GLTF_BLEND_MASK;
				}
				else if ( si->implicitMap == IM_BLEND ) {
					blendTypes[ k ] = GLTF_BLEND_ALPHA;
				}
				else if ( si->shaderText ) {
					if ( stristr( si->shaderText, "alphaFunc" ) ) {
						blendTypes[ k ] = GLTF_BLEND_MASK;
					}
					else if ( stristr( si->shaderText, "GL_SRC_ALPHA" ) ||
					          stristr( si->shaderText, "blendfunc blend" ) ) {
						blendTypes[ k ] = GLTF_BLEND_ALPHA;
					}
					else if ( stristr( si->shaderText, "blendfunc add" ) ||
					          stristr( si->shaderText, "GL_ONE GL_ONE" ) ||
					          stristr( si->shaderText, "GL_ONE GL_ONE_MINUS_SRC_COLOR" ) ) {
						blendTypes[ k ] = GLTF_BLEND_ADDITIVE;
					}
				}
			}
		}
	}


	/* load and encode textures as PNG for embedding */
	pngDataArray = safe_malloc0( sizeof( unsigned char * ) * numMaterials );
	pngSizeArray = safe_malloc0( sizeof( cgltf_size ) * numMaterials );
	numImages = 0;
	totalImageSize = 0;
	for ( i = 0; i < numBSPShaders; i++ )
	{
		if ( shaderToMaterial[ i ] >= 0 ) {
			image_t *img;
			unsigned char *pngData;
			cgltf_size pngSize;

			k = shaderToMaterial[ i ];
			img = ImageLoad( bspShaders[ i ].shader );
			if ( img && img->pixels && img->width > 0 && img->height > 0 ) {
				/* for additive blends, synthesize alpha from pixel brightness */
				if ( blendTypes[ k ] == GLTF_BLEND_ADDITIVE ) {
					int px;
					int numPixels = img->width * img->height;
					for ( px = 0; px < numPixels; px++ ) {
						unsigned char r = img->pixels[ px * 4 + 0 ];
						unsigned char g = img->pixels[ px * 4 + 1 ];
						unsigned char b = img->pixels[ px * 4 + 2 ];
						unsigned char a = r;
						if ( g > a ) a = g;
						if ( b > a ) a = b;
						img->pixels[ px * 4 + 3 ] = a;
					}
				}
				pngData = EncodePNG( img->pixels, img->width, img->height, &pngSize );
				if ( pngData ) {
					pngDataArray[ k ] = pngData;
					pngSizeArray[ k ] = pngSize;
					totalImageSize += pngSize;
					numImages++;
					Sys_FPrintf( SYS_VRB, "  Encoded texture: %s (%dx%d, %d bytes PNG)\n",
					             bspShaders[ i ].shader, img->width, img->height, (int)pngSize );
				}
				else {
					Sys_FPrintf( SYS_VRB, "  Failed to encode texture: %s\n", bspShaders[ i ].shader );
				}
			}
			else {
				Sys_FPrintf( SYS_VRB, "  Texture not found: %s\n", bspShaders[ i ].shader );
			}
		}
	}
	Sys_Printf( "Embedded %d of %d textures\n", numImages, numMaterials );

	for ( i = 0; i < numEntities; i++ )
	{
		e = &entities[ i ];
		if ( i == 0 ) {
			modelNum = 0;
		}
		else
		{
			key = ValueForKey( e, "model" );
			if ( key[ 0 ] != '*' ) {
				continue;
			}
			modelNum = atoi( key + 1 );
		}
		model = &bspModels[ modelNum ];

		primCount = 0;
		for ( j = 0; j < model->numBSPSurfaces; j++ )
		{
			s = j + model->firstBSPSurface;
			ds = &bspDrawSurfaces[ s ];
			if ( !IsSurfaceExportable( ds->surfaceType ) ) {
				continue;
			}
			primCount++;
		}
		if ( primCount > 0 ) {
			primitivesPerMesh[ meshIdx++ ] = primCount;
		}
	}


	/* allocate binary buffer */
	/* per vertex: 3 floats (pos) + 3 floats (normal) + 2 floats (uv) = 32 bytes */
	/* per index: 1 uint32 = 4 bytes */
	/* plus embedded PNG image data */
	bufferSize = (cgltf_size)totalVertices * 32 + (cgltf_size)totalIndices * 4 + totalImageSize;
	bufferData = safe_malloc0( bufferSize );


	/* allocate cgltf structures */
	/* buffer views: 4 per surface (pos/norm/uv/idx) + 1 per embedded image */
	numBufferViews = totalSurfaces * 4 + numImages;
	gltfBuffers = safe_malloc0( sizeof( cgltf_buffer ) );
	bufferViews = safe_malloc0( sizeof( cgltf_buffer_view ) * numBufferViews );
	gltfAccessors = safe_malloc0( sizeof( cgltf_accessor ) * totalSurfaces * 4 );
	gltfMaterials = safe_malloc0( sizeof( cgltf_material ) * numMaterials );
	gltfMeshes = safe_malloc0( sizeof( cgltf_mesh ) * numMeshes );
	gltfNodes = safe_malloc0( sizeof( cgltf_node ) * numMeshes );
	nodePointers = safe_malloc0( sizeof( cgltf_node * ) * numMeshes );
	gltfScenes = safe_malloc0( sizeof( cgltf_scene ) );
	allPrimitives = safe_malloc0( sizeof( cgltf_primitive ) * totalSurfaces );
	allAttributes = safe_malloc0( sizeof( cgltf_attribute ) * totalSurfaces * 3 );
	gltfImages = safe_malloc0( sizeof( cgltf_image ) * ( numImages > 0 ? numImages : 1 ) );
	gltfTextures = safe_malloc0( sizeof( cgltf_texture ) * ( numImages > 0 ? numImages : 1 ) );
	gltfSamplers = safe_malloc0( sizeof( cgltf_sampler ) );


	/* set up buffer */
	gltfBuffers[ 0 ].size = bufferSize;


	/* set up sampler (shared by all textures) */
	gltfSamplers[ 0 ].mag_filter = cgltf_filter_type_linear;
	gltfSamplers[ 0 ].min_filter = cgltf_filter_type_linear_mipmap_linear;
	gltfSamplers[ 0 ].wrap_s = cgltf_wrap_mode_repeat;
	gltfSamplers[ 0 ].wrap_t = cgltf_wrap_mode_repeat;


	/* set up materials with embedded textures */
	{
		int imgIdx = 0;
		int imgBvBase = totalSurfaces * 4;
		cgltf_size imgBufferOffset = (cgltf_size)totalVertices * 32 + (cgltf_size)totalIndices * 4;

		for ( i = 0; i < numBSPShaders; i++ )
		{
			if ( shaderToMaterial[ i ] >= 0 ) {
				shaderInfo_t *si;

				k = shaderToMaterial[ i ];
				si = ShaderInfoForShader( bspShaders[ i ].shader );

				gltfMaterials[ k ].name = bspShaders[ i ].shader;
				gltfMaterials[ k ].has_pbr_metallic_roughness = 1;
				gltfMaterials[ k ].pbr_metallic_roughness.base_color_factor[ 0 ] = 1.0f;
				gltfMaterials[ k ].pbr_metallic_roughness.base_color_factor[ 1 ] = 1.0f;
				gltfMaterials[ k ].pbr_metallic_roughness.base_color_factor[ 2 ] = 1.0f;
				gltfMaterials[ k ].pbr_metallic_roughness.base_color_factor[ 3 ] = 1.0f;
				gltfMaterials[ k ].pbr_metallic_roughness.metallic_factor = 0.0f;
				gltfMaterials[ k ].pbr_metallic_roughness.roughness_factor = 1.0f;
				gltfMaterials[ k ].alpha_cutoff = 0.5f;

				/* set alpha mode from pre-computed blend type */
				switch ( blendTypes[ k ] ) {
				case GLTF_BLEND_MASK:
					gltfMaterials[ k ].alpha_mode = cgltf_alpha_mode_mask;
					break;
				case GLTF_BLEND_ALPHA:
				case GLTF_BLEND_ADDITIVE:
					gltfMaterials[ k ].alpha_mode = cgltf_alpha_mode_blend;
					break;
				default:
					gltfMaterials[ k ].alpha_mode = cgltf_alpha_mode_opaque;
					break;
				}

				/* map shader properties to glTF material */
				if ( si ) {
					/* double-sided from cull none/disable/twosided */
					if ( si->twoSided ) {
						gltfMaterials[ k ].double_sided = 1;
					}

					/* emissive: surfaces with light value emit light */
					if ( si->value > 0 ) {
						gltfMaterials[ k ].emissive_factor[ 0 ] = si->color[ 0 ];
						gltfMaterials[ k ].emissive_factor[ 1 ] = si->color[ 1 ];
						gltfMaterials[ k ].emissive_factor[ 2 ] = si->color[ 2 ];
					}
				}

				/* if we have a PNG for this material, embed it */
				if ( pngDataArray[ k ] ) {
					/* copy PNG data into the binary buffer */
					memcpy( bufferData + imgBufferOffset, pngDataArray[ k ], pngSizeArray[ k ] );

					/* set up buffer view for this image */
					bufferViews[ imgBvBase + imgIdx ].buffer = &gltfBuffers[ 0 ];
					bufferViews[ imgBvBase + imgIdx ].offset = imgBufferOffset;
					bufferViews[ imgBvBase + imgIdx ].size = pngSizeArray[ k ];

					/* set up image */
					gltfImages[ imgIdx ].name = bspShaders[ i ].shader;
					gltfImages[ imgIdx ].buffer_view = &bufferViews[ imgBvBase + imgIdx ];
					gltfImages[ imgIdx ].mime_type = "image/png";

					/* set up texture */
					gltfTextures[ imgIdx ].name = bspShaders[ i ].shader;
					gltfTextures[ imgIdx ].image = &gltfImages[ imgIdx ];
					gltfTextures[ imgIdx ].sampler = &gltfSamplers[ 0 ];

					/* wire texture into material */
					gltfMaterials[ k ].pbr_metallic_roughness.base_color_texture.texture = &gltfTextures[ imgIdx ];
					gltfMaterials[ k ].pbr_metallic_roughness.base_color_texture.scale = 1.0f;

					imgBufferOffset += pngSizeArray[ k ];
					imgIdx++;
				}
			}
		}
	}


	/* set up mesh primitive structure */
	primOffset = 0;
	for ( meshIdx = 0; meshIdx < numMeshes; meshIdx++ )
	{
		gltfMeshes[ meshIdx ].primitives = &allPrimitives[ primOffset ];
		gltfMeshes[ meshIdx ].primitives_count = primitivesPerMesh[ meshIdx ];
		for ( j = 0; j < primitivesPerMesh[ meshIdx ]; j++ )
		{
			surfIdx = primOffset + j;
			allPrimitives[ surfIdx ].type = cgltf_primitive_type_triangles;
			allPrimitives[ surfIdx ].attributes = &allAttributes[ surfIdx * 3 ];
			allPrimitives[ surfIdx ].attributes_count = 3;
		}
		primOffset += primitivesPerMesh[ meshIdx ];
	}


	/* set up nodes */
	for ( meshIdx = 0; meshIdx < numMeshes; meshIdx++ )
	{
		gltfNodes[ meshIdx ].mesh = &gltfMeshes[ meshIdx ];
		nodePointers[ meshIdx ] = &gltfNodes[ meshIdx ];
	}


	/* set up scene */
	gltfScenes[ 0 ].nodes = nodePointers;
	gltfScenes[ 0 ].nodes_count = numMeshes;


	/* second pass: fill binary buffer and set up buffer views, accessors, primitives */
	bufferOffset = 0;
	surfIdx = 0;
	meshIdx = 0;
	patchMeshIdx = 0;

	for ( i = 0; i < numEntities; i++ )
	{
		e = &entities[ i ];
		if ( i == 0 ) {
			modelNum = 0;
		}
		else
		{
			key = ValueForKey( e, "model" );
			if ( key[ 0 ] != '*' ) {
				continue;
			}
			modelNum = atoi( key + 1 );
		}
		model = &bspModels[ modelNum ];

		/* get entity origin */
		key = ValueForKey( e, "origin" );
		if ( key[ 0 ] == '\0' ) {
			VectorClear( origin );
		}
		else
		{
			GetVectorForKey( e, "origin", origin );
		}

		hasSurfaces = 0;
		for ( j = 0; j < model->numBSPSurfaces; j++ )
		{
			s = j + model->firstBSPSurface;
			ds = &bspDrawSurfaces[ s ];
			if ( !IsSurfaceExportable( ds->surfaceType ) ) {
				continue;
			}

			/* determine vertex/index counts for this surface */
			if ( ds->surfaceType == MST_PATCH ) {
				mesh = patchMeshes[ patchMeshIdx++ ];
				numVerts = mesh->width * mesh->height;
				numIndexes = ( mesh->width - 1 ) * ( mesh->height - 1 ) * 6;
			}
			else {
				mesh = NULL;
				numVerts = ds->numVerts;
				numIndexes = ds->numIndexes;
			}

			bvBase = surfIdx * 4;
			acBase = surfIdx * 4;

			/* write positions and compute bounds */
			posMin[ 0 ] = posMin[ 1 ] = posMin[ 2 ] = FLT_MAX;
			posMax[ 0 ] = posMax[ 1 ] = posMax[ 2 ] = -FLT_MAX;

			posOffset = bufferOffset;
			posPtr = (float *)( bufferData + bufferOffset );
			for ( v = 0; v < numVerts; v++ )
			{
				vec3_t pos;
				if ( mesh ) {
					dv = &mesh->verts[ v ];
				}
				else {
					dv = &bspDrawVerts[ ds->firstVert + v ];
				}
				VectorAdd( dv->xyz, origin, pos );
				/* Quake Z-up to glTF Y-up */
				gltfCoord[ 0 ] = pos[ 0 ];
				gltfCoord[ 1 ] = pos[ 2 ];
				gltfCoord[ 2 ] = -pos[ 1 ];
				posPtr[ v * 3 + 0 ] = gltfCoord[ 0 ];
				posPtr[ v * 3 + 1 ] = gltfCoord[ 1 ];
				posPtr[ v * 3 + 2 ] = gltfCoord[ 2 ];
				for ( k = 0; k < 3; k++ )
				{
					if ( gltfCoord[ k ] < posMin[ k ] ) {
						posMin[ k ] = gltfCoord[ k ];
					}
					if ( gltfCoord[ k ] > posMax[ k ] ) {
						posMax[ k ] = gltfCoord[ k ];
					}
				}
			}
			bufferOffset += (cgltf_size)numVerts * 12;

			/* write normals */
			normOffset = bufferOffset;
			normPtr = (float *)( bufferData + bufferOffset );
			for ( v = 0; v < numVerts; v++ )
			{
				if ( mesh ) {
					dv = &mesh->verts[ v ];
				}
				else {
					dv = &bspDrawVerts[ ds->firstVert + v ];
				}
				/* Quake Z-up to glTF Y-up */
				normPtr[ v * 3 + 0 ] = dv->normal[ 0 ];
				normPtr[ v * 3 + 1 ] = dv->normal[ 2 ];
				normPtr[ v * 3 + 2 ] = -dv->normal[ 1 ];
			}
			bufferOffset += (cgltf_size)numVerts * 12;

			/* write texcoords */
			uvOffset = bufferOffset;
			uvPtr = (float *)( bufferData + bufferOffset );
			for ( v = 0; v < numVerts; v++ )
			{
				if ( mesh ) {
					dv = &mesh->verts[ v ];
				}
				else {
					dv = &bspDrawVerts[ ds->firstVert + v ];
				}
				uvPtr[ v * 2 + 0 ] = dv->st[ 0 ];
				uvPtr[ v * 2 + 1 ] = dv->st[ 1 ];
			}
			bufferOffset += (cgltf_size)numVerts * 8;

			/* write indices (reverse winding: Q3 BSP uses CW front faces,
			   glTF expects CCW front faces) */
			idxOffset = bufferOffset;
			idxPtr = (unsigned int *)( bufferData + bufferOffset );
			if ( mesh ) {
				/* generate triangle indices from tessellated grid */
				v = 0;
				for ( y = 0; y < mesh->height - 1; y++ )
				{
					for ( x = 0; x < mesh->width - 1; x++ )
					{
						int r = ( x + y ) & 1;
						int pw0 = x + y * mesh->width;
						int pw1 = x + ( y + 1 ) * mesh->width;
						int pw2 = ( x + 1 ) + ( y + 1 ) * mesh->width;
						int pw3 = ( x + 1 ) + y * mesh->width;

						if ( r ) {
							idxPtr[ v++ ] = (unsigned int)pw0;
							idxPtr[ v++ ] = (unsigned int)pw2;
							idxPtr[ v++ ] = (unsigned int)pw1;
							idxPtr[ v++ ] = (unsigned int)pw0;
							idxPtr[ v++ ] = (unsigned int)pw3;
							idxPtr[ v++ ] = (unsigned int)pw2;
						}
						else {
							idxPtr[ v++ ] = (unsigned int)pw0;
							idxPtr[ v++ ] = (unsigned int)pw3;
							idxPtr[ v++ ] = (unsigned int)pw1;
							idxPtr[ v++ ] = (unsigned int)pw3;
							idxPtr[ v++ ] = (unsigned int)pw2;
							idxPtr[ v++ ] = (unsigned int)pw1;
						}
					}
				}
			}
			else {
				for ( v = 0; v < numIndexes; v += 3 )
				{
					idxPtr[ v + 0 ] = (unsigned int)bspDrawIndexes[ ds->firstIndex + v + 0 ];
					idxPtr[ v + 1 ] = (unsigned int)bspDrawIndexes[ ds->firstIndex + v + 2 ];
					idxPtr[ v + 2 ] = (unsigned int)bspDrawIndexes[ ds->firstIndex + v + 1 ];
				}
			}
			bufferOffset += (cgltf_size)numIndexes * 4;


			/* set up buffer views */
			bufferViews[ bvBase + 0 ].buffer = &gltfBuffers[ 0 ];
			bufferViews[ bvBase + 0 ].offset = posOffset;
			bufferViews[ bvBase + 0 ].size = (cgltf_size)numVerts * 12;
			bufferViews[ bvBase + 0 ].type = cgltf_buffer_view_type_vertices;

			bufferViews[ bvBase + 1 ].buffer = &gltfBuffers[ 0 ];
			bufferViews[ bvBase + 1 ].offset = normOffset;
			bufferViews[ bvBase + 1 ].size = (cgltf_size)numVerts * 12;
			bufferViews[ bvBase + 1 ].type = cgltf_buffer_view_type_vertices;

			bufferViews[ bvBase + 2 ].buffer = &gltfBuffers[ 0 ];
			bufferViews[ bvBase + 2 ].offset = uvOffset;
			bufferViews[ bvBase + 2 ].size = (cgltf_size)numVerts * 8;
			bufferViews[ bvBase + 2 ].type = cgltf_buffer_view_type_vertices;

			bufferViews[ bvBase + 3 ].buffer = &gltfBuffers[ 0 ];
			bufferViews[ bvBase + 3 ].offset = idxOffset;
			bufferViews[ bvBase + 3 ].size = (cgltf_size)numIndexes * 4;
			bufferViews[ bvBase + 3 ].type = cgltf_buffer_view_type_indices;


			/* set up accessors */
			gltfAccessors[ acBase + 0 ].buffer_view = &bufferViews[ bvBase + 0 ];
			gltfAccessors[ acBase + 0 ].component_type = cgltf_component_type_r_32f;
			gltfAccessors[ acBase + 0 ].type = cgltf_type_vec3;
			gltfAccessors[ acBase + 0 ].count = numVerts;
			gltfAccessors[ acBase + 0 ].has_min = 1;
			gltfAccessors[ acBase + 0 ].has_max = 1;
			for ( k = 0; k < 3; k++ )
			{
				gltfAccessors[ acBase + 0 ].min[ k ] = posMin[ k ];
				gltfAccessors[ acBase + 0 ].max[ k ] = posMax[ k ];
			}

			gltfAccessors[ acBase + 1 ].buffer_view = &bufferViews[ bvBase + 1 ];
			gltfAccessors[ acBase + 1 ].component_type = cgltf_component_type_r_32f;
			gltfAccessors[ acBase + 1 ].type = cgltf_type_vec3;
			gltfAccessors[ acBase + 1 ].count = numVerts;

			gltfAccessors[ acBase + 2 ].buffer_view = &bufferViews[ bvBase + 2 ];
			gltfAccessors[ acBase + 2 ].component_type = cgltf_component_type_r_32f;
			gltfAccessors[ acBase + 2 ].type = cgltf_type_vec2;
			gltfAccessors[ acBase + 2 ].count = numVerts;

			gltfAccessors[ acBase + 3 ].buffer_view = &bufferViews[ bvBase + 3 ];
			gltfAccessors[ acBase + 3 ].component_type = cgltf_component_type_r_32u;
			gltfAccessors[ acBase + 3 ].type = cgltf_type_scalar;
			gltfAccessors[ acBase + 3 ].count = numIndexes;


			/* set up primitive */
			allPrimitives[ surfIdx ].indices = &gltfAccessors[ acBase + 3 ];
			allPrimitives[ surfIdx ].material = &gltfMaterials[ shaderToMaterial[ ds->shaderNum ] ];

			/* position attribute */
			allAttributes[ surfIdx * 3 + 0 ].name = "POSITION";
			allAttributes[ surfIdx * 3 + 0 ].type = cgltf_attribute_type_position;
			allAttributes[ surfIdx * 3 + 0 ].data = &gltfAccessors[ acBase + 0 ];

			/* normal attribute */
			allAttributes[ surfIdx * 3 + 1 ].name = "NORMAL";
			allAttributes[ surfIdx * 3 + 1 ].type = cgltf_attribute_type_normal;
			allAttributes[ surfIdx * 3 + 1 ].data = &gltfAccessors[ acBase + 1 ];

			/* texcoord attribute */
			allAttributes[ surfIdx * 3 + 2 ].name = "TEXCOORD_0";
			allAttributes[ surfIdx * 3 + 2 ].type = cgltf_attribute_type_texcoord;
			allAttributes[ surfIdx * 3 + 2 ].data = &gltfAccessors[ acBase + 2 ];

			surfIdx++;
			hasSurfaces = 1;
		}

		if ( hasSurfaces ) {
			meshIdx++;
		}
	}


	/* build cgltf_data */
	memset( &data, 0, sizeof( data ) );
	data.asset.version = "2.0";
	data.asset.generator = "Q3Map2 -convert -format gltf";

	data.buffers = gltfBuffers;
	data.buffers_count = 1;
	data.buffer_views = bufferViews;
	data.buffer_views_count = numBufferViews;
	data.accessors = gltfAccessors;
	data.accessors_count = totalSurfaces * 4;
	data.materials = gltfMaterials;
	data.materials_count = numMaterials;
	data.meshes = gltfMeshes;
	data.meshes_count = numMeshes;
	data.nodes = gltfNodes;
	data.nodes_count = numMeshes;
	data.scenes = gltfScenes;
	data.scenes_count = 1;
	data.scene = gltfScenes;

	data.images = gltfImages;
	data.images_count = numImages;
	data.textures = gltfTextures;
	data.textures_count = numImages;
	data.samplers = gltfSamplers;
	data.samplers_count = numImages > 0 ? 1 : 0;

	data.bin = bufferData;
	data.bin_size = bufferSize;

	/* write GLB */
	memset( &options, 0, sizeof( options ) );
	options.type = cgltf_file_type_glb;

	result = cgltf_write_file( &options, name, &data );
	if ( result != cgltf_result_success ) {
		Sys_FPrintf( SYS_WRN, "WARNING: Failed to write %s (error %d)\n", name, (int)result );
	}

	/* free allocations */
	for ( i = 0; i < numPatchMeshes; i++ )
	{
		FreeMesh( patchMeshes[ i ] );
	}
	free( patchMeshes );
	free( bufferData );
	free( gltfBuffers );
	free( bufferViews );
	free( gltfAccessors );
	free( gltfMaterials );
	free( gltfMeshes );
	free( gltfNodes );
	free( nodePointers );
	free( gltfScenes );
	free( allPrimitives );
	free( allAttributes );
	free( shaderToMaterial );
	free( blendTypes );
	free( primitivesPerMesh );
	for ( i = 0; i < numMaterials; i++ )
	{
		free( pngDataArray[ i ] );
	}
	free( pngDataArray );
	free( pngSizeArray );
	free( gltfImages );
	free( gltfTextures );
	free( gltfSamplers );

	/* return to sender */
	return 0;
}
