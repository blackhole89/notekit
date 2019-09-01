#include "mainwindow.h"
#include "navigation.h"
#include <iostream>

CMainWindow::CMainWindow() : nav_model("notesbase")
{
	// Load config.
	LoadConfig();
	
	// make sure document is in place for tree expansion so we can set the selection
	selected_document = config["active_document"].asString();
	
	// Sets the border width of the window.
	set_border_width(0);
	set_size_request(900,600);
	
	Glib::RefPtr<Gtk::CssProvider> sview_css = Gtk::CssProvider::create();
	sview_css->load_from_path("data/stylesheet.css");

	get_style_context()->add_class("notekit");
	override_background_color(Gdk::RGBA("rgba(0,0,0,0)"));
	
	hbar.set_show_close_button(true);
	hbar.set_title("NoteKit");
	appbutton.set_image_from_icon_name("accessories-text-editor", Gtk::ICON_SIZE_BUTTON, true);
	set_icon_name("accessories-text-editor");
	
	hbar.pack_start(appbutton);
	set_titlebar(hbar);
	
	nav_model.AttachView(this,&nav);
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

	sview_scroll.add( sview);
	//split.override_background_color(Gdk::RGBA("rgb(255,255,255)"));
	//filler.set_size_request(32,-1);
	sview.margin_x=32;
	sview.property_left_margin().set_value(32);
	//split.pack_start(filler,Gtk::PACK_SHRINK);
	//filler.override_background_color(Gdk::RGBA("rgb(255,255,255)"));
	//split.set_spacing(16);
	split.pack_start( sview_scroll );
	
	sbuffer = sview.get_source_buffer();
	
	InitToolbar();
	UpdateToolbarColors();
	split.pack_start(*toolbar,Gtk::PACK_SHRINK);
	
	add(split);

	show_all();
	
	OpenDocument(selected_document);
	
	close_after_saving=false;
	close_handler = this->signal_delete_event().connect( sigc::mem_fun(this, &CMainWindow::on_close) );
}

void CMainWindow::LoadConfig()
{
	char *buf; gsize length;
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(nav_model.base+"/config.json");
	
	try {
		file->load_contents(buf, length);
	} catch(Gio::Error &e) {
		file = Gio::File::create_for_path(std::string("data/default_config.json"));
		
		try {
			file->load_contents(buf, length);
		} catch(Gio::Error &e) {
			fprintf(stderr,"FATAL: config.json not found in note tree, and default_config.json not found in data tree!\n");
			exit(-1);
		}
	}

	Json::CharReaderBuilder rbuilder;
	std::istringstream i(buf);
	std::string errs;
	Json::parseFromStream(rbuilder, i, &config, &errs);

	g_free(buf);
}

void CMainWindow::SaveConfig()
{
	Json::StreamWriterBuilder wbuilder;
	std::string str = Json::writeString(wbuilder, config);
	
	FILE *fl = fopen( (nav_model.base+"/config.json").c_str(), "wb");
	fwrite(str.c_str(),str.length(),1,fl);
	fclose(fl);
}

void CMainWindow::InitToolbar()
{
	toolbar_builder = Gtk::Builder::create_from_file("data/toolbar.glade");
	toolbar_builder->get_widget("toolbar",toolbar);
	
	Glib::RefPtr<Gtk::IconTheme> thm = Gtk::IconTheme::get_default(); //get_for_screen(get_window()->get_screen());
	thm->append_search_path("data/icons");
	
	toolbar_style = Gtk::CssProvider::create();
	toolbar_style->load_from_data("#color1 { -gtk-icon-palette: warning #ff00ff; }");
	//toolbar_style->load_from_path("data/test.css");
	//toolbar->get_style_context()->add_provider(toolbar_style,GTK_STYLE_PROVIDER_PRIORITY_USER);
	
	for(int i=1;i<=2;++i) {
		char buf[8];
		Gtk::Widget *sdfg;
		
		sprintf(buf,"color%d",i);
		toolbar_builder->get_widget(buf,sdfg);
		sdfg->get_style_context()->add_provider(toolbar_style,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	}
	//Gtk::StyleContext::add_provider_for_screen(Gdk::Screen::get_default(),toolbar_style,GTK_STYLE_PROVIDER_PRIORITY_USER);
}

void CMainWindow::UpdateToolbarColors()
{
	std::string colorcss;
	
	int i=1;
	char buf[255];
	
	for(auto a : config["colors"]) {
		sprintf(buf,"#color%d { -gtk-icon-palette: warning rgb(%d,%d,%d); }\n", i, (int)(255*a["r"].asDouble()), (int)(255*a["g"].asDouble()), (int)(255*a["b"].asDouble()) );
		colorcss += buf;
		++i;	}
	
	toolbar_style->load_from_data(colorcss);
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
	
	/* TODO: this kills the xattrs */
	FILE *fl = fopen("temp.txt", "wb");
	fwrite(str.c_str(),str.length(),1,fl);
	fclose(fl);
	
	std::string cmd;
	cmd = "mv temp.txt \"";
	cmd += nav_model.base;
	cmd += "/";
	cmd += active_document;
	cmd += "\"";
	system(cmd.c_str());
}

void CMainWindow::OpenDocument(std::string filename)
{
	FetchAndSave();
	
	if(filename=="") {
		active_document="";
		hbar.set_subtitle("");
		config["active_document"]="";
		
		sbuffer->begin_not_undoable_action();
		sbuffer->set_text("");
		sbuffer->end_not_undoable_action();
		return;
	}
	
	char *buf; gsize length;
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(nav_model.base + "/" + filename);
	file->load_contents(buf, length);
	
	sbuffer->begin_not_undoable_action();
	sbuffer->set_text("");
	Gtk::TextBuffer::iterator i = sbuffer->begin();
	if(length) sbuffer->deserialize(sbuffer,"text/notekit-markdown",i,(guint8*)buf,length);
	//sbuffer->set_text(buf);
	sbuffer->end_not_undoable_action();
	
	
	active_document = filename;
	hbar.set_subtitle(active_document);
	config["active_document"]=filename;
	
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
	SaveConfig();
	
	if(active_document=="") return false;

	FetchAndSave();
	
	return false;
}