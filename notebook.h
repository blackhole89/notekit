#ifndef NOTEBOOK_H
#define NOTEBOOK_H

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
	NB_MODE_ERASE
};

enum {
	NB_STATE_NOTHING=0,
	NB_STATE_DRAW,
	NB_STATE_ERASE
};

class CNotebook : public Gsv::View
{
public:
	CNotebook();
	
	Glib::RefPtr<Gio::SimpleActionGroup> actions;
	
	int margin_x;
	
	/* mouse input related data */
	CStroke active; // current stroke
	int active_state; // whether we are in the process of drawing in some mode
	std::set<CBoundDrawing* > floats;
	float stroke_width; // width of current stroke
	void CommitStroke();
	void EraseAtPosition(float x, float y);
	
	GdkDevice *last_device; // most recently seen device
	bool update_cursor; // signal to main window to update cursor
	std::map<GdkDevice*,int> devicemodes; // current input modes, per device
	
	void SetCursor(Glib::RefPtr<Gdk::Cursor> c);
	
	Glib::RefPtr<Gsv::Buffer> sbuffer;
	
	Gtk::DrawingArea overlay;
	Cairo::RefPtr<Cairo::Surface> overlay_image;
	Cairo::RefPtr<Cairo::Context> overlay_ctx;
	void on_allocate(Gtk::Allocation &a);
	bool on_redraw_overlay(const Cairo::RefPtr<Cairo::Context> &ctx);
	
	float attention_ewma;
    bool on_button_press(GdkEventButton* event);
    bool on_button_release(GdkEventButton* event);
    bool on_motion_notify(GdkEventMotion* event);
	
	void on_action(std::string name, int type, int param);
	
	Glib::RefPtr<Gtk::TextTag> tag_extra_space;
	void on_highlight_updated(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end);
	
	int modifier_keys; // modifier keys active during most recent keypress
	virtual bool on_key_press_event(GdkEventKey *k);
	void on_insert(const Gtk::TextBuffer::iterator &,const Glib::ustring& str,int len);
	
	guint8* on_serialize(const Glib::RefPtr<Gtk::TextBuffer>& content_buffer, const Gtk::TextBuffer::iterator& start, const Gtk::TextBuffer::iterator& end, gsize& length, bool render_images);
	bool on_deserialize(const Glib::RefPtr<Gtk::TextBuffer>& content_buffer, Gtk::TextBuffer::iterator& iter, const guint8* data, gsize length, bool create_tags); 
	
	
	float ReadPressure(GdkDevice *d);
	void Widget2Doc(double in_x, double in_y, double &out_x, double &out_y);
};

#endif