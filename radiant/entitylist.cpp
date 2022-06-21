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

#include "entitylist.h"

#include "iselection.h"

#include <uilib/uilib.h>
#include <gtk/gtk.h>
#include <gtk/gtkcheckbutton.h>

#include "string/string.h"
#include "scenelib.h"
#include "nameable.h"
#include "signal/isignal.h"
#include "generic/object.h"

#include "gtkutil/widget.h"
#include "gtkutil/window.h"
#include "gtkutil/idledraw.h"
#include "gtkutil/accelerator.h"
#include "gtkutil/closure.h"

#include "treemodel.h"

#include "mainframe.h"

void RedrawEntityList();
typedef FreeCaller<void(), RedrawEntityList> RedrawEntityListCaller;


class EntityList
{
public:
enum EDirty
{
	eDefault,
	eSelection,
	eInsertRemove,
};

EDirty m_dirty;

IdleDraw m_idleDraw;
WindowPositionTracker m_positionTracker;

ui::Window m_window;
ui::Widget m_check;
ui::TreeView m_tree_view{ui::null};
ui::TreeModel m_tree_model{ui::null};
bool m_selection_disabled;

EntityList() :
	m_dirty( EntityList::eDefault ),
	m_idleDraw( RedrawEntityListCaller() ),
	m_window( ui::null ),
	m_check( ui::null ),
	m_selection_disabled( false ){
}

bool visible() {
	return m_window.visible();
}
};

namespace
{
EntityList* g_EntityList;

inline EntityList& getEntityList(){
	ASSERT_NOTNULL( g_EntityList );
	return *g_EntityList;
}
}


inline Nameable* Node_getNameable( scene::Node& node ){
	return NodeTypeCast<Nameable>::cast( node );
}

const char* node_get_name( scene::Node& node ){
	Nameable* nameable = Node_getNameable( node );
	return ( nameable != 0 )
		   ? nameable->name()
		   : "node";
}

template<typename value_type>
inline void gtk_tree_model_get_pointer( ui::TreeModel model, GtkTreeIter* iter, gint column, value_type** pointer ){
	GValue value = GValue_default();
	gtk_tree_model_get_value( model, iter, column, &value );
	*pointer = (value_type*)g_value_get_pointer( &value );
}



void entitylist_treeviewcolumn_celldatafunc( ui::TreeViewColumn column, ui::CellRenderer renderer, ui::TreeModel model, GtkTreeIter* iter, gpointer data ){
	scene::Node* node;
	gtk_tree_model_get_pointer( model, iter, 0, &node );
	scene::Instance* instance;
	gtk_tree_model_get_pointer( model, iter, 1, &instance );
	if ( node != 0 ) {
		gtk_cell_renderer_set_fixed_size( renderer, -1, -1 );
		char* name = const_cast<char*>( node_get_name( *node ) );
		g_object_set( G_OBJECT( renderer ), "text", name, "visible", TRUE, NULL );

		//globalOutputStream() << "rendering cell " << makeQuoted(name) << "\n";
		auto style = gtk_widget_get_style( ui::TreeView( getEntityList().m_tree_view ) );
		if ( instance->childSelected() ) {
			g_object_set( G_OBJECT( renderer ), "cell-background-gdk", &style->base[GTK_STATE_ACTIVE], NULL );
		}
		else
		{
			g_object_set( G_OBJECT( renderer ), "cell-background-gdk", &style->base[GTK_STATE_NORMAL], NULL );
		}
	}
	else
	{
		gtk_cell_renderer_set_fixed_size( renderer, -1, 0 );
		g_object_set( G_OBJECT( renderer ), "text", "", "visible", FALSE, NULL );
	}
}

static gboolean entitylist_tree_select( ui::TreeSelection selection, ui::TreeModel model, ui::TreePath path, gboolean path_currently_selected, gpointer data ){
	GtkTreeIter iter;
	gtk_tree_model_get_iter( model, &iter, path );
	scene::Node* node;
	gtk_tree_model_get_pointer( model, &iter, 0, &node );
	scene::Instance* instance;
	gtk_tree_model_get_pointer( model, &iter, 1, &instance );
	Selectable* selectable = Instance_getSelectable( *instance );

	if ( node == 0 ) {
		if ( path_currently_selected != FALSE ) {
			getEntityList().m_selection_disabled = true;
			GlobalSelectionSystem().setSelectedAll( false );
			getEntityList().m_selection_disabled = false;
		}
	}
	else if ( selectable != 0 ) {
		getEntityList().m_selection_disabled = true;
		selectable->setSelected( path_currently_selected == FALSE );
		getEntityList().m_selection_disabled = false;
		if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( getEntityList().m_check ) ) ){
			FocusAllViews();
		}
		return TRUE;
	}

	return FALSE;
}

static gboolean entitylist_tree_select_null( ui::TreeSelection selection, ui::TreeModel model, ui::TreePath path, gboolean path_currently_selected, gpointer data ){
	return TRUE;
}

void EntityList_ConnectSignals( ui::TreeView view ){
	auto select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_select_function(select, reinterpret_cast<GtkTreeSelectionFunc>(entitylist_tree_select), NULL, 0 );
}

void EntityList_DisconnectSignals( ui::TreeView view ){
	auto select = gtk_tree_view_get_selection( view );
	gtk_tree_selection_set_select_function(select, reinterpret_cast<GtkTreeSelectionFunc>(entitylist_tree_select_null), 0, 0 );
}



gboolean treemodel_update_selection( ui::TreeModel model, ui::TreePath path, GtkTreeIter* iter, gpointer data ){
	auto view = ui::TreeView::from( data );

	scene::Instance* instance;
	gtk_tree_model_get_pointer( model, iter, 1, &instance );
	Selectable* selectable = Instance_getSelectable( *instance );

	if ( selectable != 0 ) {
		auto selection = gtk_tree_view_get_selection( view );
		if ( selectable->isSelected() ) {
			gtk_tree_selection_select_path( selection, path );
		}
		else
		{
			gtk_tree_selection_unselect_path( selection, path );
		}
	}

	return FALSE;
}

void EntityList_UpdateSelection( ui::TreeModel model, ui::TreeView view ){
	EntityList_DisconnectSignals( view );
	gtk_tree_model_foreach(model, reinterpret_cast<GtkTreeModelForeachFunc>(treemodel_update_selection), view._handle );
	EntityList_ConnectSignals( view );
}


void RedrawEntityList(){
	switch ( getEntityList().m_dirty )
	{
	case EntityList::eInsertRemove:
	case EntityList::eSelection:
		EntityList_UpdateSelection( getEntityList().m_tree_model, getEntityList().m_tree_view );
	default:
		break;
	}
	getEntityList().m_dirty = EntityList::eDefault;
}

void entitylist_queue_draw(){
	getEntityList().m_idleDraw.queueDraw();
}

void EntityList_SelectionUpdate(){
	if ( getEntityList().m_selection_disabled ) {
		return;
	}

	if ( getEntityList().m_dirty < EntityList::eSelection ) {
		getEntityList().m_dirty = EntityList::eSelection;
	}
	entitylist_queue_draw();
}

void EntityList_SelectionChanged( const Selectable& selectable ){
	EntityList_SelectionUpdate();
}

void entitylist_treeview_rowcollapsed( ui::TreeView view, GtkTreeIter* iter, ui::TreePath path, gpointer user_data ){
}

void entitylist_treeview_row_expanded( ui::TreeView view, GtkTreeIter* iter, ui::TreePath path, gpointer user_data ){
	EntityList_SelectionUpdate();
}


void EntityList_SetShown( bool shown ){
	getEntityList().m_window.visible(shown);
}

void EntityList_toggleShown(){
	EntityList_SetShown( !getEntityList().visible() );
}

gint graph_tree_model_compare_name( ui::TreeModel model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data ){
	scene::Node* first;
	gtk_tree_model_get( model, a, 0, (gpointer*)&first, -1 );
	scene::Node* second;
	gtk_tree_model_get( model, b, 0, (gpointer*)&second, -1 );
	int result = 0;
	if ( first != 0 && second != 0 ) {
		result = string_compare( node_get_name( *first ), node_get_name( *second ) );
	}
	if ( result == 0 ) {
		return ( first < second ) ? -1 : ( second < first ) ? 1 : 0;
	}
	return result;
}

extern GraphTreeModel* scene_graph_get_tree_model();
void AttachEntityTreeModel(){
	getEntityList().m_tree_model = ui::TreeModel::from(scene_graph_get_tree_model());

	gtk_tree_view_set_model( getEntityList().m_tree_view, getEntityList().m_tree_model );
}

void DetachEntityTreeModel(){
	getEntityList().m_tree_model = ui::TreeModel(ui::null);

	gtk_tree_view_set_model( getEntityList().m_tree_view, 0 );
}

void EntityList_constructWindow( ui::Window main_window ){
	ASSERT_TRUE( !getEntityList().m_window );

	auto window = ui::Window(create_persistent_floating_window( "Entity List", main_window ));

	window.add_accel_group(global_accel);

	getEntityList().m_positionTracker.connect( window );


	getEntityList().m_window = window;

	{
		ui::VBox vbox = ui::VBox( FALSE, 0 );
		gtk_container_set_border_width( GTK_CONTAINER( vbox ), 0 );
		vbox.show();
		window.add( vbox );

		auto scr = create_scrolled_window( ui::Policy::AUTOMATIC, ui::Policy::AUTOMATIC );
		vbox.pack_start( scr, TRUE, TRUE, 0 );

		{
            auto view = ui::TreeView(ui::New);
			gtk_tree_view_set_headers_visible(view, FALSE );

			auto renderer = ui::CellRendererText(ui::New);
			auto column = gtk_tree_view_column_new();
			gtk_tree_view_column_pack_start( column, renderer, TRUE );
			gtk_tree_view_column_set_cell_data_func(column, renderer, reinterpret_cast<GtkTreeCellDataFunc>(entitylist_treeviewcolumn_celldatafunc), 0, 0 );

			auto select = gtk_tree_view_get_selection(view );
			gtk_tree_selection_set_mode( select, GTK_SELECTION_MULTIPLE );

			view.connect( "row_expanded", G_CALLBACK( entitylist_treeview_row_expanded ), 0 );
			view.connect( "row_collapsed", G_CALLBACK( entitylist_treeview_rowcollapsed ), 0 );

			gtk_tree_view_append_column(view, column );

			view.show();
			scr.add(view);
			getEntityList().m_tree_view = view;
		}
		{
			ui::Widget check = ui::Widget::from( gtk_check_button_new_with_label( "Focus on Selected" ) );
			gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check ), FALSE );
			check.show();
			vbox.pack_start( check, FALSE, FALSE, 0 );
			getEntityList().m_check = check;
		}
	}

	EntityList_ConnectSignals( getEntityList().m_tree_view );
	AttachEntityTreeModel();
}

void EntityList_destroyWindow(){
	DetachEntityTreeModel();
	EntityList_DisconnectSignals( getEntityList().m_tree_view );
	destroy_floating_window( getEntityList().m_window );
}

#include "preferencesystem.h"

#include "iselection.h"

namespace
{
scene::Node* nullNode = 0;
}

class NullSelectedInstance : public scene::Instance, public Selectable
{
class TypeCasts
{
InstanceTypeCastTable m_casts;
public:
TypeCasts(){
	InstanceStaticCast<NullSelectedInstance, Selectable>::install( m_casts );
}
InstanceTypeCastTable& get(){
	return m_casts;
}
};

public:
typedef LazyStatic<TypeCasts> StaticTypeCasts;

NullSelectedInstance() : Instance( scene::Path( makeReference( *nullNode ) ), 0, this, StaticTypeCasts::instance().get() ){
}

void setSelected( bool select ){
	ERROR_MESSAGE( "error" );
}
bool isSelected() const {
	return true;
}
};

typedef LazyStatic<NullSelectedInstance> StaticNullSelectedInstance;


void EntityList_Construct(){
	graph_tree_model_insert( scene_graph_get_tree_model(), StaticNullSelectedInstance::instance() );

	g_EntityList = new EntityList;

	getEntityList().m_positionTracker.setPosition( c_default_window_pos );

	GlobalPreferenceSystem().registerPreference( "EntityInfoDlg", make_property<WindowPositionTracker_String>( getEntityList().m_positionTracker ) );

	typedef FreeCaller<void(const Selectable&), EntityList_SelectionChanged> EntityListSelectionChangedCaller;
	GlobalSelectionSystem().addSelectionChangeCallback( EntityListSelectionChangedCaller() );
}
void EntityList_Destroy(){
	delete g_EntityList;

	graph_tree_model_erase( scene_graph_get_tree_model(), StaticNullSelectedInstance::instance() );
}
