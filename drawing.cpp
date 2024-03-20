#include "drawing.h"

/* **** bound drawings (i.e. those that are part of a document) **** */

/* store the Gdk::Window so we can create accelerated surfaces for whatever app is rendering on */
CBoundDrawing::CBoundDrawing(Glib::RefPtr<Gdk::Window> wnd, Glib::RefPtr<Gtk::StyleContext> style_context) : Glib::ObjectBase("CBoundDrawing"), Gtk::DrawingArea()
{
	target_window=wnd;
	style_ctx = style_context;

	w=h=1;
	selected=false;
	
	//set_size_request(100,100);
	
	signal_draw().connect(sigc::mem_fun(this,&CBoundDrawing::on_draw));
	//signal_configure_event().connect([] (GdkEventConfigure* e) {  printf("confevent %d %d\n",e->width,e->height); return true; } );
}

CBoundDrawing *CBoundDrawing::TryUpcast(Gtk::Widget *w)
{
	if(!strcmp(G_OBJECT_TYPE_NAME(w->gobj()),"gtkmm__CustomObject_CBoundDrawing")) {
		return static_cast<CBoundDrawing*>(w);
	} else return NULL;
}


/* Change the drawing's size, possibly resizing the internal buffer in the process */
/* dx, dy move the top left corner; neww, newh are relative to the OLD top left corner */
/* returns false if the resizing would result in some strokes falling off the edge */
bool CBoundDrawing::UpdateSize(int neww, int newh, int dx, int dy)
{
	neww-=dx; newh-=dy;
	
	if(dx!=0 || dy!=0) {
		// check that the new size is positive and that no strokes fell off the left
		if(neww<0 || newh<0)
			return false;
		for(auto &str : strokes) {
			for(unsigned int i=0;i<str.xcoords.size();++i) {
				if(str.xcoords[i]<dx || str.ycoords[i]<dy)
					return false;
			}
		}
		// origin changed; need to move strokes
		for(auto &str : strokes) {
			for(unsigned int i=0;i<str.xcoords.size();++i) {
				str.xcoords[i]-=dx;
				str.ycoords[i]-=dy;
			}
		}
		RebuildStrokefinder(); // buckets changed
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
	
	return true;
}

/* iterate over all the strokes and determine the minimum size to fit them */
void CBoundDrawing::RecalculateSize()
{
	int neww=1, newh=1;
	for(auto &str : strokes) {
		for(unsigned int i=0;i<str.xcoords.size();++i) {
			if(str.xcoords[i]+str.pcoords[i]>neww) neww=str.xcoords[i]+str.pcoords[i];
			if(str.ycoords[i]+str.pcoords[i]>newh) newh=str.ycoords[i]+str.pcoords[i];
		}
	}
	neww+=1; newh+=1; // margin to account for rounding error
	
	UpdateSize(neww, newh);
}

void CBoundDrawing::RebuildStrokefinder()
{
	strokefinder.clear();
	
	for(unsigned int j=0;j<strokes.size();++j) {
		for(unsigned int i=0;i<strokes[j].xcoords.size();++i) {
			strokefinder.insert( { BUCKET(strokes[j].xcoords[i],strokes[j].ycoords[i]), { (int) j, (int) i } } );
		}
	}
}

/* push and draw a new stroke, shifting it by (dx,dy) to accommodate the local coordinate system */
/* if force is false, then return false if some of the new stroke would wind up to the left of the left boundary */
bool CBoundDrawing::AddStroke(CStroke &s, float dx, float dy, bool force)
{
	// validate that incoming stroke is entirely in positive coordinate space, either by rejecting it or by changing offending entries
	if(!force) for(unsigned int i=0;i<s.xcoords.size();++i) {
		if(s.xcoords[i]<-dx || s.ycoords[i]<-dy)
			return false;
	} else for(unsigned int i=0;i<s.xcoords.size();++i) {
		if(s.xcoords[i]<-dx) s.xcoords[i]=-dx;
		if(s.ycoords[i]<-dy) s.ycoords[i]=-dy;
	}
	
	int neww=w, newh=h;
	strokes.push_back(s);
	for(unsigned int i=0;i<s.xcoords.size();++i) {
		int newx, newy, newp;
		newx=(strokes.back().xcoords[i]+=dx);
		newy=(strokes.back().ycoords[i]+=dy);
		newp=strokes.back().pcoords[i]+1; // add 1 as safety margin
		if(newx+newp>neww) neww=newx+newp;
		if(newy+newp>newh) newh=newy+newp;
		/* add to stroke cache */
		strokefinder.insert( { BUCKET(newx,newy), { (int)strokes.size()-1, (int)i } } );
	}
	
	UpdateSize(neww, newh);
	
	strokes.back().Render(image_ctx,0,0,style_ctx->get_color());
	
	return true;
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
	}
}

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

	Gdk::RGBA fg = style_ctx->get_color();
	
	/* draw a halo around any selected strokes */
	if(selected) {
		for(auto &str : strokes) {
			str.RenderSelectionGlow(image_ctx,0,0,fg);
		}
		image_ctx->save();
		image_ctx->set_source_rgba(0,0,0,0.3);
		image_ctx->rectangle(0,0,w,h);
		image_ctx->set_operator(Cairo::OPERATOR_DEST_IN);
		image_ctx->fill();
		image_ctx->restore();
	}
	
	/* redraw all strokes */
	for(auto &str : strokes) {
		str.Render(image_ctx,0,0,fg);
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

/* (de)serializing drawings */
#include "base64.h"
#include <zlib.h>

cairo_status_t append_fn(void *closure, const unsigned char* data, unsigned int length)
{
	std::vector<unsigned char> *target = (std::vector<unsigned char>*)closure;
	target->insert(target->end(),data,data+length);
	return CAIRO_STATUS_SUCCESS;
}

std::string CBoundDrawing::SerializePNG()
{
	std::string ret;
	ret+="![](data:image/png;base64,";
	std::vector<unsigned char> buf; 
	cairo_surface_write_to_png_stream(image->cobj(), append_fn, (void*)&buf);
	//image->write_to_png_stream(sigc::bind(&append_fn, &buf));
	ret+=base64Encode(buf);
	ret+=")";
	return ret;
}

std::string CBoundDrawing::SerializeSVG()
{
	std::string ret;
	ret+="![](data:image/svg+xml;base64,";
	std::vector<unsigned char> buf; 
	
	Cairo::RefPtr<Cairo::SvgSurface> svg = Cairo::SvgSurface::create(append_fn,(void*)&buf,w,h);
	Cairo::RefPtr<Cairo::Context> svg_ctx = Cairo::Context::create(svg);
	
	for(auto &str : strokes) {
		str.Render(svg_ctx,0,0, style_ctx->get_color());
	}
	
	svg_ctx.clear();
	svg.clear();
	
	/*std::vector<unsigned char> compressed;
	compressed.resize(buf.size());
	uLongf len = buf.size();
	
	compress2(&compressed[0],&len,(unsigned char*)&buf[0],buf.size(),9);
	compressed.resize(len);*/
	
	ret+=base64Encode(buf);
	ret+=")";
	return ret;
}


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
	sprintf(buf,"%d,",(int) (4*raw_data.size()) );
	
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
	for(unsigned int nstrokes = 0; nstrokes<raw_data[2]; ++nstrokes) {
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
	
	RebuildStrokefinder();
	UpdateSize(neww, newh);
	Redraw();
}

void CBoundDrawing::DumpForDebugging()
{
	printf("Bound Drawing at %p:\n",this);
	printf("  %d*%d\n",this->w,this->h);
	for(int i=0;i<strokes.size();++i) {
		printf("  Stroke %d: RGBA=%.2f %.2f %.2f %.2f\n",i,strokes[i].r,strokes[i].g,strokes[i].b,strokes[i].a);
		for(int j=0;j<strokes[i].xcoords.size();++j) {
			printf("    %.2f %.2f p=%.2f\n", strokes[i].xcoords[j],strokes[i].ycoords[j],strokes[i].pcoords[j]);
		}
	}
}

/* input coordinates are in the drawing's frame of reference */
void CBoundDrawing::Select(float x0, float x1, float y0, float y1)
{
	printf("drawing %p: select %.2f,%.2f -- %.2f,%.2f\n",this,x0,y0,x1,y1);
	if(x1<0 || y1<0 || x0>w || y0>h) {
		Unselect();
		return;
	}
	
	bool will_select=false;
	
	for(auto &s : strokes)
		if(s.Select(x0,x1,y0,y1)) will_select=true;
		
	if(selected || will_select) {
		selected=will_select;
		Redraw();
		queue_draw();
	}
	
	selected=will_select;
}

void CBoundDrawing::Unselect() {
	for(auto &s : strokes) 
		s.Unselect();
		
	if(selected) {
		selected=false;
		Redraw();
		queue_draw();
	}
}

void CBoundDrawing::on_unrealize()
{
	printf("unrealize event on %p\n", (void*) this);
	//get_parent()->remove(*this);
}

void CBoundDrawing::destroy_notify_()
{
	printf("destroy event on %p\n", (void*) this);
	strokes.~vector<CStroke>();
}

/* **** single strokes inside a drawing **** */

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

void CStroke::GetHead(float &x, float &y)
{
	if(!xcoords.size()) return;
	x = xcoords.back();
	y = ycoords.back();
}

/* Determine at what angle a new control points would turn from the head of the stroke.
 * Used to make snap decisions about requesting more input events when the user's pen 
 * accelerates a lot. */
float CStroke::GetHeadCurvatureWrt(float x, float y)
{
	if(xcoords.size()<2) return 0.0f;
	
	int n=xcoords.size()-1;
	float dx0=xcoords[n]-xcoords[n-1], dy0=ycoords[n]-ycoords[n-1], len0=sqrt(dx0*dx0+dy0*dy0);
	float dx1=x-xcoords[n], dy1=y-ycoords[n], len1=sqrt(dx1*dx1+dy1*dy1);
	float cross=dx1*dy0-dy1*dx0;
	return cross/len0/len1;
}

/* Simplify stroke by removing control points that are too close together or approximately on a straight line segment. */
void CStroke::Simplify()
{
	if(xcoords.size()<3) return;
	std::vector<float> nxcoords;
	std::vector<float> nycoords;
	std::vector<float> npcoords;

	float dx0=xcoords[1]-xcoords[0], dy0=ycoords[1]-ycoords[0], p0=pcoords[0], len0=sqrt(dx0*dx0+dy0*dy0);
	nxcoords.push_back(xcoords[0]);
	nycoords.push_back(ycoords[0]);
	npcoords.push_back(pcoords[0]);
	unsigned int pos=1;
	float curvature_acc=0, length_acc=len0;
	while((pos+1)!=xcoords.size()) {
		float dx1=xcoords[pos+1]-xcoords[pos], dy1=ycoords[pos+1]-ycoords[pos], p1=pcoords[pos];
		float cross=dx1*dy0-dy1*dx0;
		float len1=sqrt(dx1*dx1+dy1*dy1);
		float sine=cross/len0/len1;
		curvature_acc+=abs(sine);
		length_acc+=len1;
		/* all these params are tunable */
		if(curvature_acc>0.15 || length_acc>15 || abs(p0-p1)>0.3) { 
			curvature_acc=0;
			length_acc=0;
			p0=p1;
			nxcoords.push_back(xcoords[pos]);
			nycoords.push_back(ycoords[pos]);
			npcoords.push_back(pcoords[pos]);
		}
		dx0=dx1; dy0=dy1; len0=len1;
		++pos;
	}
	nxcoords.push_back(xcoords[pos]);
	nycoords.push_back(ycoords[pos]);
	npcoords.push_back(pcoords[pos]);
	
	printf("old: %d, new: %d\n",(int) xcoords.size(), (int) nxcoords.size());
	xcoords.swap(nxcoords);
	ycoords.swap(nycoords);
	pcoords.swap(npcoords);
}

void CStroke::GetBBox(float &x0, float &x1, float &y0, float &y1, int start_index)
{
	x0=xcoords[0]; x1=xcoords[0]; y0=ycoords[0]; y1=ycoords[0];
	for(unsigned int i=start_index;i<xcoords.size();++i) {
		if(xcoords[i]-pcoords[i]<x0) x0 = xcoords[i]-pcoords[i];
		if(xcoords[i]+pcoords[i]>x1) x1 = xcoords[i]+pcoords[i];
		if(ycoords[i]-pcoords[i]<y0) y0 = ycoords[i]-pcoords[i];
		if(ycoords[i]+pcoords[i]>y1) y1 = ycoords[i]+pcoords[i];
	}
}

void CStroke::ForceMinXY(float x, float y)
{
	for(unsigned int i=0;i<xcoords.size();++i) {
		if(xcoords[i]-pcoords[i]<x) xcoords[i]=x+pcoords[i];
		if(ycoords[i]-pcoords[i]<y) ycoords[i]=y+pcoords[i];
	}
}

bool CStroke::Select(float x0, float x1, float y0, float y1)
{
	bool will_select=false;
	
	selected.resize(xcoords.size());
	for(unsigned int i=0;i<xcoords.size();++i) {
		if(    (xcoords[i]+pcoords[i]>x0)
			&& (xcoords[i]-pcoords[i]<x1)
			&& (ycoords[i]+pcoords[i]>y0)
			&& (ycoords[i]-pcoords[i]<y1)) {
			selected[i]=true;
			will_select=true;
		} else {
			selected[i]=false;
		}
	}
	
	return will_select;
}

void CStroke::Unselect()
{
	selected.clear();
}
