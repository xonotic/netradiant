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

#include "cursor.h"

#include "stream/textstream.h"

#include <string.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

// Note: NetRadiantCustom disables them but we still make use of them.
#if 1
GdkCursor* create_blank_cursor(){
	return gdk_cursor_new(GDK_BLANK_CURSOR);
}

void set_cursor( ui::Widget widget, GdkCursorType cursor_type ){
	GdkCursor* cursor = gdk_cursor_new( cursor_type );
	gdk_window_set_cursor( gtk_widget_get_window(widget), cursor );
	gdk_cursor_unref( cursor );
}

void blank_cursor( ui::Widget widget ){
	set_cursor( widget, GDK_BLANK_CURSOR );
}

void default_cursor( ui::Widget widget ){
	gdk_window_set_cursor( gtk_widget_get_window( widget ), NULL );
}
#endif

void Sys_GetCursorPos( ui::Widget widget, int *x, int *y ){
	GdkDisplay *display = gtk_widget_get_display( GTK_WIDGET( widget ) );
	// No need to store the screen, it will be recovered from widget again.
	gdk_display_get_pointer( display, NULL, x, y, NULL );
}

void Sys_SetCursorPos( ui::Widget widget, int x, int y ){
	GdkDisplay *display = gtk_widget_get_display( GTK_WIDGET( widget ) );
	GdkScreen *screen = gtk_widget_get_screen( GTK_WIDGET( widget ) );
	gdk_display_warp_pointer( display, screen, x, y );
}

gboolean DeferredMotion::gtk_motion(ui::Widget widget, GdkEventMotion *event, DeferredMotion *self)
{
    self->motion( event->x, event->y, event->state );
    return FALSE;
}

gboolean FreezePointer::motion_delta(ui::Widget widget, GdkEventMotion *event, FreezePointer *self)
{
	/* FIXME: The pointer can be lost outside of the XY Window
	or the Camera Window, see the comment in freeze_pointer function */
	int current_x, current_y;
	Sys_GetCursorPos( widget, &current_x, &current_y );
	int dx = current_x - self->last_x;
	int dy = current_y - self->last_y;
	self->last_x = current_x;
	self->last_y = current_y;
	if ( dx != 0 || dy != 0 ) {
		//globalOutputStream() << "motion x: " << dx << ", y: " << dy << "\n";
#if defined(WORKAROUND_MACOS_GTK2_LAGGYPOINTER)
		ui::Dimensions dimensions = widget.dimensions();
		int window_x, window_y;
		int translated_x, translated_y;

		gdk_window_get_origin( gtk_widget_get_window( widget ), &window_x, &window_y);


		translated_x = current_x - ( window_x );
		translated_y = current_y - ( window_y );

#if 0
		int widget_x, widget_y;
		gtk_widget_translate_coordinates( GTK_WIDGET( widget ), gtk_widget_get_toplevel( GTK_WIDGET( widget ) ), 0, 0, &widget_x, &widget_y);

		globalOutputStream()
			<< "window_x: " << window_x
			<< ", window_y: " << window_y
			<< ", widget_x: " << widget_x
			<< ", widget_y: " << widget_y
			<< ", current x: " << current_x
			<< ", current_y: " << current_y
			<< ", translated x: " << translated_x
			<< ", translated_y: " << translated_y
			<< ", width: " << dimensions.width
			<< ", height: " << dimensions.height
			<< "\n";
#endif

		if ( translated_x < 32 || translated_x > dimensions.width - 32
			|| translated_y < 32 || translated_y > dimensions.height - 32 ) {
#if 0
			// Reposition the pointer to the widget center.
			int reposition_x = window_x + dimensions.width / 2;
			int reposition_y = window_y + dimensions.height / 2;
#else
			// Move the pointer to the opposite side of the XY Window
			// to maximize the distance that can be moved again.
			int reposition_x = current_x;
			int reposition_y = current_y;

			if ( translated_x < 32 ) {
				reposition_x = window_x + dimensions.width - 32;
			}
			else if ( translated_x > dimensions.width - 32 ) {
				reposition_x = window_x + 32;
			}

			if ( translated_y < 32 ) {
				reposition_y = window_y + dimensions.height - 32;
			}
			else if ( translated_y > dimensions.height - 32 ) {
				reposition_y = window_y + 32;
			}
#endif

			Sys_SetCursorPos( widget, reposition_x, reposition_y );
			self->last_x = reposition_x;
			self->last_y = reposition_y;
		}
#else
		int ddx = current_x - self->recorded_x;
		int ddy = current_y - self->recorded_y;
		if (ddx < -32 || ddx > 32 || ddy < -32 || ddy > 32) {
			Sys_SetCursorPos( widget, self->recorded_x, self->recorded_y );
			self->last_x = self->recorded_x;
			self->last_y = self->recorded_y;
		}
#endif
		self->m_function( dx, dy, event->state, self->m_data );
	}
	return FALSE;
}

void FreezePointer::freeze_pointer(ui::Widget widget, FreezePointer::MotionDeltaFunction function, void *data)
{
	/* FIXME: This bug can happen if the pointer goes outside of the
	XY Window while the right mouse button is not released,
	the XY Window loses focus and can't read the right mouse button
	release event and then cannot unfreeze the pointer, meaning the
	user can attempt to freeze the pointer in another XY window.

	This can happen with touch screen, especially those used to drive
	virtual machine pointers, the cursor can be teleported outside of
	the XY Window while maintaining pressure on the right mouse button.
	This can also happen when the render is slow.

	The bug also occurs with the Camera Window.

	FIXME: It's would be possible to tell the user to save the map
	at assert time before crashing because this bug does not corrupt
	map saving. */
	ASSERT_MESSAGE( m_function == 0, "can't freeze pointer" );

	const GdkEventMask mask = static_cast<GdkEventMask>( GDK_POINTER_MOTION_MASK
														 | GDK_POINTER_MOTION_HINT_MASK
														 | GDK_BUTTON_MOTION_MASK
														 | GDK_BUTTON1_MOTION_MASK
														 | GDK_BUTTON2_MOTION_MASK
														 | GDK_BUTTON3_MOTION_MASK
														 | GDK_BUTTON_PRESS_MASK
														 | GDK_BUTTON_RELEASE_MASK
														 | GDK_VISIBILITY_NOTIFY_MASK );

#if defined(WORKAROUND_MACOS_GTK2_LAGGYPOINTER)
	/* Keep the pointer visible during the move operation.
	Because of a bug, it remains visible even if we give
	the order to hide it anyway.
	Other parts of the code assume the pointer is visible,
	so make sure it is consistently visible accross
	third-party updates that may fix the mouse pointer
	visibility issue. */
#else
	GdkCursor* cursor = create_blank_cursor();
	//GdkGrabStatus status =
	gdk_pointer_grab( gtk_widget_get_window( widget ), TRUE, mask, 0, cursor, GDK_CURRENT_TIME );
	gdk_cursor_unref( cursor );
#endif

	Sys_GetCursorPos( widget, &recorded_x, &recorded_y );

	Sys_SetCursorPos( widget, recorded_x, recorded_y );

	last_x = recorded_x;
	last_y = recorded_y;

	m_function = function;
	m_data = data;

	handle_motion = widget.connect( "motion_notify_event", G_CALLBACK( motion_delta ), this );
}

void FreezePointer::unfreeze_pointer(ui::Widget widget)
{
	g_signal_handler_disconnect( G_OBJECT( widget ), handle_motion );

	m_function = 0;
	m_data = 0;

#if defined(WORKAROUND_MACOS_GTK2_LAGGYPOINTER)
	/* The pointer was visible during all the move operation,
	so, keep the current position. */
#else
	// Note: NetRadiantCustom still uses window instead of widget.
	Sys_SetCursorPos( widget, recorded_x, recorded_y );
#endif

	gdk_pointer_ungrab( GDK_CURRENT_TIME );
}
