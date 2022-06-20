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

#include "plugintoolbar.h"

#include <gtk/gtk.h>

#include "itoolbar.h"
#include "modulesystem.h"

#include "stream/stringstream.h"
#include "gtkutil/image.h"
#include "gtkutil/container.h"

#include "mainframe.h"
#include "plugin.h"

ui::Image new_plugin_image( const char* filename ){
	{
		StringOutputStream fullpath( 256 );
		fullpath << GameToolsPath_get() << g_pluginsDir << "bitmaps/" << filename;
		if ( auto image = image_new_from_file_with_mask(fullpath.c_str()) ) return image;
	}

	{
		StringOutputStream fullpath( 256 );
		fullpath << DataPath_get() << g_pluginsDir << "bitmaps/" << filename;
		if ( auto image = image_new_from_file_with_mask(fullpath.c_str()) ) return image;
	}

	{
		StringOutputStream fullpath( 256 );
		fullpath << DataPath_get() << g_modulesDir << "bitmaps/" << filename;
		if ( auto image = image_new_from_file_with_mask(fullpath.c_str()) ) return image;
	}

	return image_new_missing();
}

void toolbar_insert( ui::Toolbar toolbar, const char* icon, const char* text, const char* tooltip, IToolbarButton::EType type, GCallback handler, gpointer data ){
	if (type == IToolbarButton::eSpace) {
		auto it = ui::ToolItem::from(gtk_separator_tool_item_new());
		it.show();
		toolbar.add(it);
		return;
	}
	if (type == IToolbarButton::eButton) {
		auto button = ui::ToolButton::from(gtk_tool_button_new(new_plugin_image(icon), text));
		gtk_widget_set_tooltip_text(button, tooltip);
		gtk_widget_show_all(button);
		button.connect("clicked", G_CALLBACK(handler), data);
		toolbar.add(button);
		return;
	}
	if (type == IToolbarButton::eToggleButton) {
		auto button = ui::ToolButton::from(gtk_toggle_tool_button_new());
		gtk_tool_button_set_icon_widget(button, new_plugin_image(icon));
		gtk_tool_button_set_label(button, text);
		gtk_widget_set_tooltip_text(button, tooltip);
		gtk_widget_show_all(button);
		button.connect("toggled", G_CALLBACK(handler), data);
		toolbar.add(button);
		return;
	}
	ERROR_MESSAGE( "invalid toolbar button type" );
}

void ActivateToolbarButton( ui::ToolButton widget, gpointer data ){
	(const_cast<const IToolbarButton *>( reinterpret_cast<IToolbarButton *>( data )))->activate();
}

void PlugInToolbar_AddButton( ui::Toolbar toolbar, const IToolbarButton* button ){
	toolbar_insert( toolbar, button->getImage(), button->getText(), button->getTooltip(), button->getType(), G_CALLBACK( ActivateToolbarButton ), reinterpret_cast<gpointer>( const_cast<IToolbarButton*>( button ) ) );
}

ui::Toolbar g_plugin_toolbar{ui::null};

void PluginToolbar_populate(){
	class AddToolbarItemVisitor : public ToolbarModules::Visitor
	{
	ui::Toolbar m_toolbar;
public:
	AddToolbarItemVisitor( ui::Toolbar toolbar )
		: m_toolbar( toolbar ){
	}
	void visit( const char* name, const _QERPlugToolbarTable& table ) const {
		const std::size_t count = table.m_pfnToolbarButtonCount();
		for ( std::size_t i = 0; i < count; ++i )
		{
			PlugInToolbar_AddButton( m_toolbar, table.m_pfnGetToolbarButton( i ) );
		}
	}

	} visitor( g_plugin_toolbar );

	Radiant_getToolbarModules().foreachModule( visitor );
}

void PluginToolbar_clear(){
	container_remove_all( g_plugin_toolbar );
}

ui::Toolbar create_plugin_toolbar(){

	auto toolbar = ui::Toolbar::from( gtk_toolbar_new() );
	gtk_orientable_set_orientation( GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_HORIZONTAL );
	gtk_toolbar_set_style( toolbar, GTK_TOOLBAR_ICONS );
	toolbar.show();

	g_plugin_toolbar = toolbar;

	PluginToolbar_populate();

	return toolbar;
}
