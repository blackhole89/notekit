#include "mainwindow.h"
#include "navigation.h"
#include <iostream>

CMainWindow::CMainWindow() : nav_model("notesbase"), sview()
{
	// Load config.
	LoadConfig();
	
	// make sure document is in place for tree expansion so we can set the selection
	selected_document = config["active_document"].asString();
	
	// set up window
	set_border_width(0);
	set_size_request(900,600);
	
	/* load stylesheet */
	Glib::RefPtr<Gtk::CssProvider> sview_css = Gtk::CssProvider::create();
	sview_css->load_from_path("data/stylesheet.css");

	get_style_context()->add_class("notekit");
	override_background_color(Gdk::RGBA("rgba(0,0,0,0)"));
	
	/* set up header bar */
	hbar.set_show_close_button(true);
	hbar.set_title("NoteKit");
	appbutton.set_image_from_icon_name("accessories-text-editor", Gtk::ICON_SIZE_BUTTON, true);
	set_icon_name("accessories-text-editor");
	
	hbar.pack_start(appbutton);
	set_titlebar(hbar);
	
	/* install tree view */
	nav_model.AttachView(this,&nav);
	nav.get_style_context()->add_class("sidebar");
	
	nav_scroll.add(nav);
	nav_scroll.set_size_request(200,-1);
	split.pack_start(nav_scroll,Gtk::PACK_SHRINK);
	
	get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	nav.get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* install document view */
	sbuffer = sview.get_source_buffer();
	sview.get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	sview.set_name("mainView");
	sview_scroll.get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	sview_scroll.set_name("mainViewScroll");
	//sview.set_border_window_size(Gtk::TEXT_WINDOW_LEFT,16);

	sview_scroll.add( sview);
	sview.margin_x=32;
	sview.property_left_margin().set_value(32);
	split.pack_start( sview_scroll );
	
	/* install tool palette */
	InitToolbar();
	// some actions need data from the main window and therefore need to live here
	insert_action_group("notebook",sview.actions);
	UpdateToolbarColors();
	split.pack_start(*toolbar,Gtk::PACK_SHRINK);
	on_action("color1",WND_ACTION_COLOR,1);
	
	/* add the overall vbox */
	add(split);

	show_all();
	
	/* workaround */
	Gtk::RadioToolButton *b;
	toolbar_builder->get_widget("medium",b); b->set_active();
	
	OpenDocument(selected_document);
	
	close_after_saving=false;
	close_handler = this->signal_delete_event().connect( sigc::mem_fun(this, &CMainWindow::on_close) );
	
	signal_motion_notify_event().connect(sigc::mem_fun(this,&CMainWindow::on_motion_notify),false);
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
	
	/* generate colour buttons */
	Gtk::RadioToolButton *b0;
	for(int i=1;i<=config["colors"].size();++i) {
		char buf[255];
		Gtk::Widget *sdfg;
		
		sprintf(buf,"color%d",i);
		
		#define ACTION(name,param1,param2) sview.actions->add_action(name, sigc::bind( sigc::mem_fun(this,&CMainWindow::on_action), std::string(name), param1, param2 ) )
		ACTION(buf,WND_ACTION_COLOR,i);
		
		Gtk::RadioToolButton *b;
		if(i==1) {
			b = b0 = new Gtk::RadioToolButton(buf);
		} else {
			Gtk::RadioToolButton::Group g = b0->get_group(); // because for some reason, the group argument is &
			b = new Gtk::RadioToolButton(g, buf);
		}
		b->set_icon_name("pick-color-symbolic");
		b->set_name(buf);
		b->signal_button_press_event().connect(sigc::bind(sigc::mem_fun(this,&CMainWindow::on_click_color),i));
		b->get_style_context()->add_provider(toolbar_style,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
		sprintf(buf,"notebook.color%d",i);
		gtk_actionable_set_action_name(GTK_ACTIONABLE(b->gobj()), buf);
		
		Gtk::manage(b);
		toolbar->append(*b);
	}
}

void CMainWindow::GetColor(int id, float &r, float &g, float &b)
{
	Json::Value def;
	def["r"]=def["g"]=def["b"]=0;
	r=config["colors"].get(id-1,def)["r"].asFloat();
	g=config["colors"].get(id-1,def)["g"].asFloat();
	b=config["colors"].get(id-1,def)["b"].asFloat();
}

void CMainWindow::SetColor(int id, float r, float g, float b)
{
	Json::Value def;
	def["r"]=def["g"]=def["b"]=0;
	config["colors"][id-1]["r"]=r;
	config["colors"][id-1]["g"]=g;
	config["colors"][id-1]["b"]=b;
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
		sview.set_editable(false);
		sview.set_can_focus(false);
		
		active_document="";
		hbar.set_subtitle("");
		config["active_document"]="";
		
		sbuffer->begin_not_undoable_action();
		sbuffer->set_text("( Nothing opened. Please create or open a file. ) ");
		sbuffer->end_not_undoable_action();
		return;
	}
	sview.set_editable(true);
	sview.set_can_focus(true);
	
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

void CMainWindow::on_action(std::string name, int type, int param)
{
	printf("%s\n",name.c_str());
	switch(type) {
	case WND_ACTION_COLOR:
		GetColor(param, sview.active.r,sview.active.g, sview.active.b);
		sview.active.a=1;
		break;
	}
}

bool CMainWindow::on_click_color(GdkEventButton* e, int number)
{
	if(e->button == 3) {
		// right click
		Gtk::ColorChooserDialog dialog("Select a color");
		dialog.set_transient_for(*this);
		
		char buf[32];
		float r,g,b;
		GetColor(number,r,g,b);
		sprintf(buf,"rgb(%d,%d,%d)",(int)(r*255),(int)(g*255),(int)(b*255));
		
		dialog.set_rgba(Gdk::RGBA(buf));
		
		const int result = dialog.run();
		
		switch(result) {
		case Gtk::RESPONSE_OK: {
			Gdk::RGBA col = dialog.get_rgba();
			SetColor(number,col.get_red(),col.get_green(),col.get_blue());
			UpdateToolbarColors();
			break;
		}
		default:;
		}

		return true;
	}
	return false;
}

bool CMainWindow::on_motion_notify(GdkEventMotion *e)
{
	GdkDevice *d = gdk_event_get_source_device((GdkEvent*)e);
	
	if(d != sview.last_device) {
		sview.last_device = d;
		
		Gtk::RadioToolButton *b;
		
		if(!sview.devicemodes.count(d)) {
			if(gdk_device_get_n_axes(d)>4) {
				// assume it's a pen
				sview.devicemodes[d] = NB_MODE_DRAW;
			} else {
				sview.devicemodes[d] = NB_MODE_TEXT;
			}
		}
		
		switch(sview.devicemodes[d]) {
		case NB_MODE_DRAW:
			toolbar_builder->get_widget("draw",b);
			break;
		case NB_MODE_TEXT:
			toolbar_builder->get_widget("text",b);
			break;
		case NB_MODE_ERASE:
			toolbar_builder->get_widget("erase",b);
			break;
		}
		b->set_active();
	}
	return false;
}