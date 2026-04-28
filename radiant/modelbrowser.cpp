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

#include "modelbrowser.h"

#include <set>
#include <map>
#include <vector>

#include <gtk/gtk.h>

#include "ifilesystem.h"
#include "ifiletypes.h"
#include "imodel.h"

#include "os/path.h"
#include "string/string.h"
#include "stream/stringstream.h"
#include "generic/callback.h"

#include "gtkutil/toolbar.h"
#include "gtkutil/dialog.h"
#include "entity.h"
#include "entityinspector.h"
#include "mainframe.h"
#include "qe3.h"

namespace
{

// Tree store columns for the directory tree
enum
{
	DIR_NAME_COLUMN,
	DIR_FULLPATH_COLUMN,
	DIR_N_COLUMNS
};

// List store columns for the model file list
enum
{
	MODEL_NAME_COLUMN,
	MODEL_FULLPATH_COLUMN,
	MODEL_N_COLUMNS
};

// In-memory filesystem tree built from VFS
class ModelFS
{
public:
	CopiedString m_name;

	ModelFS() = default;
	ModelFS( const char* name ) : m_name( name ) {}

	bool operator<( const ModelFS& other ) const {
		return string_less_nocase( m_name.c_str(), other.m_name.c_str() );
	}

	mutable std::set<ModelFS> m_folders;
	mutable std::set<CopiedString> m_files;

	void insert( const char* filepath ) const {
		const char* slash = strchr( filepath, '/' );
		if ( slash == nullptr ) {
			m_files.insert( CopiedString( filepath ) );
		}
		else {
			CopiedString dirName( StringRange( filepath, slash ) );
			auto result = m_folders.insert( ModelFS( dirName.c_str() ) );
			result.first->insert( slash + 1 );
		}
	}
};

class ModelBrowser
{
public:
	ui::Window m_parent{ui::null};
	ui::TreeView m_treeView{ui::null};
	ui::TreeView m_fileList{ui::null};

	ModelFS m_modelFS;
	CopiedString m_currentFolderPath;
	CopiedString m_selectedModel;

	std::set<CopiedString> m_modelExtensions;
};

ModelBrowser g_ModelBrowser;

// Collect known model extensions from the file type registry
void ModelBrowser_getModelExtensions(){
	g_ModelBrowser.m_modelExtensions.clear();

	class TypeListAccumulator : public IFileTypeList
	{
	public:
		std::set<CopiedString>& m_extensions;
		TypeListAccumulator( std::set<CopiedString>& ext ) : m_extensions( ext ) {}
		void addType( const char* moduleName, filetype_t type ) override {
			// moduleName is the file extension
			m_extensions.insert( CopiedString( moduleName ) );
		}
	};

	TypeListAccumulator typelist( g_ModelBrowser.m_modelExtensions );
	GlobalFiletypes().getTypeList( ModelLoader::Name(), &typelist, true, false, false );
}

// VFS callback to collect model files
void ModelBrowser_addModelFile( const char* name ){
	g_ModelBrowser.m_modelFS.insert( name );
}
typedef FreeCaller<void(const char*), ModelBrowser_addModelFile> ModelBrowserAddModelFileCaller;

// Scan VFS for model files
void ModelBrowser_buildTree(){
	g_ModelBrowser.m_modelFS.m_folders.clear();
	g_ModelBrowser.m_modelFS.m_files.clear();

	for ( const CopiedString& ext : g_ModelBrowser.m_modelExtensions ) {
		GlobalFileSystem().forEachFile( "models/", ext.c_str(), ModelBrowserAddModelFileCaller(), 99 );
	}
}

// Recursively build the directory tree GtkTreeStore from ModelFS
void ModelBrowser_buildTreeStore_r( const ModelFS& fs, GtkTreeStore* store, GtkTreeIter* parent, const char* parentPath ){
	for ( const ModelFS& child : fs.m_folders ) {
		GtkTreeIter iter;
		gtk_tree_store_append( store, &iter, parent );

		StringOutputStream fullpath( 256 );
		fullpath << parentPath << child.m_name.c_str() << "/";

		gtk_tree_store_set( store, &iter,
			DIR_NAME_COLUMN, child.m_name.c_str(),
			DIR_FULLPATH_COLUMN, fullpath.c_str(),
			-1 );

		ModelBrowser_buildTreeStore_r( child, store, &iter, fullpath.c_str() );
	}
}

// Find a ModelFS node by path
const ModelFS* ModelFS_findPath( const ModelFS& root, const char* path ){
	if ( string_empty( path ) ) {
		return &root;
	}

	const char* slash = strchr( path, '/' );
	if ( slash == nullptr ) {
		return nullptr;
	}

	CopiedString dirName( StringRange( path, slash ) );
	for ( const ModelFS& child : root.m_folders ) {
		if ( string_equal_nocase( child.m_name.c_str(), dirName.c_str() ) ) {
			return ModelFS_findPath( child, slash + 1 );
		}
	}
	return nullptr;
}

// Populate the file list for a given directory path
void ModelBrowser_populateFileList( const char* dirPath ){
	g_ModelBrowser.m_currentFolderPath = dirPath;
	g_ModelBrowser.m_selectedModel = "";

	auto store = ui::ListStore::from( gtk_list_store_new( MODEL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING ) );
	gtk_list_store_clear( store );

	// Find the ModelFS node for this path
	// dirPath is relative like "models/weapons/" - we need to strip "models/" prefix since our tree starts there
	const ModelFS* node = ModelFS_findPath( g_ModelBrowser.m_modelFS, dirPath );
	if ( node != nullptr ) {
		for ( const CopiedString& filename : node->m_files ) {
			GtkTreeIter iter;
			gtk_list_store_append( store, &iter );

			StringOutputStream fullpath( 256 );
			fullpath << dirPath << filename.c_str();

			gtk_list_store_set( store, &iter,
				MODEL_NAME_COLUMN, filename.c_str(),
				MODEL_FULLPATH_COLUMN, fullpath.c_str(),
				-1 );
		}
	}

	gtk_tree_view_set_model( g_ModelBrowser.m_fileList, store );
	g_object_unref( G_OBJECT( store ) );
}

// Build the full directory tree widget model
void ModelBrowser_constructTreeStore(){
	auto store = ui::TreeStore::from( gtk_tree_store_new( DIR_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING ) );

	// The root "models/" entry
	GtkTreeIter rootIter;
	gtk_tree_store_append( store, &rootIter, nullptr );
	gtk_tree_store_set( store, &rootIter,
		DIR_NAME_COLUMN, "models",
		DIR_FULLPATH_COLUMN, "",
		-1 );

	ModelBrowser_buildTreeStore_r( g_ModelBrowser.m_modelFS, store, &rootIter, "" );

	gtk_tree_view_set_model( g_ModelBrowser.m_treeView, store );
	g_object_unref( G_OBJECT( store ) );

	// Expand the root node
	auto path = ui::TreePath( "0" );
	gtk_tree_view_expand_row( g_ModelBrowser.m_treeView, path, FALSE );
	gtk_tree_path_free( path );
}

// Rebuild the entire model browser tree
void ModelBrowser_refresh(){
	ModelBrowser_getModelExtensions();
	ModelBrowser_buildTree();
	ModelBrowser_constructTreeStore();

	// Clear the file list
	auto store = ui::ListStore::from( gtk_list_store_new( MODEL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING ) );
	gtk_tree_view_set_model( g_ModelBrowser.m_fileList, store );
	g_object_unref( G_OBJECT( store ) );
}
typedef FreeCaller<void(), ModelBrowser_refresh> ModelBrowserRefreshCaller;

// Callback: directory tree row activated
void ModelBrowser_TreeView_onRowActivated( ui::TreeView treeView, ui::TreePath path, ui::TreeViewColumn col, gpointer userdata ){
	GtkTreeIter iter;
	auto model = gtk_tree_view_get_model( treeView );

	if ( gtk_tree_model_get_iter( model, &iter, path ) ) {
		gchar* fullpath;
		gtk_tree_model_get( model, &iter, DIR_FULLPATH_COLUMN, &fullpath, -1 );

		// Build the VFS directory path: the stored path already has no "models/" prefix since
		// the VFS forEachFile was called with "models/" as basedir.
		// But we stored the paths relative to the VFS root in our ModelFS, so reconstruct.
		StringOutputStream dirpath( 256 );
		if ( string_empty( fullpath ) ) {
			// root "models" node: show all files directly under models/
			// Our ModelFS root contains the tree as scanned under "models/"
			// so the files at root level are directly in models/
		}
		dirpath << fullpath;

		ModelBrowser_populateFileList( dirpath.c_str() );

		g_free( fullpath );

		// Deactivate focus so keyboard shortcuts still work
		gtk_window_set_focus( GTK_WINDOW( gtk_widget_get_toplevel( GTK_WIDGET( treeView ) ) ), nullptr );
	}
}

// Callback: file list selection changed
void ModelBrowser_FileList_onSelectionChanged( GtkTreeSelection* selection, gpointer userdata ){
	GtkTreeModel* model;
	GtkTreeIter iter;

	if ( gtk_tree_selection_get_selected( selection, &model, &iter ) ) {
		gchar* fullpath;
		gtk_tree_model_get( model, &iter, MODEL_FULLPATH_COLUMN, &fullpath, -1 );

		// Prepend "models/" since VFS expects the full relative path
		StringOutputStream vfspath( 256 );
		vfspath << "models/" << fullpath;
		g_ModelBrowser.m_selectedModel = vfspath.c_str();

		g_free( fullpath );
	}
	else {
		g_ModelBrowser.m_selectedModel = "";
	}
}

// Callback: file list row activated (double click) - apply model to selected entities
void ModelBrowser_FileList_onRowActivated( ui::TreeView treeView, ui::TreePath path, ui::TreeViewColumn col, gpointer userdata ){
	GtkTreeIter iter;
	auto model = gtk_tree_view_get_model( treeView );

	if ( gtk_tree_model_get_iter( model, &iter, path ) ) {
		gchar* fullpath;
		gtk_tree_model_get( model, &iter, MODEL_FULLPATH_COLUMN, &fullpath, -1 );

		StringOutputStream vfspath( 256 );
		vfspath << "models/" << fullpath;

		// Set the model key on all selected entities
		Scene_EntitySetKeyValue_Selected( "model", vfspath.c_str() );

		g_free( fullpath );
	}
}

} // anonymous namespace

const char* ModelBrowser_getSelectedModel(){
	if ( g_ModelBrowser.m_selectedModel.empty() ) {
		return nullptr;
	}
	return g_ModelBrowser.m_selectedModel.c_str();
}

ui::Widget ModelBrowser_constructWindow( ui::Window toplevel ){
	g_ModelBrowser.m_parent = toplevel;

	auto hbox = ui::HPaned( ui::New );
	hbox.show();

	// Left side: directory tree in scrolled window
	{
		auto scr = ui::ScrolledWindow( ui::New );
		gtk_container_set_border_width( GTK_CONTAINER( scr ), 0 );
		gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
		scr.show();

		auto store = ui::TreeStore::from( gtk_tree_store_new( DIR_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING ) );
		g_ModelBrowser.m_treeView = ui::TreeView( ui::TreeModel::from( store._handle ) );
		g_object_unref( G_OBJECT( store ) );

		gtk_tree_view_set_enable_search( g_ModelBrowser.m_treeView, FALSE );
		gtk_tree_view_set_headers_visible( g_ModelBrowser.m_treeView, FALSE );
		g_ModelBrowser.m_treeView.connect( "row-activated", G_CALLBACK( ModelBrowser_TreeView_onRowActivated ), nullptr );

		auto renderer = ui::CellRendererText( ui::New );
		gtk_tree_view_insert_column_with_attributes( g_ModelBrowser.m_treeView, -1, "", renderer, "text", DIR_NAME_COLUMN, NULL );

		gtk_container_add( GTK_CONTAINER( scr ), GTK_WIDGET( g_ModelBrowser.m_treeView ) );
		g_ModelBrowser.m_treeView.show();

		gtk_paned_pack1( GTK_PANED( hbox ), GTK_WIDGET( scr ), TRUE, TRUE );
	}

	// Right side: vbox with toolbar and file list
	{
		auto vbox = ui::VBox( FALSE, 0 );
		vbox.show();

		// Toolbar
		{
			auto toolbar = ui::Toolbar::from( gtk_toolbar_new() );
			gtk_toolbar_set_style( toolbar, GTK_TOOLBAR_ICONS );
			gtk_toolbar_set_show_arrow( toolbar, FALSE );

			toolbar_append_button( toolbar, "Reload Model Tree", "refresh_models.png", ModelBrowserRefreshCaller() );

			toolbar.show();
			vbox.pack_start( toolbar, FALSE, FALSE, 0 );
		}

		// File list in scrolled window
		{
			auto scr = ui::ScrolledWindow( ui::New );
			gtk_container_set_border_width( GTK_CONTAINER( scr ), 0 );
			gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
			scr.show();

			auto store = ui::ListStore::from( gtk_list_store_new( MODEL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING ) );
			g_ModelBrowser.m_fileList = ui::TreeView( ui::TreeModel::from( store._handle ) );
			g_object_unref( G_OBJECT( store ) );

			gtk_tree_view_set_enable_search( g_ModelBrowser.m_fileList, TRUE );
			gtk_tree_view_set_headers_visible( g_ModelBrowser.m_fileList, FALSE );

			g_ModelBrowser.m_fileList.connect( "row-activated", G_CALLBACK( ModelBrowser_FileList_onRowActivated ), nullptr );

			auto selection = gtk_tree_view_get_selection( g_ModelBrowser.m_fileList );
			g_signal_connect( G_OBJECT( selection ), "changed", G_CALLBACK( ModelBrowser_FileList_onSelectionChanged ), nullptr );

			auto renderer = ui::CellRendererText( ui::New );
			gtk_tree_view_insert_column_with_attributes( g_ModelBrowser.m_fileList, -1, "Model", renderer, "text", MODEL_NAME_COLUMN, NULL );

			gtk_container_add( GTK_CONTAINER( scr ), GTK_WIDGET( g_ModelBrowser.m_fileList ) );
			g_ModelBrowser.m_fileList.show();

			vbox.pack_start( scr, TRUE, TRUE, 0 );
		}

		gtk_paned_pack2( GTK_PANED( hbox ), GTK_WIDGET( vbox ), TRUE, TRUE );
	}

	// Set reasonable default pane position
	gtk_paned_set_position( GTK_PANED( hbox ), 200 );

	// Initial tree population
	ModelBrowser_refresh();

	return hbox;
}

void ModelBrowser_destroyWindow(){
	g_ModelBrowser.m_treeView = ui::TreeView{ui::null};
	g_ModelBrowser.m_fileList = ui::TreeView{ui::null};
}

void ModelBrowser_Construct(){
}

void ModelBrowser_Destroy(){
}

// Static storage for the chooser dialog's selected model path
static CopiedString s_chooserSelectedModel;

// Modal model chooser dialog
const char* ModelBrowser_showChooser( ui::Widget parent ){
	ModalDialog dialog;
	auto window = create_modal_dialog_window( parent.window(), "Choose Model", dialog, 600, 400 );

	auto vbox = create_dialog_vbox( 4, 4 );
	window.add( vbox );

	auto hpaned = ui::HPaned( ui::New );
	hpaned.show();
	vbox.pack_start( hpaned, TRUE, TRUE, 0 );

	// Ensure we have extension/tree data
	if ( g_ModelBrowser.m_modelExtensions.empty() ) {
		ModelBrowser_getModelExtensions();
		ModelBrowser_buildTree();
	}

	// Directory tree
	ui::TreeView dirTreeView{ui::null};
	{
		auto scr = ui::ScrolledWindow( ui::New );
		gtk_container_set_border_width( GTK_CONTAINER( scr ), 0 );
		gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
		scr.show();

		auto store = ui::TreeStore::from( gtk_tree_store_new( DIR_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING ) );

		GtkTreeIter rootIter;
		gtk_tree_store_append( store, &rootIter, nullptr );
		gtk_tree_store_set( store, &rootIter,
			DIR_NAME_COLUMN, "models",
			DIR_FULLPATH_COLUMN, "",
			-1 );
		ModelBrowser_buildTreeStore_r( g_ModelBrowser.m_modelFS, store, &rootIter, "" );

		dirTreeView = ui::TreeView( ui::TreeModel::from( store._handle ) );
		g_object_unref( G_OBJECT( store ) );

		gtk_tree_view_set_enable_search( dirTreeView, FALSE );
		gtk_tree_view_set_headers_visible( dirTreeView, FALSE );

		auto renderer = ui::CellRendererText( ui::New );
		gtk_tree_view_insert_column_with_attributes( dirTreeView, -1, "", renderer, "text", DIR_NAME_COLUMN, NULL );

		gtk_container_add( GTK_CONTAINER( scr ), GTK_WIDGET( dirTreeView ) );
		dirTreeView.show();

		// Expand root
		auto path = ui::TreePath( "0" );
		gtk_tree_view_expand_row( dirTreeView, path, FALSE );
		gtk_tree_path_free( path );

		gtk_paned_pack1( GTK_PANED( hpaned ), GTK_WIDGET( scr ), TRUE, TRUE );
	}

	// File list
	ui::TreeView fileListView{ui::null};
	{
		auto scr = ui::ScrolledWindow( ui::New );
		gtk_container_set_border_width( GTK_CONTAINER( scr ), 0 );
		gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scr ), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS );
		scr.show();

		auto store = ui::ListStore::from( gtk_list_store_new( MODEL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING ) );
		fileListView = ui::TreeView( ui::TreeModel::from( store._handle ) );
		g_object_unref( G_OBJECT( store ) );

		gtk_tree_view_set_enable_search( fileListView, TRUE );
		gtk_tree_view_set_headers_visible( fileListView, FALSE );

		auto renderer = ui::CellRendererText( ui::New );
		gtk_tree_view_insert_column_with_attributes( fileListView, -1, "Model", renderer, "text", MODEL_NAME_COLUMN, NULL );

		gtk_container_add( GTK_CONTAINER( scr ), GTK_WIDGET( fileListView ) );
		fileListView.show();

		gtk_paned_pack2( GTK_PANED( hpaned ), GTK_WIDGET( scr ), TRUE, TRUE );
	}

	gtk_paned_set_position( GTK_PANED( hpaned ), 200 );

	// Wire up directory tree row activation to populate file list
	struct ChooserData {
		ui::TreeView fileListView;
	};
	auto chooserData = new ChooserData{ fileListView };

	dirTreeView.connect( "row-activated", G_CALLBACK( +[]( GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, gpointer data ) {
		auto cd = static_cast<ChooserData*>( data );
		GtkTreeIter iter;
		auto model = gtk_tree_view_get_model( GTK_TREE_VIEW( tv ) );

		if ( gtk_tree_model_get_iter( model, &iter, path ) ) {
			gchar* fullpath;
			gtk_tree_model_get( model, &iter, DIR_FULLPATH_COLUMN, &fullpath, -1 );

			auto store = ui::ListStore::from( gtk_list_store_new( MODEL_N_COLUMNS, G_TYPE_STRING, G_TYPE_STRING ) );

			const ModelFS* node = ModelFS_findPath( g_ModelBrowser.m_modelFS, fullpath );
			if ( node != nullptr ) {
				for ( const CopiedString& filename : node->m_files ) {
					GtkTreeIter listIter;
					gtk_list_store_append( store, &listIter );

					StringOutputStream fp( 256 );
					fp << fullpath << filename.c_str();

					gtk_list_store_set( store, &listIter,
						MODEL_NAME_COLUMN, filename.c_str(),
						MODEL_FULLPATH_COLUMN, fp.c_str(),
						-1 );
				}
			}

			gtk_tree_view_set_model( cd->fileListView, store );
			g_object_unref( G_OBJECT( store ) );
			g_free( fullpath );
		}
	} ), chooserData );

	// Buttons
	{
		auto buttonBox = create_dialog_hbox( 4 );
		vbox.pack_end( buttonBox, FALSE, FALSE, 0 );

		ModalDialogButton okDialogButton( dialog, eIDOK );
		auto okButton = create_modal_dialog_button( "OK", okDialogButton );
		buttonBox.pack_end( okButton, FALSE, FALSE, 0 );
		ModalDialogButton cancelDialogButton( dialog, eIDCANCEL );
		auto cancelButton = create_modal_dialog_button( "Cancel", cancelDialogButton );
		buttonBox.pack_end( cancelButton, FALSE, FALSE, 0 );
	}

	// Also accept double-click on file list as OK
	struct ChooserDialogData {
		ModalDialog* dialog;
		ui::TreeView fileListView;
	};
	auto chooserDialogData = new ChooserDialogData{ &dialog, fileListView };

	fileListView.connect( "row-activated", G_CALLBACK( +[]( GtkTreeView* tv, GtkTreePath* path, GtkTreeViewColumn* col, gpointer data ) {
		auto dd = static_cast<ChooserDialogData*>( data );
		dd->dialog->ret = eIDOK;
		dd->dialog->loop = false;
	} ), chooserDialogData );

	EMessageBoxReturn ret = modal_dialog_show( window, dialog );

	const char* result = nullptr;
	if ( ret == eIDOK ) {
		// Get selected file from the file list
		auto selection = gtk_tree_view_get_selection( fileListView );
		GtkTreeModel* treeModel;
		GtkTreeIter iter;
		if ( gtk_tree_selection_get_selected( selection, &treeModel, &iter ) ) {
			gchar* fullpath;
			gtk_tree_model_get( treeModel, &iter, MODEL_FULLPATH_COLUMN, &fullpath, -1 );

			StringOutputStream vfspath( 256 );
			vfspath << "models/" << fullpath;
			s_chooserSelectedModel = vfspath.c_str();
			result = s_chooserSelectedModel.c_str();

			g_free( fullpath );
		}
	}

	delete chooserData;
	delete chooserDialogData;
	window.destroy();

	return result;
}
