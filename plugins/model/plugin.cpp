/*
   Copyright (C) 2001-2006, William Joseph.
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

#include <stdio.h>
#include "picomodel.h"
typedef unsigned char byte;
#include <stdlib.h>
#include <algorithm>
#include <list>

#include "iscenegraph.h"
#include "irender.h"
#include "iselection.h"
#include "iimage.h"
#include "imodel.h"
#include "igl.h"
#include "ifilesystem.h"
#include "iundo.h"
#include "ifiletypes.h"

#include "modulesystem/singletonmodule.h"
#include "stream/textstream.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "typesystem.h"
#include "cmdlib.h"

#include "model.h"

void PicoPrintFunc( int level, const char *str ){
	if ( str == 0 ) {
		return;
	}
	switch ( level )
	{
	case PICO_NORMAL:
		globalOutputStream() << str << "\n";
		break;

	case PICO_VERBOSE:
		//globalOutputStream() << "PICO_VERBOSE: " << str << "\n";
		break;

	case PICO_WARNING:
		globalErrorStream() << "PICO_WARNING: " << str << "\n";
		break;

	case PICO_ERROR:
		globalErrorStream() << "PICO_ERROR: " << str << "\n";
		break;

	case PICO_FATAL:
		globalErrorStream() << "PICO_FATAL: " << str << "\n";
		break;
	}
}

void PicoLoadFileFunc( const char *name, byte **buffer, int *bufSize ){
	*bufSize = vfsLoadFile( name, (void**) buffer );
}

void PicoFreeFileFunc( void* file ){
	vfsFreeFile( file );
}

static CopiedString s_modelSaveRoot;

void PicoSetModelSaveRoot( const char *modelVfsPath ){
	if ( modelVfsPath != NULL ) {
		const char *root = GlobalFileSystem().findFile( modelVfsPath );
		if ( root != NULL && root[0] != '\0' ) {
			s_modelSaveRoot = root;
			return;
		}
	}
	s_modelSaveRoot = "";
}

int PicoSaveFileFunc( const char *name, const unsigned char *buffer, int bufSize ){
	/* resolve VFS-relative name to absolute filesystem path */
	const char *root = NULL;

	/* prefer the root where the current model was loaded from */
	if ( !string_empty( s_modelSaveRoot.c_str() ) ) {
		root = s_modelSaveRoot.c_str();
	}

	if ( root == NULL || root[0] == '\0' ) {
		root = GlobalFileSystem().findFile( name );
	}

	if ( root == NULL || root[0] == '\0' ) {
		root = GlobalFileSystem().findFile( "" );
		if ( root == NULL || root[0] == '\0' ) {
			return 0;
		}
	}

	/* construct full path */
	StringOutputStream fullPath( 256 );
	fullPath << root << name;

	FILE *f = fopen( fullPath.c_str(), "rb" );
	if ( f != NULL ) {
		/* file already exists, skip writing */
		fclose( f );
		return 1;
	}

	/* create intermediate directories if needed */
	{
		StringOutputStream dirPath( 256 );
		dirPath << fullPath.c_str();
		char *dir = const_cast<char*>( dirPath.c_str() );
		for ( char *p = dir + 1; *p != '\0'; ++p ) {
			if ( *p == '/' ) {
				*p = '\0';
				Q_mkdir( dir );
				*p = '/';
			}
		}
	}

	f = fopen( fullPath.c_str(), "wb" );
	if ( f == NULL ) {
		globalErrorStream() << "Failed to save file: " << fullPath.c_str() << "\n";
		return 0;
	}
	fwrite( buffer, 1, bufSize, f );
	fclose( f );
	globalOutputStream() << "Save file: " << fullPath.c_str() << "\n";
	return 1;
}

void pico_initialise(){
	PicoInit();
	PicoSetMallocFunc( malloc );
	PicoSetFreeFunc( free );
	PicoSetPrintFunc( PicoPrintFunc );
	PicoSetLoadFileFunc( PicoLoadFileFunc );
	PicoSetFreeFileFunc( PicoFreeFileFunc );
	PicoSetSaveFileFunc( PicoSaveFileFunc );
}


class PicoModelLoader : public ModelLoader
{
const picoModule_t* m_module;
public:
PicoModelLoader( const picoModule_t* module ) : m_module( module ){
}
scene::Node& loadModel( ArchiveFile& file ){
	return loadPicoModel( m_module, file );
}
};

class ModelPicoDependencies :
	public GlobalFileSystemModuleRef,
	public GlobalOpenGLModuleRef,
	public GlobalUndoModuleRef,
	public GlobalSceneGraphModuleRef,
	public GlobalShaderCacheModuleRef,
	public GlobalSelectionModuleRef,
	public GlobalFiletypesModuleRef
{
};

class ModelPicoAPI : public TypeSystemRef
{
PicoModelLoader m_modelLoader;
public:
typedef ModelLoader Type;

ModelPicoAPI( const char* extension, const picoModule_t* module ) :
	m_modelLoader( module ){
	StringOutputStream filter( 128 );
	filter << "*." << extension;
	GlobalFiletypesModule::getTable().addType( Type::Name(), extension, filetype_t( module->displayName, filter.c_str() ) );
}
ModelLoader* getTable(){
	return &m_modelLoader;
}
};

class PicoModelAPIConstructor
{
CopiedString m_extension;
const picoModule_t* m_module;
public:
PicoModelAPIConstructor( const char* extension, const picoModule_t* module ) :
	m_extension( extension ), m_module( module ){
}
const char* getName(){
	return m_extension.c_str();
}
ModelPicoAPI* constructAPI( ModelPicoDependencies& dependencies ){
	return new ModelPicoAPI( m_extension.c_str(), m_module );
}
void destroyAPI( ModelPicoAPI* api ){
	delete api;
}
};


typedef SingletonModule<ModelPicoAPI, ModelPicoDependencies, PicoModelAPIConstructor> PicoModelModule;
typedef std::list<PicoModelModule> PicoModelModules;
PicoModelModules g_PicoModelModules;


extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer& server ){
	initialiseModule( server );

	pico_initialise();

	const picoModule_t** modules = PicoModuleList( 0 );
	while ( *modules != 0 )
	{
		const picoModule_t* module = *modules++;
		if ( module->canload && module->load ) {
			for ( char*const* ext = module->defaultExts; *ext != 0; ++ext )
			{
				g_PicoModelModules.push_back( PicoModelModule( PicoModelAPIConstructor( *ext, module ) ) );
				g_PicoModelModules.back().selfRegister();
			}
		}
	}
}
