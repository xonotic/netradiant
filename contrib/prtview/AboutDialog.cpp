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

#include "AboutDialog.h"
#include <gtk/gtk.h>
#include <gtkutil/pointer.h>
#include <uilib/uilib.h>
#include "version.h"
#include "gtkutil/pointer.h"

#include "prtview.h"
#include "ConfigDialog.h"

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

void DoAboutDlg(){
	int loop = 1, ret = IDCANCEL;

	auto dlg = ui::Window(ui::window_type::TOP);
	gtk_window_set_transient_for( GTK_WINDOW( dlg ), GTK_WINDOW( g_pRadiantWnd ) );
	gtk_window_set_position( GTK_WINDOW( dlg ),GTK_WIN_POS_CENTER_ON_PARENT );
	gtk_window_set_modal( GTK_WINDOW( dlg ), TRUE );
	gtk_window_set_title(dlg, "About Portal Viewer");
	dlg.connect("delete_event", G_CALLBACK(dialog_delete_callback), NULL);
	dlg.connect("destroy", G_CALLBACK(gtk_widget_destroy), NULL);
	g_object_set_data(G_OBJECT(dlg), "loop", &loop);
	g_object_set_data(G_OBJECT(dlg), "ret", &ret);

	auto hbox = ui::HBox(FALSE, 10);
	hbox.show();
	dlg.add(hbox);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 10);

	char const *label_text = "Version 1.000\n\n"
			"Gtk port by Leonardo Zide\nleo@lokigames.com\n\n"
			"Written by Geoffrey DeWan\ngdewan@prairienet.org\n\n"
			"Built against NetRadiant " RADIANT_VERSION "\n"
			__DATE__;
	auto label = ui::Label(label_text);
	label.show();
	hbox.pack_start( label, TRUE, TRUE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	auto vbox = ui::VBox(FALSE, 0);
	vbox.show();
	hbox.pack_start( vbox, FALSE, FALSE, 0);

	auto button = ui::Button("OK");
	button.show();
	vbox.pack_start( button, FALSE, FALSE, 0);
	button.connect("clicked", G_CALLBACK(dialog_button_callback), GINT_TO_POINTER(IDOK));
	button.dimensions(60, -1);

	gtk_grab_add(dlg);
	dlg.show();

	while (loop)
		gtk_main_iteration();

	gtk_grab_remove(dlg);
	dlg.destroy();
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDialog message handlers
