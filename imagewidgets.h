#ifndef IMAGEWIDGETS_H
#define IMAGEWIDGETS_H

#include "config.h"

#include <gtkmm.h>

class CImageWidget : public Gtk::DrawingArea
{
public:
	CImageWidget(Glib::RefPtr<Gdk::Window> wnd);
	virtual ~CImageWidget();
	
	int w,h;
	
	bool selected;
	
	Glib::RefPtr<Gdk::Window> target_window;
	Cairo::RefPtr<Cairo::Surface> image;
	Cairo::RefPtr<Cairo::Context> image_ctx;
	void SetSize(int x, int y);
	
	virtual void Redraw();
	
	virtual int GetBaseline();

	virtual bool on_draw(const Cairo::RefPtr<Cairo::Context> &ctx);
	
	virtual void on_unrealize();
	virtual void destroy_notify_();
};

#if defined (HAVE_LASEM) || defined (HAVE_CLATEXMATH)

class CLatexWidget : public CImageWidget
{
public:
	CLatexWidget(Glib::RefPtr<Gdk::Window> wnd, Glib::ustring source, Gdk::RGBA fg);
	virtual ~CLatexWidget();
	
	Glib::ustring source;
	
	virtual void Redraw();
	
	int baseline;
	virtual int GetBaseline();
};

#endif

#endif