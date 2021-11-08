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

#include "dialog.h"

#include <gtk/gtk.h>

#include "button.h"
#include "window.h"

ui::VBox create_dialog_vbox( int spacing, int border ){
	auto vbox = ui::VBox( FALSE, spacing );
	vbox.show();
	gtk_container_set_border_width( GTK_CONTAINER( vbox ), border );
	return vbox;
}

ui::HBox create_dialog_hbox( int spacing, int border ){
	auto hbox = ui::HBox( FALSE, spacing );
	hbox.show();
	gtk_container_set_border_width( GTK_CONTAINER( hbox ), border );
	return hbox;
}

ui::Frame create_dialog_frame( const char* label, ui::Shadow shadow ){
	auto frame = ui::Frame( label );
	frame.show();
	gtk_frame_set_shadow_type( frame, (GtkShadowType) shadow );
	return frame;
}

ui::Table create_dialog_table( unsigned int rows, unsigned int columns, unsigned int row_spacing, unsigned int col_spacing, int border ){
	auto table = ui::Table( rows, columns, FALSE );
	table.show();
	gtk_table_set_row_spacings( table, row_spacing );
	gtk_table_set_col_spacings( table, col_spacing );
	gtk_container_set_border_width( GTK_CONTAINER( table ), border );
	return table;
}

ui::Button create_dialog_button( const char* label, GCallback func, gpointer data ){
	auto button = ui::Button( label );
	button.dimensions(64, -1);
	button.show();
	button.connect( "clicked", func, data );
	return button;
}

ui::Window create_dialog_window( ui::Window parent, const char* title, GCallback func, gpointer data, int default_w, int default_h ){
	ui::Window window = create_floating_window( title, parent );
	gtk_window_set_default_size( window, default_w, default_h );
	gtk_window_set_position( window, GTK_WIN_POS_CENTER_ON_PARENT );
	window.connect( "delete_event", func, data );

	return window;
}

gboolean modal_dialog_button_clicked( ui::Widget widget, ModalDialogButton* button ){
	button->m_dialog.loop = false;
	button->m_dialog.ret = button->m_value;
	return TRUE;
}

gboolean modal_dialog_delete( ui::Widget widget, GdkEvent* event, ModalDialog* dialog ){
	dialog->loop = 0;
	dialog->ret = eIDCANCEL;
	return TRUE;
}

EMessageBoxReturn modal_dialog_show( ui::Window window, ModalDialog& dialog ){
	window.show();
	g_assert( GTK_IS_WINDOW(window) );
	gtk_grab_add( GTK_WIDGET(window) );

	dialog.loop = true;
	while ( dialog.loop )
	{
		gtk_main_iteration();
	}

	window.hide();
	gtk_grab_remove( window  );

	return dialog.ret;
}

ui::Button create_modal_dialog_button( const char* label, ModalDialogButton& button ){
	return create_dialog_button( label, G_CALLBACK( modal_dialog_button_clicked ), &button );
}

ui::Window create_modal_dialog_window( ui::Window parent, const char* title, ModalDialog& dialog, int default_w, int default_h ){
	return create_dialog_window( parent, title, G_CALLBACK( modal_dialog_delete ), &dialog, default_w, default_h );
}

ui::Window create_fixedsize_modal_dialog_window( ui::Window parent, const char* title, ModalDialog& dialog, int width, int height ){
	auto window = create_modal_dialog_window( parent, title, dialog, width, height );

	gtk_window_set_resizable( window, FALSE );
	gtk_window_set_modal( window, TRUE );
	gtk_window_set_position( window, GTK_WIN_POS_CENTER );

	window_remove_minmax( window );

	//window.dimensions(width, height);
	//gtk_window_set_default_size(window, width, height);
	//gtk_window_resize(window, width, height);
	//GdkGeometry geometry = { width, height, -1, -1, width, height, -1, -1, -1, -1, GDK_GRAVITY_STATIC, };
	//gtk_window_set_geometry_hints(window, window, &geometry, (GdkWindowHints)(GDK_HINT_POS|GDK_HINT_MIN_SIZE|GDK_HINT_BASE_SIZE));

	return window;
}

gboolean dialog_button_ok( ui::Widget widget, ModalDialog* data ){
	data->loop = false;
	data->ret = eIDOK;
	return TRUE;
}

gboolean dialog_button_cancel( ui::Widget widget, ModalDialog* data ){
	data->loop = false;
	data->ret = eIDCANCEL;
	return TRUE;
}

gboolean dialog_button_yes( ui::Widget widget, ModalDialog* data ){
	data->loop = false;
	data->ret = eIDYES;
	return TRUE;
}

gboolean dialog_button_no( ui::Widget widget, ModalDialog* data ){
	data->loop = false;
	data->ret = eIDNO;
	return TRUE;
}

gboolean dialog_delete_callback( ui::Widget widget, GdkEventAny* event, ModalDialog* data ){
	widget.hide();
	data->loop = false;
	return TRUE;
}

ui::Window create_simple_modal_dialog_window( const char* title, ModalDialog& dialog, ui::Widget contents ){
	ui::Window window = create_fixedsize_modal_dialog_window(ui::Window{ui::null}, title, dialog );

	auto vbox1 = create_dialog_vbox( 8, 4 );
	window.add(vbox1);

	vbox1.add(contents);

	ui::Alignment alignment = ui::Alignment( 0.5, 0.0, 0.0, 0.0 );
	alignment.show();
	vbox1.pack_start( alignment, FALSE, FALSE, 0 );

	auto button = create_dialog_button( "OK", G_CALLBACK( dialog_button_ok ), &dialog );
	alignment.add(button);
	gtk_widget_grab_focus (button);

	return window;
}

RadioHBox RadioHBox_new( StringArrayRange names ){
	auto hbox = ui::HBox( TRUE, 4 );
	hbox.show();

	GSList* group = 0;
	auto radio = ui::RadioButton(ui::null);
	for ( StringArrayRange::Iterator i = names.first; i != names.last; ++i )
	{
		radio = ui::RadioButton::from( gtk_radio_button_new_with_label( group, *i ) );
		radio.show();
		hbox.pack_start( radio, FALSE, FALSE, 0 );

		group = gtk_radio_button_get_group( radio );
	}

	return RadioHBox( hbox, radio );
}


PathEntry PathEntry_new(){
	auto frame = ui::Frame();
	frame.show();
	gtk_frame_set_shadow_type( frame, GTK_SHADOW_IN );

	// path entry
	auto hbox = ui::HBox( FALSE, 0 );
	hbox.show();

	auto entry = ui::Entry(ui::New);
	gtk_entry_set_has_frame( entry, FALSE );
	entry.show();
	hbox.pack_start( entry, TRUE, TRUE, 0 );

	// browse button
	auto button = ui::Button(ui::New);
	button_set_icon( button, "ellipsis.png" );
	button.show();
	hbox.pack_end(button, FALSE, FALSE, 0);

	frame.add(hbox);

	return PathEntry( frame, entry, button );
}

void PathEntry_setPath( PathEntry& self, const char* path ){
	gtk_entry_set_text( self.m_entry, path );
}
typedef ReferenceCaller<PathEntry, void(const char*), PathEntry_setPath> PathEntrySetPathCaller;

void BrowsedPathEntry_clicked( ui::Widget widget, BrowsedPathEntry* self ){
	self->m_browse( PathEntrySetPathCaller( self->m_entry ) );
}

BrowsedPathEntry::BrowsedPathEntry( const BrowseCallback& browse ) :
	m_entry( PathEntry_new() ),
	m_browse( browse ){
	m_entry.m_button.connect( "clicked", G_CALLBACK( BrowsedPathEntry_clicked ), this );
}


ui::Label DialogLabel_new( const char* name ){
	auto label = ui::Label( name );
	label.show();
	gtk_misc_set_alignment( GTK_MISC( label ), 1, 0.5 );
	gtk_label_set_justify( label, GTK_JUSTIFY_LEFT );

	return label;
}

ui::Table DialogRow_new( const char* name, ui::Widget widget ){
	auto table = ui::Table( 1, 3, TRUE );
	table.show();

	gtk_table_set_col_spacings( table, 4 );
	gtk_table_set_row_spacings( table, 0 );

    table.attach(DialogLabel_new(name), {0, 1, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});

    table.attach(widget, {1, 3, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});

	return table;
}

void DialogVBox_packRow( ui::Box vbox, ui::Widget row ){
	vbox.pack_start( row, FALSE, FALSE, 0 );
}
