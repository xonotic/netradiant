/*
   Copyright (C) 1999-2006 Id Software, Inc. and contributors.
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
 */

#include "patchmanip.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "debugging/debugging.h"


#include "iselection.h"
#include "ipatch.h"

#include "math/vector.h"
#include "math/aabb.h"
#include "generic/callback.h"

#include "gtkutil/menu.h"
#include "gtkutil/image.h"
#include "map.h"
#include "mainframe.h"
#include "commands.h"
#include "gtkmisc.h"
#include "gtkdlgs.h"
#include "texwindow.h"
#include "xywindow.h"
#include "select.h"
#include "patch.h"
#include "grid.h"

PatchCreator* g_patchCreator = 0;

void Scene_PatchConstructPrefab( scene::Graph& graph, const AABB aabb, const char* shader, EPatchPrefab eType, int axis, std::size_t width = 3, std::size_t height = 3 ){
	Select_Delete();
	GlobalSelectionSystem().setSelectedAll( false );

	NodeSmartReference node( g_patchCreator->createPatch() );
	Node_getTraversable( Map_FindOrInsertWorldspawn( g_map ) )->insert( node );

	Patch* patch = Node_getPatch( node );
	patch->SetShader( shader );

	patch->ConstructPrefab( aabb, eType, axis, width, height );
	patch->controlPointsChanged();

	{
		scene::Path patchpath( makeReference( GlobalSceneGraph().root() ) );
		patchpath.push( makeReference( *Map_GetWorldspawn( g_map ) ) );
		patchpath.push( makeReference( node.get() ) );
		Instance_getSelectable( *graph.find( patchpath ) )->setSelected( true );
	}
}

void PatchAutoCapTexture( Patch& patch ) {

	AABB box = patch.localAABB();
	float x = box.extents.x();
	float y = box.extents.y();
	float z = box.extents.z();

	int cap_direction = -1;
	if ( x < y  && x < z )
		cap_direction = 0;
	else if ( y < x  && y < z )
		cap_direction = 1;
	else if ( z < x  && z < x )
		cap_direction = 2;

	if ( cap_direction >= 0 )
		patch.ProjectTexture(cap_direction);
	else
		patch.NaturalTexture();
}

void Patch_AutoCapTexture(){
	UndoableCommand command( "patchAutoCapTexture" );
	Scene_forEachVisibleSelectedPatch( &PatchAutoCapTexture );
	SceneChangeNotify();
}

void Patch_makeCaps( Patch& patch, scene::Instance& instance, EPatchCap type, const char* shader ){
	if ( ( type == eCapEndCap || type == eCapIEndCap )
		 && patch.getWidth() != 5 ) {
		globalErrorStream() << "cannot create end-cap - patch width != 5\n";
		return;
	}
	if ( ( type == eCapBevel || type == eCapIBevel )
		 && patch.getWidth() != 3 && patch.getWidth() != 5 ) {
		globalErrorStream() << "cannot create bevel-cap - patch width != 3\n";
		return;
	}
	if ( type == eCapCylinder
		 && patch.getWidth() != 9 ) {
		globalErrorStream() << "cannot create cylinder-cap - patch width != 9\n";
		return;
	}

	{
		NodeSmartReference cap( g_patchCreator->createPatch() );
		Node_getTraversable( instance.path().parent() )->insert( cap );

		Patch* cap_patch = Node_getPatch( cap );
		patch.MakeCap( cap_patch, type, ROW, true );
		cap_patch->SetShader( shader );
		PatchAutoCapTexture(*cap_patch);

		scene::Path path( instance.path() );
		path.pop();
		path.push( makeReference( cap.get() ) );
		selectPath( path, true );
	}

	{
		NodeSmartReference cap( g_patchCreator->createPatch() );
		Node_getTraversable( instance.path().parent() )->insert( cap );

		Patch* cap_patch = Node_getPatch( cap );
		patch.MakeCap( cap_patch, type, ROW, false );
		cap_patch->SetShader( shader );
		PatchAutoCapTexture(*cap_patch);

		scene::Path path( instance.path() );
		path.pop();
		path.push( makeReference( cap.get() ) );
		selectPath( path, true );
	}
}


typedef std::vector<scene::Instance*> InstanceVector;

enum ECapDialog {
	PATCHCAP_BEVEL = 0,
	PATCHCAP_ENDCAP,
	PATCHCAP_INVERTED_BEVEL,
	PATCHCAP_INVERTED_ENDCAP,
	PATCHCAP_CYLINDER
};

EMessageBoxReturn DoCapDlg( ECapDialog *type );

void Scene_PatchDoCap_Selected( scene::Graph& graph, const char* shader ){
	ECapDialog nType;

	if ( DoCapDlg( &nType ) == eIDOK ) {
		EPatchCap eType;
		switch ( nType )
		{
		case PATCHCAP_INVERTED_BEVEL:
			eType = eCapIBevel;
			break;
		case PATCHCAP_BEVEL:
			eType = eCapBevel;
			break;
		case PATCHCAP_INVERTED_ENDCAP:
			eType = eCapIEndCap;
			break;
		case PATCHCAP_ENDCAP:
			eType = eCapEndCap;
			break;
		case PATCHCAP_CYLINDER:
			eType = eCapCylinder;
			break;
		default:
			ERROR_MESSAGE( "invalid patch cap type" );
			return;
		}

		InstanceVector instances;
		Scene_forEachVisibleSelectedPatchInstance([&](PatchInstance &patch) {
			instances.push_back(&patch);
		});
		for ( InstanceVector::const_iterator i = instances.begin(); i != instances.end(); ++i )
		{
			Patch_makeCaps( *Node_getPatch( ( *i )->path().top() ), *( *i ), eType, shader );
		}
	}
}

void Patch_deform( Patch& patch, scene::Instance& instance, const int deform ){
	patch.undoSave();

	for (PatchControlIter i = patch.begin(); i != patch.end(); ++i)
	{
		PatchControl& control = *i;
		int randomNumber = int( deform * (float(std::rand()) / float(RAND_MAX)));
		control.m_vertex[2] += randomNumber;
	}

	patch.controlPointsChanged();
}

void Scene_PatchDeform( scene::Graph& graph, const int deform )
{
	InstanceVector instances;
	Scene_forEachVisibleSelectedPatchInstance([&](PatchInstance &patch) {
			instances.push_back(&patch);
	});
	for ( InstanceVector::const_iterator i = instances.begin(); i != instances.end(); ++i )
	{
		Patch_deform( *Node_getPatch( ( *i )->path().top() ), *( *i ), deform );
	}

}

void Patch_thicken( Patch& patch, scene::Instance& instance, const float thickness, bool seams, const int axis ){

		// Create a new patch node
		NodeSmartReference node(g_patchCreator->createPatch());
		// Insert the node into worldspawn
		Node_getTraversable(Map_FindOrInsertWorldspawn(g_map))->insert(node);

		// Retrieve the contained patch from the node
		Patch* targetPatch = Node_getPatch(node);

		// Create the opposite patch with the given thickness = distance
		bool no12 = true;
		bool no34 = true;
		targetPatch->createThickenedOpposite(patch, thickness, axis, no12, no34);

		// Now select the newly created patches
		{
			scene::Path patchpath(makeReference(GlobalSceneGraph().root()));
			patchpath.push(makeReference(*Map_GetWorldspawn(g_map)));
			patchpath.push(makeReference(node.get()));
			Instance_getSelectable(*GlobalSceneGraph().find(patchpath))->setSelected(true);
		}

		if (seams && thickness != 0.0f) {
			int i = 0;
			if ( no12 ){
				i = 2;
			}
			int iend = 4;
			if ( no34 ){
				iend = 2;
			}
			// Now create the four walls
			for ( ; i < iend; i++ ) {
				// Allocate new patch
				NodeSmartReference node = NodeSmartReference(g_patchCreator->createPatch());
				// Insert each node into worldspawn
				Node_getTraversable(Map_FindOrInsertWorldspawn(g_map))->insert(node);

				// Retrieve the contained patch from the node
				Patch* wallPatch = Node_getPatch(node);

				// Create the wall patch by passing i as wallIndex
				wallPatch->createThickenedWall( patch, *targetPatch, i);

				if( ( wallPatch->localAABB().extents[0] <= 0.00005 && wallPatch->localAABB().extents[1] <= 0.00005 ) ||
					( wallPatch->localAABB().extents[1] <= 0.00005 && wallPatch->localAABB().extents[2] <= 0.00005 ) ||
					( wallPatch->localAABB().extents[0] <= 0.00005 && wallPatch->localAABB().extents[2] <= 0.00005 ) ){
					//globalOutputStream() << "Thicken: Discarding degenerate patch.\n";
					Node_getTraversable( Map_FindOrInsertWorldspawn(g_map) )->erase( node );
				}
				else
				// Now select the newly created patches
				{
					scene::Path patchpath(makeReference(GlobalSceneGraph().root()));
					patchpath.push(makeReference(*Map_GetWorldspawn(g_map)));
					patchpath.push(makeReference(node.get()));
					Instance_getSelectable(*GlobalSceneGraph().find(patchpath))->setSelected(true);
				}
			}
		}

		// Invert the target patch so that it faces the opposite direction
		targetPatch->InvertMatrix();
}

void Scene_PatchThicken( scene::Graph& graph, const int thickness, bool seams, const int axis )
{
	InstanceVector instances;
	Scene_forEachVisibleSelectedPatchInstance([&](PatchInstance &patch) {
		instances.push_back(&patch);
	});
	for ( InstanceVector::const_iterator i = instances.begin(); i != instances.end(); ++i )
	{
		Patch_thicken( *Node_getPatch( ( *i )->path().top() ), *( *i ), thickness, seams, axis );
	}

}

Patch* Scene_GetUltimateSelectedVisiblePatch(){
	if ( GlobalSelectionSystem().countSelected() != 0 ) {
		scene::Node& node = GlobalSelectionSystem().ultimateSelected().path().top();
		if ( node.visible() ) {
			return Node_getPatch( node );
		}
	}
	return 0;
}


void Scene_PatchCapTexture_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		patch.ProjectTexture(Patch::m_CycleCapIndex);
	});
	Patch::m_CycleCapIndex = ( Patch::m_CycleCapIndex == 0 ) ? 1 : ( Patch::m_CycleCapIndex == 1 ) ? 2 : 0;
	SceneChangeNotify();
}

void Scene_PatchFlipTexture_Selected( scene::Graph& graph, int axis ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		patch.FlipTexture(axis);
	});
}

void Scene_PatchNaturalTexture_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		patch.NaturalTexture();
	});
	SceneChangeNotify();
}


void Scene_PatchInsertRemove_Selected( scene::Graph& graph, bool bInsert, bool bColumn, bool bFirst ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		patch.InsertRemove(bInsert, bColumn, bFirst);
	});
}

void Scene_PatchInvert_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		patch.InvertMatrix();
	});
}

void Scene_PatchRedisperse_Selected( scene::Graph& graph, EMatrixMajor major ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		patch.Redisperse(major);
	});
}

void Scene_PatchSmooth_Selected( scene::Graph& graph, EMatrixMajor major ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		patch.Smooth(major);
	});
}

void Scene_PatchTranspose_Selected( scene::Graph& graph ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		patch.TransposeMatrix();
	});
}

void Scene_PatchSetShader_Selected( scene::Graph& graph, const char* name ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		patch.SetShader(name);
	});
	SceneChangeNotify();
}

void Scene_PatchGetShader_Selected( scene::Graph& graph, CopiedString& name ){
	Patch* patch = Scene_GetUltimateSelectedVisiblePatch();
	if ( patch != 0 ) {
		name = patch->GetShader();
	}
}

void Scene_PatchSelectByShader( scene::Graph& graph, const char* name ){
	Scene_forEachVisiblePatchInstance([&](PatchInstance &patch) {
		if (shader_equal(patch.getPatch().GetShader(), name)) {
			patch.setSelected(true);
		}
	});
}


void Scene_PatchFindReplaceShader( scene::Graph& graph, const char* find, const char* replace ){
	Scene_forEachVisiblePatch([&](Patch &patch) {
		if (shader_equal(patch.GetShader(), find)) {
			patch.SetShader(replace);
		}
	});
}

void Scene_PatchFindReplaceShader_Selected( scene::Graph& graph, const char* find, const char* replace ){
	Scene_forEachVisibleSelectedPatch([&](Patch &patch) {
		if (shader_equal(patch.GetShader(), find)) {
			patch.SetShader(replace);
		}
	});
}


AABB PatchCreator_getBounds(){
	AABB aabb( aabb_for_minmax( Select_getWorkZone().d_work_min, Select_getWorkZone().d_work_max ) );

	float gridSize = GetGridSize();

	if ( aabb.extents[0] == 0 ) {
		aabb.extents[0] = gridSize;
	}
	if ( aabb.extents[1] == 0 ) {
		aabb.extents[1] = gridSize;
	}
	if ( aabb.extents[2] == 0 ) {
		aabb.extents[2] = gridSize;
	}

	if ( aabb_valid( aabb ) ) {
		return aabb;
	}
	return AABB( Vector3( 0, 0, 0 ), Vector3( 64, 64, 64 ) );
}

void DoNewPatchDlg( EPatchPrefab prefab, int minrows, int mincols, int defrows, int defcols, int maxrows, int maxcols );

void Patch_XactCylinder(){
	UndoableCommand undo( "patchCreateXactCylinder" );

	DoNewPatchDlg( eXactCylinder, 3, 7, 3, 13, 0, 0 );
}

void Patch_XactSphere(){
	UndoableCommand undo( "patchCreateXactSphere" );

	DoNewPatchDlg( eXactSphere, 5, 7, 7, 13, 0, 0 );
}

void Patch_XactCone(){
	UndoableCommand undo( "patchCreateXactCone" );

	DoNewPatchDlg( eXactCone, 3, 7, 3, 13, 0, 0 );
}

void Patch_Cylinder(){
	UndoableCommand undo( "patchCreateCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ), eCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_DenseCylinder(){
	UndoableCommand undo( "patchCreateDenseCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ), eDenseCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_VeryDenseCylinder(){
	UndoableCommand undo( "patchCreateVeryDenseCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ), eVeryDenseCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_SquareCylinder(){
	UndoableCommand undo( "patchCreateSquareCylinder" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ), eSqCylinder, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Endcap(){
	UndoableCommand undo( "patchCreateCaps" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ), eEndCap, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Bevel(){
	UndoableCommand undo( "patchCreateBevel" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ), eBevel, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Sphere(){
	UndoableCommand undo( "patchCreateSphere" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ), eSphere, GlobalXYWnd_getCurrentViewType() );
}

void Patch_SquareBevel(){
}

void Patch_SquareEndcap(){
}

void Patch_Cone(){
	UndoableCommand undo( "patchCreateCone" );

	Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ), eCone, GlobalXYWnd_getCurrentViewType() );
}

void Patch_Plane(){
	UndoableCommand undo( "patchCreatePlane" );

	DoNewPatchDlg( ePlane, 3, 3, 3, 3, 0, 0 );
}

void Patch_InsertInsertColumn(){
	UndoableCommand undo( "patchInsertColumns" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), true, true, false );
}

void Patch_InsertAddColumn(){
	UndoableCommand undo( "patchAddColumns" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), true, true, true );
}

void Patch_InsertInsertRow(){
	UndoableCommand undo( "patchInsertRows" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), true, false, false );
}

void Patch_InsertAddRow(){
	UndoableCommand undo( "patchAddRows" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), true, false, true );
}

void Patch_DeleteFirstColumn(){
	UndoableCommand undo( "patchDeleteFirstColumns" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), false, true, true );
}

void Patch_DeleteLastColumn(){
	UndoableCommand undo( "patchDeleteLastColumns" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), false, true, false );
}

void Patch_DeleteFirstRow(){
	UndoableCommand undo( "patchDeleteFirstRows" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), false, false, true );
}

void Patch_DeleteLastRow(){
	UndoableCommand undo( "patchDeleteLastRows" );

	Scene_PatchInsertRemove_Selected( GlobalSceneGraph(), false, false, false );
}

void Patch_Invert(){
	UndoableCommand undo( "patchInvert" );

	Scene_PatchInvert_Selected( GlobalSceneGraph() );
}

void Patch_RedisperseRows(){
	UndoableCommand undo( "patchRedisperseRows" );

	Scene_PatchRedisperse_Selected( GlobalSceneGraph(), ROW );
}

void Patch_RedisperseCols(){
	UndoableCommand undo( "patchRedisperseColumns" );

	Scene_PatchRedisperse_Selected( GlobalSceneGraph(), COL );
}

void Patch_SmoothRows(){
	UndoableCommand undo( "patchSmoothRows" );

	Scene_PatchSmooth_Selected( GlobalSceneGraph(), ROW );
}

void Patch_SmoothCols(){
	UndoableCommand undo( "patchSmoothColumns" );

	Scene_PatchSmooth_Selected( GlobalSceneGraph(), COL );
}

void Patch_Transpose(){
	UndoableCommand undo( "patchTranspose" );

	Scene_PatchTranspose_Selected( GlobalSceneGraph() );
}

void Patch_Cap(){
	// FIXME: add support for patch cap creation
	// Patch_CapCurrent();
	UndoableCommand undo( "patchCreateCaps" );

	Scene_PatchDoCap_Selected( GlobalSceneGraph(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ) );
}

void Patch_CycleProjection(){
	UndoableCommand undo( "patchCycleUVProjectionAxis" );

	Scene_PatchCapTexture_Selected( GlobalSceneGraph() );
}

///\todo Unfinished.
void Patch_OverlayOn(){
}

///\todo Unfinished.
void Patch_OverlayOff(){
}

void Patch_FlipTextureX(){
	UndoableCommand undo( "patchFlipTextureU" );

	Scene_PatchFlipTexture_Selected( GlobalSceneGraph(), 0 );
}

void Patch_FlipTextureY(){
	UndoableCommand undo( "patchFlipTextureV" );

	Scene_PatchFlipTexture_Selected( GlobalSceneGraph(), 1 );
}

void Patch_NaturalTexture(){
	UndoableCommand undo( "patchNaturalTexture" );

	Scene_PatchNaturalTexture_Selected( GlobalSceneGraph() );
}

void Patch_CapTexture(){
	UndoableCommand command( "patchCapTexture" );

	Scene_PatchCapTexture_Selected( GlobalSceneGraph() );
}

void Patch_ResetTexture(){
	float fx, fy;
	if ( DoTextureLayout( &fx, &fy ) == eIDOK ) {
		UndoableCommand command( "patchTileTexture" );
		Scene_PatchTileTexture_Selected( GlobalSceneGraph(), fx, fy );
	}
}

void Patch_FitTexture(){
	UndoableCommand command( "patchFitTexture" );

	Scene_PatchTileTexture_Selected( GlobalSceneGraph(), 1, 1 );
}

void DoPatchDeformDlg();

void Patch_Deform(){
	UndoableCommand undo( "patchDeform" );

	DoPatchDeformDlg();
}

void DoPatchThickenDlg();

void Patch_Thicken(){
	UndoableCommand undo( "patchThicken" );

	DoPatchThickenDlg();
}
#include "ifilter.h"


class filter_patch_all : public PatchFilter
{
public:
bool filter( const Patch& patch ) const {
	return true;
}
};

class filter_patch_shader : public PatchFilter
{
const char* m_shader;
public:
filter_patch_shader( const char* shader ) : m_shader( shader ){
}
bool filter( const Patch& patch ) const {
	return shader_equal( patch.GetShader(), m_shader );
}
};

class filter_patch_flags : public PatchFilter
{
int m_flags;
public:
filter_patch_flags( int flags ) : m_flags( flags ){
}
bool filter( const Patch& patch ) const {
	return ( patch.getShaderFlags() & m_flags ) != 0;
}
};


filter_patch_all g_filter_patch_all;
filter_patch_shader g_filter_patch_clip( "textures/common/clip" );
filter_patch_shader g_filter_patch_weapclip( "textures/common/weapclip" );
filter_patch_flags g_filter_patch_translucent( QER_TRANS );

void PatchFilters_construct(){
	add_patch_filter( g_filter_patch_all, EXCLUDE_CURVES );
	add_patch_filter( g_filter_patch_clip, EXCLUDE_CLIP );
	add_patch_filter( g_filter_patch_weapclip, EXCLUDE_CLIP );
	add_patch_filter( g_filter_patch_translucent, EXCLUDE_TRANSLUCENT );
}


#include "preferences.h"

void Patch_constructPreferences( PreferencesPage& page ){
	page.appendEntry( "Patch Subdivide Threshold", g_PatchSubdivideThreshold );
}
void Patch_constructPage( PreferenceGroup& group ){
	PreferencesPage page( group.createPage( "Patches", "Patch Display Preferences" ) );
	Patch_constructPreferences( page );
}
void Patch_registerPreferencesPage(){
	PreferencesDialog_addDisplayPage( makeCallbackF(Patch_constructPage) );
}


#include "preferencesystem.h"

void PatchPreferences_construct(){
	GlobalPreferenceSystem().registerPreference( "Subdivisions", make_property_string( g_PatchSubdivideThreshold ) );
}


#include "generic/callback.h"

void Patch_registerCommands(){
	GlobalCommands_insert( "InvertCurveTextureX", makeCallbackF(Patch_FlipTextureX), Accelerator( 'I', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "InvertCurveTextureY", makeCallbackF(Patch_FlipTextureY), Accelerator( 'I', (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalCommands_insert( "IncPatchColumn", makeCallbackF(Patch_InsertInsertColumn), Accelerator( GDK_KP_Add, (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "IncPatchRow", makeCallbackF(Patch_InsertInsertRow), Accelerator( GDK_KP_Add, (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "DecPatchColumn", makeCallbackF(Patch_DeleteLastColumn), Accelerator( GDK_KP_Subtract, (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "DecPatchRow", makeCallbackF(Patch_DeleteLastRow), Accelerator( GDK_KP_Subtract, (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "NaturalizePatch", makeCallbackF(Patch_NaturalTexture), Accelerator( 'N', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "PatchCylinder", makeCallbackF(Patch_Cylinder) );
	GlobalCommands_insert( "PatchDenseCylinder", makeCallbackF(Patch_DenseCylinder) );
	GlobalCommands_insert( "PatchVeryDenseCylinder", makeCallbackF(Patch_VeryDenseCylinder) );
	GlobalCommands_insert( "PatchSquareCylinder", makeCallbackF(Patch_SquareCylinder) );
	GlobalCommands_insert( "PatchXactCylinder", makeCallbackF(Patch_XactCylinder) );
	GlobalCommands_insert( "PatchXactSphere", makeCallbackF(Patch_XactSphere) );
	GlobalCommands_insert( "PatchXactCone", makeCallbackF(Patch_XactCone) );
	GlobalCommands_insert( "PatchEndCap", makeCallbackF(Patch_Endcap) );
	GlobalCommands_insert( "PatchBevel", makeCallbackF(Patch_Bevel) );
	GlobalCommands_insert( "PatchSquareBevel", makeCallbackF(Patch_SquareBevel) );
	GlobalCommands_insert( "PatchSquareEndcap", makeCallbackF(Patch_SquareEndcap) );
	GlobalCommands_insert( "PatchCone", makeCallbackF(Patch_Cone) );
	GlobalCommands_insert( "PatchSphere", makeCallbackF(Patch_Sphere) );
	GlobalCommands_insert( "SimplePatchMesh", makeCallbackF(Patch_Plane), Accelerator( 'P', (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalCommands_insert( "PatchInsertInsertColumn", makeCallbackF(Patch_InsertInsertColumn), Accelerator( GDK_KEY_KP_Add, (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "PatchInsertAddColumn", makeCallbackF(Patch_InsertAddColumn) );
	GlobalCommands_insert( "PatchInsertInsertRow", makeCallbackF(Patch_InsertInsertRow), Accelerator( GDK_KEY_KP_Add, (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "PatchInsertAddRow", makeCallbackF(Patch_InsertAddRow) );
	GlobalCommands_insert( "PatchDeleteFirstColumn", makeCallbackF(Patch_DeleteFirstColumn) );
	GlobalCommands_insert( "PatchDeleteLastColumn", makeCallbackF(Patch_DeleteLastColumn), Accelerator( GDK_KEY_KP_Subtract, (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "PatchDeleteFirstRow", makeCallbackF(Patch_DeleteFirstRow), Accelerator( GDK_KEY_KP_Subtract, (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "PatchDeleteLastRow", makeCallbackF(Patch_DeleteLastRow) );
	GlobalCommands_insert( "InvertCurve", makeCallbackF(Patch_Invert), Accelerator( 'I', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "RedisperseRows", makeCallbackF(Patch_RedisperseRows), Accelerator( 'E', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "RedisperseCols", makeCallbackF(Patch_RedisperseCols), Accelerator( 'E', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "SmoothRows", makeCallbackF(Patch_SmoothRows), Accelerator( 'W', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "SmoothCols", makeCallbackF(Patch_SmoothCols), Accelerator( 'W', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "MatrixTranspose", makeCallbackF(Patch_Transpose), Accelerator( 'M', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "CapCurrentCurve", makeCallbackF(Patch_Cap), Accelerator( 'C', (GdkModifierType)GDK_SHIFT_MASK ) );
	GlobalCommands_insert( "CycleCapTexturePatch", makeCallbackF(Patch_CycleProjection), Accelerator( 'N', (GdkModifierType)( GDK_SHIFT_MASK | GDK_CONTROL_MASK ) ) );
	GlobalCommands_insert( "MakeOverlayPatch", makeCallbackF(Patch_OverlayOn), Accelerator( 'Y' ) );
	GlobalCommands_insert( "ClearPatchOverlays", makeCallbackF(Patch_OverlayOff), Accelerator( 'L', (GdkModifierType)GDK_CONTROL_MASK ) );
	GlobalCommands_insert( "PatchDeform", makeCallbackF(Patch_Deform) );
	GlobalCommands_insert( "PatchThicken", makeCallbackF(Patch_Thicken) );
}

void Patch_constructToolbar( ui::Toolbar toolbar ){
	toolbar_append_button( toolbar, "Put caps on the current patch (SHIFT + C)", "cap_curve.png", "CapCurrentCurve" );
}

void Patch_constructMenu( ui::Menu menu ){
	create_menu_item_with_mnemonic( menu, "Cylinder", "PatchCylinder" );
	{
		auto menu_in_menu = create_sub_menu_with_mnemonic( menu, "More Cylinders" );
		if ( g_Layout_enableDetachableMenus.m_value ) {
			menu_tearoff( menu_in_menu );
		}
		create_menu_item_with_mnemonic( menu_in_menu, "Dense Cylinder", "PatchDenseCylinder" );
		create_menu_item_with_mnemonic( menu_in_menu, "Very Dense Cylinder", "PatchVeryDenseCylinder" );
		create_menu_item_with_mnemonic( menu_in_menu, "Square Cylinder", "PatchSquareCylinder" );
		create_menu_item_with_mnemonic( menu_in_menu, "Exact Cylinder...", "PatchXactCylinder" );
	}
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "End cap", "PatchEndCap" );
	create_menu_item_with_mnemonic( menu, "Bevel", "PatchBevel" );
	{
//		auto menu_in_menu = create_sub_menu_with_mnemonic( menu, "More End caps, Bevels" );
//		if ( g_Layout_enableDetachableMenus.m_value ) {
//			menu_tearoff( menu_in_menu );
//		}
		create_menu_item_with_mnemonic( menu, "Square Endcap", "PatchSquareBevel" );
		create_menu_item_with_mnemonic( menu, "Square Bevel", "PatchSquareEndcap" );
	}
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "Cone", "PatchCone" );
	create_menu_item_with_mnemonic( menu, "Exact Cone...", "PatchXactCone" );
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "Sphere", "PatchSphere" );
	create_menu_item_with_mnemonic( menu, "Exact Sphere...", "PatchXactSphere" );
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "Simple Patch Mesh...", "SimplePatchMesh" );
	menu_separator( menu );
	{
		auto menu_in_menu = create_sub_menu_with_mnemonic( menu, "Insert" );
		if ( g_Layout_enableDetachableMenus.m_value ) {
			menu_tearoff( menu_in_menu );
		}
		create_menu_item_with_mnemonic( menu_in_menu, "Insert (2) Columns", "PatchInsertInsertColumn" );
		create_menu_item_with_mnemonic( menu_in_menu, "Add (2) Columns", "PatchInsertAddColumn" );
		menu_separator( menu_in_menu );
		create_menu_item_with_mnemonic( menu_in_menu, "Insert (2) Rows", "PatchInsertInsertRow" );
		create_menu_item_with_mnemonic( menu_in_menu, "Add (2) Rows", "PatchInsertAddRow" );
	}
	{
		auto menu_in_menu = create_sub_menu_with_mnemonic( menu, "Delete" );
		if ( g_Layout_enableDetachableMenus.m_value ) {
			menu_tearoff( menu_in_menu );
		}
		create_menu_item_with_mnemonic( menu_in_menu, "First (2) Columns", "PatchDeleteFirstColumn" );
		create_menu_item_with_mnemonic( menu_in_menu, "Last (2) Columns", "PatchDeleteLastColumn" );
		menu_separator( menu_in_menu );
		create_menu_item_with_mnemonic( menu_in_menu, "First (2) Rows", "PatchDeleteFirstRow" );
		create_menu_item_with_mnemonic( menu_in_menu, "Last (2) Rows", "PatchDeleteLastRow" );
	}
	menu_separator( menu );
	{
		auto menu_in_menu = create_sub_menu_with_mnemonic( menu, "Matrix" );
		if ( g_Layout_enableDetachableMenus.m_value ) {
			menu_tearoff( menu_in_menu );
		}
		create_menu_item_with_mnemonic( menu_in_menu, "Invert", "InvertCurve" );
//		auto menu_3 = create_sub_menu_with_mnemonic( menu_in_menu, "Re-disperse" );
//		if ( g_Layout_enableDetachableMenus.m_value ) {
//			menu_tearoff( menu_3 );
//		}
		menu_separator( menu_in_menu );
		create_menu_item_with_mnemonic( menu, "Rows", "RedisperseRows" );
		create_menu_item_with_mnemonic( menu, "Columns", "RedisperseCols" );
//		auto menu_4 = create_sub_menu_with_mnemonic( menu_in_menu, "Smooth" );
//		if ( g_Layout_enableDetachableMenus.m_value ) {
//			menu_tearoff( menu_4 );
//		}
		create_menu_item_with_mnemonic( menu, "Rows", "SmoothRows" );
		create_menu_item_with_mnemonic( menu, "Columns", "SmoothCols" );
		create_menu_item_with_mnemonic( menu_in_menu, "Transpose", "MatrixTranspose" );

	}
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "Cap Selection", "CapCurrentCurve" );
	create_menu_item_with_mnemonic( menu, "Cycle Cap Texture", "CycleCapTexturePatch" );
	menu_separator( menu );
	{
		auto menu_in_menu = create_sub_menu_with_mnemonic( menu, "Texture" );
		if ( g_Layout_enableDetachableMenus.m_value ) {
			menu_tearoff( menu_in_menu );
		}
		create_menu_item_with_mnemonic( menu_in_menu, "Cycle Projection", "CycleCapTexturePatch" );
		create_menu_item_with_mnemonic( menu_in_menu, "Naturalize", "NaturalizePatch" );
		create_menu_item_with_mnemonic( menu_in_menu, "Invert X", "InvertCurveTextureX" );
		create_menu_item_with_mnemonic( menu_in_menu, "Invert Y", "InvertCurveTextureY" );

	}
	menu_separator( menu );
	{
		auto menu_in_menu = create_sub_menu_with_mnemonic( menu, "Overlay" );
		if ( g_Layout_enableDetachableMenus.m_value ) {
			menu_tearoff( menu_in_menu );
		}
		create_menu_item_with_mnemonic( menu_in_menu, "Set", "MakeOverlayPatch" );
		create_menu_item_with_mnemonic( menu_in_menu, "Clear", "ClearPatchOverlays" );
	}
	menu_separator( menu );
	create_menu_item_with_mnemonic( menu, "Deform...", "PatchDeform" );
	create_menu_item_with_mnemonic( menu, "Thicken...", "PatchThicken" );
}


#include "gtkutil/dialog.h"
#include "gtkutil/widget.h"

void DoNewPatchDlg( EPatchPrefab prefab, int minrows, int mincols, int defrows, int defcols, int maxrows, int maxcols ){
	ModalDialog dialog;

	ui::Window window = MainFrame_getWindow().create_dialog_window("Patch density", G_CALLBACK(dialog_delete_callback ), &dialog );

	auto accel = ui::AccelGroup(ui::New);
	window.add_accel_group( accel );
	auto width = ui::ComboBoxText(ui::New);
	auto height = ui::ComboBoxText(ui::New);
	{
		auto hbox = create_dialog_hbox( 4, 4 );
		window.add(hbox);
		{
			auto table = create_dialog_table( 2, 2, 4, 4 );
			hbox.pack_start( table, TRUE, TRUE, 0 );
			{
				auto label = ui::Label( "Width:" );
				label.show();
                table.attach(label, {0, 1, 0, 1}, {GTK_FILL, 0});
				gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
			}
			{
				auto label = ui::Label( "Height:" );
				label.show();
                table.attach(label, {0, 1, 1, 2}, {GTK_FILL, 0});
				gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
			}

			{
				auto combo = width;
#define D_ITEM( x ) if ( x >= mincols && ( !maxcols || x <= maxcols ) ) gtk_combo_box_text_append_text( combo, #x )
				D_ITEM( 3 );
				D_ITEM( 5 );
				D_ITEM( 7 );
				D_ITEM( 9 );
				D_ITEM( 11 );
				D_ITEM( 13 );
				D_ITEM( 15 );
				D_ITEM( 17 );
				D_ITEM( 19 );
				D_ITEM( 21 );
				D_ITEM( 23 );
				D_ITEM( 25 );
				D_ITEM( 27 );
				D_ITEM( 29 );
				D_ITEM( 31 ); // MAX_PATCH_SIZE is 32, so we should be able to do 31...
#undef D_ITEM
				combo.show();
                table.attach(combo, {1, 2, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});
			}
			{
				auto combo = height;
#define D_ITEM( x ) if ( x >= minrows && ( !maxrows || x <= maxrows ) ) gtk_combo_box_text_append_text( combo, #x )
				D_ITEM( 3 );
				D_ITEM( 5 );
				D_ITEM( 7 );
				D_ITEM( 9 );
				D_ITEM( 11 );
				D_ITEM( 13 );
				D_ITEM( 15 );
				D_ITEM( 17 );
				D_ITEM( 19 );
				D_ITEM( 21 );
				D_ITEM( 23 );
				D_ITEM( 25 );
				D_ITEM( 27 );
				D_ITEM( 29 );
				D_ITEM( 31 ); // MAX_PATCH_SIZE is 32, so we should be able to do 31...
#undef D_ITEM
				combo.show();
                table.attach(combo, {1, 2, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});
			}
		}

		{
			auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, TRUE, TRUE, 0 );
			{
				auto button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &dialog );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				widget_make_default( button );
				gtk_widget_grab_focus( button  );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
			{
				auto button = create_dialog_button( "Cancel", G_CALLBACK( dialog_button_cancel ), &dialog );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
		}
	}

	// Initialize dialog
	gtk_combo_box_set_active( width, ( defcols - mincols ) / 2 );
	gtk_combo_box_set_active( height, ( defrows - minrows ) / 2 );

	if ( modal_dialog_show( window, dialog ) == eIDOK ) {
		int w = gtk_combo_box_get_active( width ) * 2 + mincols;
		int h = gtk_combo_box_get_active( height ) * 2 + minrows;

		Scene_PatchConstructPrefab( GlobalSceneGraph(), PatchCreator_getBounds(), TextureBrowser_GetSelectedShader( GlobalTextureBrowser() ), prefab, GlobalXYWnd_getCurrentViewType(), w, h );
	}

	window.destroy();
}


void DoPatchDeformDlg(){
	ModalDialog dialog;
	GtkWidget* deformW;

	ui::Window window = create_dialog_window( MainFrame_getWindow(), "Patch deform", G_CALLBACK( dialog_delete_callback ), &dialog );

	GtkAccelGroup* accel = gtk_accel_group_new();
	gtk_window_add_accel_group( window, accel );

	{
		GtkHBox* hbox = create_dialog_hbox( 4, 4 );
		gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( hbox ) );
		{
			GtkTable* table = create_dialog_table( 2, 2, 4, 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
			{
				GtkLabel* label = GTK_LABEL( gtk_label_new( "Max deform:" ) );
				gtk_widget_show( GTK_WIDGET( label ) );
				gtk_table_attach( table, GTK_WIDGET( label ), 0, 1, 0, 1,
								  (GtkAttachOptions) ( GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
			}
			{
				GtkWidget* entry = gtk_entry_new();
				gtk_entry_set_text( GTK_ENTRY( entry ), "16" );
				gtk_widget_show( entry );
				gtk_table_attach( table, entry, 1, 2, 0, 1,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );

				deformW = entry;
			}
		}
		{
			auto vbox = create_dialog_vbox( 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( vbox ), TRUE, TRUE, 0 );
			{
				auto button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &dialog );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				widget_make_default( button );
				gtk_widget_grab_focus( button  );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
			{
				auto button = create_dialog_button( "Cancel", G_CALLBACK( dialog_button_cancel ), &dialog );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
		}
	}

	if ( modal_dialog_show( window, dialog ) == eIDOK ) {
		int deform = static_cast<int>( atoi( gtk_entry_get_text( GTK_ENTRY( deformW ) ) ) );
		Scene_PatchDeform( GlobalSceneGraph(), deform );
	}

	gtk_widget_destroy( GTK_WIDGET( window ) );
}



EMessageBoxReturn DoCapDlg( ECapDialog* type ){
	ModalDialog dialog;
	ModalDialogButton ok_button( dialog, eIDOK );
	ModalDialogButton cancel_button( dialog, eIDCANCEL );
	ui::Widget bevel{ui::null};
	ui::Widget ibevel{ui::null};
	ui::Widget endcap{ui::null};
	ui::Widget iendcap{ui::null};
	ui::Widget cylinder{ui::null};

	ui::Window window = MainFrame_getWindow().create_modal_dialog_window( "Cap", dialog );

	auto accel_group = ui::AccelGroup(ui::New);
	window.add_accel_group( accel_group );

	{
		auto hbox = create_dialog_hbox( 4, 4 );
		window.add(hbox);

		{
			// Gef: Added a vbox to contain the toggle buttons
			auto radio_vbox = create_dialog_vbox( 4 );
			hbox.add(radio_vbox);

			{
				auto table = ui::Table( 5, 2, FALSE );
				table.show();
				radio_vbox.pack_start( table, TRUE, TRUE, 0 );
				gtk_table_set_row_spacings( table, 5 );
				gtk_table_set_col_spacings( table, 5 );

				{
					auto image = new_local_image( "cap_bevel.png" );
					image.show();
                    table.attach(image, {0, 1, 0, 1}, {GTK_FILL, 0});
				}
				{
					auto image = new_local_image( "cap_endcap.png" );
					image.show();
                    table.attach(image, {0, 1, 1, 2}, {GTK_FILL, 0});
				}
				{
					auto image = new_local_image( "cap_ibevel.png" );
					image.show();
                    table.attach(image, {0, 1, 2, 3}, {GTK_FILL, 0});
				}
				{
					auto image = new_local_image( "cap_iendcap.png" );
					image.show();
                    table.attach(image, {0, 1, 3, 4}, {GTK_FILL, 0});
				}
				{
					auto image = new_local_image( "cap_cylinder.png" );
					image.show();
                    table.attach(image, {0, 1, 4, 5}, {GTK_FILL, 0});
				}

				GSList* group = 0;
				{
					ui::Widget button = ui::Widget::from(gtk_radio_button_new_with_label( group, "Bevel" ));
					button.show();
                    table.attach(button, {1, 2, 0, 1}, {GTK_FILL | GTK_EXPAND, 0});
					group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( button ) );

					bevel = button;
				}
				{
					ui::Widget button = ui::Widget::from(gtk_radio_button_new_with_label( group, "Endcap" ));
					button.show();
                    table.attach(button, {1, 2, 1, 2}, {GTK_FILL | GTK_EXPAND, 0});
					group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( button ) );

					endcap = button;
				}
				{
					ui::Widget button = ui::Widget::from(gtk_radio_button_new_with_label( group, "Inverted Bevel" ));
					button.show();
                    table.attach(button, {1, 2, 2, 3}, {GTK_FILL | GTK_EXPAND, 0});
					group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( button ) );

					ibevel = button;
				}
				{
					ui::Widget button = ui::Widget::from(gtk_radio_button_new_with_label( group, "Inverted Endcap" ));
					button.show();
                    table.attach(button, {1, 2, 3, 4}, {GTK_FILL | GTK_EXPAND, 0});
					group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( button ) );

					iendcap = button;
				}
				{
					ui::Widget button = ui::Widget::from(gtk_radio_button_new_with_label( group, "Cylinder" ));
					button.show();
                    table.attach(button, {1, 2, 4, 5}, {GTK_FILL | GTK_EXPAND, 0});
					group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( button ) );

					cylinder = button;
				}
			}
		}

		{
			auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, FALSE, FALSE, 0 );
			{
				auto button = create_modal_dialog_button( "OK", ok_button );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				widget_make_default( button );
				gtk_widget_add_accelerator( button , "clicked", accel_group, GDK_KEY_Return, (GdkModifierType)0, GTK_ACCEL_VISIBLE );
			}
			{
				auto button = create_modal_dialog_button( "Cancel", cancel_button );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( button , "clicked", accel_group, GDK_KEY_Escape, (GdkModifierType)0, GTK_ACCEL_VISIBLE );
			}
		}
	}

	// Initialize dialog
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( bevel ), TRUE );

	EMessageBoxReturn ret = modal_dialog_show( window, dialog );
	if ( ret == eIDOK ) {
		if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( bevel ) ) ) {
			*type = PATCHCAP_BEVEL;
		}
		else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( endcap ) ) ) {
			*type = PATCHCAP_ENDCAP;
		}
		else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( ibevel ) ) ) {
			*type = PATCHCAP_INVERTED_BEVEL;
		}
		else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( iendcap ) ) ) {
			*type = PATCHCAP_INVERTED_ENDCAP;
		}
		else if ( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( cylinder ) ) ) {
			*type = PATCHCAP_CYLINDER;
		}
	}

	window.destroy();

	return ret;
}


void DoPatchThickenDlg(){
	ModalDialog dialog;
	GtkWidget* thicknessW;
	GtkWidget* seamsW;
	GtkWidget* radX;
	GtkWidget* radY;
	GtkWidget* radZ;
	GtkWidget* radNormals;

	ui::Window window = create_dialog_window( MainFrame_getWindow(), "Patch thicken", G_CALLBACK( dialog_delete_callback ), &dialog );

	GtkAccelGroup* accel = gtk_accel_group_new();
	gtk_window_add_accel_group( window, accel );

	{
		auto hbox = create_dialog_hbox( 4, 4 );
		gtk_container_add( GTK_CONTAINER( window ), GTK_WIDGET( hbox ) );
		{
			auto table = create_dialog_table( 2, 4, 4, 4 );
			gtk_box_pack_start( GTK_BOX( hbox ), GTK_WIDGET( table ), TRUE, TRUE, 0 );
			{
				GtkLabel* label = GTK_LABEL( gtk_label_new( "Thickness:" ) );
				gtk_widget_show( GTK_WIDGET( label ) );
				gtk_table_attach( table, GTK_WIDGET( label ), 0, 1, 0, 1,
								  (GtkAttachOptions) ( GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );
			}
			{
				GtkWidget* entry = gtk_entry_new();
				gtk_entry_set_text( GTK_ENTRY( entry ), "16" );
				gtk_widget_set_size_request( entry, 40, -1 );
				gtk_widget_show( entry );
				gtk_table_attach( table, entry, 1, 2, 0, 1,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );

				thicknessW = entry;
			}
			{
				// Create the "create seams" label
				GtkWidget* _seamsCheckBox = gtk_check_button_new_with_label( "Side walls" );
				gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( _seamsCheckBox ), TRUE );
				gtk_widget_show( _seamsCheckBox );
				gtk_table_attach( table, _seamsCheckBox, 3, 4, 0, 1,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				seamsW = _seamsCheckBox;

			}
			{
				// Create the radio button group for choosing the extrude axis
				GtkWidget* _radNormals = gtk_radio_button_new_with_label( NULL, "Normal" );
				GtkWidget* _radX = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(_radNormals), "X" );
				GtkWidget* _radY = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(_radNormals), "Y" );
				GtkWidget* _radZ = gtk_radio_button_new_with_label_from_widget( GTK_RADIO_BUTTON(_radNormals), "Z" );
				gtk_widget_show( _radNormals );
				gtk_widget_show( _radX );
				gtk_widget_show( _radY );
				gtk_widget_show( _radZ );


				// Pack the buttons into the table
				gtk_table_attach( table, _radNormals, 0, 1, 1, 2,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_table_attach( table, _radX, 1, 2, 1, 2,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_table_attach( table, _radY, 2, 3, 1, 2,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				gtk_table_attach( table, _radZ, 3, 4, 1, 2,
								  (GtkAttachOptions) ( GTK_EXPAND | GTK_FILL ),
								  (GtkAttachOptions) ( 0 ), 0, 0 );
				radX = _radX;
				radY = _radY;
				radZ = _radZ;
				radNormals = _radNormals;
			}
		}
		{
			auto vbox = create_dialog_vbox( 4 );
			hbox.pack_start( vbox, FALSE, FALSE, 0 );
			{
				auto button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &dialog );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				widget_make_default( button );
				gtk_widget_grab_focus( button  );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Return, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
			{
				auto button = create_dialog_button( "Cancel", G_CALLBACK( dialog_button_cancel ), &dialog );
				vbox.pack_start( button, FALSE, FALSE, 0 );
				gtk_widget_add_accelerator( button , "clicked", accel, GDK_KEY_Escape, (GdkModifierType)0, (GtkAccelFlags)0 );
			}
		}
	}

	if ( modal_dialog_show( window, dialog ) == eIDOK ) {
		int axis;
		bool seams;
		float thickness = static_cast<float>( atoi( gtk_entry_get_text( GTK_ENTRY( thicknessW ) ) ) );
		seams = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON( seamsW )) ? true : false;

		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radX))) {
			axis = 0;
		}
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radY))) {
			axis = 1;
		}
		else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radZ))) {
			axis = 2;
		}
		else  {
			// Extrude along normals
			axis = 3;
		}
		Scene_PatchThicken( GlobalSceneGraph(), thickness, seams, axis );
	}

	gtk_widget_destroy( GTK_WIDGET( window ) );
}
