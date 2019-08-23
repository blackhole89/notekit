#include "mainwindow.h"
#include <gtkmm/application.h>

CMainWindow *mainwindow;

int main (int argc, char *argv[])
{
	Gsv::init();
	
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, "test.notekit");

	mainwindow = new CMainWindow();

	//Shows the window and returns when it is closed.
	return app->run(*mainwindow);
}