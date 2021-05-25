#include "mainwindow.h"
#include "navigation.h"
#include <iostream>
#include <fontconfig/fontconfig.h>

#ifdef HAVE_CLATEXMATH
#define __OS_Linux__
#include "latex.h"
#endif

CMainWindow::CMainWindow(const Glib::RefPtr<Gtk::Application>& app) : Gtk::ApplicationWindow(app), nav_model(), sview()
{
	// Determine paths to operate on.
	CalculatePaths();
	
	// Load config.
	LoadConfig();
	
	printf("== This is notekit, built at " __TIMESTAMP__ ". ==\n");
	printf("Detected paths:\n");
	printf("Config: %s\n",config_path.c_str());
	printf("Active notes path: %s\n",config["base_path"].asString().c_str());
	printf("Default notes path: %s\n",default_base_path.c_str());
	printf("Resource path: %s\n",data_path.c_str());
	printf("\n");
	
	#ifdef HAVE_CLATEXMATH
	tex::LaTeX::init((data_path+"/data/latex").c_str());
	/* allow \newcommand to override quietly, since we will be rerendering \newcommands unpredictably */
	tex::TeXRender *r = tex::LaTeX::parse(tex::utf82wide("\\fatalIfCmdConflict{false}"),1,1,1,0);
	if(r) delete r;
	#endif
	
	sview.Init(data_path, config["use_highlight_proxy"].asBool());
	nav_model.SetBasePath(config["base_path"].asString());
	
	// make sure document is in place for tree expansion so we can set the selection
	selected_document = config["active_document"].asString();
	
	// set up window
	set_border_width(0);
	set_default_size(900,600);
	
	// load additional fonts
	FcConfigAppFontAddDir(NULL, (FcChar8*)(data_path+"/data/fonts").c_str());
	
	/* load stylesheet */
	Glib::RefPtr<Gtk::CssProvider> sview_css = Gtk::CssProvider::create();
	sview_css->load_from_path(data_path+"/data/stylesheet.css");

	get_style_context()->add_class("notekit");
	override_background_color(Gdk::RGBA("rgba(0,0,0,0)"));
	
	/* set up menu */
	am.prefs.set_label("Preferences");
	am.prefs.signal_activate().connect( sigc::mem_fun(this,&CMainWindow::RunConfigWindow) );
	appmenu.append(am.prefs);
	am.exprt.set_label("Export current");
	am.exprt.signal_activate().connect( sigc::mem_fun(this,&CMainWindow::FetchAndExport) );
	appmenu.append(am.exprt);
	appmenu.append(am.sep);
	am.hide_sidebar.set_label("Hide sidebar");
	// this should probably use Gio::Menu actions instead...? documentation is an unholy mess on this
	am.hide_sidebar.signal_activate().connect( sigc::bind( sigc::mem_fun(this,&CMainWindow::on_action), "toggle-sidebar", WND_ACTION_TOGGLE_SIDEBAR, 1)); 
	appmenu.append(am.hide_sidebar);
	appmenu.show_all();
	
	/* set up header bar */
	use_hbar = config["use_headerbar"].asBool();
	
	if(use_hbar) {
		hbar.set_show_close_button(true);
		hbar.set_title("NoteKit");
		appbutton.set_image_from_icon_name("accessories-text-editor", Gtk::ICON_SIZE_BUTTON, true);
		appbutton.set_menu(appmenu);
		set_icon_name("accessories-text-editor");
	
		hbar.pack_start(appbutton);
		set_titlebar(hbar);
	}
	
	/* install tree view */
	nav_model.AttachView(this,&nav);
	nav.get_style_context()->add_class("sidebar");
	
	nav_scroll.add(nav);
	nav_scroll.set_size_request(200,-1);
	split.pack_start(nav_scroll,Gtk::PACK_SHRINK);
	
	get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	nav.get_style_context()->add_provider(sview_css,GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* load cursor types for document */
	pen_cursor = Gdk::Cursor::create(Gdk::Display::get_default(),"default");
	text_cursor = Gdk::Cursor::create(Gdk::Display::get_default(),"text");
	eraser_cursor = Gdk::Cursor::create(Gdk::Display::get_default(),"cell");
	selection_cursor = Gdk::Cursor::create(Gdk::Display::get_default(),"cross");

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
	
	#define ACTION(name,param1,param2) sview.actions->add_action(name, sigc::bind( sigc::mem_fun(this,&CMainWindow::on_action), std::string(name), param1, param2 ) )
	ACTION("next-note",WND_ACTION_NEXT_NOTE,1);
	ACTION("prev-note",WND_ACTION_PREV_NOTE,1);
	
	/* add the top-level layout */
	if(use_hbar) {
		add(split);
	} else {
		/* without a headerbar, we also need to have a vbox for the classic menu here */
		cm.view.set_label("View");
		// TODO: this results in Gtk-CRITICALs on quit as appmenu elements are double freed. 
		// Is there any way we can set a submenu here without making free of cm.view recursively free appmenu?
		cm.view.set_submenu(appmenu); 
		cm.mbar.append(cm.view);
		cm.menu_box.pack_start(cm.mbar,Gtk::PACK_SHRINK);
		cm.menu_box.pack_start(split);
		add(cm.menu_box);
	}

	show_all();
	
	/* workaround to make sure the right pen width is selected at start */
	Gtk::RadioToolButton *b;
	toolbar_builder->get_widget("medium",b); b->set_active(false); b->set_active(true);
	
	OpenDocument(selected_document);
	
	close_handler = this->signal_delete_event().connect( sigc::mem_fun(this, &CMainWindow::on_close) );
	
	signal_motion_notify_event().connect(sigc::mem_fun(this,&CMainWindow::on_motion_notify),false);
	
	idle_timer = Glib::signal_timeout().connect(sigc::mem_fun(this,&CMainWindow::on_idle),5000,Glib::PRIORITY_LOW);
}

int mkdirp(std::string dir)
{
	std::string cmd;
	cmd = "mkdir -p \"";
	cmd += dir;
	cmd += "\"";
	return system(cmd.c_str());
}

/* Half-baked interpretation of XDG spec */
void CMainWindow::CalculatePaths()
{
	char *dbg = getenv("NK_DEVEL");
	if(dbg != NULL) {
		fprintf(stderr,"INFO: Debug mode set! Will operate in %s.\n", dbg);
		data_path=dbg;
		config_path= data_path + "/notesbase";
		default_base_path= data_path + "/notesbase";
		return;
	}

	char *home = getenv("HOME");
	if(!home || !*home) {
		fprintf(stderr,"WARNING: Could not determine user's home directory! Will operate in current working directory.\n");
		config_path="notesbase";
		default_base_path="notesbase";
		data_path=".";
		return;
	}
	
	char *config_home = getenv("XDG_CONFIG_HOME");
	if(!config_home || !*config_home) config_path=std::string(home)+"/.config/notekit";
	else config_path=std::string(config_home)+"/notekit";
	
	if(mkdirp(config_path)) {
		fprintf(stderr,"WARNING: Could not create config path '%s'. Falling back to current working directory.\n",config_path.c_str());
		config_path="notesbase";
	}
	
	char *data_home = getenv("XDG_DATA_HOME");
	if(!data_home || !*data_home) default_base_path=std::string(home)+"/.local/share/notekit";
	else default_base_path=std::string(data_home)+"/notekit";
	
	/* TODO: we might not need this if we have a manually-set base path already */
	if(mkdirp(default_base_path)) {
		fprintf(stderr,"WARNING: Could not create default base path '%s'. Falling back to current working directory.\n",default_base_path.c_str());
		default_base_path="notesbase";
	}
	
	char *data_dirs = getenv("XDG_DATA_DIRS");
	if(!data_dirs || !*data_dirs)
		data_dirs=strdup("/usr/local/share:/usr/share");
	else
	    data_dirs=strdup(data_dirs);
	
	char *next=data_dirs;
	strtok(data_dirs,":");
	struct stat statbuf;
	data_path=".";
	do {
		std::string putative_dir = std::string(next) + "/notekit";
		if(!stat(putative_dir.c_str(),&statbuf)) {
			data_path=putative_dir;
			break;
		}
	} while( (next = strtok(NULL,":")) );
	free(data_dirs);
}

void CMainWindow::LoadConfig()
{
	char *buf; gsize length;
	Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(config_path+"/config.json");
	
	try {
		file->load_contents(buf, length);
	} catch(Gio::Error &e) {
		file = Gio::File::create_for_path(data_path+"/data/default_config.json");
		
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
	
	/* set default base path if none has been explicitly set */
	if(!config["base_path"].isString() || config["base_path"].asString()=="") {
		config["base_path"]=default_base_path;
	}

	g_free(buf);
}

void CMainWindow::SaveConfig()
{
	Json::StreamWriterBuilder wbuilder;
	std::string str = Json::writeString(wbuilder, config);
	
	FILE *fl = fopen( (config_path+"/config.json").c_str(), "wb");
	fwrite(str.c_str(),str.length(),1,fl);
	fclose(fl);
}

void CMainWindow::RunConfigWindow()
{
	Glib::RefPtr<Gtk::Builder> config_builder;
	
	config_builder = Gtk::Builder::create_from_file(data_path+"/data/preferences.glade");
	Gtk::Dialog *dlg;
	config_builder->get_widget("preferences",dlg); 
	Gtk::FileChooserButton *dir;
	Gtk::CheckButton *use_headerbar, *use_highlight_proxy;
	config_builder->get_widget("base_path",dir); 
	config_builder->get_widget("use_headerbar",use_headerbar);
	config_builder->get_widget("use_highlight_proxy",use_highlight_proxy);
	dir->set_current_folder(config["base_path"].asString());
	use_headerbar->set_active(config["use_headerbar"].asBool());
	use_highlight_proxy->set_active(config["use_highlight_proxy"].asBool());
	if(dlg->run() == Gtk::RESPONSE_OK) {
		config["base_path"]=dir->get_current_folder();
		config["use_highlight_proxy"]=use_highlight_proxy->get_active();
		config["use_headerbar"]=use_headerbar->get_active();
		SaveConfig();
	}
	dlg->hide();
}

void CMainWindow::InitToolbar()
{
	toolbar_builder = Gtk::Builder::create_from_file(data_path+"/data/toolbar.glade");
	toolbar_builder->get_widget("toolbar",toolbar);
	
	Glib::RefPtr<Gtk::IconTheme> thm = Gtk::IconTheme::get_default(); //get_for_screen(get_window()->get_screen());
	thm->append_search_path(data_path+"/data/icons");
	
	toolbar_style = Gtk::CssProvider::create();
	toolbar_style->load_from_data("#color1 { -gtk-icon-palette: warning #ff00ff; }");
	
	/* generate colour buttons */
	Gtk::RadioToolButton *b0 = nullptr;
	for(unsigned int i=1;i<=config["colors"].size();++i) {
		char buf[255];
		//Gtk::Widget *sdfg;
		
		sprintf(buf,"color%d",i);
		
		#define ACTION(name,param1,param2) sview.actions->add_action(name, sigc::bind( sigc::mem_fun(this,&CMainWindow::on_action), std::string(name), param1, param2 ) )
		ACTION(buf,WND_ACTION_COLOR,i);
		
		Gtk::RadioToolButton *b = nullptr;
		if(i == 1) {
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
		++i;
	}
	
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
	sview.last_modified=0;
	
	if(active_document=="") return;
	
	gsize len;
	guint8 *buf;
	buf = sbuffer->serialize(sbuffer,"text/notekit-markdown",sbuffer->begin(),sbuffer->end(),len);
	std::string str((const char*)buf,len);
	g_free(buf);
	//Glib::ustring str = sbuffer->get_text(true);
	
	std::string filename;
	filename = nav_model.base;
	filename += "/";
	filename += active_document;
	std::string tmp_filename;
	tmp_filename = filename;
	tmp_filename += ".tmp";
	
	/* TODO: this kills the xattrs */
	FILE *fl = fopen(tmp_filename.c_str(), "wb");
	fwrite(str.c_str(),str.length(),1,fl);
	fclose(fl);
	
	/* atomic replace */
	rename(tmp_filename.c_str(), filename.c_str());
}

void CMainWindow::FetchAndExport()
{
	if(active_document=="") return;
	
	Gtk::FileChooserDialog d("Export current note", Gtk::FILE_CHOOSER_ACTION_SAVE);
	d.set_transient_for(*this);
	d.add_button("_Cancel", Gtk::RESPONSE_CANCEL);
	d.add_button("_Export", Gtk::RESPONSE_OK);
	
	auto filter_md = Gtk::FileFilter::create();
	filter_md->set_name("Portable markdown");
	filter_md->add_mime_type("text/plain");
	filter_md->add_mime_type("text/markdown");
	d.add_filter(filter_md);
	
	auto filter_pdf = Gtk::FileFilter::create();
	filter_pdf->set_name("Pandoc to PDF (requires pandoc)");
	filter_pdf->add_mime_type("application/pdf");
	d.add_filter(filter_pdf);
	
	switch(d.run()) {
	case Gtk::RESPONSE_OK: {
		gsize len;
		guint8 *buf;
		buf = sbuffer->serialize(sbuffer,"text/plain",sbuffer->begin(),sbuffer->end(),len);
		std::string str((const char*)buf,len);
		g_free(buf);
		
		if(d.get_filter() == filter_md) {
			FILE *fl = fopen(d.get_filename().c_str(), "wb");
			fwrite(str.c_str(),str.length(),1,fl);
			fclose(fl);
		} else if(d.get_filter() == filter_pdf) {
			char *tempmd = tempnam(NULL,"nkexport");
			FILE *fl = fopen(tempmd, "wb");
			fwrite(str.c_str(),str.length(),1,fl);
			fclose(fl);
			
			char cmdbuf[1024];
			snprintf(cmdbuf,1024,"pandoc -f markdown+hard_line_breaks+compact_definition_lists -t latex -o \"%s\" \"%s\"",d.get_filename().c_str(),tempmd);
			int retval;
			if(retval=system(cmdbuf)) {
				printf("Exporting to PDF (temporary file: %s): failure (%d). Temporary file not deleted.\n",tempmd,retval);
			} else {
				printf("Exporting to PDF (temporary file: %s): success.\n",tempmd);
				::remove(tempmd);
			}
		}
		
	break; }
	default:;
	}
}

void CMainWindow::OpenDocument(std::string filename)
{
	FetchAndSave();
	
	if(filename=="") {
		sview.set_editable(false);
		sview.set_can_focus(false);
		
		SetActiveFilename("");
		
		sbuffer->begin_not_undoable_action();
		sbuffer->set_text("( Nothing opened. Please create or open a file. ) ");
		sbuffer->end_not_undoable_action();
		return;
	}
	sview.set_editable(true);
	sview.set_can_focus(true);
	
	char *buf; gsize length;
	try {
		Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(nav_model.base + "/" + filename);
		file->load_contents(buf, length); 
	} catch(Gio::Error &e) {
		sview.set_editable(false);
		sview.set_can_focus(false);
		
		fprintf(stderr,"Error: Failed to load document %s!\n",filename.c_str());
		SetActiveFilename("");
		
		sbuffer->begin_not_undoable_action();
		char error_msg[512];
		sprintf(error_msg,"( Failed to open %s. Please create or open a file. ) ",filename.c_str());
		sbuffer->set_text(error_msg);
		sbuffer->end_not_undoable_action();
		
		return;
	}
	
	sbuffer->begin_not_undoable_action();
	sbuffer->set_text("");
	Gtk::TextBuffer::iterator i = sbuffer->begin();
	if(length) sbuffer->deserialize(sbuffer,"text/notekit-markdown",i,(guint8*)buf,length);
	//sbuffer->set_text(buf);
	sbuffer->end_not_undoable_action();
	
	SetActiveFilename(filename);
	
	/*Glib::RefPtr<Gtk::TextBuffer::ChildAnchor> anch = sbuffer->create_child_anchor(sbuffer->begin());
	testbutton.set_text("hello world");
	sview.add_child_at_anchor(testbutton,anch);
	testbutton.show();*/
	g_free(buf);
}

/* set apparent opened document filename without actually loading/unload anything */
void CMainWindow::SetActiveFilename(std::string filename)
{
	active_document = filename;
	selected_document = filename;
	if(use_hbar) {
		hbar.set_subtitle(active_document);
	} else {
		set_title(active_document + " - NoteKit");
	}
	config["active_document"]=filename;
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
	case WND_ACTION_NEXT_NOTE:
		nav_model.NextDoc();
		break;
	case WND_ACTION_PREV_NOTE:
		nav_model.PrevDoc();
		break;
	case WND_ACTION_TOGGLE_SIDEBAR:
		if(am.hide_sidebar.get_active()) {
			nav_scroll.set_visible(false);
		} else {
			nav_scroll.set_visible(true);
		}
		break;
	}
}

bool CMainWindow::on_idle()
{
	// autosave if document has been modified and nothing has been typed for 5 seconds
	if(sview.last_modified > 0 && g_get_real_time()-sview.last_modified > 5000000LL) {
		printf("autosave\n");
		FetchAndSave();
	}
	
	return true;
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
	
	bool device_switched=false;
	
	if(d != sview.last_device) {
		sview.last_device = d;
		device_switched=true;
		
		Gtk::RadioToolButton *b = nullptr;
		
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
		case NB_MODE_SELECT:
			toolbar_builder->get_widget("select",b);
			break;
		}
		b->set_active();
	}
	
	if(device_switched || sview.update_cursor) {
		sview.update_cursor=false;
		
		switch(sview.devicemodes[d]) {
		case NB_MODE_DRAW:
			sview.SetCursor(pen_cursor);
			sview.set_cursor_visible(false);
			break;
		case NB_MODE_TEXT:
			sview.SetCursor(text_cursor);
			sview.set_cursor_visible(true);
			break;
		case NB_MODE_ERASE:
			sview.SetCursor(eraser_cursor);
			sview.set_cursor_visible(false);
			break;
		case NB_MODE_SELECT:
			sview.SetCursor(selection_cursor);
			sview.set_cursor_visible(false);
			break;
		}
	}
	
	return false;
}
