add_library(gtkutil STATIC
        accelerator.cpp accelerator.h
        button.cpp button.h
        clipboard.cpp clipboard.h
        closure.h
        container.h
        cursor.cpp cursor.h
        dialog.cpp dialog.h
        entry.cpp entry.h
        filechooser.cpp filechooser.h
        frame.cpp frame.h
        glfont.cpp glfont.h
        glwidget.cpp glwidget.h
        idledraw.h
        image.cpp image.h
        menu.cpp menu.h
        messagebox.cpp messagebox.h
        nonmodal.cpp nonmodal.h
        paned.cpp paned.h
        pointer.h
        toolbar.cpp toolbar.h
        widget.cpp widget.h
        window.cpp window.h
        xorrectangle.cpp xorrectangle.h
        )

target_include_directories(gtkutil PRIVATE uilib)
target_link_libraries(gtkutil PRIVATE uilib)

find_package(GTK${GTK_TARGET} REQUIRED)
target_include_directories(gtkutil PRIVATE ${GTK${GTK_TARGET}_INCLUDE_DIRS})
target_link_libraries(gtkutil PRIVATE ${GTK${GTK_TARGET}_LIBRARIES})

if (GTK_TARGET EQUAL 2)
    find_package(GtkGLExt REQUIRED)
    target_include_directories(gtkutil PRIVATE ${GtkGLExt_INCLUDE_DIRS})
    target_link_libraries(gtkutil PRIVATE ${GtkGLExt_LIBRARIES})
endif ()
