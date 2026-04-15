/* -----------------------------------------------------------------------------

   PicoModel Library - glTF 2.0 support via cgltf

   Supports both .gltf (JSON) and .glb (binary) files.

   Uses cgltf single-header library: https://github.com/jkuhlmann/cgltf

   ----------------------------------------------------------------------------- */

/* dependencies */
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include "picointernal.h"

/* _gltf_canload:
 *  validates a glTF/GLB model file.
 */
static int _gltf_canload( PM_PARAMS_CANLOAD ){
	/* check data length */
	if ( bufSize < 12 ) {
		return PICO_PMV_ERROR_SIZE;
	}

	/* check file extension */
	if ( _pico_stristr( fileName, ".glb" ) != NULL ) {
		/* binary glTF - validate magic bytes */
		const unsigned char *b = (const unsigned char *)buffer;
		if ( b[0] == 'g' && b[1] == 'l' && b[2] == 'T' && b[3] == 'F' ) {
			return PICO_PMV_OK;
		}
		return PICO_PMV_ERROR_IDENT;
	}

	if ( _pico_stristr( fileName, ".gltf" ) != NULL ) {
		return PICO_PMV_OK;
	}

	return PICO_PMV_ERROR;
}

/* cgltf file read callback that uses picomodel's VFS */
static cgltf_result _gltf_file_read( const struct cgltf_memory_options *memory_options,
		const struct cgltf_file_options *file_options,
		const char *path, cgltf_size *size, void **data ){
	unsigned char *buffer = NULL;
	int bufSize = 0;

	(void)memory_options;
	(void)file_options;

	_pico_load_file( path, &buffer, &bufSize );
	if ( buffer == NULL || bufSize < 0 ) {
		return cgltf_result_file_not_found;
	}

	*data = buffer;
	*size = (cgltf_size)bufSize;
	return cgltf_result_success;
}

/* cgltf file release callback */
static void _gltf_file_release( const struct cgltf_memory_options *memory_options,
		const struct cgltf_file_options *file_options,
		void *data ){
	(void)memory_options;
	(void)file_options;

	if ( data != NULL ) {
		_pico_free_file( data );
	}
}

/* _gltf_extract_image:
 * Extracts an embedded glTF image (data URI or buffer_view) to a file
 * next to the model. Returns a pointer to an allocated string with the
 * VFS-relative path on success, or NULL on failure.
 * The caller must free the returned string with _pico_free().
 */
static char *_gltf_extract_image( const cgltf_options *options,
		const char *modelFileName, const cgltf_image *img, int imageIndex ){
	const unsigned char *imageData = NULL;
	cgltf_size imageSize = 0;
	void *decodedData = NULL;
	const char *ext = ".png";
	char outPath[1024];
	char modelDir[1024];
	char baseName[256];
	const char *p;

	/* determine file extension from mime type */
	if ( img->mime_type != NULL ) {
		if ( _pico_stristr( img->mime_type, "jpeg" ) != NULL ||
			 _pico_stristr( img->mime_type, "jpg" ) != NULL ) {
			ext = ".jpg";
		}
	}

	/* try to get image data from data URI */
	if ( img->uri != NULL && strncmp( img->uri, "data:", 5 ) == 0 ) {
		const char *comma = strchr( img->uri, ',' );
		if ( comma != NULL && comma - img->uri >= 7 &&
			 strncmp( comma - 7, ";base64", 7 ) == 0 ) {
			/* detect extension from data URI mime type (e.g. data:image/jpeg;base64) */
			if ( _pico_stristr( img->uri, "image/jpeg" ) != NULL ) {
				ext = ".jpg";
			}
			const char *base64Start = comma + 1;
			cgltf_size encodedLen = strlen( base64Start );
			/* calculate exact decoded size accounting for padding */
			int padding = 0;
			if ( encodedLen > 0 && base64Start[encodedLen - 1] == '=' ) {
				padding++;
				if ( encodedLen > 1 && base64Start[encodedLen - 2] == '=' ) {
					padding++;
				}
			}
			cgltf_size decodedSize = ( encodedLen / 4 ) * 3 - padding;

			if ( decodedSize > 0 &&
				 cgltf_load_buffer_base64( options, decodedSize, base64Start, &decodedData ) == cgltf_result_success ) {
				imageData = (const unsigned char *)decodedData;
				imageSize = decodedSize;

				/* detect actual format from magic bytes */
				if ( imageSize >= 8 && imageData[0] == 0x89 && imageData[1] == 'P' &&
					 imageData[2] == 'N' && imageData[3] == 'G' ) {
					ext = ".png";
				}
				else if ( imageSize >= 2 && imageData[0] == 0xFF && imageData[1] == 0xD8 ) {
					ext = ".jpg";
				}
			}
		}
	}

	/* try to get image data from buffer_view (GLB embedded) */
	if ( imageData == NULL && img->buffer_view != NULL ) {
		if ( img->buffer_view->buffer != NULL &&
			 img->buffer_view->buffer->data != NULL ) {
			imageData = (const unsigned char *)img->buffer_view->buffer->data +
						img->buffer_view->offset;
			imageSize = img->buffer_view->size;
		}
	}

	if ( imageData == NULL || imageSize == 0 ) {
		if ( decodedData != NULL ) {
			_pico_free( decodedData );
		}
		return NULL;
	}

	/* build output path: same directory as model, with image name or index */
	_pico_nofname( modelFileName, modelDir, sizeof( modelDir ) );

	if ( img->name != NULL && img->name[0] != '\0' ) {
		snprintf( baseName, sizeof( baseName ), "%s", img->name );
		/* strip any existing extension */
		_pico_setfext( baseName, NULL );
	}
	else {
		/* extract model base name for unique naming */
		p = _pico_nopath( modelFileName );
		snprintf( baseName, sizeof( baseName ), "%s", p );
		_pico_setfext( baseName, NULL );
		/* append image index */
		{
			char suffix[32];
			snprintf( suffix, sizeof( suffix ), "_img%d", imageIndex );
			if ( strlen( baseName ) + strlen( suffix ) < sizeof( baseName ) ) {
				strcat( baseName, suffix );
			}
		}
	}

	snprintf( outPath, sizeof( outPath ), "%s%s%s", modelDir, baseName, ext );

	/* write the file via the save callback */
	if ( _pico_save_file( outPath, imageData, (int)imageSize ) ) {
		char *result_path;
		if ( decodedData != NULL ) {
			_pico_free( decodedData );
		}
		result_path = _pico_clone_alloc( outPath );
		return result_path;
	}

	if ( decodedData != NULL ) {
		_pico_free( decodedData );
	}
	return NULL;
}

/* _gltf_load:
 *  loads a glTF/GLB model file.
 */
static picoModel_t *_gltf_load( PM_PARAMS_LOAD ){
	cgltf_options options;
	cgltf_data *data = NULL;
	cgltf_result result;
	picoModel_t *model;
	cgltf_size mi, pi, ni;
	int surfNum = 0;

	/* initialize cgltf options with VFS callbacks */
	memset( &options, 0, sizeof( options ) );
	options.file.read = _gltf_file_read;
	options.file.release = _gltf_file_release;

	/* parse the glTF/GLB data from the buffer picomodel already loaded */
	result = cgltf_parse( &options, buffer, (cgltf_size)bufSize, &data );
	if ( result != cgltf_result_success ) {
		_pico_printf( PICO_ERROR, "glTF: failed to parse '%s' (error %d)", fileName, (int)result );
		return NULL;
	}

	/* load external buffer data (.bin files for .gltf, or embedded in .glb) */
	result = cgltf_load_buffers( &options, data, fileName );
	if ( result != cgltf_result_success ) {
		_pico_printf( PICO_ERROR, "glTF: failed to load buffers for '%s' (error %d)", fileName, (int)result );
		cgltf_free( data );
		return NULL;
	}

	/* validate the parsed data */
	result = cgltf_validate( data );
	if ( result != cgltf_result_success ) {
		_pico_printf( PICO_WARNING, "glTF: validation warnings for '%s' (error %d)", fileName, (int)result );
		/* continue anyway - many files have minor validation issues */
	}

	/* create a new pico model */
	model = PicoNewModel();
	if ( model == NULL ) {
		_pico_printf( PICO_ERROR, "glTF: failed to allocate model" );
		cgltf_free( data );
		return NULL;
	}

	/* set model name */
	PicoSetModelName( model, fileName );
	PicoSetModelFileName( model, fileName );

	/* iterate all nodes to process meshes with their transforms */
	for ( ni = 0; ni < data->nodes_count; ni++ )
	{
		cgltf_node *node = &data->nodes[ni];
		cgltf_mesh *mesh;
		float worldMatrix[16];

		if ( node->mesh == NULL ) {
			continue;
		}

		mesh = node->mesh;

		/* compute the world transform for this node */
		cgltf_node_transform_world( node, worldMatrix );

		/* iterate primitives in this mesh */
		for ( pi = 0; pi < mesh->primitives_count; pi++ )
		{
			cgltf_primitive *prim = &mesh->primitives[pi];
			const cgltf_accessor *posAccessor = NULL;
			const cgltf_accessor *normAccessor = NULL;
			const cgltf_accessor *texAccessor = NULL;
			const cgltf_accessor *colorAccessor = NULL;
			picoSurface_t *surface;
			picoShader_t *shader;
			cgltf_size v, idx;
			int hasTransform;

			/* only handle triangle primitives */
			if ( prim->type != cgltf_primitive_type_triangles ) {
				continue;
			}

			/* find vertex attributes */
			for ( mi = 0; mi < prim->attributes_count; mi++ )
			{
				cgltf_attribute *attr = &prim->attributes[mi];
				switch ( attr->type )
				{
				case cgltf_attribute_type_position:
					posAccessor = attr->data;
					break;
				case cgltf_attribute_type_normal:
					normAccessor = attr->data;
					break;
				case cgltf_attribute_type_texcoord:
					if ( attr->index == 0 ) {
						texAccessor = attr->data;
					}
					break;
				case cgltf_attribute_type_color:
					if ( attr->index == 0 ) {
						colorAccessor = attr->data;
					}
					break;
				default:
					break;
				}
			}

			/* position is required */
			if ( posAccessor == NULL ) {
				_pico_printf( PICO_WARNING, "glTF: skipping primitive without POSITION attribute in '%s'", fileName );
				continue;
			}

			/* create a new surface */
			surface = PicoNewSurface( model );
			if ( surface == NULL ) {
				_pico_printf( PICO_ERROR, "glTF: failed to allocate surface" );
				PicoFreeModel( model );
				cgltf_free( data );
				return NULL;
			}

			PicoSetSurfaceType( surface, PICO_TRIANGLES );

			/* set surface name */
			if ( mesh->name != NULL ) {
				char surfName[128];
				if ( mesh->primitives_count > 1 ) {
					snprintf( surfName, sizeof( surfName ), "%s_%d", mesh->name, (int)pi );
				}
				else {
					snprintf( surfName, sizeof( surfName ), "%s", mesh->name );
				}
				PicoSetSurfaceName( surface, surfName );
			}
			else {
				char surfName[64];
				snprintf( surfName, sizeof( surfName ), "surface_%d", surfNum );
				PicoSetSurfaceName( surface, surfName );
			}

			/* set up shader/material */
			if ( prim->material != NULL && prim->material->name != NULL ) {
				shader = PicoNewShader( model );
				if ( shader != NULL ) {
					PicoSetShaderName( shader, prim->material->name );

					/* try to get the base color texture name for the shader path */
					if ( prim->material->has_pbr_metallic_roughness &&
						 prim->material->pbr_metallic_roughness.base_color_texture.texture != NULL &&
						 prim->material->pbr_metallic_roughness.base_color_texture.texture->image != NULL ) {
						cgltf_image *img = prim->material->pbr_metallic_roughness.base_color_texture.texture->image;
						if ( img->uri != NULL && strncmp( img->uri, "data:", 5 ) != 0 ) {
							PicoSetShaderMapName( shader, img->uri );
						}
						else {
							/* try to extract embedded image to disk */
							int imgIdx = (int)( img - data->images );
							char *extractedPath = _gltf_extract_image( &options, fileName, img, imgIdx );
							if ( extractedPath != NULL ) {
								PicoSetShaderMapName( shader, extractedPath );
								_pico_free( extractedPath );
							}
							else if ( img->name != NULL ) {
								PicoSetShaderMapName( shader, img->name );
							}
						}
					}

					PicoSetSurfaceShader( surface, shader );
				}
			}

			/* check if the node has a non-identity transform */
			hasTransform = node->has_translation || node->has_rotation ||
						   node->has_scale || node->has_matrix;

			/* read vertices */
			for ( v = 0; v < posAccessor->count; v++ )
			{
				picoVec3_t xyz;
				cgltf_float pos[3] = { 0, 0, 0 };

				cgltf_accessor_read_float( posAccessor, v, pos, 3 );

				/* glTF is right-handed Y-up, Quake is right-handed Z-up */
				/* transform: X stays, Y becomes Z, Z becomes -Y */
				if ( hasTransform ) {
					/* apply node transform first, then coordinate system conversion */
					cgltf_float transformed[3];
					transformed[0] = worldMatrix[0] * pos[0] + worldMatrix[4] * pos[1] + worldMatrix[8] * pos[2] + worldMatrix[12];
					transformed[1] = worldMatrix[1] * pos[0] + worldMatrix[5] * pos[1] + worldMatrix[9] * pos[2] + worldMatrix[13];
					transformed[2] = worldMatrix[2] * pos[0] + worldMatrix[6] * pos[1] + worldMatrix[10] * pos[2] + worldMatrix[14];
					xyz[0] = transformed[0];
					xyz[1] = -transformed[2];
					xyz[2] = transformed[1];
				}
				else {
					xyz[0] = pos[0];
					xyz[1] = -pos[2];
					xyz[2] = pos[1];
				}

				PicoSetSurfaceXYZ( surface, (int)v, xyz );

				/* normals */
				if ( normAccessor != NULL && v < normAccessor->count ) {
					picoVec3_t normal;
					cgltf_float n[3] = { 0, 0, 1 };

					cgltf_accessor_read_float( normAccessor, v, n, 3 );

					if ( hasTransform ) {
						cgltf_float tn[3];
						/* transform normal by the 3x3 rotation part of the world matrix */
						tn[0] = worldMatrix[0] * n[0] + worldMatrix[4] * n[1] + worldMatrix[8] * n[2];
						tn[1] = worldMatrix[1] * n[0] + worldMatrix[5] * n[1] + worldMatrix[9] * n[2];
						tn[2] = worldMatrix[2] * n[0] + worldMatrix[6] * n[1] + worldMatrix[10] * n[2];
						normal[0] = tn[0];
						normal[1] = -tn[2];
						normal[2] = tn[1];
					}
					else {
						normal[0] = n[0];
						normal[1] = -n[2];
						normal[2] = n[1];
					}

					PicoSetSurfaceNormal( surface, (int)v, normal );
				}

				/* texture coordinates */
				if ( texAccessor != NULL && v < texAccessor->count ) {
					picoVec2_t st;
					cgltf_float uv[2] = { 0, 0 };

					cgltf_accessor_read_float( texAccessor, v, uv, 2 );

					st[0] = uv[0];
					st[1] = uv[1];
					PicoSetSurfaceST( surface, 0, (int)v, st );
				}

				/* vertex colors */
				if ( colorAccessor != NULL && v < colorAccessor->count ) {
					picoColor_t color;
					cgltf_float rgba[4] = { 1, 1, 1, 1 };
					int numComponents;

					numComponents = (int)cgltf_num_components( colorAccessor->type );
					cgltf_accessor_read_float( colorAccessor, v, rgba, numComponents < 4 ? numComponents : 4 );

					color[0] = (picoByte_t)( rgba[0] * 255.0f + 0.5f );
					color[1] = (picoByte_t)( rgba[1] * 255.0f + 0.5f );
					color[2] = (picoByte_t)( rgba[2] * 255.0f + 0.5f );
					color[3] = (picoByte_t)( rgba[3] * 255.0f + 0.5f );
					PicoSetSurfaceColor( surface, 0, (int)v, color );
				}
				else {
					picoColor_t color;
					_pico_set_color( color, 255, 255, 255, 255 );
					PicoSetSurfaceColor( surface, 0, (int)v, color );
				}
			}

			/* read indices - reverse winding order because the Y-up to Z-up
			   coordinate conversion flips handedness */
			if ( prim->indices != NULL ) {
				for ( idx = 0; idx + 2 < prim->indices->count; idx += 3 )
				{
					cgltf_size i0 = cgltf_accessor_read_index( prim->indices, idx );
					cgltf_size i1 = cgltf_accessor_read_index( prim->indices, idx + 1 );
					cgltf_size i2 = cgltf_accessor_read_index( prim->indices, idx + 2 );
					PicoSetSurfaceIndex( surface, (int)idx, (picoIndex_t)i0 );
					PicoSetSurfaceIndex( surface, (int)idx + 1, (picoIndex_t)i2 );
					PicoSetSurfaceIndex( surface, (int)idx + 2, (picoIndex_t)i1 );
				}
			}
			else {
				/* non-indexed geometry: generate sequential indices with reversed winding */
				for ( idx = 0; idx + 2 < posAccessor->count; idx += 3 )
				{
					PicoSetSurfaceIndex( surface, (int)idx, (picoIndex_t)idx );
					PicoSetSurfaceIndex( surface, (int)idx + 1, (picoIndex_t)( idx + 2 ) );
					PicoSetSurfaceIndex( surface, (int)idx + 2, (picoIndex_t)( idx + 1 ) );
				}
			}

			surfNum++;
		}
	}

	/* if no nodes reference meshes (orphaned meshes), load them without transforms */
	if ( surfNum == 0 ) {
		for ( mi = 0; mi < data->meshes_count; mi++ )
		{
			cgltf_mesh *mesh = &data->meshes[mi];

			for ( pi = 0; pi < mesh->primitives_count; pi++ )
			{
				cgltf_primitive *prim = &mesh->primitives[pi];
				const cgltf_accessor *posAccessor = NULL;
				const cgltf_accessor *normAccessor = NULL;
				const cgltf_accessor *texAccessor = NULL;
				const cgltf_accessor *colorAccessor = NULL;
				picoSurface_t *surface;
				picoShader_t *shader;
				cgltf_size v, idx, ai;

				if ( prim->type != cgltf_primitive_type_triangles ) {
					continue;
				}

				for ( ai = 0; ai < prim->attributes_count; ai++ )
				{
					cgltf_attribute *attr = &prim->attributes[ai];
					switch ( attr->type )
					{
					case cgltf_attribute_type_position:
						posAccessor = attr->data;
						break;
					case cgltf_attribute_type_normal:
						normAccessor = attr->data;
						break;
					case cgltf_attribute_type_texcoord:
						if ( attr->index == 0 ) {
							texAccessor = attr->data;
						}
						break;
					case cgltf_attribute_type_color:
						if ( attr->index == 0 ) {
							colorAccessor = attr->data;
						}
						break;
					default:
						break;
					}
				}

				if ( posAccessor == NULL ) {
					continue;
				}

				surface = PicoNewSurface( model );
				if ( surface == NULL ) {
					PicoFreeModel( model );
					cgltf_free( data );
					return NULL;
				}

				PicoSetSurfaceType( surface, PICO_TRIANGLES );

				if ( mesh->name != NULL ) {
					char surfName[128];
					if ( mesh->primitives_count > 1 ) {
						snprintf( surfName, sizeof( surfName ), "%s_%d", mesh->name, (int)pi );
					}
					else {
						snprintf( surfName, sizeof( surfName ), "%s", mesh->name );
					}
					PicoSetSurfaceName( surface, surfName );
				}
				else {
					char surfName[64];
					snprintf( surfName, sizeof( surfName ), "surface_%d", surfNum );
					PicoSetSurfaceName( surface, surfName );
				}

				if ( prim->material != NULL && prim->material->name != NULL ) {
					shader = PicoNewShader( model );
					if ( shader != NULL ) {
						PicoSetShaderName( shader, prim->material->name );

						if ( prim->material->has_pbr_metallic_roughness &&
							 prim->material->pbr_metallic_roughness.base_color_texture.texture != NULL &&
							 prim->material->pbr_metallic_roughness.base_color_texture.texture->image != NULL ) {
							cgltf_image *img = prim->material->pbr_metallic_roughness.base_color_texture.texture->image;
							if ( img->uri != NULL && strncmp( img->uri, "data:", 5 ) != 0 ) {
								PicoSetShaderMapName( shader, img->uri );
							}
							else {
								/* try to extract embedded image to disk */
								int imgIdx = (int)( img - data->images );
								char *extractedPath = _gltf_extract_image( &options, fileName, img, imgIdx );
								if ( extractedPath != NULL ) {
									PicoSetShaderMapName( shader, extractedPath );
									_pico_free( extractedPath );
								}
								else if ( img->name != NULL ) {
									PicoSetShaderMapName( shader, img->name );
								}
							}
						}

						PicoSetSurfaceShader( surface, shader );
					}
				}

				for ( v = 0; v < posAccessor->count; v++ )
				{
					picoVec3_t xyz;
					cgltf_float pos[3] = { 0, 0, 0 };

					cgltf_accessor_read_float( posAccessor, v, pos, 3 );

					/* Y-up to Z-up conversion */
					xyz[0] = pos[0];
					xyz[1] = -pos[2];
					xyz[2] = pos[1];
					PicoSetSurfaceXYZ( surface, (int)v, xyz );

					if ( normAccessor != NULL && v < normAccessor->count ) {
						picoVec3_t normal;
						cgltf_float n[3] = { 0, 0, 1 };
						cgltf_accessor_read_float( normAccessor, v, n, 3 );

						normal[0] = n[0];
						normal[1] = -n[2];
						normal[2] = n[1];
						PicoSetSurfaceNormal( surface, (int)v, normal );
					}

					if ( texAccessor != NULL && v < texAccessor->count ) {
						picoVec2_t st;
						cgltf_float uv[2] = { 0, 0 };
						cgltf_accessor_read_float( texAccessor, v, uv, 2 );
						st[0] = uv[0];
						st[1] = uv[1];
						PicoSetSurfaceST( surface, 0, (int)v, st );
					}

					if ( colorAccessor != NULL && v < colorAccessor->count ) {
						picoColor_t color;
						cgltf_float rgba[4] = { 1, 1, 1, 1 };
						int numComponents = (int)cgltf_num_components( colorAccessor->type );
						cgltf_accessor_read_float( colorAccessor, v, rgba, numComponents < 4 ? numComponents : 4 );
						color[0] = (picoByte_t)( rgba[0] * 255.0f + 0.5f );
						color[1] = (picoByte_t)( rgba[1] * 255.0f + 0.5f );
						color[2] = (picoByte_t)( rgba[2] * 255.0f + 0.5f );
						color[3] = (picoByte_t)( rgba[3] * 255.0f + 0.5f );
						PicoSetSurfaceColor( surface, 0, (int)v, color );
					}
					else {
						picoColor_t color;
						_pico_set_color( color, 255, 255, 255, 255 );
						PicoSetSurfaceColor( surface, 0, (int)v, color );
					}
				}

				/* reverse winding order because the Y-up to Z-up
				   coordinate conversion flips handedness */
				if ( prim->indices != NULL ) {
					for ( idx = 0; idx + 2 < prim->indices->count; idx += 3 )
					{
						cgltf_size i0 = cgltf_accessor_read_index( prim->indices, idx );
						cgltf_size i1 = cgltf_accessor_read_index( prim->indices, idx + 1 );
						cgltf_size i2 = cgltf_accessor_read_index( prim->indices, idx + 2 );
						PicoSetSurfaceIndex( surface, (int)idx, (picoIndex_t)i0 );
						PicoSetSurfaceIndex( surface, (int)idx + 1, (picoIndex_t)i2 );
						PicoSetSurfaceIndex( surface, (int)idx + 2, (picoIndex_t)i1 );
					}
				}
				else {
					for ( idx = 0; idx + 2 < posAccessor->count; idx += 3 )
					{
						PicoSetSurfaceIndex( surface, (int)idx, (picoIndex_t)idx );
						PicoSetSurfaceIndex( surface, (int)idx + 1, (picoIndex_t)( idx + 2 ) );
						PicoSetSurfaceIndex( surface, (int)idx + 2, (picoIndex_t)( idx + 1 ) );
					}
				}

				surfNum++;
			}
		}
	}

	if ( surfNum == 0 ) {
		_pico_printf( PICO_WARNING, "glTF: no triangle surfaces found in '%s'", fileName );
	}

	cgltf_free( data );
	return model;
}

/* pico file format module definition */
const picoModule_t picoModuleGLTF =
{
	"1.0",                      /* module version string */
	"glTF 2.0",                 /* module display name */
	"cgltf/picomodel",          /* author's name */
	"2026 Martin Gerhardy",   /* module copyright */
	{
		"gltf","glb",NULL,NULL  /* default extensions to use */
	},
	_gltf_canload,              /* validation routine */
	_gltf_load,                 /* load routine */
	NULL,                       /* save validation routine */
	NULL                        /* save routine */
};
