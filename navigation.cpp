#include "navigation.h"
#include "mainwindow.h"

#include <giomm/file.h>

CNavigationView::Columns::Columns() : Gtk::TreeModel::ColumnRecord()
{
	add(name);
	add(full_path);
	add(ext);
	add(ord);
	add(type);
}

CNavigationView::CNavigationView(std::string bp)
{
	store=Gtk::TreeStore::create(cols);
	
	base=bp;
	
	ExpandDirectory("",NULL);
	
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

void CNavigationView::on_row_edit_start(Gtk::CellEditable* cell_editable, const Glib::ustring& path)
{
	auto pEntry = dynamic_cast<Gtk::Entry*>(cell_editable);
	if(pEntry) {
		if(pEntry->get_text() == "+")
			pEntry->set_text("");
	}
}

extern CMainWindow *mainwindow;

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
					
					(*newrow)[cols.name] = new_text;
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

bool CNavigationView::on_expand_row(const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path)
{
	if( (*iter)[cols.type] == CT_DIR_UNLOADED )
		ExpandDirectory( (Glib::ustring)(*iter)[cols.full_path] + (Glib::ustring)(*iter)[cols.name], &iter->children() );
	(*iter)[cols.type] = CT_DIR_LOADED;
	return false;
}

void CNavigationView::AttachView(Gtk::TreeView *view)
{
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
	
	//conns[0] = store->signal_row_changed().connect(sigc::mem_fun(*this,&CNavigationView::on_row_edit));
	
	v->append_column("Name",cols.name);
	
	/* set up editing */
	Gtk::CellRendererText *cr = (Gtk::CellRendererText*)v->get_column_cell_renderer(0);
	cr->property_editable() = true;
	conns[0] = cr->signal_editing_started().connect(sigc::mem_fun(this,&CNavigationView::on_row_edit_start));
	conns[1] = cr->signal_edited().connect(sigc::mem_fun(this,&CNavigationView::on_row_edit_commit));
	
	conns[2] = v->signal_row_activated().connect(sigc::mem_fun(this,&CNavigationView::on_row_activated));
	conns[3] = v->signal_test_expand_row().connect(sigc::mem_fun(this,&CNavigationView::on_expand_row));
	
	Gtk::TreeModel::iterator adder;
	adder = store->append();
	(*adder)[cols.name]="+";
	(*adder)[cols.full_path]="";
	(*adder)[cols.type]=CT_ADDER;
}

void CNavigationView::HandleRename(std::string oldname, std::string newname)
{
	//printf("handlerename %s->%s: %s\n",oldname.c_str(),newname.c_str(),mainwindow->active_document.c_str() );
	if(mainwindow->active_document == oldname)
		mainwindow->active_document = newname;
}

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

void CNavigationView::CreateAdder(Gtk::TreeModel::iterator r)
{
	Gtk::TreeModel::iterator adder;
	adder = store->append(r->children());
	(*adder)[cols.name]="+";
	(*adder)[cols.full_path]=(*r)[cols.full_path]+(*r)[cols.name]+"/";
	(*adder)[cols.type]=CT_ADDER;
}

void CNavigationView::ExpandDirectory(std::string path, const Gtk::TreeNodeChildren *node)
{
	try {
		Glib::RefPtr<Gio::File> file = Gio::File::create_for_path(base+path);
		Glib::RefPtr<Gio::FileEnumerator> child_enumeration = file->enumerate_children("standard::name,nkorder");
		
		Glib::RefPtr<Gio::FileInfo> file_info;
		std::vector<Glib::RefPtr<Gio::FileInfo> > files;
		while ((file_info = child_enumeration->next_file())) files.push_back(file_info);
		/*std::sort(files.begin(), files.end(), [](Glib::RefPtr<Gio::FileInfo> &a, Glib::RefPtr<Gio::FileInfo> &b) {
			return (a->get_name() > b->get_name()); } ); */
		
		for( auto file_info : files )
		{
			std::string fname = file_info->get_name();
			Glib::ustring order = file_info->get_attribute_as_string("nkorder");

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
		printf("%s\n",e.what().c_str());
	}
}
