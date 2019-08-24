#ifndef NOTEBOOK_H
#define NOTEBOOK_H

#include <set>

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
	CBoundDrawing();
	
	std::vector<CStroke> strokes;
	
	int w,h;
	
	bool selected;
	
	void UpdateSize();
	void AddStroke(CStroke &s, float dx, float dy);
	
	virtual bool on_button_press_event(GdkEventButton* event);
	virtual bool on_button_release_event(GdkEventButton* event);
	virtual bool on_motion_notify_event(GdkEventMotion* event);
	virtual bool on_draw(const Cairo::RefPtr<Cairo::Context> &ctx);
	
	virtual void on_unrealize();
	virtual void destroy_notify_();
	
	std::string Serialize();
	void Deserialize(std::string input);
};

class CNotebook : public Gsv::View
{
public:
	CNotebook();
	
	CStroke active;
	bool is_drawing;
	std::set<CBoundDrawing* > floats;
	void CommitStroke();
	
	Glib::RefPtr<Gsv::Buffer> sbuffer;
	
	Gtk::DrawingArea overlay;
	void on_allocate(Gtk::Allocation &a);
	bool on_redraw_overlay(const Cairo::RefPtr<Cairo::Context> &ctx);
	
    bool on_button_press(GdkEventButton* event);
    bool on_button_release(GdkEventButton* event);
    bool on_motion_notify(GdkEventMotion* event);
	
	int modifier_keys; // modifier keys active during most recent keypress
	virtual bool on_key_press_event(GdkEventKey *k);
	void on_insert(const Gtk::TextBuffer::iterator &,const Glib::ustring& str,int len);
	
	guint8* on_serialize(const Glib::RefPtr<Gtk::TextBuffer>& content_buffer, const Gtk::TextBuffer::iterator& start, const Gtk::TextBuffer::iterator& end, gsize& length);
	bool on_deserialize(const Glib::RefPtr<Gtk::TextBuffer>& content_buffer, Gtk::TextBuffer::iterator& iter, const guint8* data, gsize length, bool create_tags); 
	
	
	float ReadPressure(GdkDevice *d);
	void Widget2Doc(double in_x, double in_y, double &out_x, double &out_y);
};

#endif