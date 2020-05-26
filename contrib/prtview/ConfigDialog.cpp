/*
   PrtView plugin for GtkRadiant
   Copyright (C) 2001 Geoffrey Dewan, Loki software and qeradiant.com

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ConfigDialog.h"
#include <stdio.h>
#include <gtk/gtk.h>
#include <uilib/uilib.h>
#include "gtkutil/pointer.h"

#include "iscenegraph.h"

#include "prtview.h"
#include "portals.h"

static void dialog_button_callback( ui::Widget widget, gpointer data ){
	int *loop, *ret;

	auto parent = widget.window();
	loop = (int*)g_object_get_data( G_OBJECT( parent ), "loop" );
	ret = (int*)g_object_get_data( G_OBJECT( parent ), "ret" );

	*loop = 0;
	*ret = gpointer_to_int( data );
}

static gint dialog_delete_callback( ui::Widget widget, GdkEvent* event, gpointer data ){
	widget.hide();
	int *loop = (int *) g_object_get_data(G_OBJECT(widget), "loop");
	*loop = 0;
	return TRUE;
}

// =============================================================================
// Color selection dialog

static int DoColor( PackedColour *c ){
	GdkColor clr;
	int loop = 1, ret = IDCANCEL;

	clr.red = (guint16) (GetRValue(*c) * (65535 / 255));
	clr.blue = (guint16) (GetGValue(*c) * (65535 / 255));
	clr.green = (guint16) (GetBValue(*c) * (65535 / 255));

	auto dlg = ui::Widget::from(gtk_color_selection_dialog_new( "Choose Color" ));

	gtk_window_set_transient_for( GTK_WINDOW( dlg ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_position( GTK_WINDOW( dlg ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( dlg ), TRUE );
	gtk_color_selection_set_current_color( GTK_COLOR_SELECTION( gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(dlg)) ), &clr );
	dlg.connect( "delete_event", G_CALLBACK( dialog_delete_callback ), NULL );
	dlg.connect( "destroy", G_CALLBACK( gtk_widget_destroy ), NULL );

	GtkWidget *ok_button, *cancel_button;
	g_object_get(dlg, "ok-button", &ok_button, "cancel-button", &cancel_button, nullptr);

	ui::Widget::from(ok_button).connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDOK ) );
	ui::Widget::from(cancel_button).connect( "clicked", G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDCANCEL ) );
	g_object_set_data( G_OBJECT( dlg ), "loop", &loop );
	g_object_set_data( G_OBJECT( dlg ), "ret", &ret );

	dlg.show();
	gtk_grab_add( dlg );

	while ( loop )
		gtk_main_iteration();

	gtk_color_selection_get_current_color( GTK_COLOR_SELECTION( gtk_color_selection_dialog_get_color_selection(GTK_COLOR_SELECTION_DIALOG(dlg)) ), &clr );

	gtk_grab_remove( dlg );
	dlg.destroy();

	if ( ret == IDOK ) {
		*c = RGB( clr.red / (65535 / 255), clr.green / (65535 / 255), clr.blue / (65535 / 255));
	}

	return ret;
}

static void Set2DText(ui::Widget label ){
	char s[40];

	sprintf( s, "Line Width = %6.3f", portals.width_2d * 0.5f );

	gtk_label_set_text( GTK_LABEL( label ), s );
}

static void Set3DText(ui::Widget label ){
	char s[40];

	sprintf( s, "Line Width = %6.3f", portals.width_3d * 0.5f );

	gtk_label_set_text( GTK_LABEL( label ), s );
}

static void Set3DTransText(ui::Widget label ){
	char s[40];

	sprintf( s, "Polygon transparency = %d%%", (int)portals.trans_3d );

	gtk_label_set_text( GTK_LABEL( label ), s );
}

static void SetClipText(ui::Widget label ){
	char s[40];

	sprintf( s, "Cubic clip range = %d", (int)portals.clip_range * 64 );

	gtk_label_set_text( GTK_LABEL( label ), s );
}

static void OnScroll2d(ui::Adjustment adj, gpointer data ){
	portals.width_2d = static_cast<float>( gtk_adjustment_get_value(adj) );
	Set2DText( ui::Widget::from(data) );

	Portals_shadersChanged();
	SceneChangeNotify();
}

static void OnScroll3d(ui::Adjustment adj, gpointer data ){
	portals.width_3d = static_cast<float>( gtk_adjustment_get_value(adj) );
	Set3DText( ui::Widget::from( data ) );

	SceneChangeNotify();
}

static void OnScrollTrans(ui::Adjustment adj, gpointer data ){
	portals.trans_3d = static_cast<float>( gtk_adjustment_get_value(adj) );
	Set3DTransText( ui::Widget::from( data ) );

	SceneChangeNotify();
}

static void OnScrollClip(ui::Adjustment adj, gpointer data ){
	portals.clip_range = static_cast<float>( gtk_adjustment_get_value(adj) );
	SetClipText( ui::Widget::from( data ) );

	SceneChangeNotify();
}

static void OnAntiAlias2d(ui::Widget widget, gpointer data ){
	portals.aa_2d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	Portals_shadersChanged();

	SceneChangeNotify();
}

static void OnConfig2d(ui::Widget widget, gpointer data ){
	portals.show_2d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	SceneChangeNotify();
}

static void OnColor2d(ui::Widget widget, gpointer data ){
	if ( DoColor( &portals.color_2d ) == IDOK ) {
		Portals_shadersChanged();

		SceneChangeNotify();
	}
}

static void OnConfig3d(ui::Widget widget, gpointer data ){
	portals.show_3d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	SceneChangeNotify();
}


static void OnAntiAlias3d(ui::Widget widget, gpointer data ){
	portals.aa_3d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	Portals_shadersChanged();
	SceneChangeNotify();
}

static void OnColor3d(ui::Widget widget, gpointer data ){
	if ( DoColor( &portals.color_3d ) == IDOK ) {
		Portals_shadersChanged();

		SceneChangeNotify();
	}
}

static void OnColorFog(ui::Widget widget, gpointer data ){
	if ( DoColor( &portals.color_fog ) == IDOK ) {
		Portals_shadersChanged();

		SceneChangeNotify();
	}
}

static void OnFog(ui::Widget widget, gpointer data ){
	portals.fog = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	Portals_shadersChanged();
	SceneChangeNotify();
}

static void OnSelchangeZbuffer(ui::Widget widget, gpointer data ){
	portals.zbuffer = gpointer_to_int( data );

	Portals_shadersChanged();
	SceneChangeNotify();
}

static void OnPoly(ui::Widget widget, gpointer data ){
	portals.polygons = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) );

	SceneChangeNotify();
}

static void OnLines(ui::Widget widget, gpointer data ){
	portals.lines = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) );

	SceneChangeNotify();
}

static void OnClip(ui::Widget widget, gpointer data ){
	portals.clip = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget ) ) ? true : false;

	SceneChangeNotify();
}

void DoConfigDialog(){
	int loop = 1, ret = IDCANCEL;

	auto dlg = ui::Window( ui::window_type::TOP );

	gtk_window_set_transient_for( GTK_WINDOW( dlg ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_position( GTK_WINDOW( dlg ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( dlg ), TRUE );

	gtk_window_set_title( dlg, "Portal Viewer Configuration" );
	dlg.connect( "delete_event",
						G_CALLBACK( dialog_delete_callback ), NULL );
	dlg.connect( "destroy",
						G_CALLBACK( gtk_widget_destroy ), NULL );
	g_object_set_data( G_OBJECT( dlg ), "loop", &loop );
	g_object_set_data( G_OBJECT( dlg ), "ret", &ret );

	auto vbox = ui::VBox( FALSE, 5 );
	vbox.show();
	dlg.add(vbox);
	gtk_container_set_border_width( GTK_CONTAINER( vbox ), 5 );

	auto frame = ui::Frame( "3D View" );
	frame.show();
	vbox.pack_start( frame, TRUE, TRUE, 0 );

	auto vbox2 = ui::VBox( FALSE, 5 );
	vbox2.show();
	frame.add(vbox2);
	gtk_container_set_border_width( GTK_CONTAINER( vbox2 ), 5 );

	auto hbox = ui::HBox( FALSE, 5 );
	hbox.show();
	vbox2.pack_start( hbox, TRUE, TRUE, 0 );

	auto adj = ui::Adjustment( portals.width_3d, 2, 40, 1, 1, 0 );
	auto lw3slider = ui::HScale( adj );
	lw3slider.show();
	hbox.pack_start( lw3slider, TRUE, TRUE, 0 );
	gtk_scale_set_draw_value( GTK_SCALE( lw3slider ), FALSE );

	auto lw3label = ui::Label( "" );
	lw3label.show();
	hbox.pack_start( lw3label, FALSE, TRUE, 0 );
	adj.connect( "value_changed", G_CALLBACK( OnScroll3d ), lw3label );

	auto table = ui::Table( 2, 4, FALSE );
	table.show();
	vbox2.pack_start( table, TRUE, TRUE, 0 );
    gtk_table_set_row_spacings(table, 5);
    gtk_table_set_col_spacings(table, 5);

	auto button = ui::Button( "Color" );
	button.show();
    table.attach(button, {0, 1, 0, 1}, {GTK_FILL, 0});
	button.connect( "clicked", G_CALLBACK( OnColor3d ), NULL );

	button = ui::Button( "Depth Color" );
	button.show();
    table.attach(button, {0, 1, 1, 2}, {GTK_FILL, 0});
	button.connect( "clicked", G_CALLBACK( OnColorFog ), NULL );

	auto aa3check = ui::CheckButton( "Anti-Alias (May not work on some video cards)" );
	aa3check.show();
    table.attach(aa3check, {1, 4, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});
	aa3check.connect( "toggled", G_CALLBACK( OnAntiAlias3d ), NULL );

	auto depthcheck = ui::CheckButton( "Depth Cue" );
	depthcheck.show();
    table.attach(depthcheck, {1, 2, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});
	depthcheck.connect( "toggled", G_CALLBACK( OnFog ), NULL );

	auto linescheck = ui::CheckButton( "Lines" );
	linescheck.show();
    table.attach(linescheck, {2, 3, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});
	linescheck.connect( "toggled", G_CALLBACK( OnLines ), NULL );

	auto polyscheck = ui::CheckButton( "Polygons" );
	polyscheck.show();
    table.attach(polyscheck, {3, 4, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});
	polyscheck.connect( "toggled", G_CALLBACK( OnPoly ), NULL );

	auto zlist = ui::ComboBoxText(ui::New);
	zlist.show();
	vbox2.pack_start( zlist, TRUE, FALSE, 0 );

	gtk_combo_box_text_append_text(zlist, "Z-Buffer Test and Write (recommended for solid or no polygons)");
	gtk_combo_box_text_append_text(zlist, "Z-Buffer Test Only (recommended for transparent polygons)");
	gtk_combo_box_text_append_text(zlist, "Z-Buffer Off");

	zlist.connect("changed", G_CALLBACK(+[](ui::ComboBox self, void *) {
		OnSelchangeZbuffer(self, GINT_TO_POINTER(gtk_combo_box_get_active(self)));
	}), nullptr);

	table = ui::Table( 2, 2, FALSE );
	table.show();
	vbox2.pack_start( table, TRUE, TRUE, 0 );
    gtk_table_set_row_spacings(table, 5);
    gtk_table_set_col_spacings(table, 5);

	adj = ui::Adjustment( portals.trans_3d, 0, 100, 1, 1, 0 );
	auto transslider = ui::HScale( adj );
	transslider.show();
    table.attach(transslider, {0, 1, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});
	gtk_scale_set_draw_value( GTK_SCALE( transslider ), FALSE );

	auto translabel = ui::Label( "" );
	translabel.show();
    table.attach(translabel, {1, 2, 0, 1}, {GTK_FILL, 0});
	gtk_misc_set_alignment( GTK_MISC( translabel ), 0.0, 0.0 );
	adj.connect( "value_changed", G_CALLBACK( OnScrollTrans ), translabel );

	adj = ui::Adjustment( portals.clip_range, 1, 128, 1, 1, 0 );
	auto clipslider = ui::HScale( adj );
	clipslider.show();
    table.attach(clipslider, {0, 1, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});
	gtk_scale_set_draw_value( GTK_SCALE( clipslider ), FALSE );

	auto cliplabel = ui::Label( "" );
	cliplabel.show();
    table.attach(cliplabel, {1, 2, 1, 2}, {GTK_FILL, 0});
	gtk_misc_set_alignment( GTK_MISC( cliplabel ), 0.0, 0.0 );
	adj.connect( "value_changed", G_CALLBACK( OnScrollClip ), cliplabel );

	hbox = ui::HBox( TRUE, 5 );
	hbox.show();
	vbox2.pack_start( hbox, TRUE, FALSE, 0 );

	auto show3check = ui::CheckButton( "Show" );
	show3check.show();
	hbox.pack_start( show3check, TRUE, TRUE, 0 );
	show3check.connect( "toggled", G_CALLBACK( OnConfig3d ), NULL );

	auto portalcheck = ui::CheckButton( "Portal cubic clipper" );
	portalcheck.show();
	hbox.pack_start( portalcheck, TRUE, TRUE, 0 );
	portalcheck.connect( "toggled", G_CALLBACK( OnClip ), NULL );

	frame = ui::Frame( "2D View" );
	frame.show();
	vbox.pack_start( frame, TRUE, TRUE, 0 );

	vbox2 = ui::VBox( FALSE, 5 );
	vbox2.show();
	frame.add(vbox2);
	gtk_container_set_border_width( GTK_CONTAINER( vbox2 ), 5 );

	hbox = ui::HBox( FALSE, 5 );
	hbox.show();
	vbox2.pack_start( hbox, TRUE, FALSE, 0 );

	adj = ui::Adjustment( portals.width_2d, 2, 40, 1, 1, 0 );
	auto lw2slider = ui::HScale( adj );
	lw2slider.show();
	hbox.pack_start( lw2slider, TRUE, TRUE, 0 );
	gtk_scale_set_draw_value( GTK_SCALE( lw2slider ), FALSE );

	auto lw2label = ui::Label( "" );
	lw2label.show();
	hbox.pack_start( lw2label, FALSE, TRUE, 0 );
	adj.connect( "value_changed", G_CALLBACK( OnScroll2d ), lw2label );

	hbox = ui::HBox( FALSE, 5 );
	hbox.show();
	vbox2.pack_start( hbox, TRUE, FALSE, 0 );

	button = ui::Button( "Color" );
	button.show();
	hbox.pack_start( button, FALSE, FALSE, 0 );
	button.connect( "clicked", G_CALLBACK( OnColor2d ), NULL );
	button.dimensions(60, -1);

	auto aa2check = ui::CheckButton( "Anti-Alias (May not work on some video cards)" );
	aa2check.show();
	hbox.pack_start( aa2check, TRUE, TRUE, 0 );
	aa2check.connect( "toggled", G_CALLBACK( OnAntiAlias2d ), NULL );

	hbox = ui::HBox( FALSE, 5 );
	hbox.show();
	vbox2.pack_start( hbox, TRUE, FALSE, 0 );

	auto show2check = ui::CheckButton( "Show" );
	show2check.show();
	hbox.pack_start( show2check, FALSE, FALSE, 0 );
	show2check.connect( "toggled", G_CALLBACK( OnConfig2d ), NULL );

	hbox = ui::HBox( FALSE, 5 );
	hbox.show();
	vbox.pack_start( hbox, FALSE, FALSE, 0 );

	button = ui::Button( "OK" );
	button.show();
	hbox.pack_end(button, FALSE, FALSE, 0);
	button.connect( "clicked",
						G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDOK ) );
	button.dimensions(60, -1);

	// initialize dialog
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( show2check ), portals.show_2d );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( aa2check ), portals.aa_2d );
	Set2DText( lw2label );

	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( show3check ), portals.show_3d );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( depthcheck ), portals.fog );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( polyscheck ), portals.polygons );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( linescheck ), portals.lines );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( aa3check ), portals.aa_3d );
	gtk_combo_box_set_active(zlist, portals.zbuffer);
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( portalcheck ), portals.clip );

	Set3DText( lw3label );
	Set3DTransText( translabel );
	SetClipText( cliplabel );

	gtk_grab_add( dlg );
	dlg.show();

	while ( loop )
		gtk_main_iteration();

	gtk_grab_remove( dlg );
	dlg.destroy();
}
