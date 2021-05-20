#ifndef DRAWING_H
#define DRAWING_H

#include "config.h"

#include <gtkmm.h>

#include <set>
#include <vector>
#include <unordered_map>

class CStroke {
public:
	double r,g,b,a;
	std::vector<float> xcoords;
	std::vector<float> ycoords;
	std::vector<float> pcoords;
	
	std::vector<bool> selected;
	
	void Reset();
	void Append(float x, float y, float p);
	void GetHead(float &x, float &y);
	float GetHeadCurvatureWrt(float x, float y);
	void Simplify();
	void Render(const Cairo::RefPtr<Cairo::Context> &ctx, float basex, float basey, int start_index=1);
	void GetBBox(float &x0, float &x1, float &y0, float &y1, int start_index=0);
	
	void RenderSelectionGlow(const Cairo::RefPtr<Cairo::Context> &ctx, float basex, float basey);
	bool Select(float x0, float x1, float y0, float y1);
	void Unselect();
};

class CBoundDrawing : public Gtk::DrawingArea
{
public:
	CBoundDrawing(Glib::RefPtr<Gdk::Window> wnd);
	
	static CBoundDrawing *TryUpcast(Gtk::Widget *w);
	
	std::vector<CStroke> strokes;
	
	/* accelerator structure for quickly finding strokes e.g. when erasing */
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
	
	void Select(float x0, float x1, float y0, float y1);
	void Unselect();
	
	virtual bool on_button_press_event(GdkEventButton* event);
	virtual bool on_button_release_event(GdkEventButton* event);
	virtual bool on_motion_notify_event(GdkEventMotion* event);
	virtual bool on_draw(const Cairo::RefPtr<Cairo::Context> &ctx);
	
	virtual void on_unrealize();
	virtual void destroy_notify_();
	
	std::string Serialize();
	std::string SerializePNG();
	std::string SerializeSVG();
	void Deserialize(std::string input);
	void DumpForDebugging();
};

#endif
