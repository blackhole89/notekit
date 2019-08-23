#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <gtkmm.h>

//#include <webkit2/webkit2.h>

#include "notebook.h"
#include "navigation.h"

class CMainWindow : public Gtk::Window
{
public:
	CMainWindow();
	virtual ~CMainWindow();

	std::string active_document;
	std::string loading_document;
	std::string saving_document;
	
	bool close_after_saving;
	sigc::connection close_handler;

	void HardClose();
	void FetchAndSave();
	void OpenDocument(std::string filename);
	
	void FocusDocument();
protected:
	//Signal handlers:
	bool on_close(GdkEventAny* any_event);

	Gtk::HBox split;
	Gtk::Box filler;
	Gtk::TreeView nav;
	CNavigationView nav_model;
	
	Gtk::ScrolledWindow sview_scroll;
	CNotebook sview;
	Glib::RefPtr<Gsv::Buffer> sbuffer;
	
	Gtk::Button testbutton;
	
	Gtk::HeaderBar hbar;
};

#endif // MAINWINDOW_H