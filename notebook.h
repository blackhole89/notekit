#ifndef NOTEBOOK_H
#define NOTEBOOK_H

#include <set>
#include <unordered_map>

#include <gtkmm.h>
#include <gtksourceviewmm.h>

class CStroke {
public:
	float r,g,b,a;
	std::vector<float> xcoords;
	std::vector<float> ycoords;
	std::vector<float> pcoords;
	
	void Reset();
	void Append(float x, float y, float p);
	void Render(const Cairo::RefPtr<Cairo::Context> &ctx, float basex, float basey);
	void GetBBox(float &x0, float &x1, float &y0, float &y1);
};

class CBoundDrawing : public Gtk::DrawingArea
{
public:
	CBoundDrawing(Glib::RefPtr<Gdk::Window> wnd);
	
	std::vector<CStroke> strokes;
	
	static const int BUCKET_SIZE = 16;
	static constexpr int BUCKET(int x,int y) {
		return ((y/BUCKET_SIZE)<<16) + (x/BUCKET_SIZE);
	}
	struct strokeRef { int index; int offset; };
	std::unordered_multimap<int, strokeRef> strokefinder;
	
	int w,h;
	
	bool selected;
	
	Glib::RefPtr<Gdk::Window> target_window;
	Cairo::RefPtr<Cairo::Surface> image;
	Cairo::RefPtr<Cairo::Context> image_ctx;
	void UpdateSize(int w, int h, int dx=0, int dy=0);
	void Redraw();
	void RebuildStrokefinder();
	
	void RecalculateSize();
	void AddStroke(CStroke &s, float dx, float dy);
	void EraseAt(float x, float y, float radius, bool whole_stroke);
	
	virtual bool on_button_press_event(GdkEventButton* event);
	virtual bool on_button_release_event(GdkEventButton* event);
	virtual bool on_motion_notify_event(GdkEventMotion* event);
	virtual bool on_draw(const Cairo::RefPtr<Cairo::Context> &ctx);
	
	virtual void on_unrealize();
	virtual void destroy_notify_();
	
	std::string Serialize();
	std::string SerializePNG();
	void Deserialize(std::string input);
};

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
	std::map<GdkDevice*,int> devicemodes; // current input modes, per device
	
	Glib::RefPtr<Gsv::Buffer> sbuffer;
	
	Gtk::DrawingArea overlay;
	void on_allocate(Gtk::Allocation &a);
	bool on_redraw_overlay(const Cairo::RefPtr<Cairo::Context> &ctx);
	
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