#include "notebook.h"

#include "notebook_clipboard.hpp"

CNotebook::CNotebook()
{
	sbuffer = get_source_buffer();

	/* load syntax highlighting scheme */
	Glib::RefPtr<Gsv::StyleSchemeManager> styleman = Gsv::StyleSchemeManager::create();
	styleman->set_search_path({"sourceview/"});
	Glib::RefPtr<Gsv::StyleScheme> the_scheme;
	the_scheme = styleman->get_scheme("classic");
	sbuffer->set_style_scheme(the_scheme);
	
	Glib::RefPtr<Gsv::LanguageManager> langman = Gsv::LanguageManager::create();
	langman->set_search_path({"sourceview/"});
	sbuffer->set_language(langman->get_language("markdown"));
	
	/* register our serialisation formats */
	sbuffer->register_serialize_format("text/notekit-markdown",sigc::mem_fun(this,&CNotebook::on_serialize));
	sbuffer->register_deserialize_format("text/notekit-markdown",sigc::mem_fun(this,&CNotebook::on_deserialize));
	
	Glib::RefPtr<Gtk::TargetList> l = sbuffer->get_copy_target_list();
	
	/* create drawing overlay */
	add_child_in_window(overlay,Gtk::TEXT_WINDOW_WIDGET,0,0);
	
	/* define actions that the toolbar will hook up to */
	actions = Gio::SimpleActionGroup::create();
	#define ACTION(name,param1,param2) actions->add_action(name, sigc::bind( sigc::mem_fun(this,&CNotebook::on_action), std::string(name), param1, param2 ) )
	ACTION("cmode-text",NB_ACTION_CMODE,NB_MODE_TEXT);
	ACTION("cmode-draw",NB_ACTION_CMODE,NB_MODE_DRAW);
	ACTION("cmode-erase",NB_ACTION_CMODE,NB_MODE_ERASE);
	ACTION("stroke1",NB_ACTION_STROKE,1);
	ACTION("stroke2",NB_ACTION_STROKE,2);
	ACTION("stroke3",NB_ACTION_STROKE,3);
	
	/* connect signals */
	signal_size_allocate().connect(sigc::mem_fun(this,&CNotebook::on_allocate));
	overlay.signal_draw().connect(sigc::mem_fun(this,&CNotebook::on_redraw_overlay));
	signal_button_press_event().connect(sigc::mem_fun(this,&CNotebook::on_button_press),false);
	signal_button_release_event().connect(sigc::mem_fun(this,&CNotebook::on_button_release),false);
	signal_motion_notify_event().connect(sigc::mem_fun(this,&CNotebook::on_motion_notify),false);
	sbuffer->signal_insert().connect(sigc::mem_fun(this,&CNotebook::on_insert),true);
	sbuffer->signal_highlight_updated().connect(sigc::mem_fun(this,&CNotebook::on_highlight_updated),true);
	
	/* overwrite clipboard signals with our custom impls */
	GtkTextViewClass *k = GTK_TEXT_VIEW_GET_CLASS(gobj());
	k->copy_clipboard = notebook_copy_clipboard;
	k->cut_clipboard = notebook_cut_clipboard;
	k->paste_clipboard = notebook_paste_clipboard;
	//g_signal_connect(gobj(),"copy-clipboard",G_CALLBACK(notebook_copy_clipboard),NULL);
	
	active_state=NB_STATE_NOTHING;
	
	/* create tags for style aspects that the syntax highlighter doesn't handle */
	tag_extra_space = sbuffer->create_tag();
	tag_extra_space->property_pixels_below_lines().set_value(8);
	tag_extra_space->property_pixels_above_lines().set_value(8);
	
	set_wrap_mode(Gtk::WRAP_WORD_CHAR);}

void CNotebook::on_action(std::string name,int type,int param)
{
	switch(type) {
	case NB_ACTION_CMODE:
		devicemodes[last_device]=param;
		break;
	case NB_ACTION_STROKE:
		stroke_width=param;
		break;
	}
}

void CNotebook::on_allocate(Gtk::Allocation &a)
{
	//printf("alloc %d %d\n",a.get_width(),a.get_height());
	overlay.size_allocate(a);
	if(overlay.get_window()) {
		/* if we don't do this, some mystery subset of signals gets eaten */
		overlay.get_window()->set_pass_through(true);
	}
}

void CStroke::Render(const Cairo::RefPtr<Cairo::Context> &ctx, float basex, float basey)
{
	ctx->translate(-basex,-basey);
	ctx->set_source_rgba(r,g,b,a);
	ctx->set_line_cap(Cairo::LINE_CAP_ROUND);
	for(int i=1;i<xcoords.size();++i) {
		ctx->set_line_width(pcoords[i]);
		ctx->move_to(xcoords[i-1],ycoords[i-1]);
		ctx->line_to(xcoords[i],ycoords[i]);
		ctx->stroke();
	}
	ctx->translate(basex,basey);
	
}

#include <gdk/gdkkeysyms.h>

bool CNotebook::on_key_press_event(GdkEventKey *k)
{
	modifier_keys = k->state & (GDK_MODIFIER_MASK);
	return Gsv::View::on_key_press_event(k);
}

void CNotebook::on_insert(const Gtk::TextBuffer::iterator &iter,const Glib::ustring& str,int len)
{	
	if(str=="\n") {
		if(modifier_keys & GDK_SHIFT_MASK) return;
		if(!iter.ends_line()) return;
		
		/* extract previous line's first word and indentation preceding it */
		Gtk::TextBuffer::iterator start = iter; start.backward_line();
		Gtk::TextBuffer::iterator end = start; 
		if(end.get_char()==' ' || end.get_char()=='\t') end.forward_find_char([] (char c) { return (c!=' ' && c!='\t') || c=='\n'; });
		end.forward_find_char([] (char c) { return c==' ' || c=='\t' || c=='\n'; });
		end.forward_char();
		Glib::ustring str = sbuffer->get_text(start,end,true);
		end.forward_char();
		
		int num=0,pad=0,len=0;
		//printf("word: %s\n",str.c_str());
		
		/* get initial indentation */
		sscanf(str.c_str()," %n",&pad);
		str=str.substr(pad);
		
		//printf("word: %s\n",str.c_str());
		
		/* try to parse as enumeration or the like */
		sscanf(str.c_str(),"%d.%*1[ \t]%n",&num,&len);
		char buf[512];
		if(len==str.length() && num>0) {
			if(end >= iter) {
				sbuffer->erase(start,iter);
				return;
			}
			sprintf(buf,"%*s%d. ",pad,"",num+1);
			sbuffer->insert(iter,buf);
		} else if(str=="* ") {
			if(end >= iter) {
				sbuffer->erase(start,iter);
				return;
			}
			sprintf(buf,"%*s* ",pad,"");
			sbuffer->insert(iter,buf);
		} else if(str=="- ") {
			if(end >= iter) {
				sbuffer->erase(start,iter);
				return;
			}
			sprintf(buf,"%*s- ",pad,"");
			sbuffer->insert(iter,buf);
		} else if(str=="> ") {
			if(end == iter) {
				sbuffer->erase(start,iter);
				return;
			}
			sprintf(buf,"%*s> ",pad,"");
			sbuffer->insert(iter,buf);
		}
	}
	/* TODO: we really should fix up the iterator, but this version of gtkmm erroneously sets iter to const */
}

/* redraw cairo overlay: active stroke, special widgets like lines, etc. */
bool CNotebook::on_redraw_overlay(const Cairo::RefPtr<Cairo::Context> &ctx)
{
	int bx, by;
	window_to_buffer_coords(Gtk::TEXT_WINDOW_WIDGET,0,0,bx,by);
	
	active.Render(ctx,bx,by);
	
	/* draw horizontal rules */
	Gdk::Rectangle rect;
	get_visible_rect(rect);
	
	Gsv::Buffer::iterator i, end;
	get_iter_at_location(i,rect.get_x(),rect.get_y());
	get_iter_at_location(end,rect.get_x()+rect.get_width(),rect.get_y()+rect.get_height());
	
	do{
		if(sbuffer->iter_has_context_class(i, "hline")) {
			int y, height;
			get_line_yrange(i,y,height);
			int linex0,liney0;
			buffer_to_window_coords(Gtk::TEXT_WINDOW_WIDGET,0,y+height,linex0,liney0);
			
			ctx->set_line_width(1.0);
			ctx->set_source_rgba(.627,.659,.75,1);
			ctx->move_to(linex0+margin_x,liney0-height/2);
			ctx->line_to(linex0+rect.get_width()-margin_x,liney0-height/2);
			ctx->stroke();
		}
	}while(sbuffer->iter_forward_to_context_class_toggle(i, "hline") && i<end);
	
	return true;
}

void CNotebook::on_highlight_updated(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end)
{
	Gtk::TextBuffer::iterator i = start;
	do {
		Gtk::TextBuffer::iterator next = i;
		if(!sbuffer->iter_forward_to_context_class_toggle(next, "extra-space")) next=end;
		if(sbuffer->iter_has_context_class(i, "extra-space")) {
			sbuffer->apply_tag(tag_extra_space, i, next);
		} else {
			sbuffer->remove_tag(tag_extra_space, i, next);
		}
		i=next;
	} while(i<end);
}

void CNotebook::Widget2Doc(double in_x, double in_y, double &out_x, double &out_y)
{
	int bx, by;
	window_to_buffer_coords(Gtk::TEXT_WINDOW_WIDGET,0,0,bx,by);
	
	out_x = in_x+bx;
	out_y = in_y+by;
}

bool CNotebook::on_button_press(GdkEventButton *e)
{
	GdkDevice *d = gdk_event_get_source_device((GdkEvent*)e);
	if(!devicemodes.count(d)) {
		if(gdk_device_get_n_axes(d)>4) {
			// assume it's a pen
			devicemodes[d] = NB_MODE_DRAW;
		} else {
			devicemodes[d] = NB_MODE_TEXT;
		}
	}
	switch(devicemodes[d]) {
		case NB_MODE_TEXT:
			return false;
		case NB_MODE_DRAW: {
			double x,y;
			Widget2Doc(e->x,e->y,x,y);
		
			printf("sig: %f %f %f %f\n",e->x, e->y, x,y);
		
			active.Reset();
			//active.r=active.g=active.b=0; active.a=1;
			active.Append(x,y, stroke_width*ReadPressure(e->device));
			
			active_state = NB_STATE_DRAW;
		
			return true;
		}
		case NB_MODE_ERASE: {
			active_state = NB_STATE_ERASE;
			
			return true;
		}
		default: return false;
	}
	
}

bool CNotebook::on_button_release(GdkEventButton *e)
{
	if(active_state!=NB_STATE_NOTHING) {
		if(active_state==NB_STATE_DRAW) {
			sbuffer->begin_not_undoable_action();
			CommitStroke();
			sbuffer->end_not_undoable_action();
		}
		
		active_state=NB_STATE_NOTHING;
		
		return true;
	}
	return false;
}

bool CNotebook::on_motion_notify(GdkEventMotion *e)
{
	if(active_state==NB_STATE_DRAW) {
		double x,y;
		Widget2Doc(e->x,e->y,x,y); 
	
		active.Append(x,y,stroke_width*ReadPressure(e->device));
		
		overlay.queue_draw_area(e->x-32,e->y-32,64,64);
	} else if (active_state==NB_STATE_ERASE) {
		double x,y;
		Widget2Doc(e->x,e->y,x,y); 
		
		EraseAtPosition(x,y);
	}
	return false;
}

float CNotebook::ReadPressure(GdkDevice *d) 
{
    gdouble r;
    gdouble axes[16];
    GdkWindow *w=get_window(Gtk::TEXT_WINDOW_WIDGET)->gobj();
    gdk_device_get_state(d,w,axes,NULL);
    if(gdk_device_get_axis(d,axes,GDK_AXIS_PRESSURE,&r))
        return r;
    else return 1.0;
}

/* x, y are in buffer coordinates */
void CNotebook::EraseAtPosition(float x, float y)
{
	Gtk::TextBuffer::iterator i;
	int trailing;
	get_iter_at_position(i,trailing,x,y);
	
	if(i.get_child_anchor()) {
		auto w = i.get_child_anchor()->get_widgets();
		if(w.size()) {
			CBoundDrawing* d = dynamic_cast<CBoundDrawing*>(w[0]);
			/* found a region to erase from; we can adjust for its real location,
			 * but that's relative to the text body, so need to fixup */
			int bx, by;
			window_to_buffer_coords(Gtk::TEXT_WINDOW_TEXT,0,0,bx,by);
			d->EraseAt(x-(d->get_allocation().get_x())-bx,y-(d->get_allocation().get_y())-by,stroke_width,true);
		}
	}
}

void CNotebook::CommitStroke()
{
	float x0,y0,x1,y1;
	active.GetBBox(x0,x1,y0,y1);
	
	CBoundDrawing *d = NULL;
	
	//printf("%f %f %f %f\n",x0,x1,y0,y1);
	//add_child_in_window(*d,Gtk::TEXT_WINDOW_TEXT,x0,y0);
	
	/* search for preceding regions we can merge with */
	Gtk::TextBuffer::iterator i,k;
	get_iter_at_location(i,x0,y0);
	k=i; // for later
	do {
		if(i.get_child_anchor()) {
			auto w = i.get_child_anchor()->get_widgets();
			if(w.size()) {
				printf("match?\n");
				d = dynamic_cast<CBoundDrawing*>(w[0]);
				break;
			}
		}
		Gtk::TextBuffer::iterator j = i;
		--j;
		gunichar prev_char = j.get_char();
		if(!g_unichar_isspace(prev_char) && prev_char!=0xFFFC) {
			break;
		}
		i=j;
	} while(i!=sbuffer->begin());
	
	if(d!=NULL) {
		/* found a region to merge with above; we can adjust for its real location,
		 * but that's relative to the text body, so need to fixup */
		int bx, by;
		window_to_buffer_coords(Gtk::TEXT_WINDOW_TEXT,0,0,bx,by);
		//printf("fix: %d %d\n",bx,by);
		d->AddStroke(active,-(d->get_allocation().get_x())-bx,-(d->get_allocation().get_y())-by);
	} else {
		/* can't merge up, but maybe there's a region further down that can be expanded? */
		do {
			if(k.get_child_anchor()) {
				auto w = k.get_child_anchor()->get_widgets();
				if(w.size()) {
					printf("match below?\n");
					d = dynamic_cast<CBoundDrawing*>(w[0]);
					break;
				}
			}
			gunichar next_char = k.get_char();
			if(!g_unichar_isspace(next_char) && next_char!=0xFFFC) {
				break;
			}
			++k;
		} while(k!=sbuffer->end());
		
		if(d!=NULL) {
			/* found a region to merge with below; we need to expand it before adding this stroke */
			
			/* find where the drawing should start */
			get_iter_at_location(i,x0,y0);
			Gdk::Rectangle r;
			get_iter_location(i,r);
			
			printf("get rect: %d %d %d %d\n",r.get_x(),r.get_y(),r.get_width(),r.get_height());
			
			/* once again, need to correct allocation to be in buffer coords */
			int bx, by;
			window_to_buffer_coords(Gtk::TEXT_WINDOW_TEXT,0,0,bx,by);
			
			int dx = -d->get_allocation().get_x()-bx + r.get_x(),
				dy = -d->get_allocation().get_y()-by + r.get_y();
			
			d->UpdateSize(d->w, d->h, dx, dy);
			
			d->AddStroke(active,-(r.get_x()),-(r.get_y()));
			
			/* eat intermittent spaces */
			sbuffer->erase(i,k);
		} else {
			/* couldn't find a region to merge with; create a new image */
			d = new CBoundDrawing(get_window(Gtk::TEXT_WINDOW_TEXT));
			Gtk::manage(d); 
			get_iter_at_location(i,x0,y0);
			
			/* figure out where it got anchored in the text, so we can translate the stroke correctly */
			Gdk::Rectangle r;
			get_iter_location(i,r);
			
			if(r.get_x() > x0) {
				/* end of document, line too far to the right; add a new line */
				/* TODO: check this logic */
				sbuffer->insert(i,"\n");
				get_iter_at_location(i,x0,y0);
				get_iter_location(i,r);
			}
			
			auto anch = sbuffer->create_child_anchor(i);
			add_child_at_anchor(*d,anch);
			
			//printf("anchoring: %f %f -> %d %d\n",x0,y0,r.get_x(),r.get_y());
		
			d->AddStroke(active,-(r.get_x()),-(r.get_y()));
		}
	}
	
	d->show();
	
	floats.insert(d);
	
	active.Reset();
}

guint8* CNotebook::on_serialize(const Glib::RefPtr<Gtk::TextBuffer>& content_buffer, const Gtk::TextBuffer::iterator& start, const Gtk::TextBuffer::iterator& end, gsize& length)
{
	Glib::ustring buf;
	Gtk::TextBuffer::iterator pos0 = start, pos1 = start;
	while(pos0!=end) {
		while(!pos1.get_child_anchor() && pos1!=end) ++pos1;
		buf += pos0.get_text(pos1);
		pos0=pos1;
		if(pos0!=end && pos0.get_child_anchor()) {
			auto w = pos0.get_child_anchor()->get_widgets();
			if(w.size()) {
				CBoundDrawing *d = dynamic_cast<CBoundDrawing*>(w[0]);
				buf += d->Serialize();
			}
			++pos0; ++pos1;
		}
	}
	
	guint8* ret = (guint8*)g_malloc(buf.bytes());
	memcpy(ret,buf.data(),buf.bytes());
	length = buf.bytes();
	return ret;
}

bool CNotebook::on_deserialize(const Glib::RefPtr<Gtk::TextBuffer>& content_buffer, Gtk::TextBuffer::iterator& iter, const guint8* data, gsize length, bool create_tags)
{
	std::string str((const char*)data,length);
	
	int pos=0,pos0=0;
	bool drawing=false;
	while(pos!=str.length()) {
		if(!drawing) {
			pos=str.find("![](nk:",pos);
			if(pos==std::string::npos) pos=str.length();
			else {
				drawing=true;
			}
			iter=content_buffer->insert(iter,str.c_str()+pos0, str.c_str()+pos);
			pos0=pos;
		} else {
			pos+=7; pos0=pos;
			pos=str.find(')',pos);
			
			CBoundDrawing *d = new CBoundDrawing(get_window(Gtk::TEXT_WINDOW_TEXT));
			Gtk::manage(d);
			
			/* store iterator through mark creation */
			printf("before: %d\n",iter.get_offset());
			auto mk = content_buffer->create_mark(iter,false);
			auto anch = content_buffer->create_child_anchor(iter);
			iter = content_buffer->get_iter_at_mark(mk);
			printf("after: %d\n",iter.get_offset());
			content_buffer->delete_mark(mk);
			
			
			add_child_at_anchor(*d,anch);
			
			d->Deserialize(str.substr(pos0,pos-pos0));
			d->show();
			
			floats.insert(d);
			
			pos0=pos=pos+1;
			
			drawing=false;
		}
	}
	
	return true;
}

/* stroke */
void CStroke::Reset()
{
	xcoords.clear();
	ycoords.clear();
	pcoords.clear();
}

void CStroke::Append(float x, float y, float p)
{
	xcoords.push_back(x);
	ycoords.push_back(y);
	pcoords.push_back(p);
}

void CStroke::GetBBox(float &x0, float &x1, float &y0, float &y1)
{
	x0=xcoords[0]; x1=xcoords[0]; y0=ycoords[0]; y1=ycoords[0];
	for(int i=0;i<xcoords.size();++i) {
		if(xcoords[i]-pcoords[i]<x0) x0 = xcoords[i]-pcoords[i];
		if(xcoords[i]+pcoords[i]>x1) x1 = xcoords[i]+pcoords[i];
		if(ycoords[i]-pcoords[i]<y0) y0 = ycoords[i]-pcoords[i];
		if(ycoords[i]+pcoords[i]>y1) y1 = ycoords[i]+pcoords[i];
	}
}

/* **** bound drawings (i.e. those that are part of a document) **** */

/* store the Gdk::Window so we can create accelerated surfaces for whatever app is rendering on */
CBoundDrawing::CBoundDrawing(Glib::RefPtr<Gdk::Window> wnd) : Glib::ObjectBase("CBoundDrawing"), Gtk::DrawingArea()
{
	target_window=wnd;
	w=h=1;
	selected=false;
	
	//set_size_request(100,100);
	
	signal_draw().connect(sigc::mem_fun(this,&CBoundDrawing::on_draw));
	signal_configure_event().connect([] (GdkEventConfigure* e) {  printf("confevent %d %d\n",e->width,e->height); return true; } );
}


/* Change the drawing's size, possibly resizing the internal buffer in the process */
/* dx, dy move the top left corner; neww, newh are relative to the OLD top left corner */
void CBoundDrawing::UpdateSize(int neww, int newh, int dx, int dy)
{
	neww-=dx; newh-=dy;
	
	if(dx!=0 || dy!=0) {
		// origin changed; need to move strokes
		for(auto &str : strokes) {
			for(int i=0;i<str.xcoords.size();++i) {
				str.xcoords[i]-=dx;
				str.ycoords[i]-=dy;
			}
		}
	}
	if(w!=neww || h!=newh) {
		// size changed; need to recreate Cairo surface
		Cairo::RefPtr<Cairo::Surface> newptr = target_window->create_similar_surface(Cairo::CONTENT_COLOR_ALPHA,neww,newh);
		image_ctx = Cairo::Context::create(newptr);
		// copy old surface
		if(image) {
			image_ctx->save();
			image_ctx->set_source(image,-dx,-dy);
			image_ctx->rectangle(-dx,-dy,w,h);
			image_ctx->set_operator(Cairo::OPERATOR_SOURCE);
			image_ctx->fill();
			image_ctx->restore();
		}
		// replace surface
		image = newptr;
	}
	
	w=neww; h=newh;
	set_size_request(w,h);
}

/* iterate over all the strokes and determine the minimum size to fit them */
void CBoundDrawing::RecalculateSize()
{
	int neww=1, newh=1;
	for(auto &str : strokes) {
		for(int i=0;i<str.xcoords.size();++i) {
			if(str.xcoords[i]+str.pcoords[i]>neww) neww=str.xcoords[i]+str.pcoords[i];
			if(str.ycoords[i]+str.pcoords[i]>newh) newh=str.ycoords[i]+str.pcoords[i];
		}
	}
	
	UpdateSize(neww, newh);
}

/* push and draw a new stroke, shifting it by (dx,dy) to accommodate the local coordinate system */
void CBoundDrawing::AddStroke(CStroke &s, float dx, float dy)
{
	int neww=w, newh=h;
	strokes.push_back(s);
	for(int i=0;i<s.xcoords.size();++i) {
		int newx, newy, newp;
		newx=(strokes.back().xcoords[i]+=dx);
		newy=(strokes.back().ycoords[i]+=dy);
		newp=strokes.back().pcoords[i];
		if(newx+newp>neww) neww=newx+newp;
		if(newy+newp>newh) newh=newy+newp;
		/* add to stroke cache */
		strokefinder.insert( { BUCKET(newx,newy), { strokes.size()-1, i } } );
	}
	
	UpdateSize(neww, newh);
	
	strokes.back().Render(image_ctx,0,0);
}

void CBoundDrawing::EraseAt(float x, float y, float radius, bool whole_stroke)
{	
	std::set<int> buckets;
	buckets.insert(BUCKET(x,y));
	buckets.insert(BUCKET(x-radius,y-radius));
	buckets.insert(BUCKET(x+radius,y-radius));
	buckets.insert(BUCKET(x-radius,y+radius));
	buckets.insert(BUCKET(x+radius,y+radius));
	
	bool did_erase = false;

	using T = std::unordered_multimap<int, strokeRef>::iterator;
	for(int b : buckets) {
	restart:
		std::pair<T,T> is = strokefinder.equal_range(b); // pair of iterators
		
		for(T i=is.first; i!=is.second; ++i) {
			float dx = strokes[i->second.index].xcoords[i->second.offset] - x;
			float dy = strokes[i->second.index].ycoords[i->second.offset] - y;
			
			if(dx*dx + dy*dy < radius*radius) {
				did_erase=true;
				int to_del = i->second.index;
				strokes.erase(strokes.begin()+to_del);
				/* remove stroke from strokefinder, move up remaining strokes */
				for(T i=strokefinder.begin(),j;i!=strokefinder.end();i=j) {
					j=i; ++j;
					if(i->second.index==to_del) strokefinder.erase(i);
					else if(i->second.index>to_del) {
						strokefinder.insert( { i->first,{ i->second.index-1, i->second.offset } } );
						strokefinder.erase(i);
					}
				}
				goto restart;
			}
		}
	}
	
	if(did_erase) {
		/*printf("Remaining: %d\n",strokefinder.size());
		for(T i=strokefinder.begin();i!=strokefinder.end();++i) {
			printf("  %d: %d %d\n",i->first,i->second.index,i->second.offset);
		}*/
		Redraw();
		queue_draw();
	}}

bool CBoundDrawing::on_draw(const Cairo::RefPtr<Cairo::Context> &ctx)
{
	/* for(auto &str : strokes) {
		str.Render(ctx,0,0);
		//printf("%f %f\n",strokes[0].xcoords[0],strokes[0].ycoords[0]);
	}*/
	/* blit our internal buffer */
	ctx->set_source(image,0,0);
	ctx->rectangle(0,0,w,h);
	ctx->fill();
	
	return true;
}

void CBoundDrawing::Redraw()
{
	/* clear buffer */
	image_ctx->save();
	image_ctx->set_source_rgba(0,0,0,0);
	image_ctx->rectangle(0,0,w,h);
	image_ctx->set_operator(Cairo::OPERATOR_SOURCE);
	image_ctx->fill();
	image_ctx->restore();
	
	/* redraw all strokes */
	for(auto &str : strokes) {
		str.Render(image_ctx,0,0);
		//printf("%f %f\n",strokes[0].xcoords[0],strokes[0].ycoords[0]);
	}
}

bool CBoundDrawing::on_button_press_event(GdkEventButton* event)
{
	return false;
}

bool CBoundDrawing::on_button_release_event(GdkEventButton* event)
{
	return false;
}

bool CBoundDrawing::on_motion_notify_event(GdkEventMotion* event)
{
	
	return false;
}

#include "base64.h"
#include <zlib.h>

std::string CBoundDrawing::Serialize()
{
	std::string ret;
	ret+="![](nk:";
	
	std::vector<unsigned int> raw_data;
	raw_data.push_back(w);
	raw_data.push_back(h);
	raw_data.push_back(strokes.size());
	for(auto &s : strokes) {
		raw_data.push_back(*(unsigned int*)&s.r);
		raw_data.push_back(*(unsigned int*)&s.g);
		raw_data.push_back(*(unsigned int*)&s.b);
		raw_data.push_back(*(unsigned int*)&s.a);
		raw_data.push_back(s.xcoords.size());
		int pos = raw_data.size(), len=s.xcoords.size();
		raw_data.resize( raw_data.size()+3*len );
		memcpy(&raw_data[pos], &s.xcoords[0], 4*len );
		memcpy(&raw_data[pos+len], &s.ycoords[0], 4*len );
		memcpy(&raw_data[pos+2*len], &s.pcoords[0], 4*len );
	}
	
	std::vector<unsigned char> compressed;
	compressed.resize(4*raw_data.size());
	uLongf len = 4*raw_data.size();
	
	compress2(&compressed[0],&len,(unsigned char*)&raw_data[0],4*raw_data.size(),9);
	compressed.resize(len);
	
	char buf[20];
	sprintf(buf,"%d,",4*raw_data.size());
	
	ret+=buf;
	ret+=base64Encode(compressed);
	
	ret+=")";
	return ret;
}

void CBoundDrawing::Deserialize(std::string input)
{
	int len,num_chars;
	sscanf(input.c_str(),"%d,%n",&len,&num_chars);
	
	input.erase(0,num_chars);
	
	std::vector<unsigned char> compressed = base64Decode(input);
	
	std::vector<unsigned int> raw_data;
	raw_data.resize(len/4);
	
	uLongf long_len = len;
	uncompress((unsigned char*)&raw_data[0],&long_len,&compressed[0],compressed.size());
	
	int neww = raw_data[0];
	int newh = raw_data[1];
	int pos=3;
	for(int nstrokes = 0; nstrokes<raw_data[2]; ++nstrokes) {
		strokes.push_back(CStroke());
		strokes.back().r = *(float*)&raw_data[pos  ];
		strokes.back().g = *(float*)&raw_data[pos+1];
		strokes.back().b = *(float*)&raw_data[pos+2];
		strokes.back().a = *(float*)&raw_data[pos+3];
		int ncoords = raw_data[pos+4];
		strokes.back().xcoords.resize(ncoords); memcpy(&strokes.back().xcoords[0], &raw_data[pos+5], 4*ncoords);
		strokes.back().ycoords.resize(ncoords); memcpy(&strokes.back().ycoords[0], &raw_data[pos+5+ncoords], 4*ncoords);
		strokes.back().pcoords.resize(ncoords); memcpy(&strokes.back().pcoords[0], &raw_data[pos+5+2*ncoords], 4*ncoords);
		pos += 5+3*ncoords;
	}
	
	UpdateSize(neww, newh);
	Redraw();
}

void CBoundDrawing::on_unrealize()
{
	printf("unrealize event on %08X\n", this);
	//get_parent()->remove(*this);
}

void CBoundDrawing::destroy_notify_()
{
	printf("destroy event on %08X\n", this);
	strokes.~vector<CStroke>();
}
