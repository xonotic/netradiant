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

#include "plugin.h"

#include "iscenegraph.h"
#include "irender.h"
#include "iselection.h"
#include "iimage.h"
#include "imodel.h"
#include "igl.h"
#include "ifilesystem.h"
#include "iundo.h"
#include "ifiletypes.h"
#include "iscriplib.h"

#include "modulesystem/singletonmodule.h"
#include "typesystem.h"

#include "iqm.h"


class IQMModelLoader : public ModelLoader {
public:
scene::Node &loadModel( ArchiveFile &file ){
	return loadIQMModel( file );
}
};

class ModelDependencies :
	public GlobalFileSystemModuleRef,
	public GlobalOpenGLModuleRef,
	public GlobalUndoModuleRef,
	public GlobalSceneGraphModuleRef,
	public GlobalShaderCacheModuleRef,
	public GlobalSelectionModuleRef,
	public GlobalFiletypesModuleRef {
};

class ModelIQMAPI : public TypeSystemRef {
IQMModelLoader m_modeliqm;
public:
typedef ModelLoader Type;

STRING_CONSTANT( Name, "iqm" );

ModelIQMAPI(){
	GlobalFiletypesModule::getTable().addType( Type::Name(), Name(), filetype_t( "InterQuake Models", "*.iqm" ) );
}

ModelLoader *getTable(){
	return &m_modeliqm;
}
};

typedef SingletonModule<ModelIQMAPI, ModelDependencies> ModelIQMModule;

ModelIQMModule g_ModelIQMModule;

extern "C" void RADIANT_DLLEXPORT Radiant_RegisterModules( ModuleServer &server ){
	initialiseModule( server );

	g_ModelIQMModule.selfRegister();
}
