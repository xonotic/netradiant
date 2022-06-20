#include "xorrectangle.h"

#include <gtk/gtk.h>

#include "gtkutil/glwidget.h"
#include "igl.h"

#include <gtk/gtkglwidget.h>

//#include "stream/stringstream.h"

bool XORRectangle::initialised() const
{
    return !!cr;
}

void XORRectangle::lazy_init()
{
    if (!initialised()) {
        cr = gdk_cairo_create(gtk_widget_get_window(m_widget));
    }
}

void XORRectangle::draw() const
{
#ifndef WORKAROUND_MACOS_GTK2_DESTROY
    const int x = float_to_integer(m_rectangle.x);
    const int y = float_to_integer(m_rectangle.y);
    const int w = float_to_integer(m_rectangle.w);
    const int h = float_to_integer(m_rectangle.h);
    GtkAllocation allocation;
    gtk_widget_get_allocation(m_widget, &allocation);
    cairo_rectangle(cr, x, -(h) - (y - allocation.height), w, h);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_operator(cr, CAIRO_OPERATOR_DIFFERENCE);
    cairo_stroke(cr);
#endif
}

XORRectangle::XORRectangle(ui::GLArea widget) : m_widget(widget), cr(0)
{
}

XORRectangle::~XORRectangle()
{
    if (initialised()) {
#ifndef WORKAROUND_MACOS_GTK2_DESTROY
        cairo_destroy(cr);
#endif
    }
}

void XORRectangle::set(rectangle_t rectangle)
{
    if (gtk_widget_get_realized(m_widget)) {
		if( m_rectangle.w != rectangle.w || m_rectangle.h != rectangle.h ){
		//if( !(m_rectangle.w == 0 && m_rectangle.h == 0 && rectangle.w == 0 && rectangle.h == 0) ){
		//globalOutputStream() << "m_x" << m_rectangle.x << " m_y" << m_rectangle.y << " m_w" << m_rectangle.w << " m_h" << m_rectangle.h << "\n";
		//globalOutputStream() << "__x" << rectangle.x << " __y" << rectangle.y << " __w" << rectangle.w << " __h" << rectangle.h << "\n";
			if ( glwidget_make_current( m_widget ) != FALSE ) {
				GlobalOpenGL_debugAssertNoErrors();

				gint width, height;
				gdk_gl_drawable_get_size( gtk_widget_get_gl_drawable( m_widget ), &width, &height );

				glViewport( 0, 0, width, height );
				glMatrixMode( GL_PROJECTION );
				glLoadIdentity();
				glOrtho( 0, width, 0, height, -100, 100 );
				glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
				glDisable( GL_DEPTH_TEST );

				glDrawBuffer( GL_FRONT );

				glEnable( GL_BLEND );
				glBlendFunc( GL_ONE_MINUS_DST_COLOR, GL_ZERO );

				glLineWidth( 2 );
				glColor3f( 1, 1, 1 );
				glDisable( GL_TEXTURE_2D );
				glBegin( GL_LINE_LOOP );
				glVertex2f( m_rectangle.x, m_rectangle.y + m_rectangle.h );
				glVertex2f( m_rectangle.x + m_rectangle.w, m_rectangle.y + m_rectangle.h );
				glVertex2f( m_rectangle.x + m_rectangle.w, m_rectangle.y );
				glVertex2f( m_rectangle.x, m_rectangle.y );
				glEnd();

				glBegin( GL_LINE_LOOP );
				glVertex2f( rectangle.x, rectangle.y + rectangle.h );
				glVertex2f( rectangle.x + rectangle.w, rectangle.y + rectangle.h );
				glVertex2f( rectangle.x + rectangle.w, rectangle.y );
				glVertex2f( rectangle.x, rectangle.y );
				glEnd();

				glDrawBuffer( GL_BACK );
				GlobalOpenGL_debugAssertNoErrors();
				//glwidget_swap_buffers( m_widget );
				glwidget_make_current( m_widget );
			}
		}
		m_rectangle = rectangle;
    }
}
