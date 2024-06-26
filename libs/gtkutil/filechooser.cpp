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

#include "filechooser.h"

#include "ifiletypes.h"

#include <list>
#include <vector>
#include <gtk/gtk.h>
#include <uilib/uilib.h>

#include "string/string.h"
#include "stream/stringstream.h"
#include "container/array.h"
#include "os/path.h"
#include "os/file.h"

#include "messagebox.h"


struct filetype_pair_t
{
	filetype_pair_t()
		: m_moduleName( "" ){
	}
	filetype_pair_t( const char* moduleName, filetype_t type )
		: m_moduleName( moduleName ), m_type( type ){
	}
	const char* m_moduleName;
	filetype_t m_type;
};

class FileTypeList : public IFileTypeList
{
struct filetype_copy_t
{
	filetype_copy_t( const filetype_pair_t& other )
		: m_moduleName( other.m_moduleName ), m_name( other.m_type.name ), m_pattern( other.m_type.pattern ){
	}
	CopiedString m_moduleName;
	CopiedString m_name;
	CopiedString m_pattern;
};

typedef std::list<filetype_copy_t> Types;
Types m_types;
public:

typedef Types::const_iterator const_iterator;
const_iterator begin() const {
	return m_types.begin();
}
const_iterator end() const {
	return m_types.end();
}

std::size_t size() const {
	return m_types.size();
}

void addType( const char* moduleName, filetype_t type ){
	m_types.push_back( filetype_pair_t( moduleName, type ) );
}
};


class GTKMasks
{
const FileTypeList& m_types;
public:
std::vector<CopiedString> m_filters;
std::vector<CopiedString> m_masks;

GTKMasks( const FileTypeList& types ) : m_types( types ){
	m_masks.reserve( m_types.size() );
	for ( FileTypeList::const_iterator i = m_types.begin(); i != m_types.end(); ++i )
	{
		std::size_t len = strlen( ( *i ).m_name.c_str() ) + strlen( ( *i ).m_pattern.c_str() ) + 3;
		StringOutputStream buffer( len + 1 ); // length + null char

		buffer << ( *i ).m_name.c_str() << " <" << ( *i ).m_pattern.c_str() << ">";

		m_masks.push_back( buffer.c_str() );
	}

	m_filters.reserve( m_types.size() );
	for ( FileTypeList::const_iterator i = m_types.begin(); i != m_types.end(); ++i )
	{
		m_filters.push_back( ( *i ).m_pattern );
	}
}

filetype_pair_t GetTypeForGTKMask( const char *mask ) const {
	std::vector<CopiedString>::const_iterator j = m_masks.begin();
	for ( FileTypeList::const_iterator i = m_types.begin(); i != m_types.end(); ++i, ++j )
	{
		if ( string_equal( ( *j ).c_str(), mask ) ) {
			return filetype_pair_t( ( *i ).m_moduleName.c_str(), filetype_t( ( *i ).m_name.c_str(), ( *i ).m_pattern.c_str() ) );
		}
	}
	return filetype_pair_t();
}

};

static char g_file_dialog_file[1024];

const char* file_dialog_show( ui::Window parent, bool open, const char* title, const char* path, const char* pattern, bool want_load, bool want_import, bool want_save ){
	filetype_t type;

	if ( pattern == 0 ) {
		pattern = "*";
	}

	FileTypeList typelist;
	GlobalFiletypes().getTypeList( pattern, &typelist, want_load, want_import, want_save );

	GTKMasks masks( typelist );

	if ( title == 0 ) {
		title = open ? "Open File" : "Save File";
	}

	ui::Dialog dialog{ui::null};
	if ( open ) {
		dialog = ui::Dialog::from(gtk_file_chooser_dialog_new( title,
											  parent,
											  GTK_FILE_CHOOSER_ACTION_OPEN,
											  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
											  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
											  NULL ));
	}
	else
	{
		dialog = ui::Dialog::from(gtk_file_chooser_dialog_new( title,
											  parent,
											  GTK_FILE_CHOOSER_ACTION_SAVE,
											  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
											  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
											  NULL ));
		gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER( dialog ), "unnamed" );
	}

	gtk_window_set_modal( dialog, TRUE );
	gtk_window_set_position( dialog, GTK_WIN_POS_CENTER_ON_PARENT );

	// we expect an actual path below, if the path is 0 we might crash
	if ( path != nullptr && !string_empty( path ) ) {
		ASSERT_MESSAGE( path_is_absolute( path ), "file_dialog_show: path not absolute: " << makeQuoted( path ) );

		std::string new_path( path );

		// replacing dir separators as appropriate
		for ( char &c : new_path ) {
			if ( c == '/' ) {
			       c = G_DIR_SEPARATOR;
			}
		}
		// remove separator from end of path if required
		if ( new_path.back() == G_DIR_SEPARATOR ) {
			new_path.pop_back();
		}

		gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER( dialog ), new_path.c_str() );
	}

	// we should add all important paths as shortcut folder...
	// gtk_file_chooser_add_shortcut_folder(GTK_FILE_CHOOSER(dialog), "/tmp/", NULL);

	if ( open && masks.m_filters.size() > 1 ){
		GtkFileFilter* filter = gtk_file_filter_new();
		gtk_file_filter_set_name( filter, "Supported formats" );
		for ( std::size_t i = 0; i < masks.m_filters.size(); ++i )
		{
			gtk_file_filter_add_pattern( filter, masks.m_filters[i].c_str() );
		}
		gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( dialog ), filter );
	}

	for ( std::size_t i = 0; i < masks.m_filters.size(); ++i )
	{
		GtkFileFilter* filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern( filter, masks.m_filters[i].c_str() );
		gtk_file_filter_set_name( filter, masks.m_masks[i].c_str() );
		gtk_file_chooser_add_filter( GTK_FILE_CHOOSER( dialog ), filter );
	}

	if ( gtk_dialog_run( GTK_DIALOG( dialog ) ) == GTK_RESPONSE_ACCEPT ) {
		strcpy( g_file_dialog_file, gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dialog ) ) );

		if ( !string_equal( pattern, "*" ) ) {
			GtkFileFilter* filter = gtk_file_chooser_get_filter( GTK_FILE_CHOOSER( dialog ) );
			if ( filter != 0 && !string_equal( gtk_file_filter_get_name( filter ), "Supported formats" ) ) { // no filter set? some file-chooser implementations may allow the user to set no filter, which we treat as 'all files'
				type = masks.GetTypeForGTKMask( gtk_file_filter_get_name( filter ) ).m_type;
				// last ext separator
				const char* extension = path_get_extension( g_file_dialog_file );
				// no extension
				if ( string_empty( extension ) ) {
					strcat( g_file_dialog_file, type.pattern + 1 );
				}
				else
				{
					strcpy( g_file_dialog_file + ( extension - g_file_dialog_file ), type.pattern + 2 );
				}
			}
		}

		// convert back to unix format
		for ( char* w = g_file_dialog_file; *w != '\0'; w++ )
		{
			if ( *w == '\\' ) {
				*w = '/';
			}
		}
	}
	else
	{
		g_file_dialog_file[0] = '\0';
	}

	ui::Widget(dialog).destroy();

	// don't return an empty filename
	if ( g_file_dialog_file[0] == '\0' ) {
		return NULL;
	}

	return g_file_dialog_file;
}

char* dir_dialog( ui::Window parent, const char* title, const char* path ){
	auto dialog = ui::Dialog::from(gtk_file_chooser_dialog_new( title,
													 parent,
													 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
													 GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
													 GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
													 NULL ));

	gtk_window_set_modal( dialog, TRUE );
	gtk_window_set_position( dialog, GTK_WIN_POS_CENTER_ON_PARENT );

	if ( !string_empty( path ) ) {
		gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER( dialog ), path );
	}

	char* filename = 0;
	if ( gtk_dialog_run( GTK_DIALOG( dialog ) ) == GTK_RESPONSE_ACCEPT ) {
		filename = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER( dialog ) );
	}

	dialog.destroy();

	return filename;
}

const char* file_dialog( ui::Window parent, bool open, const char* title, const char* path, const char* pattern, bool want_load, bool want_import, bool want_save ){
	for (;; )
	{
		const char* file = file_dialog_show( parent, open, title, path, pattern, want_load, want_import, want_save );

		if ( open
			 || !file
			 || !file_exists( file )
			 || ui::alert(parent, "The file specified already exists.\nDo you want to replace it?", title, ui::alert_type::NOYES, ui::alert_icon::Question ) == ui::alert_response::YES ) {
			return file;
		}
	}
}
