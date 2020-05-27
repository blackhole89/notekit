#include "drawing.h"

/* **** bound drawings (i.e. those that are part of a document) **** */

/* store the Gdk::Window so we can create accelerated surfaces for whatever app is rendering on */
CBoundDrawing::CBoundDrawing(Glib::RefPtr<Gdk::Window> wnd) : Glib::ObjectBase("CBoundDrawing"), Gtk::DrawingArea()
{
	target_window=wnd;
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
void CBoundDrawing::UpdateSize(int neww, int newh, int dx, int dy)
{
	neww-=dx; newh-=dy;
	
	if(dx!=0 || dy!=0) {
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
void CBoundDrawing::AddStroke(CStroke &s, float dx, float dy)
{
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
		str.Render(svg_ctx,0,0);
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

void CBoundDrawing::on_unrealize()
{
	printf("unrealize event on %08lX\n", (unsigned long) this);
	//get_parent()->remove(*this);
}

void CBoundDrawing::destroy_notify_()
{
	printf("destroy event on %08lX\n", (unsigned long) this);
	strokes.~vector<CStroke>();
}
