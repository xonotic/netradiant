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
// Floating dialog that contains a notebook with at least Entities and Group tabs
// I merged the 2 MS Windows dialogs in a single class
//
// Leonardo Zide (leo@lokigames.com)
//

#include "groupdialog.h"
#include "globaldefs.h"

#include "debugging/debugging.h"

#include <vector>
#include <gtk/gtk.h>

#include "gtkutil/widget.h"
#include "gtkutil/accelerator.h"
#include "entityinspector.h"
#include "gtkmisc.h"
#include "multimon.h"
#include "console.h"
#include "commands.h"
#include "gtkutil/window.h"

#if defined(WORKAROUND_WINDOWS_GTK2_GLWIDGET) || defined(WORKAROUND_MACOS_GTK2_GLWIDGET)
#include "texwindow.h"
#endif // WORKAROUND_WINDOWS_GTK2_GLWIDGET || WORKAROUND_MACOS_GTK2_GLWIDGET

class GroupDlg
{
public:
ui::Widget m_pNotebook{ui::null};
ui::Window m_window{ui::null};

GroupDlg();
void Create( ui::Window parent );

void Show(){
	// workaround for strange gtk behaviour - modifying the contents of a window while it is not visible causes the window position to change without sending a configure_event
	m_position_tracker.sync( m_window );
#define GARUX_GTK_WORKAROUND
#ifndef GARUX_GTK_WORKAROUND
	/* workaround for gtk 2.24 issue: not displayed glwidget after toggle */
	GtkWidget* glwidget = GTK_WIDGET( g_object_get_data( G_OBJECT( m_window ), "glwidget" ) );
	if ( glwidget ){
		//if ( widget_is_visible( glwidget ) )
			//globalOutputStream() << "glwidget have been already visible :0\n"; /* is not hidden aswell, according to this */
		gtk_widget_hide( glwidget );
		gtk_widget_show( glwidget );
	}
#endif
	m_window.show();
}
void Hide(){
	m_window.hide();
}

WindowPositionTracker m_position_tracker;
};

namespace
{
GroupDlg g_GroupDlg;

std::size_t g_current_page;
std::vector<Callback<void(const Callback<void(const char *)> &)>> g_pages;
}

static void workaround_macos_show_hide(){
#ifdef WORKAROUND_MACOS_GTK2_GLWIDGET
	if ( g_current_page == 2 )
	{
		TextureBrowser_showGLWidget();
	}
	else
	{
		TextureBrowser_hideGLWidget();
	}
#endif // WORKAROUND_MACOS_GTK2_GLWIDGET
}

void GroupDialog_updatePageTitle( ui::Window window, std::size_t pageIndex ){
	if ( pageIndex < g_pages.size() ) {
		g_pages[pageIndex]( PointerCaller<GtkWindow, void(const char*), gtk_window_set_title>( window ) );
	}

	workaround_macos_show_hide();
}

static gboolean switch_page( GtkNotebook *notebook, gpointer page, guint page_num, gpointer data ){
	g_current_page = page_num;
	GroupDialog_updatePageTitle( ui::Window::from(data), page_num );
	return FALSE;
}

GroupDlg::GroupDlg() : m_window( ui::null ){
	m_position_tracker.setPosition( c_default_window_pos );
}

void GroupDlg::Create( ui::Window parent ){
	ASSERT_MESSAGE( !m_window, "dialog already created" );

	auto window = ui::Window(create_persistent_floating_window( "Entities", parent ));

	global_accel_connect_window( window );

	window_connect_focus_in_clear_focus_widget( window );

	m_window = window;

#if GDEF_OS_WINDOWS
	if ( g_multimon_globals.m_bStartOnPrimMon ) {
		WindowPosition pos( m_position_tracker.getPosition() );
		PositionWindowOnPrimaryScreen( pos );
		m_position_tracker.setPosition( pos );
	}
#endif
	m_position_tracker.connect( window );

	{
		ui::Widget notebook = ui::Widget::from(gtk_notebook_new());
		notebook.show();
		window.add(notebook);
		gtk_notebook_set_tab_pos( GTK_NOTEBOOK( notebook ), GTK_POS_BOTTOM );
		m_pNotebook = notebook;

		notebook.connect( "switch_page", G_CALLBACK( switch_page ), (gpointer) window );
	}
}

ui::Widget GroupDialog_addPage( const char* tabLabel, ui::Widget widget, const Callback<void(const Callback<void(const char *)> &)>& title ){
	ui::Widget w = ui::Label( tabLabel );
	w.show();
	auto page = ui::Widget::from(gtk_notebook_get_nth_page( GTK_NOTEBOOK( g_GroupDlg.m_pNotebook ), gtk_notebook_insert_page( GTK_NOTEBOOK( g_GroupDlg.m_pNotebook ), widget, w, -1 ) ));
	g_pages.push_back( title );

	return page;
}

bool GroupDialog_isShown(){
	return g_GroupDlg.m_window.visible();
}
void GroupDialog_setShown( bool shown ){
	shown ? g_GroupDlg.Show() : g_GroupDlg.Hide();
}
void GroupDialog_ToggleShow(){
	GroupDialog_setShown( !GroupDialog_isShown() );
}

void GroupDialog_constructWindow( ui::Window main_window ){
	g_GroupDlg.Create( main_window );
}
void GroupDialog_destroyWindow(){
	ASSERT_TRUE( g_GroupDlg.m_window );
	destroy_floating_window( g_GroupDlg.m_window );
	g_GroupDlg.m_window = ui::Window{ui::null};
}

ui::Window GroupDialog_getWindow(){
	return ui::Window(g_GroupDlg.m_window);
}
void GroupDialog_show(){
	g_GroupDlg.Show();
}

ui::Widget GroupDialog_getPage(){
	return ui::Widget::from(gtk_notebook_get_nth_page( GTK_NOTEBOOK( g_GroupDlg.m_pNotebook ), gint( g_current_page ) ) );
}

void GroupDialog_setPage( ui::Widget page ){
	g_current_page = gtk_notebook_page_num( GTK_NOTEBOOK( g_GroupDlg.m_pNotebook ), page );
	gtk_notebook_set_current_page( GTK_NOTEBOOK( g_GroupDlg.m_pNotebook ), gint( g_current_page ) );

	workaround_macos_show_hide();
}

#ifdef WORKAROUND_WINDOWS_GTK2_GLWIDGET
void GroupDialog_cycle(){
	g_current_page = ( g_current_page + 1 ) % g_pages.size();
	gtk_notebook_set_current_page( GTK_NOTEBOOK( g_GroupDlg.m_pNotebook ), gint( g_current_page ) );
}
#endif // WORKAROUND_WINDOWS_GTK2_GLWIDGET

void GroupDialog_showPage( ui::Widget page ){

	if ( GroupDialog_getPage() == page ) {
		GroupDialog_ToggleShow();

#ifdef WORKAROUND_WINDOWS_GTK2_GLWIDGET
		/* workaround for gtk 2.24 issue: not displayed glwidget after toggle */
		/* this is very ugly: cycle to next tab then return to current tab immediately to force the refresh
		 * this fixes the drawing of texture tab when window is restored and current tab is texture tab
		 * this is called for nothing when windows is minimized and called for nothing when current tab
		 * is not texture tab, hopefully it's a workaround that would disappear with gtk 3 */
		GroupDialog_cycle();
		GroupDialog_setPage( page );
#endif // WORKAROUND_WINDOWS_GTK2_GLWIDGET

	}
	else
	{
		g_GroupDlg.m_window.show();
		GroupDialog_setPage( page );
	}

	workaround_macos_show_hide();
}

void GroupDialog_updatePageTitle( ui::Widget page ){
	if ( GroupDialog_getPage() == page ) {
		GroupDialog_updatePageTitle( g_GroupDlg.m_window, g_current_page );
	}
}

#include "preferencesystem.h"

void GroupDialog_Construct(){
	GlobalPreferenceSystem().registerPreference( "EntityWnd", make_property<WindowPositionTracker_String>( g_GroupDlg.m_position_tracker ) );

	GlobalCommands_insert( "ViewEntityInfo", makeCallbackF(GroupDialog_ToggleShow), Accelerator( 'N' ) );
}
void GroupDialog_Destroy(){
}
