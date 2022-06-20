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

#include "toolbar.h"

#include <uilib/uilib.h>
#include <gtk/gtk.h>

#include "generic/callback.h"

#include "accelerator.h"
#include "button.h"
#include "image.h"


void toolbar_append( ui::Toolbar toolbar, ui::ToolItem button, const char* description ){
	gtk_widget_show_all(button);
	gtk_widget_set_tooltip_text(button, description);
	toolbar.add(button);
}

ui::ToolButton toolbar_append_button( ui::Toolbar toolbar, const char* description, const char* icon ){
	auto button = ui::ToolButton::from(gtk_tool_button_new(new_local_image(icon), nullptr));
	toolbar_append(toolbar, button, description);
	return button;
}

ui::ToolButton toolbar_append_button( ui::Toolbar toolbar, const char* description, const char* icon, const Callback<void()>& callback ){
	auto button = ui::ToolButton::from(gtk_tool_button_new(new_local_image(icon), nullptr));
	button_connect_callback(button, callback);
	toolbar_append(toolbar, button, description);
	return button;
}

ui::ToggleToolButton toolbar_append_toggle_button( ui::Toolbar toolbar, const char* description, const char* icon, const Callback<void()>& callback ){
	auto button = ui::ToggleToolButton::from(gtk_toggle_tool_button_new());
	toggle_button_connect_callback(button, callback);
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(button), new_local_image(icon));
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(button), description);
	toolbar_append(toolbar, button, description);
	return button;
}

ui::ToolButton toolbar_append_button( ui::Toolbar toolbar, const char* description, const char* icon, const Command& command ){
	return toolbar_append_button( toolbar, description, icon, command.m_callback );
}

void toggle_button_set_active_callback(void *it, bool active ){
	auto button = ui::ToggleToolButton::from(it);
	toggle_button_set_active_no_signal( button, active );
}

ui::ToggleToolButton toolbar_append_toggle_button( ui::Toolbar toolbar, const char* description, const char* icon, const Toggle& toggle ){
	auto button = toolbar_append_toggle_button( toolbar, description, icon, toggle.m_command.m_callback );
	toggle.m_exportCallback( PointerCaller<void, void(bool), toggle_button_set_active_callback>( button._handle ) );
	return button;
}
