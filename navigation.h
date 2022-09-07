#ifndef NAVIGATION_H
#define NAVIGATION_H

#include "config.h"

#include <gtkmm.h>

enum {
	CT_FILE,
	CT_DIR_UNLOADED,
	CT_DIR_LOADED,
	CT_ADDER
};

class CMainWindow;
class CNavigationView;

class CNavigationTreeStore : public Gtk::TreeStore {
public:
	class Columns : public Gtk::TreeModel::ColumnRecord {
	public:
		Columns();
		
		Gtk::TreeModelColumn<Glib::ustring> name;
		Gtk::TreeModelColumn<Glib::ustring> full_path;
		Gtk::TreeModelColumn<Glib::ustring> ext;
		Gtk::TreeModelColumn<Glib::ustring> ord;
		Gtk::TreeModelColumn<int> type;
		Gtk::TreeModelColumn<int> expanded;
	};
protected: 
	CNavigationTreeStore(const Columns &, CNavigationView*);
	
	const Columns *cols; // annoyingly, we need to keep a pointer or else do heavy refactoring
	CNavigationView *nview; // likewise here

	virtual bool row_draggable_vfunc(const Gtk::TreeModel::Path& path) const override;
	virtual bool row_drop_possible_vfunc(const Gtk::TreeModel::Path& dest, const Gtk::SelectionData& selection_data) const override;
	virtual bool drag_data_received_vfunc (const Gtk::TreeModel::Path& dest, const Gtk::SelectionData& selection_data) override;
public:
	static Glib::RefPtr<CNavigationTreeStore> create(const Columns &cols, CNavigationView*);
};

class CNavigationView {
public:
	CNavigationTreeStore::Columns cols;

	CNavigationView();

	std::string base;
	void SetBasePath(std::string newbase);
	
	CMainWindow *mainwindow;
	Gtk::TreeView *v;
	/*Gtk::CellRendererText name_renderer;
	Gtk::CellRendererPixbuf icon_renderer;
	Gtk::TreeViewColumn name_col;*/
	
	Glib::RefPtr<CNavigationTreeStore> store;
	
	std::string Row2Path(Gtk::TreeModel::iterator row);

	void AttachView(CMainWindow *w, Gtk::TreeView *v);
	void ExpandDirectory(std::string path, const Gtk::TreeNodeChildren *node);
	void ExpandAndSelect(Glib::ustring path);
	
	void NextDoc();
	void PrevDoc();
	
	void HandleRename(std::string oldname, std::string newname);
	void FixPaths(std::string path, const Gtk::TreeNodeChildren *node);
	void MaybeDeleteSelected();
	bool TryMove(const Gtk::TreeModel::Path &src, const Gtk::TreeModel::Path &dst);
	
	void CreateAdder(Gtk::TreeModel::iterator row);
	
	void on_row_edit_start(Gtk::CellEditable* cell_editable, const Glib::ustring& path);
	void on_row_edit_commit(const Glib::ustring& path_string, const Glib::ustring& new_text);
	void on_row_activated(const Gtk::TreeModel::Path &path, const Gtk::TreeViewColumn *col);
	bool on_expand_row(const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path);
	void on_postexpand_row(const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path);
	void on_collapse_row(const Gtk::TreeModel::iterator& iter, const Gtk::TreeModel::Path& path);
	
	void on_render_cell(Gtk::CellRenderer *cr, const Gtk::TreeModel::iterator &iter);
	
	Gtk::Menu popup;
	void on_button_press_event(GdkEventButton* event);
	
	sigc::connection conns[7];
};

#endif