#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "config.h"

#include <gtkmm.h>

#include "about.h"
#include "notebook.h"
#include "navigation.h"
#include "settings.h"

enum {
	WND_ACTION_COLOR,
	WND_ACTION_NEXT_NOTE,
	WND_ACTION_PREV_NOTE,
	WND_ACTION_SHOW_FIND,
	WND_ACTION_TOGGLE_SIDEBAR,
	WND_ACTION_TOGGLE_PRESENTATION,
	WND_ACTION_TOGGLE_MARKDOWN_RENDERING
};

int mkdirp(std::string dir);

class CMainWindow : public Gtk::ApplicationWindow
{
public:
	CMainWindow(const Glib::RefPtr<Gtk::Application> &);
	virtual ~CMainWindow();

	std::string active_document;
	std::string selected_document;
	
	sigc::connection close_handler;

	void HardClose();
	void FetchAndSave();
	void OpenDocument(std::string filename);
	void SetupDocumentWindow(Glib::ustring filename);
	
	void FetchAndExport();
	
	void FocusDocument();
	
	void FollowLink(Glib::ustring url);

	typedef std::tuple<double, double, double, double> color_t;
protected:
	/* low level window for csd check */
	GdkWindow* window;

	//Signal handlers:
	bool on_close(GdkEventAny* any_event);
	void on_action(std::string name,int type, int param);
	bool on_click_color(GdkEventButton *btn, unsigned int num);
	bool on_motion_notify(GdkEventMotion* event);
	void on_search_text_changed();
	void on_search_next();
	void on_search_prev();
	void on_search_stop();
	bool on_search_lost_focus(GdkEventFocus *event);
	bool on_search_key_press(GdkEventKey *event);
	
	sigc::connection idle_timer;
	bool on_idle();

	Gtk::HBox split;
	//Gtk::Box filler;
	
	/* config */
	std::string default_base_path;
	std::string data_path;
	gchar* json_config_path = NULL;
	void CalculatePaths();
	const gchar* InitializeSettings();
	void RunPreferenceDiag();

	/* settings */
	Glib::RefPtr<Gio::Settings> settings;
	void SettingChange(const Glib::ustring& key);
	bool binit = true;
	void UpdateBasePath();
	
	/* tree view on the left */
	Gtk::ScrolledWindow nav_scroll;
	Gtk::TreeView nav;
	CNavigationView nav_model;
	
	/* document view */
	Gtk::VBox doc_view_box;
	Gtk::ScrolledWindow sview_scroll;
	CNotebook sview;
	Glib::RefPtr<Gsv::Buffer> sbuffer;
	Glib::RefPtr<Gdk::Cursor> pen_cursor, text_cursor, eraser_cursor, selection_cursor;
	
	/* search bar */
	Gtk::SearchBar search_bar;
	Gtk::SearchEntry search_entry;
	
	/* tool palette */
	Glib::RefPtr<Gtk::Builder> toolbar_builder;
	Glib::RefPtr<Gtk::CssProvider> toolbar_style;
	Gtk::Toolbar *toolbar;
	void InitToolbar();
	void UpdateToolbarColors();
	unsigned int active_color = 0;
	
	/* header */
	Gtk::HeaderBar hbar;
	Gtk::MenuButton appbutton;
	
	Gtk::Button presentbtn;

	AboutDiag about;
	void RunAboutDiag();
	/* menu */
	bool navigation = true;
	bool presentation_mode = false;
	bool markdown_rendering = true;
	Glib::RefPtr<Gio::SimpleAction> export_action = add_action("export", sigc::mem_fun(this,&CMainWindow::FetchAndExport));
	Glib::RefPtr<Gio::SimpleAction> pref_action = add_action("pref", sigc::mem_fun(this,&CMainWindow::RunPreferenceDiag));
	Glib::RefPtr<Gio::SimpleAction> about_action = add_action("about", sigc::mem_fun(this,&CMainWindow::RunAboutDiag));
	Glib::RefPtr<Gio::SimpleAction> sidebar_action = add_action_bool("sidebar", sigc::bind( sigc::mem_fun(this,&CMainWindow::on_action), "toggle-sidebar", WND_ACTION_TOGGLE_SIDEBAR, 1), navigation);
	Glib::RefPtr<Gio::SimpleAction> presentation_action = add_action_bool("presentation", sigc::bind( sigc::mem_fun(this,&CMainWindow::on_action), "toggle-presentation-mode", WND_ACTION_TOGGLE_PRESENTATION, 1), presentation_mode);
	Glib::RefPtr<Gio::SimpleAction> markdown_rendering_action = add_action_bool("markdown-rendering", sigc::bind( sigc::mem_fun(this,&CMainWindow::on_action), "markdown-rendering", WND_ACTION_TOGGLE_MARKDOWN_RENDERING, 0), markdown_rendering);
	Glib::RefPtr<Gio::Menu> menu = Gio::Menu::create();
	Gtk::Menu appmenu;

	/* classic menu */
	struct {
		Gtk::VBox menu_box;
		Gtk::MenuBar mbar;
	} cm;

	Gtk::FileChooserButton *dir;

	void SettingBasepathUpdate();
	void SettingDocumentUpdate();
	void SettingCsdUpdate();
	void SettingProximityUpdate();
	void SettingSidebarUpdate();
	void SettingClassicSidebarUpdate();
	void SettingPresentationModeUpdate();

	typedef std::map<const Glib::ustring, sigc::slot<void()>> settingmap_t;
	settingmap_t settingmap {
		{"base-path", sigc::mem_fun(this,&CMainWindow::SettingBasepathUpdate)},
		{"active-document", sigc::mem_fun(this,&CMainWindow::SettingDocumentUpdate)},
		{"colors", sigc::mem_fun(this,&CMainWindow::UpdateToolbarColors)},
		{"csd", sigc::mem_fun(this,&CMainWindow::SettingCsdUpdate)},
		{"proximity-widgets", sigc::mem_fun(this,&CMainWindow::SettingProximityUpdate)},
		{"sidebar", sigc::mem_fun(this,&CMainWindow::SettingSidebarUpdate)},
		{"classic-sidebar", sigc::mem_fun(this,&CMainWindow::SettingClassicSidebarUpdate)},
		{"presentation-mode", sigc::mem_fun(this,&CMainWindow::SettingPresentationModeUpdate)}
	};
};

#endif // MAINWINDOW_H
