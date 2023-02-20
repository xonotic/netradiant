#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "debugging/debugging.h"
#include "callbacks.h"
#include "support.h"
#include "gtkutil/dialog.h"
#include "plugin.h"

#define GLADE_HOOKUP_OBJECT( component,widget,name ) \
	g_object_set_data_full( G_OBJECT( component ), name, \
							g_object_ref( (void *) widget ), (GDestroyNotify) g_object_unref )

#define GLADE_HOOKUP_OBJECT_NO_REF( component,widget,name )	\
	g_object_set_data( G_OBJECT( component ), name, (void *) widget )

// created by glade
ui::Widget create_w_plugplug2( ui::Window main_window ){
	GSList *r_collapse_group = NULL;
	ModalDialog dialog;

	auto w_plugplug2 = main_window.create_dialog_window( "BrushExport", G_CALLBACK( dialog_delete_callback ), &dialog );
	gtk_widget_set_name( w_plugplug2, "w_plugplug2" );
	gtk_window_set_position( GTK_WINDOW( w_plugplug2 ), GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_transient_for( GTK_WINDOW( w_plugplug2 ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_destroy_with_parent( w_plugplug2, TRUE );

	auto vbox1 = ui::VBox( FALSE, 0 );
	gtk_widget_set_name( vbox1, "vbox1" );
	vbox1.show();
	w_plugplug2.add(vbox1);
	gtk_container_set_border_width( GTK_CONTAINER( vbox1 ), 5 );

	auto hbox2 = ui::HBox( TRUE, 5 );
	gtk_widget_set_name( hbox2, "hbox2" );
	hbox2.show();
	vbox1.pack_start( hbox2, FALSE, FALSE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( hbox2 ), 5 );

	auto vbox4 = ui::VBox( TRUE, 0 );
	gtk_widget_set_name( vbox4, "vbox4" );
	vbox4.show();
	hbox2.pack_start( vbox4, TRUE, FALSE, 0 );

	auto r_collapse = ui::Widget::from(gtk_radio_button_new_with_mnemonic( NULL, "Collapse mesh" ));
	gtk_widget_set_name( r_collapse, "r_collapse" );
	gtk_widget_set_tooltip_text(r_collapse, "Collapse all brushes into a single group");
	r_collapse.show();
	vbox4.pack_start( r_collapse, FALSE, FALSE, 0 );
	gtk_radio_button_set_group( GTK_RADIO_BUTTON( r_collapse ), r_collapse_group );
	r_collapse_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( r_collapse ) );

	auto r_collapsebymaterial = ui::Widget::from(gtk_radio_button_new_with_mnemonic( NULL, "Collapse by material" ));
	gtk_widget_set_name( r_collapsebymaterial, "r_collapsebymaterial" );
	gtk_widget_set_tooltip_text(r_collapsebymaterial, "Collapse into groups by material");
	r_collapsebymaterial.show();
	vbox4.pack_start( r_collapsebymaterial, FALSE, FALSE, 0 );
	gtk_radio_button_set_group( GTK_RADIO_BUTTON( r_collapsebymaterial ), r_collapse_group );
	r_collapse_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( r_collapsebymaterial ) );

	auto r_nocollapse = ui::Widget::from(gtk_radio_button_new_with_mnemonic( NULL, "Don't collapse" ));
	gtk_widget_set_name( r_nocollapse, "r_nocollapse" );
	gtk_widget_set_tooltip_text(r_nocollapse, "Every brush is stored in its own group");
	r_nocollapse.show();
	vbox4.pack_start( r_nocollapse, FALSE, FALSE, 0 );
	gtk_radio_button_set_group( GTK_RADIO_BUTTON( r_nocollapse ), r_collapse_group );
	r_collapse_group = gtk_radio_button_get_group( GTK_RADIO_BUTTON( r_nocollapse ) );

	auto vbox3 = ui::VBox( FALSE, 0 );
	gtk_widget_set_name( vbox3, "vbox3" );
	vbox3.show();
	hbox2.pack_start( vbox3, FALSE, FALSE, 0 );

	auto b_export = ui::Button::from(gtk_button_new_from_stock( "gtk-save" ));
	gtk_widget_set_name( b_export, "b_export" );
	b_export.show();
	vbox3.pack_start( b_export, TRUE, FALSE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( b_export ), 5 );

	auto b_close = ui::Button::from(gtk_button_new_from_stock( "gtk-cancel" ));
	gtk_widget_set_name( b_close, "b_close" );
	b_close.show();
	vbox3.pack_start( b_close, TRUE, FALSE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( b_close ), 5 );

	auto vbox2 = ui::VBox( FALSE, 5 );
	gtk_widget_set_name( vbox2, "vbox2" );
	vbox2.show();
	vbox1.pack_start( vbox2, TRUE, TRUE, 0 );
	gtk_container_set_border_width( GTK_CONTAINER( vbox2 ), 2 );

	auto label1 = ui::Label( "Ignored materials:" );
	gtk_widget_set_name( label1, "label1" );
	label1.show();
	vbox2.pack_start( label1, FALSE, FALSE, 0 );

	auto scrolledwindow1 = ui::ScrolledWindow(ui::New);
	gtk_widget_set_name( scrolledwindow1, "scrolledwindow1" );
	scrolledwindow1.show();
	vbox2.pack_start( scrolledwindow1, TRUE, TRUE, 0 );
	gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW( scrolledwindow1 ), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC );
	gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scrolledwindow1 ), GTK_SHADOW_IN );

	auto t_materialist = ui::TreeView(ui::New);
	gtk_widget_set_name( t_materialist, "t_materialist" );
	t_materialist.show();
	scrolledwindow1.add(t_materialist);
	gtk_tree_view_set_headers_visible(t_materialist, FALSE );
	gtk_tree_view_set_enable_search(t_materialist, FALSE );

	auto ed_materialname = ui::Entry(ui::New);
	gtk_widget_set_name( ed_materialname, "ed_materialname" );
	ed_materialname.show();
	vbox2.pack_start( ed_materialname, FALSE, FALSE, 0 );

	auto hbox1 = ui::HBox( TRUE, 0 );
	gtk_widget_set_name( hbox1, "hbox1" );
	hbox1.show();
	vbox2.pack_start( hbox1, FALSE, FALSE, 0 );

	auto b_addmaterial = ui::Button::from(gtk_button_new_from_stock( "gtk-add" ));
	gtk_widget_set_name( b_addmaterial, "b_addmaterial" );
	b_addmaterial.show();
	hbox1.pack_start( b_addmaterial, FALSE, FALSE, 0 );

	auto b_removematerial = ui::Button::from(gtk_button_new_from_stock( "gtk-remove" ));
	gtk_widget_set_name( b_removematerial, "b_removematerial" );
	b_removematerial.show();
	hbox1.pack_start( b_removematerial, FALSE, FALSE, 0 );

	auto t_limitmatnames = ui::Widget::from(gtk_check_button_new_with_mnemonic( "Use short material names (max. 20 chars)" ));
	gtk_widget_set_name( t_limitmatnames, "t_limitmatnames" );
	t_limitmatnames.show();
	vbox2.pack_end( t_limitmatnames, FALSE, FALSE, 0 );

	auto t_objects = ui::Widget::from(gtk_check_button_new_with_mnemonic( "Create (o)bjects instead of (g)roups" ));
	gtk_widget_set_name( t_objects, "t_objects" );
	t_objects.show();
	vbox2.pack_end(t_objects, FALSE, FALSE, 0);

	auto t_exportmaterials = ui::CheckButton::from(gtk_check_button_new_with_mnemonic( "Create material information (.mtl file)" ));
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( t_exportmaterials ), true );
	gtk_widget_set_name( t_exportmaterials, "t_exportmaterials" );
	t_exportmaterials.show();
	vbox2.pack_end(t_exportmaterials, FALSE, FALSE, 10);

	using namespace callbacks;
	w_plugplug2.connect( "destroy", G_CALLBACK( OnDestroy ), NULL );
	g_signal_connect_swapped( G_OBJECT( b_close ), "clicked", G_CALLBACK( OnDestroy ), NULL );

	b_export.connect( "clicked", G_CALLBACK( OnExportClicked ), NULL );
	b_addmaterial.connect( "clicked", G_CALLBACK( OnAddMaterial ), NULL );
	b_removematerial.connect( "clicked", G_CALLBACK( OnRemoveMaterial ), NULL );
	t_exportmaterials.connect( "clicked", G_CALLBACK( OnExportMatClicked ), NULL );

	/* Store pointers to all widgets, for use by lookup_widget(). */
	GLADE_HOOKUP_OBJECT_NO_REF( w_plugplug2, w_plugplug2, "w_plugplug2" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, vbox1, "vbox1" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, hbox2, "hbox2" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, vbox4, "vbox4" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, r_collapse, "r_collapse" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, r_collapsebymaterial, "r_collapsebymaterial" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, r_nocollapse, "r_nocollapse" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, vbox3, "vbox3" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, b_export, "b_export" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, b_close, "b_close" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, vbox2, "vbox2" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, label1, "label1" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, scrolledwindow1, "scrolledwindow1" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, t_materialist, "t_materialist" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, ed_materialname, "ed_materialname" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, hbox1, "hbox1" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, b_addmaterial, "b_addmaterial" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, b_removematerial, "b_removematerial" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, t_exportmaterials, "t_exportmaterials" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, t_limitmatnames, "t_limitmatnames" );
	GLADE_HOOKUP_OBJECT( w_plugplug2, t_objects, "t_objects" );

	return w_plugplug2;
}

// global main window, is 0 when not created
ui::Widget g_brushexp_window{ui::null};

// spawn plugin window (and make sure it got destroyed first or never created)
void CreateWindow( ui::Window main_window ){
	ASSERT_TRUE( !g_brushexp_window );

	ui::Widget wnd = create_w_plugplug2( main_window );

	// column & renderer
    auto col = ui::TreeViewColumn::from(gtk_tree_view_column_new());
	gtk_tree_view_column_set_title( col, "materials" );
	auto view = ui::TreeView::from(lookup_widget(wnd, "t_materialist"));
	gtk_tree_view_append_column(view, col );
	auto renderer = ui::CellRendererText(ui::New);
	gtk_tree_view_insert_column_with_attributes(view, -1, "", renderer, "text", 0, NULL );

	// list store
	auto ignorelist = ui::ListStore::from(gtk_list_store_new( 1, G_TYPE_STRING ));
	gtk_tree_view_set_model(view, ignorelist );
    ignorelist.unref();

	gtk_widget_show_all( wnd );
	g_brushexp_window = wnd;
}

void DestroyWindow(){
	ASSERT_TRUE( g_brushexp_window );
	ui::Widget(g_brushexp_window).destroy();
	g_brushexp_window = ui::Widget(ui::null);
}

bool IsWindowOpen(){
	return g_brushexp_window;
}
