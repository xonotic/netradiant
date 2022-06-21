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

//
// Find/Replace textures dialogs
//
// Leonardo Zide (leo@lokigames.com)
//

#include "findtexturedialog.h"

#include <gtk/gtk.h>

#include "debugging/debugging.h"

#include "ishaders.h"

#include "gtkutil/window.h"
#include "stream/stringstream.h"

#include "commands.h"
#include "dialog.h"
#include "select.h"
#include "textureentry.h"



class FindTextureDialog : public Dialog
{
public:
WindowPositionTracker m_position_tracker;
static void setReplaceStr( const char* name );
static void setFindStr( const char* name );
static bool isOpen();
static void show();
typedef FreeCaller<void(), &FindTextureDialog::show> ShowCaller;
static void updateTextures( const char* name );

FindTextureDialog();
virtual ~FindTextureDialog();
ui::Window BuildDialog();

void constructWindow( ui::Window parent ){
	m_parent = parent;
	Create();
}
void destroyWindow(){
	Destroy();
}


bool m_bSelectedOnly;
CopiedString m_strFind;
CopiedString m_strReplace;
};

FindTextureDialog g_FindTextureDialog;
static bool g_bFindActive = true;

namespace
{
void FindTextureDialog_apply(){
	StringOutputStream find( 256 );
	StringOutputStream replace( 256 );

	find << "textures/" << g_FindTextureDialog.m_strFind.c_str();
	replace << "textures/" << g_FindTextureDialog.m_strReplace.c_str();
	FindReplaceTextures( find.c_str(), replace.c_str(), g_FindTextureDialog.m_bSelectedOnly );
}

static void OnApply( ui::Widget widget, gpointer data ){
	g_FindTextureDialog.exportData();
	FindTextureDialog_apply();
}

static void OnFind( ui::Widget widget, gpointer data ){
	g_FindTextureDialog.exportData();
	FindTextureDialog_apply();
}

static void OnOK( ui::Widget widget, gpointer data ){
	g_FindTextureDialog.exportData();
	FindTextureDialog_apply();
	g_FindTextureDialog.HideDlg();
}

static void OnClose( ui::Widget widget, gpointer data ){
	g_FindTextureDialog.HideDlg();
}


static gint find_focus_in( ui::Widget widget, GdkEventFocus *event, gpointer data ){
	g_bFindActive = true;
	return FALSE;
}

static gint replace_focus_in( ui::Widget widget, GdkEventFocus *event, gpointer data ){
	g_bFindActive = false;
	return FALSE;
}
}

// =============================================================================
// FindTextureDialog class

FindTextureDialog::FindTextureDialog(){
	m_bSelectedOnly = FALSE;
	//m_position_tracker.setPosition( c_default_window_pos );
}

FindTextureDialog::~FindTextureDialog(){
}

ui::Window FindTextureDialog::BuildDialog(){
    ui::Widget label{ui::null};
	ui::Widget button{ui::null};
	ui::Entry entry{ui::null};

	auto dlg = ui::Window(create_floating_window( "Find / Replace Texture(s)", m_parent ));

	m_position_tracker.connect( dlg );

	auto hbox = ui::HBox( FALSE, 5 );
	hbox.show();
	dlg.add(hbox);
	gtk_container_set_border_width( GTK_CONTAINER( hbox ), 5 );

	auto vbox = ui::VBox( FALSE, 5 );
	vbox.show();
	hbox.pack_start( vbox, TRUE, TRUE, 0 );

    auto table = ui::Table(2, 2, FALSE);
	table.show();
	vbox.pack_start( table, TRUE, TRUE, 0 );
    gtk_table_set_row_spacings(table, 5);
    gtk_table_set_col_spacings(table, 5);

	label = ui::Label( "Find:" );
	label.show();
    table.attach(label, {0, 1, 0, 1}, {GTK_FILL, 0});
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );

	label = ui::Label( "Replace:*" );
	gtk_widget_set_tooltip_text( label, "Empty = search mode" );
	label.show();
    table.attach(label, {0, 1, 1, 2}, {GTK_FILL, 0});
	gtk_misc_set_alignment( GTK_MISC( label ), 0, 0.5 );

	entry = ui::Entry(ui::New);
	entry.show();
    table.attach(entry, {1, 2, 0, 1}, {GTK_EXPAND | GTK_FILL, 0});
	entry.connect( "focus_in_event",
					  G_CALLBACK( find_focus_in ), 0 );
	AddDialogData( entry, m_strFind );
	GlobalTextureEntryCompletion::instance().connect( entry );

	entry = ui::Entry(ui::New);
	gtk_widget_set_tooltip_text( entry, "Empty = search mode" );
	entry.show();
    table.attach(entry, {1, 2, 1, 2}, {GTK_EXPAND | GTK_FILL, 0});
	entry.connect( "focus_in_event",
					  G_CALLBACK( replace_focus_in ), 0 );
	AddDialogData( entry, m_strReplace );
	GlobalTextureEntryCompletion::instance().connect( entry );

	auto check = ui::CheckButton( "Within selected brushes only" );
	check.show();
	vbox.pack_start( check, TRUE, TRUE, 0 );
	AddDialogData( check, m_bSelectedOnly );

	vbox = ui::VBox( FALSE, 5 );
	vbox.show();
	hbox.pack_start( vbox, FALSE, FALSE, 0 );

	button = ui::Button( "Apply" );
	button.show();
	vbox.pack_start( button, FALSE, FALSE, 0 );
	button.connect( "clicked",
					  G_CALLBACK( OnApply ), 0 );
	button.dimensions(60, -1);

	button = ui::Button( "Close" );
	button.show();
	vbox.pack_start( button, FALSE, FALSE, 0 );
	button.connect( "clicked",
					  G_CALLBACK( OnClose ), 0 );
	button.dimensions(60, -1);

	return dlg;
}

void FindTextureDialog::updateTextures( const char* name ){
	if ( isOpen() ) {
		if ( g_bFindActive ) {
			setFindStr( name + 9 );
		}
		else
		{
			setReplaceStr( name + 9 );
		}
	}
}

bool FindTextureDialog::isOpen(){
	return g_FindTextureDialog.GetWidget().visible();
}

void FindTextureDialog::setFindStr( const char* name ){
	g_FindTextureDialog.exportData();
	g_FindTextureDialog.m_strFind = name;
	g_FindTextureDialog.importData();
}

void FindTextureDialog::setReplaceStr( const char* name ){
	g_FindTextureDialog.exportData();
	g_FindTextureDialog.m_strReplace = name;
	g_FindTextureDialog.importData();
}

void FindTextureDialog::show(){
	// workaround for strange gtk behaviour - modifying the contents of a window while it is not visible causes the window position to change without sending a configure_event
	g_FindTextureDialog.m_position_tracker.sync( g_FindTextureDialog.GetWidget() );
	g_FindTextureDialog.ShowDlg();
	gtk_window_present( g_FindTextureDialog.GetWidget() );
}


void FindTextureDialog_constructWindow( ui::Window main_window ){
	g_FindTextureDialog.constructWindow( main_window );
}

void FindTextureDialog_destroyWindow(){
	g_FindTextureDialog.destroyWindow();
}

bool FindTextureDialog_isOpen(){
	return g_FindTextureDialog.isOpen();
}

void FindTextureDialog_selectTexture( const char* name ){
	g_FindTextureDialog.updateTextures( name );
}

#include "preferencesystem.h"

void FindTextureDialog_Construct(){
	GlobalCommands_insert( "FindReplaceTextures", FindTextureDialog::ShowCaller() );
	GlobalPreferenceSystem().registerPreference( "FindReplacehWnd",  make_property_string<WindowPositionTracker_String>( g_FindTextureDialog.m_position_tracker ) );
}

void FindTextureDialog_Destroy(){
}
