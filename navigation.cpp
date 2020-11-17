#include "navigation.h"
#include "mainwindow.h"

#include <giomm/file.h>

//extern CMainWindow *mainwindow;

CNavigationTreeStore::Columns::Columns() : Gtk::TreeModel::ColumnRecord()
{
	add(name);
	add(full_path);
	add(ext);
	add(ord);
	add(type);
	add(expanded);
}

CNavigationTreeStore::CNavigationTreeStore(const Columns &cols, CNavigationView* nv) : Gtk::TreeStore(cols)
{
	this->cols = &cols;
	this->nview = nv;
}

Glib::RefPtr<CNavigationTreeStore> CNavigationTreeStore::create(const Columns &cols, CNavigationView* nview)
{
	return Glib::RefPtr<CNavigationTreeStore>( new CNavigationTreeStore(cols, nview) );
}

bool CNavigationTreeStore::row_draggable_vfunc(const Gtk::TreeModel::Path& path) const
{
	// const_cast due to gtkmm lack of get_const_iter. okay since we never write
	const_iterator i = const_cast<CNavigationTreeStore*>(this)->get_iter(path);
	if(i) {
		// the adder can't be dragged
		if((*i)[cols->type]!=CT_ADDER) return true;
	}
	return false;
}

bool CNavigationTreeStore::row_drop_possible_vfunc(const Gtk::TreeModel::Path& dest, const Gtk::SelectionData& selection_data) const
{
	// need to make sure we are not dragging below an adder node
	Gtk::TreeModel::Path pred = dest;
	if(pred.prev()) {
		const_iterator i = const_cast<CNavigationTreeStore*>(this)->get_iter(pred);
		if(i && (*i)[cols->type]==CT_ADDER) {
			return false;
		}
	}
	// need to make sure dest is a child of a folder, and not a file
	Gtk::TreeModel::Path parent = dest;
	if(!parent.up() || parent.empty()) {
		// top level
		return true;
	}
	const_iterator i = const_cast<CNavigationTreeStore*>(this)->get_iter(parent);
	if(i) {
		if((*i)[cols->type]==CT_DIR_LOADED || (*i)[cols->type]==CT_DIR_UNLOADED)
			return true;
	}
	return false;
}

bool CNavigationTreeStore::drag_data_received_vfunc (const Gtk::TreeModel::Path& dest, const Gtk::SelectionData& selection_data)
{	
	Glib::RefPtr<Gtk::TreeModel> refThis = Glib::RefPtr<CNavigationTreeStore>(this); // wtf, gtkmm
	refThis->reference(); // I suppose this prevents us from deallocating ourselves
	
	Gtk::TreeModel::Path src;
	Gtk::TreeModel::Path::get_from_selection_data(selection_data,refThis,src);
	
	printf("drag data received: %s\n",src.to_string().c_str());
	
	if(nview->TryMove(src,dest)) {
		return false; //Gtk::TreeStore::drag_data_received_vfunc(dest,selection_data);
	} else {
		return false;
	}
}

CNavigationView::CNavigationView()
{
	store=CNavigationTreeStore::create(cols, this);
	
	/* set up sorting */
	store->set_sort_func(cols.name,[this] (const Gtk::TreeModel::iterator& a, const Gtk::TreeModel::iterator& b) {
		if((*b)[cols.type]==CT_ADDER) return -1;
		else if((*a)[cols.type]==CT_ADDER) return 1;
		else {
			Glib::ustring namea, nameb;
			if(((Glib::ustring)(*a)[cols.ord]).size()) namea = (*a)[cols.ord]; else namea= (*a)[cols.name];
			if(((Glib::ustring)(*b)[cols.ord]).size()) nameb = (*b)[cols.ord]; else nameb= (*b)[cols.name];
			return namea<nameb?-1:1;
		}
	});
	store->set_sort_column(cols.name,Gtk::SORT_ASCENDING);
}

void CNavigationView::SetBasePath(std::string newbase)
{
	base=newbase;
}

void CNavigationView::on_row_edit_start(Gtk::CellEditable* cell_editable, const Glib::ustring& path)
{
	auto pEntry = dynamic_cast<Gtk::Entry*>(cell_editable);
	if(pEntry) {
		if(pEntry->get_text() == "+")
			pEntry->set_text("");
	}
}

void CNavigationView::on_row_edit_commit(const Glib::ustring& path_string, const Glib::ustring& new_text)
{
	Gtk::TreePath path(path_string);
	std::string cmd;
	
	if(auto row = store->get_iter(path)) {
		switch( (*row)[cols.type] ) {
		case CT_FILE:
		case CT_DIR_LOADED:
		case CT_DIR_UNLOADED:
			cmd = "mv \"";
			cmd += base;
			cmd += (Glib::ustring)(*row)[cols.full_path];
			cmd += (Glib::ustring)(*row)[cols.name];
			cmd += (Glib::ustring)(*row)[cols.ext];
			cmd += "\" \"";
			cmd += base;
			cmd += (Glib::ustring)(*row)[cols.full_path];
			cmd += new_text;
			cmd += (Glib::ustring)(*row)[cols.ext];
			cmd += "\"";
			if(!system(cmd.c_str())) {
				std::string oldname = Row2Path(row);
				(*row)[cols.name] = new_text;
				std::string newname = Row2Path(row);
				
				if( (*row)[cols.type] == CT_DIR_LOADED || (*row)[cols.type] == CT_DIR_UNLOADED ) {
					FixPaths( (Glib::ustring)(*row)[cols.full_path] + (Glib::ustring)(*row)[cols.name], &row->children() );
				} else {
					HandleRename( oldname, newname );
				}
			}
			break;
		case CT_ADDER:
			if(new_text[new_text.length()-1]=='/') {
				cmd = "mkdir \"";
				cmd += base;
				cmd += (Glib::ustring)(*row)[cols.full_path];
				cmd += new_text;
				cmd += "\"";
				
				if(!system(cmd.c_str())) {
					auto newrow = store->insert(row);
					
					(*newrow)[cols.name] = new_text.substr(0,new_text.size()-1);
					(*newrow)[cols.full_path] = (Glib::ustring)(*row)[cols.full_path];
					(*newrow)[cols.ext] = "";
					(*newrow)[cols.type] = CT_DIR_LOADED;
					
					CreateAdder(newrow);
					
					v->expand_row(store->get_path(newrow),false);
					v->set_cursor(store->get_path(newrow));
				}
			} else {
				cmd = "touch \"";
				cmd += base;
				cmd += (Glib::ustring)(*row)[cols.full_path];
				cmd += new_text;
				cmd += ".md\"";
				
				if(!system(cmd.c_str())) {
					auto newrow = store->insert(row);
					
					(*newrow)[cols.name] = new_text;
					(*newrow)[cols.full_path] = (Glib::ustring)(*row)[cols.full_path];
					(*newrow)[cols.ext] = ".md";
					(*newrow)[cols.type] = CT_FILE;
					
					v->set_cursor(store->get_path(newrow));
					mainwindow->OpenDocument(Row2Path(newrow));
				}
			}
			break;
		}
	}
}

std::string CNavigationView::Row2Path(Gtk::TreeModel::iterator row)
{
	std::string fname;
	fname += (Glib::ustring)(*row)[cols.full_path];
	fname += (Glib::ustring)(*row)[cols.name];
	fname += (Glib::ustring)(*row)[cols.ext];
	return fname;
}

void CNavigationView::on_row_activated(const Gtk::TreeModel::Path &path, const Gtk::TreeViewColumn *col)
{
	printf("activate row\n");
	
	auto row = store->get_iter(path);
	
	if((*row)[cols.type]!=CT_FILE) return;
	
	std::string fname = Row2Path(row);
	
	mainwindow->OpenDocument(fname);
}

void CNavigationView::NextDoc()
{
	Gtk::TreeModel::Path p,pold; Gtk::TreeViewColumn* c;
	v->get_cursor(p,c);
	pold=p;
	if(v->row_expanded(p)) p.down();
	else do {
		p=pold;
		p.next();
		pold.up(); // todo: check logic (what if pold is already top level?)
	} while(!store->get_iter(p) && pold);
	
	if(store->get_iter(p)) {
		v->set_cursor(p);
		v->row_activated(p,*c);
	}
}

void CNavigationView::PrevDoc()
{
	Gtk::TreeModel::Path p,pold; Gtk::TreeViewColumn* c;
	v->get_cursor(p,c);
	pold=p;
	p.prev();
	if(p==pold) {
		// couldn't move to prev, so try moving up
		if(p.size()>1) p.up();
	} else if(store->get_iter(p)) {
		// moved one prev; might have been expanded, so move
		// to the bottom of subtree we may have leapfrogged
		while(v->row_expanded(p)) {
			p.down();
			while(store->get_iter(p)) p.next();
			p.prev();
		}
	}
	
	if(store->get_iter(p)) {
		v->set_cursor(p);
		v->row_activated(p,*c);
	}
}

#ifdef XATTRS
#include <sys/xattr.h>
#endif

bool CNavigationView::on_expand_row(const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path)
{
	if( (*iter)[cols.type] == CT_DIR_UNLOADED )
		ExpandDirectory( (Glib::ustring)(*iter)[cols.full_path] + (Glib::ustring)(*iter)[cols.name], &iter->children() );
	(*iter)[cols.type] = CT_DIR_LOADED;
#ifdef XATTRS	
	int attr='1';
	setxattr((base+Row2Path(iter)).c_str(),"user.nkexpand",&attr,1,0);
#else
	Glib::RefPtr<Gio::File> f = Gio::File::create_for_path(base+Row2Path(iter));
	f->set_attribute_string("xattr::nkexpand","1",Gio::FILE_QUERY_INFO_NONE);
#endif
	(*iter)[cols.expanded]=1;

	return false;
}

void CNavigationView::on_postexpand_row(const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path)
{
	for(auto r : iter->children()) {
		if( r[cols.expanded] ) {
			v->expand_row(store->get_path(r),false);
		}
		Glib::ustring name = r[cols.full_path]+r[cols.name]+r[cols.ext];
		if( name==mainwindow->selected_document ) {
			v->get_selection()->select( r );
		}
	}
}

void CNavigationView::on_collapse_row(const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path)
{
	/* not unloading the row for now */
#ifdef XATTRS
	int attr='0';
	setxattr((base+Row2Path(iter)).c_str(),"user.nkexpand",&attr,0,0);
#else
	Glib::RefPtr<Gio::File> f = Gio::File::create_for_path(base+Row2Path(iter));
	f->set_attribute_string("xattr::nkexpand","",Gio::FILE_QUERY_INFO_NONE);
#endif
	(*iter)[cols.expanded]=0;
}

void CNavigationView::on_button_press_event(GdkEventButton* event)
{
	if( (event->type == GDK_BUTTON_PRESS) && (event->button == 3) )
	{
		popup.popup_at_pointer((GdkEvent*)event);
	}
}


void CNavigationView::AttachView(CMainWindow *w, Gtk::TreeView *view)
{
	mainwindow=w;
	v=view;	
	
	v->get_selection()->set_mode(Gtk::SELECTION_BROWSE);
	/*v->get_selection()->set_select_function([this] (const Glib::RefPtr<Gtk::TreeModel>& m, const Gtk::TreeModel::Path& p, bool) {
		int type = (*m->get_iter(p))[cols.type];
		if(type==CT_DIR_LOADED || type==CT_DIR_UNLOADED) return false;
		return true;
	}); */ // this stops us from editing the directory name too...
	v->set_size_request(200,-1);
	v->set_headers_visible(false);
	v->set_enable_tree_lines(true);
	v->set_model(store);
	v->set_activate_on_single_click(true);
	
	v->enable_model_drag_source();
	v->enable_model_drag_dest();
	
	v->set_reorderable();
	
	//conns[0] = store->signal_row_changed().connect(sigc::mem_fun(*this,&CNavigationView::on_row_edit));
	
	/* set up the sole column of the treeview */
	v->append_column("Name",cols.name);
	Gtk::CellRendererText *cr = (Gtk::CellRendererText*)v->get_column_cell_renderer(0);
	v->get_column(0)->set_cell_data_func(*cr, sigc::mem_fun(this,&CNavigationView::on_render_cell));

	/* connect signals */
	conns[0] = cr->signal_editing_started().connect(sigc::mem_fun(this,&CNavigationView::on_row_edit_start));
	conns[1] = cr->signal_edited().connect(sigc::mem_fun(this,&CNavigationView::on_row_edit_commit));
	
	conns[2] = v->signal_row_activated().connect(sigc::mem_fun(this,&CNavigationView::on_row_activated));
	conns[3] = v->signal_test_expand_row().connect(sigc::mem_fun(this,&CNavigationView::on_expand_row));
	conns[4] = v->signal_row_expanded().connect(sigc::mem_fun(this,&CNavigationView::on_postexpand_row));
	conns[5] = v->signal_row_collapsed().connect(sigc::mem_fun(this,&CNavigationView::on_collapse_row));
	
	conns[6] = v->signal_button_press_event().connect_notify(sigc::mem_fun(this,&CNavigationView::on_button_press_event),false);
	
	/* expand root directory */
	ExpandDirectory("",NULL);
	for(auto r : store->children()) {
		if( r[cols.expanded] ) {
			v->expand_row(store->get_path(r),false);
		}
		Glib::ustring name = r[cols.full_path]+r[cols.name]+r[cols.ext];
		if( name==mainwindow->selected_document ) {
			v->get_selection()->select( r );
		}
	}
	
	Gtk::TreeModel::iterator adder;
	adder = store->append();
	(*adder)[cols.name]="+";
	(*adder)[cols.full_path]="/";
	(*adder)[cols.type]=CT_ADDER;
	
	/* create popup menu */
	auto item = new Gtk::MenuItem("_Delete", true);
	Gtk::manage(item);
	item->signal_activate().connect( sigc::mem_fun(this,&CNavigationView::MaybeDeleteSelected) );
	popup.append(*item);
	popup.accelerate(*v);
	popup.show_all();
}

/* Instruct the CellRenderer to render the row pointed at by iter. */
void CNavigationView::on_render_cell(Gtk::CellRenderer *cr_, const Gtk::TreeModel::iterator &iter)
{
	Gtk::CellRendererText *cr = (Gtk::CellRendererText*)cr_;
	cr->property_text()= (*iter)[cols.name];
	/* todo: figure out good UX for editing other cells without clashing with drag-drop */
	cr->property_editable() = true; // (*iter)[cols.type]==CT_ADDER;
}

void CNavigationView::MaybeDeleteSelected()
{
	Gtk::TreeIter i = v->get_selection()->get_selected();
	switch( (*i)[cols.type] ) {
		case CT_FILE: {
			Gtk::MessageDialog dialog(*mainwindow, "Delete note "+(*i)[cols.name]+"?",
				false /* use_markup */, Gtk::MESSAGE_QUESTION,
				Gtk::BUTTONS_OK_CANCEL);
				dialog.set_secondary_text("This action can not be undone.");

			int result = dialog.run();

			//Handle the response:
			if(result==Gtk::RESPONSE_OK) {
				mainwindow->OpenDocument("");
				
				std::string cmd;
				cmd = "rm \"";
				cmd += base;
				cmd += (Glib::ustring)(*i)[cols.full_path];
				cmd += (Glib::ustring)(*i)[cols.name];
				cmd += (Glib::ustring)(*i)[cols.ext];
				cmd += "\"";

				if(!system(cmd.c_str())) {
					store->erase(i);
				}
			}
			break;
		}
		case CT_DIR_LOADED:
		case CT_DIR_UNLOADED: {
			Gtk::MessageDialog dialog(*mainwindow, "Delete FOLDER "+(*i)[cols.name]+"?",
				false /* use_markup */, Gtk::MESSAGE_WARNING,
				Gtk::BUTTONS_OK_CANCEL);
				dialog.set_secondary_text("This action can not be undone, and many notes may be lost.");

			int result = dialog.run();

			//Handle the response:
			if(result==Gtk::RESPONSE_OK) {
				mainwindow->OpenDocument("");
				
				std::string cmd;
				cmd = "rm -r \"";
				cmd += base;
				cmd += (Glib::ustring)(*i)[cols.full_path];
				cmd += (Glib::ustring)(*i)[cols.name];
				cmd += (Glib::ustring)(*i)[cols.ext];
				cmd += "\"";

				if(!system(cmd.c_str())) {
					store->erase(i);
				}
			}
			break;
		}
	}
}

/* Try moving a node of the tree (file or dir) in response to a drag-drop */
bool CNavigationView::TryMove(const Gtk::TreeModel::Path &src, const Gtk::TreeModel::Path &dst)
{
	/* identify source of drag and create a file for it */
	Gtk::TreeModel::iterator srci = store->get_iter(src);
	if(!srci) return false;

	std::string srcn = Row2Path(srci);
	auto srcf = Gio::File::create_for_path(srcn);
	
	/* calculate destination */
	std::string dstn,dstpathn;
	Gtk::TreeModel::Path parent = dst;
	Gtk::TreeModel::const_iterator pari;
	if(!parent.up() || parent.empty()) {
		dstn="/";
	} else {
		pari = store->get_iter(parent);
		if(!pari) return false;
		
		dstn=Row2Path(pari)+"/";
	}
	dstpathn=dstn; // path above destination location
	dstn += (Glib::ustring)(*srci)[cols.name];
	dstn += (Glib::ustring)(*srci)[cols.ext];
	
	printf("Move: %s -> %s\n",srcn.c_str(),dstn.c_str());
	
	try{
		auto srcf = Gio::File::create_for_path(base+srcn);
		auto dstf = Gio::File::create_for_path(base+dstn);
		
		srcf->move(dstf,Gio::FILE_COPY_ALL_METADATA);
		
		/* if we are here, the move succeeded; update the full_path records of the file/dir and everything inside it */
		
		(*srci)[cols.full_path] = dstpathn;
		HandleRename(srcn,dstn);
		
		/* finally, create a new entry in the tree store and get rid of the old one */
		Gtk::TreeModel::iterator dsti;
		Gtk::TreeModel::Path pred = dst;
		if(!pred.prev()) {
			/* inserting at the beginning of a folder, prepend */
			if(dstpathn=="/") {
				dsti = store->prepend();
			} else {
				/* pari is valid */
				dsti = store->prepend(pari->children());
			}
		} else {
			Gtk::TreeModel::iterator predi;
			predi = store->get_iter(pred);
			if(!predi) {
				printf("Failed to get insertion point for %s! Navigation view may now be inconsistent.\n",pred.to_string().c_str());
				return false;
			}
			dsti = store->insert_after(predi);
		}
		
		(*dsti)[cols.name] = (Glib::ustring)(*srci)[cols.name];
		(*dsti)[cols.full_path] = (Glib::ustring)(*srci)[cols.full_path];
		(*dsti)[cols.ext] = (Glib::ustring)(*srci)[cols.ext];
		(*dsti)[cols.ord] = (Glib::ustring)(*srci)[cols.ord]; // todo?
		(*dsti)[cols.expanded]= (int)(*srci)[cols.expanded];
		(*dsti)[cols.type]= (int)(*srci)[cols.type];

		if( (*dsti)[cols.type] == CT_DIR_LOADED || (*dsti)[cols.type] == CT_DIR_UNLOADED ) {
			/* we moved a directory, so the full_paths of its children may need to be updated recursively */
			FixPaths(dstn, &srci->children() );
			
			/* erase it so we can set the selection correctly when expanding */
			store->erase(srci);
			
			(*dsti)[cols.type] = CT_DIR_UNLOADED;
			
			CreateAdder(dsti);
			
			if( (*dsti)[cols.expanded] ) {
				v->expand_row(store->get_path(dsti),false); // better safe than sorry, though expect get_path(dsti)==dst
			}
		} else if( (*srci)[cols.type] == CT_FILE ) {
			/* erase here so we can set the selection correctly */
			store->erase(srci);
			
			(*dsti)[cols.type] = CT_FILE;
			
			v->get_selection()->select( dsti );
		}
	} catch(Gio::Error &e) {
		printf("Failed to move: %s\n",e.what().c_str());
		return false;
	}
	
	return true;
}

void CNavigationView::HandleRename(std::string oldname, std::string newname)
{
	//printf("handlerename %s->%s: %s\n",oldname.c_str(),newname.c_str(),mainwindow->active_document.c_str() );
	if(mainwindow->active_document == oldname)
		mainwindow->SetActiveFilename(newname);
}

/* Recursively update the full_path entries for all of a tree node's children to react to a directory having been renamed.
 * This is ugly and brittle, and we probably should move away from having the full_path field at all, in favour
 * of dynamically computing it from a Gtk::TreeModel::Path when required. */
void CNavigationView::FixPaths(std::string path, const Gtk::TreeNodeChildren *node)
{
	for(auto &a: *node) { 
		std::string oldname = Row2Path(a);
		
		a[cols.full_path] = path+"/";
		
		HandleRename( oldname, Row2Path(a) ); // change active filename in main window if necessary
		if(a[cols.type] == CT_DIR_LOADED || a[cols.type] == CT_DIR_UNLOADED) {
			FixPaths(path+"/"+a[cols.name], &a.children());
		}
	}
}

/* Create the "add note/directory" row under the directory pointed at by r. */
void CNavigationView::CreateAdder(Gtk::TreeModel::iterator r)
{
	Gtk::TreeModel::iterator adder;
	adder = store->append(r->children());
	(*adder)[cols.name]="+";
	(*adder)[cols.full_path]=(*r)[cols.full_path]+(*r)[cols.name]+"/";
	(*adder)[cols.type]=CT_ADDER;
}

/* Load the contents of a previously unexpanded directory node */
void CNavigationView::ExpandDirectory(std::string path, const Gtk::TreeNodeChildren *node)
{
	try {
		Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(base+path);
		Glib::RefPtr<Gio::FileEnumerator> child_enumeration = file->enumerate_children("standard::name,standard::type,xattr::nkorder,xattr::nkexpand",Gio::FILE_QUERY_INFO_NONE);
		
		Glib::RefPtr<Gio::FileInfo> file_info;
		std::vector<Glib::RefPtr<Gio::FileInfo> > files;
		while ((file_info = child_enumeration->next_file())) files.push_back(file_info);
		/*std::sort(files.begin(), files.end(), [](Glib::RefPtr<Gio::FileInfo> &a, Glib::RefPtr<Gio::FileInfo> &b) {
			return (a->get_name() > b->get_name()); } ); */
		
		for( auto file_info : files )
		{
			std::string fname = file_info->get_name(); 
			Glib::ustring order = file_info->get_attribute_as_string("xattr::nkorder");

			bool is_dir = file_info->get_file_type()==Gio::FILE_TYPE_DIRECTORY;
			
			Gtk::TreeModel::iterator r;
			
			if(is_dir) { 
				if(node) r=store->prepend(*node);
				else r=store->prepend();
			
				(*r)[cols.name] = fname;
				(*r)[cols.full_path] = path+"/";
				(*r)[cols.ext] = "";
				(*r)[cols.ord] = order;
				(*r)[cols.type] = CT_DIR_UNLOADED;
				
				CreateAdder(r);
				
				/* WARN: this needs the view to already be attached */
				if(file_info->get_attribute_string("xattr::nkexpand").size()) {
					(*r)[cols.expanded] = 1;
				} else {
					(*r)[cols.expanded] = 0;
				}
			} else {
				size_t pos;
				if((pos = fname.find(".md"))!=std::string::npos) {
					if(node) r=store->prepend(*node);
					else r=store->prepend();
					
					(*r)[cols.name] = fname.substr(0,pos);
					(*r)[cols.full_path] = path+"/";
					(*r)[cols.ext] = ".md";
					(*r)[cols.ord] = order;
					(*r)[cols.type] = CT_FILE;
				}
			}
		}
	} catch(Gio::Error &e) {
		printf("Gio error when expanding directory: %s\n",e.what().c_str());
	}
}
