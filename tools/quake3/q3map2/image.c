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
#define IMAGE_C



/* dependencies */
#include "q3map2.h"

#include "webp/decode.h"

/* -------------------------------------------------------------------------------

   this file contains image pool management with reference counting. note: it isn't
   reentrant, so only call it from init/shutdown code or wrap calls in a mutex

   ------------------------------------------------------------------------------- */

/*
   LoadDDSBuffer()
   loads a dxtc (1, 3, 5) dds buffer into a valid rgba image
 */

static void LoadDDSBuffer( byte *buffer, int size, byte **pixels, int *width, int *height ){
	int w, h;
	ddsPF_t pf;


	/* dummy check */
	if ( buffer == NULL || size <= 0 || pixels == NULL || width == NULL || height == NULL ) {
		return;
	}

	/* null out */
	*pixels = 0;
	*width = 0;
	*height = 0;

	/* get dds info */
	if ( DDSGetInfo( (ddsBuffer_t*) buffer, &w, &h, &pf ) ) {
		Sys_FPrintf( SYS_WRN, "WARNING: Invalid DDS texture\n" );
		return;
	}

	/* only certain types of dds textures are supported */
	if ( pf != DDS_PF_ARGB8888 && pf != DDS_PF_DXT1 && pf != DDS_PF_DXT3 && pf != DDS_PF_DXT5 ) {
		Sys_FPrintf( SYS_WRN, "WARNING: Only DDS texture formats ARGB8888, DXT1, DXT3, and DXT5 are supported (%d)\n", pf );
		return;
	}

	/* create image pixel buffer */
	*width = w;
	*height = h;
	*pixels = safe_malloc( w * h * 4 );

	/* decompress the dds texture */
	DDSDecompress( (ddsBuffer_t*) buffer, *pixels );
}

#ifdef BUILD_CRUNCH
/*
    LoadCRNBuffer
    loads a crn image into a valid rgba image
*/
void LoadCRNBuffer( byte *buffer, int size, byte **pixels, int *width, int *height) {
	/* dummy check */
	if ( buffer == NULL || size <= 0 || pixels == NULL || width == NULL || height == NULL ) {
		return;
	}
	if ( !GetCRNImageSize( buffer, size, width, height ) ) {
		Sys_FPrintf( SYS_WRN, "WARNING: Error getting crn imag dimensions.\n");;
		return;
	}
	unsigned int outBufSize = *width * *height * 4;
	*pixels = safe_malloc( outBufSize );
	if ( !ConvertCRNtoRGBA( buffer, size, outBufSize, *pixels) ) {
		Sys_FPrintf( SYS_WRN, "WARNING: Error decoding crn image.\n", 0 );
		return;
	}
}
#endif // BUILD_CRUNCH


/*
   PNGReadData()
   callback function for libpng to read from a memory buffer
   note: this function is a total hack, as it reads/writes the png struct directly!
 */

typedef struct pngBuffer_s
{
	byte    *buffer;
	png_size_t size, offset;
} pngBuffer_t;

void PNGReadData( png_struct *png, png_byte *buffer, png_size_t size ){
	pngBuffer_t     *pb = (pngBuffer_t*) png_get_io_ptr( png );


	if ( ( pb->offset + size ) > pb->size ) {
		size = ( pb->size - pb->offset );
	}
	memcpy( buffer, &pb->buffer[ pb->offset ], size );
	pb->offset += size;
	//%	Sys_Printf( "Copying %d bytes from 0x%08X to 0x%08X (offset: %d of %d)\n", size, &pb->buffer[ pb->offset ], buffer, pb->offset, pb->size );
}



/*
   LoadPNGBuffer()
   loads a png file buffer into a valid rgba image
 */

static void LoadPNGBuffer( byte *buffer, int size, byte **pixels, int *width, int *height ){
	png_struct  *png;
	png_info    *info, *end;
	pngBuffer_t pb;
	int bitDepth, colorType;
	png_uint_32 w, h, i;
	byte        **rowPointers;

	/* dummy check */
	if ( buffer == NULL || size <= 0 || pixels == NULL || width == NULL || height == NULL ) {
		return;
	}

	/* null out */
	*pixels = 0;
	*width = 0;
	*height = 0;

	/* determine if this is a png file */
	if ( png_sig_cmp( buffer, 0, 8 ) != 0 ) {
		Sys_FPrintf( SYS_WRN, "WARNING: Invalid PNG file\n" );
		return;
	}

	/* create png structs */
	png = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if ( png == NULL ) {
		Sys_FPrintf( SYS_WRN, "WARNING: Unable to create PNG read struct\n" );
		return;
	}

	info = png_create_info_struct( png );
	if ( info == NULL ) {
		Sys_FPrintf( SYS_WRN, "WARNING: Unable to create PNG info struct\n" );
		png_destroy_read_struct( &png, NULL, NULL );
		return;
	}

	end = png_create_info_struct( png );
	if ( end == NULL ) {
		Sys_FPrintf( SYS_WRN, "WARNING: Unable to create PNG end info struct\n" );
		png_destroy_read_struct( &png, &info, NULL );
		return;
	}

	/* set read callback */
	pb.buffer = buffer;
	pb.size = size;
	pb.offset = 0;
	png_set_read_fn( png, &pb, PNGReadData );

	/* set error longjmp */
	if ( setjmp( png_jmpbuf(png) ) ) {
		Sys_FPrintf( SYS_WRN, "WARNING: An error occurred reading PNG image\n" );
		png_destroy_read_struct( &png, &info, &end );
		return;
	}

	/* fixme: add proper i/o stuff here */

	/* read png info */
	png_read_info( png, info );

	/* read image header chunk */
	png_get_IHDR( png, info,
				  &w, &h, &bitDepth, &colorType, NULL, NULL, NULL );

	/* the following will probably bork on certain types of png images, but hey... */

	/* force indexed/gray/trans chunk to rgb */
	if ( ( colorType == PNG_COLOR_TYPE_PALETTE && bitDepth <= 8 ) ||
		 ( colorType == PNG_COLOR_TYPE_GRAY && bitDepth <= 8 ) ||
		 png_get_valid( png, info, PNG_INFO_tRNS ) ) {
		png_set_expand( png );
	}

	/* strip 16bpc -> 8bpc */
	if ( bitDepth == 16 ) {
		png_set_strip_16( png );
	}

	/* pad rgb to rgba */
	if ( bitDepth == 8 && colorType == PNG_COLOR_TYPE_RGB ) {
		png_set_filler( png, 255, PNG_FILLER_AFTER );
	}

	/* create image pixel buffer */
	*width = w;
	*height = h;
	/* initialize with zeros, this memory area may not be entirely rewritten */
	*pixels = safe_malloc0( w * h * 4 );

	/* create row pointers */
	rowPointers = safe_malloc( h * sizeof( byte* ) );
	for ( i = 0; i < h; i++ )
		rowPointers[ i ] = *pixels + ( i * w * 4 );

	/* read the png */
	png_read_image( png, rowPointers );

	/* clean up */
	free( rowPointers );
	png_destroy_read_struct( &png, &info, &end );
}



static void LoadWEBPBuffer( byte *buffer, int size, byte **pixels, int *width, int *height ){

	int image_width;
	int image_height;
	
	if ( !WebPGetInfo( buffer, ( size_t) size, &image_width, &image_height ) )
	{
		Sys_FPrintf( SYS_WRN, "WARNING: An error occurred reading WEBP image info\n" );
		return;
	}

	/* create image pixel buffer */
	*pixels = safe_malloc( image_width * image_height * 4 );
	*width = image_width;
	*height = image_height;

	int out_stride = image_width  * 4;
	int out_size =  image_height * out_stride;

		if ( !WebPDecodeRGBAInto( buffer, (size_t) size, *pixels, out_size, out_stride ) )
		{
		Sys_FPrintf( SYS_WRN, "WARNING: An error occurred reading WEBP image\n" );
			return;
		}
}



/*
   ImageInit()
   implicitly called by every function to set up image list
 */

static void ImageInit( void ){
	int i;


	if ( numImages <= 0 ) {
		/* clear images (fixme: this could theoretically leak) */
		memset( images, 0, sizeof( images ) );

		/* generate *bogus image */
		images[ 0 ].name = safe_malloc( strlen( DEFAULT_IMAGE ) + 1 );
		strcpy( images[ 0 ].name, DEFAULT_IMAGE );
		images[ 0 ].filename = safe_malloc( strlen( DEFAULT_IMAGE ) + 1 );
		strcpy( images[ 0 ].filename, DEFAULT_IMAGE );
		images[ 0 ].width = 64;
		images[ 0 ].height = 64;
		images[ 0 ].refCount = 1;
		images[ 0 ].pixels = safe_malloc( 64 * 64 * 4 );
		for ( i = 0; i < ( 64 * 64 * 4 ); i++ )
			images[ 0 ].pixels[ i ] = 255;
	}
}



/*
   ImageFree()
   frees an rgba image
 */

void ImageFree( image_t *image ){
	/* dummy check */
	if ( image == NULL ) {
		return;
	}

	/* decrement refcount */
	image->refCount--;

	/* free? */
	if ( image->refCount <= 0 ) {
		if ( image->name != NULL ) {
			free( image->name );
		}
		image->name = NULL;
		if ( image->filename != NULL ) {
			free( image->filename );
		}
		image->filename = NULL;
		free( image->pixels );
		image->width = 0;
		image->height = 0;
		numImages--;
	}
}



/*
   ImageFind()
   finds an existing rgba image and returns a pointer to the image_t struct or NULL if not found
 */

image_t *ImageFind( const char *filename ){
	int i;
	char name[ 1024 ];


	/* init */
	ImageInit();

	/* dummy check */
	if ( filename == NULL || filename[ 0 ] == '\0' ) {
		return NULL;
	}

	/* strip file extension off name */
	strcpy( name, filename );
	StripExtension( name );

	/* search list */
	for ( i = 0; i < MAX_IMAGES; i++ )
	{
		if ( images[ i ].name != NULL && !strcmp( name, images[ i ].name ) ) {
			return &images[ i ];
		}
	}

	/* no matching image found */
	return NULL;
}



/*
   ImageLoad()
   loads an rgba image and returns a pointer to the image_t struct or NULL if not found
 */

image_t *ImageLoad( const char *filename ){
	int i;
	image_t     *image;
	char name[ 1024 ];
	int size;
	byte        *buffer = NULL;
	qboolean alphaHack = qfalse;


	/* init */
	ImageInit();

	/* dummy check */
	if ( filename == NULL || filename[ 0 ] == '\0' ) {
		return NULL;
	}

	/* try with extension then without extension
	so light.blend will be unfortunately first
	tried as light.tga like it always did, but
	then, light.blend.tga will be tried as well */
	for ( i = 0; i < 2; i++ )
	{
		strcpy( name, filename );

		if ( i == 0 )
		{
			/* strip file extension off name */
			StripExtension( name );
		}

		/* try to find existing image */
		image = ImageFind( name );
		if ( image != NULL ) {
			image->refCount++;
			return image;
		}

		/* none found, so find first non-null image */
		image = NULL;
		for ( i = 0; i < MAX_IMAGES; i++ )
		{
			if ( images[ i ].name == NULL ) {
				image = &images[ i ];
				break;
			}
		}

		/* too many images? */
		if ( image == NULL ) {
			Error( "MAX_IMAGES (%d) exceeded, there are too many image files referenced by the map.", MAX_IMAGES );
		}

		/* set it up */
		image->name = safe_malloc( strlen( name ) + 1 );
		strcpy( image->name, name );

		/* add a dummy extension that can be removed */
		strcat( name, ".tga" );

		/* Sys_FPrintf( SYS_WRN, "==> Looking for %s\n", name ); */

		/* attempt to load tga */
		StripExtension( name );
		strcat( name, ".tga" );
		size = vfsLoadFile( (const char*) name, (void**) &buffer, 0 );
		if ( size > 0 ) {
			LoadTGABuffer( buffer, buffer + size, &image->pixels, &image->width, &image->height );
			break;
		}

		/* attempt to load png */
		StripExtension( name );
		strcat( name, ".png" );
		size = vfsLoadFile( (const char*) name, (void**) &buffer, 0 );
		if ( size > 0 ) {
			LoadPNGBuffer( buffer, size, &image->pixels, &image->width, &image->height );
			break;
		}

		/* attempt to load jpg */
		StripExtension( name );
		strcat( name, ".jpg" );
		size = vfsLoadFile( (const char*) name, (void**) &buffer, 0 );
		if ( size > 0 ) {
			if ( LoadJPGBuff( buffer, size, &image->pixels, &image->width, &image->height ) == -1 && image->pixels != NULL ) {
				// On error, LoadJPGBuff might store a pointer to the error message in image->pixels
				Sys_FPrintf( SYS_WRN, "WARNING: LoadJPGBuff: %s\n", (unsigned char*) image->pixels );
			}
			alphaHack = qtrue;
			break;
		}

		/* attempt to load dds */
		StripExtension( name );
		strcat( name, ".dds" );
		size = vfsLoadFile( (const char*) name, (void**) &buffer, 0 );

		/* also look for .dds image in dds/ prefix like Doom3 or DarkPlaces */
		if ( size <= 0 ) {
			char ddsname[ 1024 ];
			strcpy( ddsname, "dds/" );
			strcat( ddsname, image->name );
			StripExtension( ddsname );
			strcat( ddsname, ".dds" );
			size = vfsLoadFile( (const char*) ddsname, (void**) &buffer, 0 );
		}

		if ( size > 0 ) {
			LoadDDSBuffer( buffer, size, &image->pixels, &image->width, &image->height );
			break;
		}

		/* attempt to load ktx */
		StripExtension( name );
		strcat( name, ".ktx" );
		size = vfsLoadFile( (const char*) name, (void**) &buffer, 0 );
		if ( size > 0 ) {
			LoadKTXBufferFirstImage( buffer, size, &image->pixels, &image->width, &image->height );
			break;
		}

		#ifdef BUILD_CRUNCH
		/* attempt to load crn */
		StripExtension( name );
		strcat( name, ".crn" );
		size = vfsLoadFile( (const char*) name, (void**) &buffer, 0 );
		if ( size > 0 ) {
			LoadCRNBuffer( buffer, size, &image->pixels, &image->width, &image->height );
			break;
		}
		#endif // BUILD_CRUNCH

		/* attempt to load webp */
		StripExtension( name );
		strcat( name, ".webp" );
		size = vfsLoadFile( (const char*) name, (void**) &buffer, 0 );
		if ( size > 0 ) {
			LoadWEBPBuffer( buffer, size, &image->pixels, &image->width, &image->height );
			break;
		}
	}

	/* free file buffer */
	free( buffer );

	/* make sure everything's kosher */
	if ( size <= 0 || image->width <= 0 || image->height <= 0 || image->pixels == NULL ) {
		// Sys_FPrintf( SYS_WRN, "WARNING: Image not found: \"%s\"\n", filename );
		//%	Sys_Printf( "size = %d  width = %d  height = %d  pixels = 0x%08x (%s)\n",
		//%		size, image->width, image->height, image->pixels, name );
		free( image->name );
		image->name = NULL;
		return NULL;
	}

	/* tell user which image file is found for the given texture path */
	Sys_FPrintf( SYS_VRB, "Loaded image: \"%s\"\n", name );

	/* set filename */
	image->filename = safe_malloc( strlen( name ) + 1 );
	strcpy( image->filename, name );

	/* set count */
	image->refCount = 1;
	numImages++;

	if ( alphaHack ) {
		StripExtension( name );
		strcat( name, "_alpha.jpg" );
		size = vfsLoadFile( (const char*) name, (void**) &buffer, 0 );
		if ( size > 0 ) {
			unsigned char *pixels;
			int width, height;
			if ( LoadJPGBuff( buffer, size, &pixels, &width, &height ) == -1 ) {
				if (pixels) {
					// On error, LoadJPGBuff might store a pointer to the error message in pixels
					Sys_FPrintf( SYS_WRN, "WARNING: LoadJPGBuff %s %s\n", name, (unsigned char*) pixels );
				}
			} else {
				if ( width == image->width && height == image->height ) {
					int i;
					for ( i = 0; i < width * height; ++i )
						image->pixels[4 * i + 3] = pixels[4 * i + 2];  // copy alpha from blue channel
				}
				free( pixels );
			}
			free( buffer );
		}
	}

	/* return the image */
	return image;
}
