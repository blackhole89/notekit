#include "mainwindow.h"
#include <gtkmm/application.h>

CMainWindow *mainwindow;

int main (int argc, char *argv[])
{
	Gsv::init();
	
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, "test.notekit");

	mainwindow = new CMainWindow();
	
	app->set_accel_for_action("notebook.next-note", "<Primary>Tab");
	app->set_accel_for_action("notebook.prev-note", "<Primary><Shift>Tab");

	//Shows the window and returns when it is closed.
	auto ret = app->run(*mainwindow);

	if(mainwindow != nullptr){
		delete mainwindow;
	}

	return ret;
}
