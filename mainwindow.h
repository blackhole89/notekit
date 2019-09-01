#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gtkmm.h>

#include <json/json.h>

#include "notebook.h"
#include "navigation.h"

enum {
	WND_ACTION_COLOR
};

class CMainWindow : public Gtk::Window
{
public:
	CMainWindow();
	virtual ~CMainWindow();

	std::string active_document;
	std::string selected_document;
	
	bool close_after_saving;
	sigc::connection close_handler;

	void HardClose();
	void FetchAndSave();
	void OpenDocument(std::string filename);
	
	void FocusDocument();
	
	void GetColor(int id, float &r, float &g, float &b);
protected:
	//Signal handlers:
	bool on_close(GdkEventAny* any_event);
	void on_action(std::string name,int type, int param);

	Gtk::HBox split;
	//Gtk::Box filler;
	
	/* config */
	Json::Value config;
	void LoadConfig();
	void SaveConfig();
	
	/* tree view on the left */
	Gtk::ScrolledWindow nav_scroll;
	Gtk::TreeView nav;
	CNavigationView nav_model;
	
	/* document view */
	Gtk::ScrolledWindow sview_scroll;
	CNotebook sview;
	Glib::RefPtr<Gsv::Buffer> sbuffer;
	
	/* tool palette */
	Glib::RefPtr<Gtk::Builder> toolbar_builder;
	Glib::RefPtr<Gtk::CssProvider> toolbar_style;
	Gtk::Toolbar *toolbar;
	void InitToolbar();
	void UpdateToolbarColors();
	
	/* header */
	Gtk::HeaderBar hbar;
	Gtk::Button appbutton;
};

#endif // MAINWINDOW_H