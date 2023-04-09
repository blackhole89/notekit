#include "mainwindow.h"
#include <gtkmm/application.h>

CMainWindow *mainwindow;

#ifdef _WIN32
#include <windows.h>

int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
#else
int main (int argc, char *argv[])
#endif
{
	Gsv::init();

#ifdef _WIN32
	if (g_getenv("NK_WIN32_DEBUG") && AllocConsole()) {
		freopen("CONOUT$", "w", stdout);
		freopen("CONOUT$", "w", stderr);
	}
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create("com.github.blackhole89.notekit");
#else	
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(argc, argv, "com.github.blackhole89.notekit");
#endif
	
	app->signal_activate().connect( [app,&mainwindow]() {
			mainwindow=new CMainWindow(app);
			app->add_window(*mainwindow);
		} );
	
	app->set_accel_for_action("notebook.next-note", "<Primary>Tab");
	app->set_accel_for_action("notebook.prev-note", "<Primary><Shift>Tab");
	app->set_accel_for_action("notebook.show-find", "<Primary>F");
	app->set_accel_for_action("win.sidebar", "F9");
	app->set_accel_for_action("win.presentation", "F11");

	auto ret = app->run();


	if(mainwindow != nullptr){
		delete mainwindow;
	}

	return ret;
}
