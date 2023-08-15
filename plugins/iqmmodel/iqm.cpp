/*
   Copyright (C) 2001-2006, William Joseph.
   Copyright (C) 2010-2014 COR Entertainment, LLC.
   All Rights Reserved.

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

#include "iqm.h"

#include "ifilesystem.h"
#include "imodel.h"

#include "imagelib.h"
#include "bytestreamutils.h"

#include "../md3model/model.h"

typedef unsigned char byte;

/*
   ========================================================================

   .IQM triangle model file format

   ========================================================================
 */

typedef struct {
	float s;
	float t;
} iqmSt_t;

void istream_read_iqmSt( PointerInputStream &inputStream, iqmSt_t &st ){
	st.s = istream_read_float32_le( inputStream );
	st.t = istream_read_float32_le( inputStream );
}

typedef struct {
	unsigned int indices[3];
} iqmTriangle_t;

void istream_read_iqmTriangle( PointerInputStream &inputStream, iqmTriangle_t &triangle ){
	triangle.indices[0] = istream_read_int32_le( inputStream );
	triangle.indices[1] = istream_read_int32_le( inputStream );
	triangle.indices[2] = istream_read_int32_le( inputStream );
}

typedef struct {
	float v[3];
} iqmPos_t;

void istream_read_iqmPos( PointerInputStream &inputStream, iqmPos_t &iqmPos ){
	iqmPos.v[0] = istream_read_float32_le( inputStream );
	iqmPos.v[1] = istream_read_float32_le( inputStream );
	iqmPos.v[2] = istream_read_float32_le( inputStream );
}

const int IQM_POSITION = 0;
const int IQM_TEXCOORD = 1;
const int IQM_NORMAL = 2;
const int IQM_TANGENT = 3;
const int IQM_BLENDINDEXES = 4;
const int IQM_BLENDWEIGHTS = 5;
const int IQM_COLOR = 6;
const int IQM_CUSTOM = 0x10;

const int IQM_BYTE = 0;
const int IQM_UBYTE = 1;
const int IQM_SHORT = 2;
const int IQM_USHORT = 3;
const int IQM_INT = 4;
const int IQM_UINT = 5;
const int IQM_HALF = 6;
const int IQM_FLOAT = 7;
const int IQM_DOUBLE = 8;

// animflags
const int IQM_LOOP = 1;

typedef struct iqmHeader_s {
	byte id[16];
	unsigned int version;
	unsigned int filesize;
	unsigned int flags;
	unsigned int num_text, ofs_text;
	unsigned int num_meshes, ofs_meshes;
	unsigned int num_vertexarrays, num_vertexes, ofs_vertexarrays;
	unsigned int num_triangles, ofs_triangles, ofs_neighbors;
	unsigned int num_joints, ofs_joints;
	unsigned int num_poses, ofs_poses;
	unsigned int num_anims, ofs_anims;
	unsigned int num_frames, num_framechannels, ofs_frames, ofs_bounds;
	unsigned int num_comment, ofs_comment;
	unsigned int num_extensions, ofs_extensions;
} iqmHeader_t;

void istream_read_iqmHeader( PointerInputStream &inputStream, iqmHeader_t &header ){
	inputStream.read( header.id, 16 );
#define READINT( x ) header.x = istream_read_int32_le( inputStream );
	READINT( version )
	READINT( filesize )
	READINT( flags )
	READINT( num_text )
	READINT( ofs_text )
	READINT( num_meshes )
	READINT( ofs_meshes )
	READINT( num_vertexarrays )
	READINT( num_vertexes )
	READINT( ofs_vertexarrays )
	READINT( num_triangles )
	READINT( ofs_triangles )
	READINT( ofs_neighbors )
	READINT( num_joints )
	READINT( ofs_joints )
	READINT( num_frames )
	READINT( num_framechannels )
	READINT( ofs_frames )
	READINT( ofs_bounds )
	READINT( num_comment )
	READINT( ofs_comment )
	READINT( num_extensions )
	READINT( ofs_extensions )
#undef READINT
}

typedef struct iqmmesh_s {
	unsigned int name;
	unsigned int material;
	unsigned int first_vertex;
	unsigned int num_vertexes;
	unsigned int first_triangle;
	unsigned int num_triangles;
} iqmmesh_t;

void istream_read_iqmMesh( PointerInputStream &inputStream, iqmmesh_t &iqmmesh ){
#define READUINT( x ) iqmmesh.x = istream_read_uint32_le( inputStream );
	READUINT( name )
	READUINT( material )
	READUINT( first_vertex )
	READUINT( num_vertexes )
	READUINT( first_triangle )
	READUINT( num_triangles )
#undef READUINT
}

typedef struct iqmvertexarray_s {
	unsigned int type;
	unsigned int flags;
	unsigned int format;
	unsigned int size;
	unsigned int offset;
} iqmvertexarray_t;

void istream_read_iqmVertexarray( PointerInputStream &inputStream, iqmvertexarray_t &vertexarray ){
#define READINT( x ) vertexarray.x = istream_read_int32_le( inputStream );
	READINT( type )
	READINT( flags )
	READINT( format )
	READINT( size )
	READINT( offset )
#undef READINT
}

ArbitraryMeshVertex IQMVertex_construct( const iqmPos_t *pos, const iqmPos_t *norm, const iqmSt_t *st ){
	return ArbitraryMeshVertex(
		Vertex3f( pos->v[0], pos->v[1], pos->v[2] ),
		Normal3f( norm->v[0], norm->v[1], norm->v[2] ),
		TexCoord2f( st->s, st->t )
		);
}

void IQMSurface_read( Model &model, const byte *buffer, ArchiveFile &file ){
	iqmHeader_t header;
	{
		PointerInputStream inputStream( buffer );
		istream_read_iqmHeader( inputStream, header );
	}

	int ofs_position = -1, ofs_st = -1, ofs_normal = -1;
	PointerInputStream vaStream( buffer + header.ofs_vertexarrays );
	for ( unsigned int i = 0; i < header.num_vertexarrays; i++ ) {
		iqmvertexarray_t va;
		istream_read_iqmVertexarray( vaStream, va );

		switch ( va.type ) {
		case IQM_POSITION:
			if ( va.format == IQM_FLOAT && va.size == 3 ) {
				ofs_position = va.offset;
			}
			break;
		case IQM_TEXCOORD:
			if ( va.format == IQM_FLOAT && va.size == 2 ) {
				ofs_st = va.offset;
			}
			break;
		case IQM_NORMAL:
			if ( va.format == IQM_FLOAT && va.size == 3 ) {
				ofs_normal = va.offset;
			}
			break;
		}
	}

	PointerInputStream posStream( buffer + ofs_position );
	Array<iqmPos_t> iqmPos( header.num_vertexes );
	for ( Array<iqmPos_t>::iterator i = iqmPos.begin(); i != iqmPos.end(); ++i ) {
		istream_read_iqmPos( posStream, *i );
	}

	PointerInputStream normStream( buffer + ofs_normal );
	Array<iqmPos_t> iqmNorm( header.num_vertexes );
	for ( Array<iqmPos_t>::iterator i = iqmNorm.begin(); i != iqmNorm.end(); ++i ) {
		istream_read_iqmPos( normStream, *i );
	}

	Array<iqmSt_t> iqmSt( header.num_vertexes );
	PointerInputStream stStream( buffer + ofs_st );
	for ( Array<iqmSt_t>::iterator i = iqmSt.begin(); i != iqmSt.end(); ++i ) {
		istream_read_iqmSt( stStream, *i );
	}

	PointerInputStream iqmMesh( buffer + header.ofs_meshes );
	for ( unsigned int m = 0; m < header.num_meshes; m++ ) {
		Surface &surface = model.newSurface();

		iqmmesh_t iqmmesh;
		istream_read_iqmMesh( iqmMesh, iqmmesh );

		bool material_found = false;
		// if not malformed data neither missing string
		if ( iqmmesh.material <= header.num_text && iqmmesh.material > 0 ) {
			char *material;
			material = (char*) buffer + header.ofs_text + iqmmesh.material;

			if ( material[0] != '\0' ) {
				surface.setShader( material );
				material_found = true;
			}
		}

		if ( !material_found ) {
			// empty string will trigger "textures/shader/notex" on display
			surface.setShader( "" );
		}

		UniqueVertexBuffer<ArbitraryMeshVertex> inserter( surface.vertices() );
		inserter.reserve( iqmmesh.num_vertexes );

		surface.indices().reserve( iqmmesh.num_vertexes );

		unsigned int triangle_offset = header.ofs_triangles + iqmmesh.first_triangle * sizeof( iqmTriangle_t );
		PointerInputStream triangleStream( buffer + triangle_offset );
		for ( unsigned int i = 0; i < iqmmesh.num_triangles; ++i ) {
			iqmTriangle_t triangle;
			istream_read_iqmTriangle( triangleStream, triangle );
			for ( int j = 0; j < 3; j++ ) {
				surface.indices().insert( inserter.insert( IQMVertex_construct(
															   &iqmPos[triangle.indices[j]],
															   &iqmNorm[triangle.indices[j]],
															   &iqmSt[triangle.indices[j]] ) ) );
			}
		}

		surface.updateAABB();
	}
}

void IQMModel_read( Model &model, const byte *buffer, ArchiveFile &file ){
	IQMSurface_read( model, buffer, file );
	model.updateAABB();
}

scene::Node &IQMModel_new( const byte *buffer, ArchiveFile &file ){
	ModelNode *modelNode = new ModelNode();
	IQMModel_read( modelNode->model(), buffer, file );
	return modelNode->node();
}

scene::Node &IQMModel_default(){
	ModelNode *modelNode = new ModelNode();
	Model_constructNull( modelNode->model() );
	return modelNode->node();
}

scene::Node &IQMModel_fromBuffer( unsigned char *buffer, ArchiveFile &file ){
	if ( memcmp( buffer, "INTERQUAKEMODEL", 16 ) ) {
		globalErrorStream() << "IQM read error: incorrect ident\n";
		return IQMModel_default();
	}
	else {
		return IQMModel_new( buffer, file );
	}
}

scene::Node &loadIQMModel( ArchiveFile &file ){
	ScopedArchiveBuffer buffer( file );
	return IQMModel_fromBuffer( buffer.buffer, file );
}
