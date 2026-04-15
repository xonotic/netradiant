#include "widget.h"
#include <gtk/gtk.h>

void widget_queue_draw(ui::Widget &widget)
{
#if GTK_TARGET == 3
    if (GTK_IS_GL_AREA(widget)) {
        gtk_gl_area_queue_render(GTK_GL_AREA((GtkWidget *) widget));
        return;
    }
#endif
    gtk_widget_queue_draw(widget);
}

void widget_make_default(ui::Widget widget)
{
    gtk_widget_set_can_default(widget, true);
    gtk_widget_grab_default(widget);
}

gboolean ToggleShown::notify_visible(ui::Widget widget, gpointer dummy, ToggleShown *self)
{
    self->update();
    return FALSE;
}

gboolean ToggleShown::destroy(ui::Widget widget, ToggleShown *self)
{
    self->m_shownDeferred = gtk_widget_get_visible(self->m_widget) != FALSE;
    self->m_widget = ui::Widget(ui::null);
    return FALSE;
}

void ToggleShown::update()
{
    m_item.update();
}

bool ToggleShown::active() const
{
    if (!m_widget) {
        return m_shownDeferred;
    } else {
        return gtk_widget_get_visible(m_widget) != FALSE;
    }
}

void ToggleShown::exportActive(const Callback<void(bool)> &importCallback)
{
    importCallback(active());
}

void ToggleShown::set(bool shown)
{
    if (!m_widget) {
        m_shownDeferred = shown;
    } else {
        m_widget.visible(shown);
    }
}

void ToggleShown::toggle()
{
    m_widget.visible(!m_widget.visible());
}

void ToggleShown::connect(ui::Widget widget)
{
    m_widget = widget;
    m_widget.visible(m_shownDeferred);
    m_widget.connect("notify::visible", G_CALLBACK(notify_visible), this);
    m_widget.connect("destroy", G_CALLBACK(destroy), this);
    update();
}

gboolean WidgetFocusPrinter::focus_in(ui::Widget widget, GdkEventFocus *event, WidgetFocusPrinter *self)
{
    globalOutputStream() << self->m_name << " takes focus\n";
    return FALSE;
}

gboolean WidgetFocusPrinter::focus_out(ui::Widget widget, GdkEventFocus *event, WidgetFocusPrinter *self)
{
    globalOutputStream() << self->m_name << " loses focus\n";
    return FALSE;
}

void WidgetFocusPrinter::connect(ui::Widget widget)
{
    widget.connect("focus_in_event", G_CALLBACK(focus_in), this);
    widget.connect("focus_out_event", G_CALLBACK(focus_out), this);
}
