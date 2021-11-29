#ifndef NOTEBOOK_H
#define NOTEBOOK_H

#include "config.h"

#include <set>

#include <gtkmm.h>
#include <gtksourceviewmm.h>

#include "drawing.h"

enum {
	NB_ACTION_CMODE,
	NB_ACTION_STROKE,
	NB_ACTION_COLOR
};

enum {
	NB_MODE_TEXT,
	NB_MODE_DRAW,
	NB_MODE_ERASE,
	NB_MODE_SELECT
};

enum {
	NB_STATE_NOTHING=0,
	NB_STATE_DRAW,
	NB_STATE_ERASE,
	NB_STATE_SELECT
};

class CNotebook : public Gsv::View
{
public:
	CNotebook();
	
	void Init(std::string data_path, bool use_highlight_proxy);
	std::string GetHighlightProxyDir();
	
	Glib::RefPtr<Gio::SimpleActionGroup> actions;
	
	int margin_x;
	
	/* mouse input related data */
	CStroke active; // current stroke
	int active_state; // whether we are in the process of drawing in some mode
	std::set<CBoundDrawing* > floats;
	float stroke_width; // width of current stroke
	void CommitStroke();
	void EraseAtPosition(float x, float y);
	
	double sel_x0, sel_y0, sel_x1, sel_y1;
	void SelectBox(float x0, float x1, float y0, float y1);
	
	GdkDevice *last_device; // most recently seen device
	bool update_cursor; // signal to main window to update cursor
	std::map<GdkDevice*,int> devicemodes; // current input modes, per device
	
	Glib::RefPtr<Gdk::Cursor> pointer_cursor; // default pointer cursor
	
	void SetCursor(Glib::RefPtr<Gdk::Cursor> c);
	
	Glib::RefPtr<Gsv::Buffer> sbuffer;
	
	/* menu-related events */
	void on_action(std::string name, int type, int param);
	
	/* drawing-related events */
	Gtk::DrawingArea overlay;
	Cairo::RefPtr<Cairo::Surface> overlay_image;
	Cairo::RefPtr<Cairo::Context> overlay_ctx;
	void on_allocate(Gtk::Allocation &a);
	bool on_redraw_overlay(const Cairo::RefPtr<Cairo::Context> &ctx);
	
	float attention_ewma;
    bool on_button_press(GdkEventButton* event);
    bool on_button_release(GdkEventButton* event);
    bool on_motion_notify(GdkEventMotion* event);
	
	float ReadPressure(GdkEvent *e);
	void Widget2Doc(double in_x, double in_y, double &out_x, double &out_y);
	
	/* text-related events and functionality */
	Glib::RefPtr<Gtk::TextTag> tag_proximity;
	Glib::RefPtr<Gtk::TextMark> last_position;
	void on_move_cursor();
	
	void on_highlight_updated(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end);
	void on_leave_region(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end);
	void on_enter_region(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end);
	
	int modifier_keys; // modifier keys active during most recent keypress
	int latest_keyval; // most recent keypress
	gint64 last_modified; // time of last modification or 0 if no modification since last save
	virtual bool on_key_press_event(GdkEventKey *k);
	void on_insert(const Gtk::TextBuffer::iterator &,const Glib::ustring& str,int len);
	void on_changed();
	virtual bool on_event(GdkEvent *ev);
	
	// additional Gtk::TextTags for metadata and formatting unsupported by Gsv language spec
	void DebugTags(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end);
	Glib::RefPtr<Gtk::TextTag> tag_extra_space, tag_blockquote, tag_invisible, tag_hidden, tag_mono, tag_is_anchor;
	
	std::map<int, Glib::RefPtr<Gtk::TextTag> > baseline_tags;
	Glib::RefPtr<Gtk::TextTag> GetBaselineTag(int baseline);
	
	/* embedded widget handling */
	
	std::string document_path; // base path for relative URLs
	void SetDocumentPath(std::string);
	
	std::string DepositImage(GdkPixbuf *pixbuf);
	
	// list of Gsv language spec context classes that can be rendered to widgets
	static std::unordered_map<std::string, std::function<Gtk::Widget *(CNotebook*, Gtk::TextBuffer::iterator, Gtk::TextBuffer::iterator)> > widget_classes;
	
	void QueueChildAnchor(Glib::RefPtr<Gtk::TextMark> mstart);
	void RenderToWidget(Glib::ustring wtype, Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end);
	void UnrenderWidgets(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end);
	void CleanUpSpan(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end);
	
	/* export and import for copypaste/save */
	guint8* on_serialize(const Glib::RefPtr<Gtk::TextBuffer>& content_buffer, const Gtk::TextBuffer::iterator& start, const Gtk::TextBuffer::iterator& end, gsize& length, bool render_images);
	bool on_deserialize(const Glib::RefPtr<Gtk::TextBuffer>& content_buffer, Gtk::TextBuffer::iterator& iter, const guint8* data, gsize length, bool create_tags); 
	
	/* iterator stack for preserving iterators across buffer changes */
	std::stack<Glib::RefPtr<Gtk::TextMark> > iter_stack;
	Glib::RefPtr<Gtk::TextMark> PushIter(Gtk::TextIter i);
	Gtk::TextIter PopIter();
};

#endif
