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

// LoadPortalFileDialog.cpp : implementation file
//

#include "LoadPortalFileDialog.h"

#include <gtk/gtk.h>
#include <gtkutil/pointer.h>
#include "stream/stringstream.h"
#include "convert.h"
#include "gtkutil/pointer.h"

#include "qerplugin.h"

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

static void change_clicked(ui::Widget widget, gpointer data ){
	char* filename = NULL;

	auto file_sel = ui::Widget::from(gtk_file_chooser_dialog_new ( "Locate portal (.prt) file", nullptr, GTK_FILE_CHOOSER_ACTION_OPEN,
											 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
											 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
											 nullptr));

	gtk_window_set_transient_for( GTK_WINDOW( file_sel ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_position( GTK_WINDOW( file_sel ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( file_sel ), TRUE );

	gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(file_sel), portals.fn );

	if (gtk_dialog_run (GTK_DIALOG (file_sel)) == GTK_RESPONSE_ACCEPT)
	{
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (file_sel));
	}
	ui::Widget(file_sel).destroy();

	if ( filename != NULL ) {
		strcpy( portals.fn, filename );
		gtk_entry_set_text( GTK_ENTRY( data ), filename );
		g_free( filename );
	}
}

int DoLoadPortalFileDialog(){
	int loop = 1, ret = IDCANCEL;

	auto dlg = ui::Window( ui::window_type::TOP );

	gtk_window_set_transient_for( GTK_WINDOW( dlg ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_position( GTK_WINDOW( dlg ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( dlg ), TRUE );

	gtk_window_set_title( dlg, "Load .prt" );
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

	auto entry = ui::Entry(ui::New);
	entry.show();
	gtk_editable_set_editable( GTK_EDITABLE( entry ), FALSE );
	vbox.pack_start( entry, FALSE, FALSE, 0 );

	auto hbox = ui::HBox( FALSE, 5 );
	hbox.show();
	vbox.pack_start( hbox, FALSE, FALSE, 0 );

	auto check3d = ui::CheckButton( "Show 3D" );
	check3d.show();
	hbox.pack_start( check3d, FALSE, FALSE, 0 );

	auto check2d = ui::CheckButton( "Show 2D" );
	check2d.show();
	hbox.pack_start( check2d, FALSE, FALSE, 0 );

	auto button = ui::Button( "Change" );
	button.show();
	hbox.pack_end(button, FALSE, FALSE, 0);
	button.connect( "clicked", G_CALLBACK( change_clicked ), entry );
	button.dimensions(60, -1);

	hbox = ui::HBox( FALSE, 5 );
	hbox.show();
	vbox.pack_start( hbox, FALSE, FALSE, 0 );

	button = ui::Button( "Cancel" );
	button.show();
	hbox.pack_end(button, FALSE, FALSE, 0);
	button.connect( "clicked",
						G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDCANCEL ) );
	button.dimensions(60, -1);

	button = ui::Button( "OK" );
	button.show();
	hbox.pack_end(button, FALSE, FALSE, 0);
	button.connect( "clicked",
						G_CALLBACK( dialog_button_callback ), GINT_TO_POINTER( IDOK ) );
	button.dimensions(60, -1);

	strcpy( portals.fn, GlobalRadiant().getMapName() );
	char* fn = strrchr( portals.fn, '.' );
	if ( fn != NULL ) {
		strcpy( fn, ".prt" );
	}

	StringOutputStream value( 256 );
	value << portals.fn;
	gtk_entry_set_text( GTK_ENTRY( entry ), value.c_str() );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check2d ), portals.show_2d );
	gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( check3d ), portals.show_3d );

	gtk_grab_add( dlg );
	dlg.show();

	while ( loop )
		gtk_main_iteration();

	if ( ret == IDOK ) {
		portals.Purge();

		portals.show_3d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( check3d ) ) ? true : false;
		portals.show_2d = gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( check2d ) ) ? true : false;
	}

	gtk_grab_remove( dlg );
	dlg.destroy();

	return ret;
}
