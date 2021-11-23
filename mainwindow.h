#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "config.h"

#include <gtkmm.h>

#include <json/json.h>

#include "notebook.h"
#include "navigation.h"

enum {
	WND_ACTION_COLOR,
	WND_ACTION_NEXT_NOTE,
	WND_ACTION_PREV_NOTE,
	WND_ACTION_TOGGLE_SIDEBAR
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
	void SetActiveFilename(std::string filename);
	
	void FetchAndExport();
	
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
	
	/* menu */
	Gtk::Menu appmenu;
	struct {
		Gtk::MenuItem prefs;
		Gtk::SeparatorMenuItem sep;
		Gtk::CheckMenuItem hide_sidebar;
		Gtk::MenuItem exprt;
	} am;
	
	/* classic menu */
	struct {
		Gtk::VBox menu_box;
		Gtk::MenuBar mbar;
		Gtk::MenuItem view;
	} cm;
};

#endif // MAINWINDOW_H