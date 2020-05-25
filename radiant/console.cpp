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

#include "console.h"

#include <time.h>
#include <uilib/uilib.h>
#include <gtk/gtk.h>

#include "gtkutil/accelerator.h"
#include "gtkutil/messagebox.h"
#include "gtkutil/container.h"
#include "gtkutil/menu.h"
#include "gtkutil/nonmodal.h"
#include "stream/stringstream.h"
#include "convert.h"

#include "version.h"
#include "aboutmsg.h"
#include "gtkmisc.h"
#include "mainframe.h"

// handle to the console log file
namespace
{
FILE* g_hLogFile;
}

bool g_Console_enableLogging = false;

// called whenever we need to open/close/check the console log file
void Sys_LogFile( bool enable ){
	if ( enable && !g_hLogFile ) {
		// settings say we should be logging and we don't have a log file .. so create it
		if ( !SettingsPath_get()[0] ) {
			return; // cannot open a log file yet
		}
		// open a file to log the console (if user prefs say so)
		// the file handle is g_hLogFile
		// the log file is erased
		StringOutputStream name( 256 );
		name << SettingsPath_get() << "radiant.log";
		g_hLogFile = fopen( name.c_str(), "w" );
		if ( g_hLogFile != 0 ) {
			globalOutputStream() << "Started logging to " << name.c_str() << "\n";
			time_t localtime;
			time( &localtime );
			globalOutputStream() << "Today is: " << ctime( &localtime )
								 << "This is NetRadiant '" RADIANT_VERSION "' compiled " __DATE__ "\n" RADIANT_ABOUTMSG "\n";
		}
		else{
			ui::alert( ui::root, "Failed to create log file, check write permissions in Radiant directory.\n",
							"Console logging", ui::alert_type::OK, ui::alert_icon::Error );
		}
	}
	else if ( !enable && g_hLogFile != 0 ) {
		// settings say we should not be logging but still we have an active logfile .. close it
		time_t localtime;
		time( &localtime );
		globalOutputStream() << "Closing log file at " << ctime( &localtime ) << "\n";
		fclose( g_hLogFile );
		g_hLogFile = 0;
	}
}

ui::TextView g_console{ui::null};

void console_clear(){
	g_console.text("");
}

void console_populate_popup( ui::TextView textview, ui::Menu menu, gpointer user_data ){
	menu_separator( menu );

	ui::Widget item(ui::MenuItem( "Clear" ));
	item.connect( "activate", G_CALLBACK( console_clear ), 0 );
	item.show();
	menu.add(item);
}

gboolean destroy_set_null( ui::Window widget, ui::Widget* p ){
	*p = ui::Widget{ui::null};
	return FALSE;
}

WidgetFocusPrinter g_consoleWidgetFocusPrinter( "console" );

ui::Widget Console_constructWindow( ui::Window toplevel ){
	auto scr = ui::ScrolledWindow(ui::New);
	scr.overflow(ui::Policy::AUTOMATIC, ui::Policy::AUTOMATIC);
	gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW( scr ), GTK_SHADOW_IN );
	scr.show();

	{
		auto text = ui::TextView(ui::New);
		text.dimensions(0, -1); // allow shrinking
		gtk_text_view_set_wrap_mode( text, GTK_WRAP_WORD );
		gtk_text_view_set_editable( text, FALSE );
		scr.add(text);
		text.show();
		g_console = text;

		//globalExtendedASCIICharacterSet().print();

		widget_connect_escape_clear_focus_widget( g_console );

		//g_consoleWidgetFocusPrinter.connect(g_console);

		g_console.connect( "populate-popup", G_CALLBACK( console_populate_popup ), 0 );
		g_console.connect( "destroy", G_CALLBACK( destroy_set_null ), &g_console );
	}

	gtk_container_set_focus_chain( GTK_CONTAINER( scr ), NULL );

	return scr;
}

//#pragma GCC push_options
//#pragma GCC optimize ("O0")

class GtkTextBufferOutputStream : public TextOutputStream
{
GtkTextBuffer* textBuffer;
GtkTextIter* iter;
GtkTextTag* tag;
public:
GtkTextBufferOutputStream( GtkTextBuffer* textBuffer, GtkTextIter* iter, GtkTextTag* tag ) : textBuffer( textBuffer ), iter( iter ), tag( tag ){
}
std::size_t __attribute__((optimize("O0"))) write( const char* buffer, std::size_t length ){
	gtk_text_buffer_insert_with_tags( textBuffer, iter, buffer, gint( length ), tag, NULL );
	return length;
}
};

//#pragma GCC pop_options

std::size_t Sys_Print( int level, const char* buf, std::size_t length ){
	bool contains_newline = std::find( buf, buf + length, '\n' ) != buf + length;

	if ( level == SYS_ERR ) {
		Sys_LogFile( true );
	}

	if ( g_hLogFile != 0 ) {
		fwrite( buf, 1, length, g_hLogFile );
		if ( contains_newline ) {
			fflush( g_hLogFile );
		}
	}

	if ( level != SYS_NOCON ) {
		if ( g_console ) {
			auto buffer = gtk_text_view_get_buffer( g_console );

			GtkTextIter iter;
			gtk_text_buffer_get_end_iter( buffer, &iter );

			static auto end = gtk_text_buffer_create_mark( buffer, "end", &iter, FALSE );

			const GdkColor yellow = { 0, 0xb0ff, 0xb0ff, 0x0000 };
			const GdkColor red = { 0, 0xffff, 0x0000, 0x0000 };

			static auto error_tag = gtk_text_buffer_create_tag( buffer, "red_foreground", "foreground-gdk", &red, NULL );
			static auto warning_tag = gtk_text_buffer_create_tag( buffer, "yellow_foreground", "foreground-gdk", &yellow, NULL );
			static auto standard_tag = gtk_text_buffer_create_tag( buffer, "black_foreground", NULL );
			GtkTextTag* tag;
			switch ( level )
			{
			case SYS_WRN:
				tag = warning_tag;
				break;
			case SYS_ERR:
				tag = error_tag;
				break;
			case SYS_STD:
			case SYS_VRB:
			default:
				tag = standard_tag;
				break;
			}


			{
				GtkTextBufferOutputStream textBuffer( buffer, &iter, tag );
				if ( !globalCharacterSet().isUTF8() ) {
					BufferedTextOutputStream<GtkTextBufferOutputStream> buffered( textBuffer );
					buffered << StringRange( buf, buf + length );
				}
				else
				{
					textBuffer << StringRange( buf, buf + length );
				}
			}

			// update console widget immediatly if we're doing something time-consuming
			if ( contains_newline ) {
				gtk_text_view_scroll_mark_onscreen( g_console, end );

				if ( !ScreenUpdates_Enabled() && gtk_widget_get_realized( g_console ) ) {
					ScreenUpdates_process();
				}
			}
		}
	}
	return length;
}


class SysPrintOutputStream : public TextOutputStream
{
public:
std::size_t write( const char* buffer, std::size_t length ){
	return Sys_Print( SYS_STD, buffer, length );
}
};

class SysPrintErrorStream : public TextOutputStream
{
public:
std::size_t write( const char* buffer, std::size_t length ){
	return Sys_Print( SYS_ERR, buffer, length );
}
};

SysPrintOutputStream g_outputStream;

TextOutputStream& getSysPrintOutputStream(){
	return g_outputStream;
}

SysPrintErrorStream g_errorStream;

TextOutputStream& getSysPrintErrorStream(){
	return g_errorStream;
}
