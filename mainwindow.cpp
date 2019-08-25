#include "mainwindow.h"
#include "navigation.h"
#include <iostream>

#include <webkitdom/webkitdom.h>

CMainWindow::CMainWindow() : nav_model("notesbase")
{
	// Sets the border width of the window.
	set_border_width(0);
	set_size_request(800,600);
	
	Glib::RefPtr<Gtk::CssProvider> sview_css = Gtk::CssProvider::create();
	sview_css->load_from_path("sourceview/stylesheet.css");

	get_style_context()->add_class("notekit");
	override_background_color(Gdk::RGBA("rgba(0,0,0,0)"));
	
	hbar.set_show_close_button(true);
	hbar.set_title("NoteKit");
	appbutton.set_image_from_icon_name("accessories-text-editor", Gtk::ICON_SIZE_BUTTON, true);
	set_icon_name("accessories-text-editor");
	
	hbar.pack_start(appbutton);
	set_titlebar(hbar);
	
	nav_model.AttachView(&nav);
	nav.get_style_context()->add_class("sidebar");
	
	nav_scroll.add(nav);
	nav_scroll.set_size_request(200,-1);
	split.pack_start(nav_scroll,Gtk::PACK_SHRINK);
	
	get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	nav.get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	sview.get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	sview.set_name("mainView");
	sview_scroll.get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	sview_scroll.set_name("mainViewScroll");
	//sview.set_border_window_size(Gtk::TEXT_WINDOW_LEFT,16);
	
	sbuffer = sview.get_source_buffer();

	sview_scroll.add( sview);
	//split.override_background_color(Gdk::RGBA("rgb(255,255,255)"));
	//filler.set_size_request(32,-1);
	sview.margin_x=32;
	sview.property_left_margin().set_value(32);
	//split.pack_start(filler,Gtk::PACK_SHRINK);
	filler.override_background_color(Gdk::RGBA("rgb(255,255,255)"));
	//split.set_spacing(16);
	split.pack_end( sview_scroll );
	add(split);

	show_all();
	
	loading_document = "";
	
	close_after_saving=false;
	close_handler = this->signal_delete_event().connect( sigc::mem_fun(this, &CMainWindow::on_close) );
}

void CMainWindow::HardClose()
{
	close_handler.disconnect();
	close();
}

void CMainWindow::FocusDocument()
{
	sview.grab_focus();
}

void CMainWindow::FetchAndSave()
{
	if(active_document=="") return;
	
	gsize len;
	guint8 *buf;
	buf = sbuffer->serialize(sbuffer,"text/notekit-markdown",sbuffer->begin(),sbuffer->end(),len);
	std::string str((const char*)buf,len);
	g_free(buf);
	//Glib::ustring str = sbuffer->get_text(true);
	
	FILE *fl = fopen("temp.txt", "wb");
	fwrite(str.c_str(),str.length(),1,fl);
	fclose(fl);
	
	std::string cmd;
	cmd = "mv temp.txt \"";
	cmd += "notesbase/";
	cmd += active_document;
	cmd += "\"";
	system(cmd.c_str());
}

void CMainWindow::OpenDocument(std::string filename)
{
	if(loading_document != "") return;
	
	FetchAndSave();
	
	if(filename=="") {
		active_document="";
		hbar.set_subtitle("");
		
		sbuffer->begin_not_undoable_action();
		sbuffer->set_text("");
		sbuffer->end_not_undoable_action();
		return;
	}
	
	char *buf; gsize length;
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(std::string("notesbase/")+filename);
	file->load_contents(buf, length);
	
	sbuffer->begin_not_undoable_action();
	sbuffer->set_text("");
	Gtk::TextBuffer::iterator i = sbuffer->begin();
	if(length) sbuffer->deserialize(sbuffer,"text/notekit-markdown",i,(guint8*)buf,length);
	//sbuffer->set_text(buf);
	sbuffer->end_not_undoable_action();
	
	
	active_document = filename;
	hbar.set_subtitle(active_document);
	
	/*Glib::RefPtr<Gtk::TextBuffer::ChildAnchor> anch = sbuffer->create_child_anchor(sbuffer->begin());
	testbutton.set_text("hello world");
	sview.add_child_at_anchor(testbutton,anch);
	testbutton.show();*/
	g_free(buf);
}

CMainWindow::~CMainWindow()
{
}

bool CMainWindow::on_close(GdkEventAny* any_event)
{
	if(active_document=="") return false;

	FetchAndSave();
	
	return false;
}