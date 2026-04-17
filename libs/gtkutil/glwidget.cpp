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

// OpenGL widget based on GtkGLExt / GtkGLArea

#include "glwidget.h"

#include "igl.h"

// Function callbacks
static void (*sharedContextCreated)() = nullptr;
static void (*sharedContextDestroyed)() = nullptr;

unsigned int g_context_count = 0;

ui::GLArea g_shared{ui::null};

static void _glwidget_context_created(ui::GLArea self, void *data)
{
    if (++g_context_count == 1) {
        g_shared = self;
        g_object_ref(g_shared._handle);

        glwidget_make_current(g_shared);
        GlobalOpenGL().contextValid = true;

        sharedContextCreated();
    }
}

static void _glwidget_context_destroyed(ui::GLArea self, void *data)
{
    if (--g_context_count == 0) {
        GlobalOpenGL().contextValid = false;

        sharedContextDestroyed();

        g_shared.unref();
        g_shared = ui::GLArea(ui::null);
    }
}

void glwidget_set_shared_context_constructors(
		void created(), void destroyed() )
{
	sharedContextCreated = created;
	sharedContextDestroyed = destroyed;
}

#if GTK_TARGET == 3

#include <gtk/gtk.h>

// Shared GL context for all GtkGLArea widgets.
// GDK3 docs say contexts on the same display share GL resources, but this
// doesn't work reliably on all systems/drivers/compositors. To guarantee
// that textures loaded in one view (e.g. camera) are visible in another
// (e.g. texture browser), we force all GtkGLArea widgets to use a single
// GdkGLContext via the "create-context" signal.
static GdkGLContext *g_shared_gl_context = nullptr;

static GdkGLContext *on_create_context(GtkGLArea *area, gpointer user_data)
{
    if (g_shared_gl_context) {
        return GDK_GL_CONTEXT(g_object_ref(g_shared_gl_context));
    }
    // First widget: let the default handler create the context.
    return NULL;
}

static void glwidget_context_created(ui::GLArea self)
{
    if (!g_shared_gl_context) {
        g_shared_gl_context = gtk_gl_area_get_context(GTK_GL_AREA(self._handle));
        if (g_shared_gl_context) {
            g_object_ref(g_shared_gl_context);
        }
    }
    _glwidget_context_created(self, nullptr);
}

static void glwidget_context_destroyed(ui::GLArea self)
{
    _glwidget_context_destroyed(self, nullptr);
    if (g_context_count == 0 && g_shared_gl_context) {
        g_object_unref(g_shared_gl_context);
        g_shared_gl_context = nullptr;
    }
}

ui::GLArea glwidget_new(bool zbuffer)
{
    auto self = ui::GLArea::from(GTK_GL_AREA(gtk_gl_area_new()));
    gtk_gl_area_set_has_depth_buffer(self, zbuffer);
    gtk_gl_area_set_auto_render(self, false);

    // https://docs.gtk.org/gtk3/signal.GLArea.create-context.html
    self.connect("create-context", G_CALLBACK(on_create_context), nullptr);
    self.connect("realize", G_CALLBACK(glwidget_context_created), nullptr);
    self.connect("unrealize", G_CALLBACK(glwidget_context_destroyed), nullptr);
    return self;
}

bool glwidget_make_current(ui::GLArea self)
{
    gtk_gl_area_make_current(self);
    return true;
}

void glwidget_swap_buffers(ui::GLArea self)
{
    g_assert(GTK_IS_GL_AREA(self));
    // GtkGLArea automatically presents the back buffer after the
    // "render" signal handler returns. No explicit swap needed.
}

#endif

#if GTK_TARGET == 2

#include <gtk/gtk.h>
#include <gtk/gtkglwidget.h>

#include "pointer.h"

struct config_t {
    const char *name;
    int *attribs;
};
typedef const config_t *configs_iterator;

static int config_rgba32[] = {
        GDK_GL_RGBA,
        GDK_GL_DOUBLEBUFFER,
        GDK_GL_BUFFER_SIZE, 24,
        GDK_GL_ATTRIB_LIST_NONE,
};

static int config_rgba[] = {
        GDK_GL_RGBA,
        GDK_GL_DOUBLEBUFFER,
        GDK_GL_BUFFER_SIZE, 16,
        GDK_GL_ATTRIB_LIST_NONE,
};

static const config_t configs[] = {
        {
                "colour-buffer = 32bpp, depth-buffer = none",
                config_rgba32,
        },
        {
                "colour-buffer = 16bpp, depth-buffer = none",
                config_rgba,
        }
};

static GdkGLConfig *glconfig_new()
{
    for (configs_iterator i = configs, end = configs + 2; i != end; ++i) {
        if (auto glconfig = gdk_gl_config_new(i->attribs)) {
            globalOutputStream() << "OpenGL window configuration: " << i->name << "\n";
            return glconfig;
        }
    }
    globalOutputStream() << "OpenGL window configuration: colour-buffer = auto, depth-buffer = none\n";
    return gdk_gl_config_new_by_mode((GdkGLConfigMode) (GDK_GL_MODE_RGBA | GDK_GL_MODE_DOUBLE));
}

static int config_rgba32_depth32[] = {
        GDK_GL_RGBA,
        GDK_GL_DOUBLEBUFFER,
        GDK_GL_BUFFER_SIZE,
        24,
        GDK_GL_DEPTH_SIZE,
        32,
        GDK_GL_ATTRIB_LIST_NONE,
};

static int config_rgba32_depth24[] = {
        GDK_GL_RGBA,
        GDK_GL_DOUBLEBUFFER,
        GDK_GL_BUFFER_SIZE, 24,
        GDK_GL_DEPTH_SIZE, 24,
        GDK_GL_ATTRIB_LIST_NONE,
};

static int config_rgba32_depth16[] = {
        GDK_GL_RGBA,
        GDK_GL_DOUBLEBUFFER,
        GDK_GL_BUFFER_SIZE, 24,
        GDK_GL_DEPTH_SIZE, 16,
        GDK_GL_ATTRIB_LIST_NONE,
};

static int config_rgba32_depth[] = {
        GDK_GL_RGBA,
        GDK_GL_DOUBLEBUFFER,
        GDK_GL_BUFFER_SIZE, 24,
        GDK_GL_DEPTH_SIZE, 1,
        GDK_GL_ATTRIB_LIST_NONE,
};

static int config_rgba_depth16[] = {
        GDK_GL_RGBA,
        GDK_GL_DOUBLEBUFFER,
        GDK_GL_BUFFER_SIZE, 16,
        GDK_GL_DEPTH_SIZE, 16,
        GDK_GL_ATTRIB_LIST_NONE,
};

static int config_rgba_depth[] = {
        GDK_GL_RGBA,
        GDK_GL_DOUBLEBUFFER,
        GDK_GL_BUFFER_SIZE, 16,
        GDK_GL_DEPTH_SIZE, 1,
        GDK_GL_ATTRIB_LIST_NONE,
};

static const config_t configs_with_depth[] =
        {
                {
                        "colour-buffer = 32bpp, depth-buffer = 32bpp",
                        config_rgba32_depth32,
                },
                {
                        "colour-buffer = 32bpp, depth-buffer = 24bpp",
                        config_rgba32_depth24,
                },
                {
                        "colour-buffer = 32bpp, depth-buffer = 16bpp",
                        config_rgba32_depth16,
                },
                {
                        "colour-buffer = 32bpp, depth-buffer = auto",
                        config_rgba32_depth,
                },
                {
                        "colour-buffer = 16bpp, depth-buffer = 16bpp",
                        config_rgba_depth16,
                },
                {
                        "colour-buffer = auto, depth-buffer = auto",
                        config_rgba_depth,
                },
        };

static GdkGLConfig *glconfig_new_with_depth()
{
    for (configs_iterator i = configs_with_depth, end = configs_with_depth + 6; i != end; ++i) {
        if (auto glconfig = gdk_gl_config_new(i->attribs)) {
            globalOutputStream() << "OpenGL window configuration: " << i->name << "\n";
            return glconfig;
        }
    }
    globalOutputStream() << "OpenGL window configuration: colour-buffer = auto, depth-buffer = auto (fallback)\n";
    return gdk_gl_config_new_by_mode((GdkGLConfigMode) (GDK_GL_MODE_RGBA | GDK_GL_MODE_DOUBLE | GDK_GL_MODE_DEPTH));
}

static int glwidget_context_created(ui::GLArea self, void *data)
{
    _glwidget_context_created(self, data);
    return false;
}

int glwidget_context_destroyed(ui::GLArea self, void *data)
{
    _glwidget_context_destroyed(self, data);
    return false;
}

static bool glwidget_enable_gl(ui::GLArea self, ui::Widget root, gpointer data)
{
    if (!root && !gtk_widget_is_gl_capable(self)) {
        const auto zbuffer = g_object_get_data(G_OBJECT(self), "zbuffer");
        GdkGLConfig *glconfig = zbuffer ? glconfig_new_with_depth() : glconfig_new();
        ASSERT_MESSAGE(glconfig, "failed to create OpenGL config");

        const auto share_list = g_shared ? gtk_widget_get_gl_context(g_shared) : nullptr;
        gtk_widget_set_gl_capability(self, glconfig, share_list, true, GDK_GL_RGBA_TYPE);

        gtk_widget_realize(self);
        if (!g_shared) {
            g_shared = self;
        }
        // free glconfig?
    }
    return false;
}

ui::GLArea glwidget_new(bool zbuffer)
{
    auto self = ui::GLArea::from(gtk_drawing_area_new());

    g_object_set_data(G_OBJECT(self), "zbuffer", gint_to_pointer(zbuffer));

    self.connect("hierarchy-changed", G_CALLBACK(glwidget_enable_gl), 0);

    self.connect("realize", G_CALLBACK(glwidget_context_created), 0);
    self.connect("unrealize", G_CALLBACK(glwidget_context_destroyed), 0);

    return self;
}

void glwidget_swap_buffers(ui::GLArea self)
{
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(self);
    gdk_gl_drawable_swap_buffers(gldrawable);
}

bool glwidget_make_current(ui::GLArea self)
{
    GdkGLContext *glcontext = gtk_widget_get_gl_context(self);
    GdkGLDrawable *gldrawable = gtk_widget_get_gl_drawable(self);
    return gdk_gl_drawable_gl_begin(gldrawable, glcontext);
}

#endif
