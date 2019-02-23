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

#if !defined( INCLUDED_GTKUTIL_WIDGET_H )
#define INCLUDED_GTKUTIL_WIDGET_H

#include <list>
#include <uilib/uilib.h>
#include <gdk/gdk.h>
#include "generic/callback.h"
#include "warnings.h"
#include "debugging/debugging.h"
#include "property.h"

inline bool widget_is_visible( GtkWidget* widget ){
	return GTK_WIDGET_VISIBLE( widget ) != FALSE;
}

inline void widget_set_visible( GtkWidget* widget, bool show ){
	if ( show ) {
		/* workaround for gtk 2.24 issue: not displayed glwidget after toggle */
		GtkWidget* glwidget = GTK_WIDGET( g_object_get_data( G_OBJECT( widget ), "glwidget" ) );
		if ( glwidget ){
			//if ( widget_is_visible( glwidget ) )
				//globalOutputStream() << "glwidget have been already visible :0\n"; /* is not hidden aswell, according to this */
			gtk_widget_hide( glwidget );
			gtk_widget_show( glwidget );
		}
		gtk_widget_show( widget );
	}
	else
	{
		gtk_widget_hide( widget );
	}
}

class ToggleItem {
    Callback<void(const Callback<void(bool)> &)> m_exportCallback;
    typedef std::list<Callback<void(bool)>> ImportCallbacks;
    ImportCallbacks m_importCallbacks;
public:
    ToggleItem(const Callback<void(const Callback<void(bool)> &)> &exportCallback) : m_exportCallback(exportCallback)
    {
    }

    void update()
    {
        for (ImportCallbacks::iterator i = m_importCallbacks.begin(); i != m_importCallbacks.end(); ++i) {
            m_exportCallback(*i);
        }
    }

    void addCallback(const Callback<void(bool)> &callback)
    {
        m_importCallbacks.push_back(callback);
        m_exportCallback(callback);
    }

    typedef MemberCaller<ToggleItem, void(const Callback<void(bool)> &), &ToggleItem::addCallback> AddCallbackCaller;
};

class ToggleShown {
    bool m_shownDeferred;

    ToggleShown(const ToggleShown &other); // NOT COPYABLE
    ToggleShown &operator=(const ToggleShown &other); // NOT ASSIGNABLE

    static gboolean notify_visible(ui::Widget widget, gpointer dummy, ToggleShown *self);

    static gboolean destroy(ui::Widget widget, ToggleShown *self);

public:
    ui::Widget m_widget;
    ToggleItem m_item;

    ToggleShown(bool shown)
            : m_shownDeferred(shown), m_widget(ui::null), m_item(ActiveCaller(*this))
    {
    }

    void update();

    bool active() const;

    void exportActive(const Callback<void(bool)> &importCallback);

    typedef MemberCaller<ToggleShown, void(const Callback<void(bool)> &), &ToggleShown::exportActive> ActiveCaller;

    void set(bool shown);

    void toggle();

    typedef MemberCaller<ToggleShown, void(), &ToggleShown::toggle> ToggleCaller;

    void connect(ui::Widget widget);
};


void widget_queue_draw(ui::Widget &widget);

typedef ReferenceCaller<ui::Widget, void(), widget_queue_draw> WidgetQueueDrawCaller;


void widget_make_default(ui::Widget widget);

class WidgetFocusPrinter {
    const char *m_name;

    static gboolean focus_in(ui::Widget widget, GdkEventFocus *event, WidgetFocusPrinter *self);

    static gboolean focus_out(ui::Widget widget, GdkEventFocus *event, WidgetFocusPrinter *self);

public:
    WidgetFocusPrinter(const char *name) : m_name(name)
    {
    }

    void connect(ui::Widget widget);
};

#endif
