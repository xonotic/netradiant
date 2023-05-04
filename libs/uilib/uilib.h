#ifndef INCLUDED_UILIB_H
#define INCLUDED_UILIB_H

#include <cstdint>
#include <string>
#include <glib-object.h>

struct _GdkEventKey;
struct _GtkAccelGroup;
struct _GtkAdjustment;
struct _GtkAlignment;
struct _GtkBin;
struct _GtkBox;
struct _GtkButton;
struct _GtkCellEditable;
struct _GtkCellRenderer;
struct _GtkCellRendererText;
struct _GtkCheckButton;
struct _GtkCheckMenuItem;
struct _GtkComboBox;
struct _GtkComboBoxText;
struct _GtkContainer;
struct _GtkDialog;
struct _GtkEditable;
struct _GtkEntry;
struct _GtkEntryCompletion;
struct _GtkFrame;
struct _GtkHBox;
struct _GtkHPaned;
struct _GtkHScale;
struct _GtkImage;
struct _GtkItem;
struct _GtkLabel;
struct _GtkListStore;
struct _GtkTreeIter;
struct _GtkMenu;
struct _GtkMenuBar;
struct _GtkMenuItem;
struct _GtkMenuShell;
struct _GtkMisc;
struct _GtkObject;
struct _GtkPaned;
struct _GtkRadioButton;
struct _GtkRadioMenuItem;
struct _GtkRadioToolButton;
struct _GtkRange;
struct _GtkScale;
struct _GtkScrolledWindow;
struct _GtkSpinButton;
struct _GtkTable;
struct _GtkTearoffMenuItem;
struct _GtkTextView;
struct _GtkToggleButton;
struct _GtkToggleToolButton;
struct _GtkToolbar;
struct _GtkToolButton;
struct _GtkToolItem;
struct _GtkTreeModel;
struct _GtkTreePath;
struct _GtkTreeSelection;
struct _GtkTreeStore;
struct _GtkTreeView;
struct _GtkTreeViewColumn;
struct _GtkVBox;
struct _GtkVPaned;
struct _GtkWidget;
struct _GtkWindow;
struct _GTypeInstance;

#if GTK_TARGET == 3
struct _GtkGLArea;
#endif

#if GTK_TARGET == 2
using _GtkGLArea = struct _GtkDrawingArea;
#endif

struct ModalDialog;

namespace ui {

    bool init(int *argc, char **argv[], char const *parameter_string, char const **error);

    void main();

    void process();

    enum class window_type {
        TOP,
        POPUP
    };

    enum class Shadow {
        NONE,
        IN,
        OUT,
        ETCHED_IN,
        ETCHED_OUT
    };

    enum class Policy {
        ALWAYS,
        AUTOMATIC,
        NEVER
    };

	enum class Packing {
		START,
		END
	};

    namespace details {

        enum class Convert {
            Implicit, Explicit
        };

        template<class Self, class T, Convert mode>
        struct Convertible;

        template<class Self, class T>
        struct Convertible<Self, T, Convert::Implicit> {
            operator T() const
            { return reinterpret_cast<T>(static_cast<Self const *>(this)->_handle); }
        };

        template<class Self, class T>
        struct Convertible<Self, T, Convert::Explicit> {
            explicit operator T() const
            { return reinterpret_cast<T>(static_cast<Self const *>(this)->_handle); }
        };

        template<class Self, class... T>
        struct All : T ... {
            All()
            {};
        };

        template<class Self, class Interfaces>
        struct Mixin;
        template<class Self>
        struct Mixin<Self, void()> {
            using type = All<Self>;
        };
        template<class Self, class... Interfaces>
        struct Mixin<Self, void(Interfaces...)> {
            using type = All<Self, Interfaces...>;
        };
    }

    const struct Null {} null = {};
    const struct New_t {} New = {};

    class Object :
            public details::Convertible<Object, _GtkObject *, details::Convert::Explicit>,
            public details::Convertible<Object, _GTypeInstance *, details::Convert::Explicit> {
    public:
        using self = Object *;
        using native = _GtkObject *;
        native _handle;

        explicit Object(native h) : _handle(h)
        {}

        explicit operator bool() const
        { return _handle != nullptr; }

        explicit operator void *() const
        { return _handle; }

        void unref()
        { g_object_unref(_handle); }

        void ref()
        { g_object_ref(_handle); }

        template<class Lambda>
        gulong connect(char const *detailed_signal, Lambda &&c_handler, void *data);

        template<class Lambda>
        gulong connect(char const *detailed_signal, Lambda &&c_handler, Object data);
    };
    static_assert(sizeof(Object) == sizeof(Object::native), "object slicing");

#define WRAP(name, super, T, interfaces, ctors, methods) \
    class name; \
    class I##name : public details::Convertible<name, T *, details::Convert::Implicit> { \
    public: \
        using self = name *; \
        methods \
    }; \
    class name : public super, public I##name, public details::Mixin<name, void interfaces>::type { \
    public: \
        using self = name *; \
        using native = T *; \
    protected: \
        explicit name(native h) noexcept : super(reinterpret_cast<super::native>(h)) {} \
    public: \
        explicit name(Null n) noexcept : name((native) nullptr) {} \
        explicit name(New_t); \
        static name from(native h) { return name(h); } \
        static name from(void *ptr) { return name((native) ptr); } \
        ctors \
    }; \
    inline bool operator<(name self, name other) { return self._handle < other._handle; } \
    static_assert(sizeof(name) == sizeof(super), "object slicing")

    // https://developer.gnome.org/gtk2/stable/ch01.html

    // GInterface

    WRAP(CellEditable, Object, _GtkCellEditable, (),
    ,
    );

    WRAP(Editable, Object, _GtkEditable, (),
    ,
         void editable(bool value);
    );

    WRAP(TreeModel, Object, _GtkTreeModel, (),
    ,
    );

    // GObject

    struct Dimensions {
        int width;
        int height;
    };

    class Window;
    WRAP(Widget, Object, _GtkWidget, (),
    ,
         Window window();
         const char *file_dialog(
                 bool open,
                 const char *title,
                 const char *path = nullptr,
                 const char *pattern = nullptr,
                 bool want_load = false,
                 bool want_import = false,
                 bool want_save = false
         );
         bool visible();
         void visible(bool shown);
         void show();
         void hide();
         Dimensions dimensions();
         void dimensions(int width, int height);
         void destroy();
    );

    WRAP(Container, Widget, _GtkContainer, (),
    ,
         void add(Widget widget);

         void remove(Widget widget);

         template<class Lambda>
         void foreach(Lambda &&lambda);
    );

    WRAP(Bin, Container, _GtkBin, (),
    ,
    );

    class AccelGroup;
    WRAP(Window, Bin, _GtkWindow, (),
         explicit Window(window_type type);
    ,
         Window create_dialog_window(
                 const char *title,
                 void func(),
                 void *data,
                 int default_w = -1,
                 int default_h = -1
         );

         Window create_modal_dialog_window(
                 const char *title,
                 ModalDialog &dialog,
                 int default_w = -1,
                 int default_h = -1
         );

         Window create_floating_window(const char *title);

         std::uint64_t on_key_press(
                 bool (*f)(Widget widget, _GdkEventKey *event, void *extra),
                 void *extra = nullptr
         );

         void add_accel_group(AccelGroup group);
    );

    WRAP(Dialog, Window, _GtkDialog, (),
    ,
    );

    WRAP(Alignment, Bin, _GtkAlignment, (),
         Alignment(float xalign, float yalign, float xscale, float yscale);
    ,
    );

    WRAP(Frame, Bin, _GtkFrame, (),
         explicit Frame(const char *label = nullptr);
    ,
    );

    WRAP(Button, Bin, _GtkButton, (),
         explicit Button(const char *label);
    ,
    );

    WRAP(ToggleButton, Button, _GtkToggleButton, (),
    ,
         bool active() const;
         void active(bool value);
    );

    WRAP(CheckButton, ToggleButton, _GtkCheckButton, (),
         explicit CheckButton(const char *label);
    ,
    );

    WRAP(RadioButton, CheckButton, _GtkRadioButton, (),
    ,
    );

    WRAP(Item, Bin, _GtkItem, (),
    ,
    );

    WRAP(MenuItem, Item, _GtkMenuItem, (),
         explicit MenuItem(const char *label, bool mnemonic = false);
    ,
    );

    WRAP(CheckMenuItem, MenuItem, _GtkCheckMenuItem, (),
    ,
    );

    WRAP(RadioMenuItem, CheckMenuItem, _GtkRadioMenuItem, (),
    ,
    );

    WRAP(TearoffMenuItem, MenuItem, _GtkTearoffMenuItem, (),
    ,
    );

    WRAP(ComboBox, Bin, _GtkComboBox, (),
    ,
    );

    WRAP(ComboBoxText, ComboBox, _GtkComboBoxText, (),
    ,
    );

    WRAP(ToolItem, Bin, _GtkToolItem, (),
    ,
    );

    WRAP(ToolButton, ToolItem, _GtkToolButton, (),
    ,
    );

    WRAP(ToggleToolButton, ToolButton, _GtkToggleToolButton, (),
    ,
    );

    WRAP(RadioToolButton, ToggleToolButton, _GtkRadioToolButton, (),
    ,
    );

    WRAP(ScrolledWindow, Bin, _GtkScrolledWindow, (),
    ,
         void overflow(Policy x, Policy y);
    );

    WRAP(Box, Container, _GtkBox, (),
    ,
         void pack_start(ui::Widget child, bool expand, bool fill, unsigned int padding);
         void pack_end(ui::Widget child, bool expand, bool fill, unsigned int padding);
         void set_child_packing(ui::Widget child, bool expand, bool fill, unsigned int padding, ui::Packing packing);
    );

    WRAP(VBox, Box, _GtkVBox, (),
         VBox(bool homogenous, int spacing);
    ,
    );

    WRAP(HBox, Box, _GtkHBox, (),
         HBox(bool homogenous, int spacing);
    ,
    );

    WRAP(Paned, Container, _GtkPaned, (),
    ,
    );

    WRAP(HPaned, Paned, _GtkHPaned, (),
    ,
    );

    WRAP(VPaned, Paned, _GtkVPaned, (),
    ,
    );

    WRAP(MenuShell, Container, _GtkMenuShell, (),
    ,
    );

    WRAP(MenuBar, MenuShell, _GtkMenuBar, (),
    ,
    );

    WRAP(Menu, MenuShell, _GtkMenu, (),
    ,
    );

    struct TableAttach {
        unsigned int left, right, top, bottom;
    };

    struct TableAttachOptions {
        // todo: type safety
        unsigned int x, y;
    };

    struct TablePadding {
        unsigned int x, y;
    };

    WRAP(Table, Container, _GtkTable, (),
         Table(std::size_t rows, std::size_t columns, bool homogenous);
    ,
         // 5 = expand | fill
         void attach(Widget child, TableAttach attach, TableAttachOptions options = {5, 5}, TablePadding padding = {0, 0});
    );

    WRAP(TextView, Container, _GtkTextView, (),
    ,
         void text(char const *str);
    );

    WRAP(Toolbar, Container, _GtkToolbar, (),
    ,
    );

    class TreeModel;
    WRAP(TreeView, Widget, _GtkTreeView, (),
         TreeView(TreeModel model);
    ,
    );

    WRAP(Misc, Widget, _GtkMisc, (),
    ,
    );

    WRAP(Label, Widget, _GtkLabel, (),
         explicit Label(const char *label);
    ,
         void text(char const *str);
    );

    WRAP(Image, Widget, _GtkImage, (),
    ,
    );

    WRAP(Entry, Widget, _GtkEntry, (IEditable, ICellEditable),
         explicit Entry(std::size_t max_length);
    ,
        char const *text();
        void text(char const *str);
    );

    class Adjustment;
    WRAP(SpinButton, Entry, _GtkSpinButton, (),
         SpinButton(Adjustment adjustment, double climb_rate, std::size_t digits);
    ,
    );

    WRAP(Range, Widget, _GtkRange, (),
    ,
    );

    WRAP(Scale, Range, _GtkScale, (),
    ,
    );

    WRAP(HScale, Scale, _GtkHScale, (),
         explicit HScale(Adjustment adjustment);
         HScale(double min, double max, double step);
    ,
    );

    WRAP(Adjustment, Object, _GtkAdjustment, (),
         Adjustment(double value,
                    double lower, double upper,
                    double step_increment, double page_increment,
                    double page_size);
    ,
    );

    WRAP(CellRenderer, Object, _GtkCellRenderer, (),
    ,
    );

    WRAP(CellRendererText, CellRenderer, _GtkCellRendererText, (),
    ,
    );

    struct TreeViewColumnAttribute {
        const char *attribute;
        int column;
    };
    WRAP(TreeViewColumn, Object, _GtkTreeViewColumn, (),
         TreeViewColumn(const char *title, CellRenderer renderer, std::initializer_list<TreeViewColumnAttribute> attributes);
    ,
    );

    WRAP(AccelGroup, Object, _GtkAccelGroup, (),
    ,
    );

    WRAP(EntryCompletion, Object, _GtkEntryCompletion, (),
    ,
    );

    WRAP(ListStore, Object, _GtkListStore, (ITreeModel),
    ,
         void clear();

         template<class... T>
         void append(T... args);

         void append();
    );

    WRAP(TreeStore, Object, _GtkTreeStore, (ITreeModel),
    ,
    );

    WRAP(TreeSelection, Object, _GtkTreeSelection, (),
    ,
    );

    // GBoxed

    WRAP(TreePath, Object, _GtkTreePath, (),
         explicit TreePath(const char *path);
    ,
    );

    // Custom

    WRAP(GLArea, Widget, _GtkGLArea, (),
    ,
         guint on_render(GCallback pFunction, void *data);
    );

#undef WRAP

    // global

    enum class alert_response {
        OK,
        CANCEL,
        YES,
        NO,
    };

    enum class alert_type {
        OK,
        OKCANCEL,
        YESNO,
        YESNOCANCEL,
        NOYES,
    };

    enum class alert_icon {
        Default,
        Error,
        Warning,
        Question,
        Asterisk,
    };

    extern class Window root;

    alert_response alert(
            Window parent,
            std::string text,
            std::string title = RADIANT_NAME,
            alert_type type = alert_type::OK,
            alert_icon icon = alert_icon::Default
    );

    // callbacks

    namespace {
        using GtkCallback = void (*)(_GtkWidget *, void *);
        extern "C" {
        void gtk_container_foreach(_GtkContainer *, GtkCallback, void *);
        }
    }

#define this (*static_cast<self>(this))

    template<class Lambda>
    gulong Object::connect(char const *detailed_signal, Lambda &&c_handler, void *data)
    {
        return g_signal_connect(G_OBJECT(this), detailed_signal, c_handler, data);
    }

    template<class Lambda>
    gulong Object::connect(char const *detailed_signal, Lambda &&c_handler, Object data)
    {
        return g_signal_connect(G_OBJECT(this), detailed_signal, c_handler, (_GtkObject *) data);
    }

    template<class Lambda>
    void IContainer::foreach(Lambda &&lambda)
    {
        GtkCallback cb = [](_GtkWidget *widget, void *data) -> void {
            using Function = typename std::decay<Lambda>::type;
            Function *f = static_cast<Function *>(data);
            (*f)(Widget::from(widget));
        };
        gtk_container_foreach(this, cb, &lambda);
    }

    namespace {
        extern "C" {
        void gtk_list_store_insert_with_values(_GtkListStore *, _GtkTreeIter *, gint position, ...);
        }
    }

    template<class... T>
    void IListStore::append(T... args) {
        static_assert(sizeof...(args) % 2 == 0, "received an odd number of arguments");
        gtk_list_store_insert_with_values(this, NULL, -1, args..., -1);
    }

#undef this

}

#endif
