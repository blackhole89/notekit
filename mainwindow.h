#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "config.h"

#include <gtkmm.h>

#include <json/json.h>

#include "about.h"
#include "notebook.h"
#include "navigation.h"

enum {
	WND_ACTION_COLOR,
	WND_ACTION_NEXT_NOTE,
	WND_ACTION_PREV_NOTE,
	WND_ACTION_TOGGLE_SIDEBAR,
	WND_ACTION_TOGGLE_ZEN
};

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
	void SetActiveFilename(std::string filename);
	
	void FocusDocument();
	
	void GetColor(int id, float &r, float &g, float &b);
	void SetColor(int id, float r, float g, float b);
protected:
	//Signal handlers:
	bool on_close(GdkEventAny* any_event);
	void on_action(std::string name,int type, int param);
	bool on_click_color(GdkEventButton *btn, int num);
	bool on_motion_notify(GdkEventMotion* event);
	
	sigc::connection idle_timer;
	bool on_idle();

	Gtk::HBox split;
	//Gtk::Box filler;
	
	/* config */
	std::string config_path;
	std::string default_base_path;
	std::string data_path;
	void CalculatePaths();
	Json::Value config;
	void LoadConfig();
	void SaveConfig();
	void RunConfigWindow();
	
	/* tree view on the left */
	Gtk::ScrolledWindow nav_scroll;
	Gtk::TreeView nav;
	CNavigationView nav_model;
	
	/* document view */
	Gtk::ScrolledWindow sview_scroll;
	CNotebook sview;
	Glib::RefPtr<Gsv::Buffer> sbuffer;
	Glib::RefPtr<Gdk::Cursor> pen_cursor, text_cursor, eraser_cursor, selection_cursor;
	
	/* tool palette */
	Glib::RefPtr<Gtk::Builder> toolbar_builder;
	Glib::RefPtr<Gtk::CssProvider> toolbar_style;
	Gtk::Toolbar *toolbar;
	void InitToolbar();
	void UpdateToolbarColors();
	
	/* header */
	bool use_hbar;
	Gtk::HeaderBar hbar;
	Gtk::MenuButton appbutton;
	
	Gtk::Button zenbtn;
	bool zen = false;

	AboutDiag about;
	/* menu */
	bool navigation = true;
	Glib::RefPtr<Gio::SimpleAction> pref_action = add_action("pref", sigc::mem_fun(this,&CMainWindow::RunConfigWindow));
	Glib::RefPtr<Gio::SimpleAction> about_action = add_action("about", sigc::mem_fun(about,&AboutDiag::show));
	Glib::RefPtr<Gio::SimpleAction> sidebar_action = add_action_bool("sidebar", sigc::bind( sigc::mem_fun(this,&CMainWindow::on_action), "toggle-sidebar", WND_ACTION_TOGGLE_SIDEBAR, 1), &navigation);
	Glib::RefPtr<Gio::SimpleAction> zen_action = add_action_bool("zen", sigc::bind( sigc::mem_fun(this,&CMainWindow::on_action), "toggle-zen", WND_ACTION_TOGGLE_ZEN, 1), zen);
	Glib::RefPtr<Gio::Menu> menu = Gio::Menu::create();
	Gtk::Menu appmenu;
	
	/* classic menu */
	struct {
		Gtk::VBox menu_box;
		Gtk::MenuBar mbar;
	} cm;
};

#endif // MAINWINDOW_H
