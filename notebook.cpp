#include "notebook.h"

#include "notebook_clipboard.hpp"
#include "imagewidgets.h"

CNotebook::CNotebook()
{
	last_device=NULL;
}

/* DIRTY HACK: If the layout changes a lot, sometimes GtkTextView will
 * get events without it being valid (event race?). This can lead
 * to crashes. Therefore, we'll manually check for layout validity
 * at the beginning of events. */
extern "C" bool gtk_text_layout_is_valid(void*);

bool CNotebook::on_event(GdkEvent*)
{
	if(!gtk_text_layout_is_valid(*(void**)GTK_TEXT_VIEW(gobj())->priv)) {
		//printf("event blocked\n");
		return true;
	}
	return false;
}

/* Initialise notebook widget, loading style files etc. from data_path. */
void CNotebook::Init(std::string data_path)
{
	sbuffer = get_source_buffer();

	/* load syntax highlighting scheme */
	Glib::RefPtr<Gsv::StyleSchemeManager> styleman = Gsv::StyleSchemeManager::create();
	styleman->set_search_path({data_path+"/sourceview/"});
	Glib::RefPtr<Gsv::StyleScheme> the_scheme;
	the_scheme = styleman->get_scheme("classic");
	sbuffer->set_style_scheme(the_scheme);
	
	Glib::RefPtr<Gsv::LanguageManager> langman = Gsv::LanguageManager::create();
	langman->set_search_path({data_path+"/sourceview/"});
	sbuffer->set_language(langman->get_language("markdown"));
	
	/* register our serialisation formats */
	sbuffer->register_serialize_format("text/notekit-markdown", sigc::bind( sigc::mem_fun(this,&CNotebook::on_serialize), false ));
	sbuffer->register_deserialize_format("text/notekit-markdown",sigc::mem_fun(this,&CNotebook::on_deserialize));
	
	sbuffer->register_serialize_format("text/plain", sigc::bind( sigc::mem_fun(this,&CNotebook::on_serialize), true ));
	
	Glib::RefPtr<Gtk::TargetList> l = sbuffer->get_copy_target_list();
	
	/* create drawing overlay */
	//add_child_in_window(overlay,Gtk::TEXT_WINDOW_WIDGET,0,0);
	
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
	sbuffer->signal_changed().connect(sigc::mem_fun(this,&CNotebook::on_changed),true);
	sbuffer->signal_highlight_updated().connect(sigc::mem_fun(this,&CNotebook::on_highlight_updated),true);
	sbuffer->property_cursor_position().signal_changed().connect(sigc::mem_fun(this,&CNotebook::on_move_cursor));
	
	last_position=sbuffer->create_mark("last_position", sbuffer->begin(),true);
	tag_proximity=sbuffer->create_tag();
	//use for debug
	//tag_proximity->property_background_rgba().set_value(Gdk::RGBA("rgb(255,128,128)"));
	
	/* overwrite clipboard signals with our custom impls */
	GtkTextViewClass *k = GTK_TEXT_VIEW_GET_CLASS(gobj());
	k->copy_clipboard = notebook_copy_clipboard;
	k->cut_clipboard = notebook_cut_clipboard;
	k->paste_clipboard = notebook_paste_clipboard;
	/* DIRTY HACK: GtkSourceView's cursor movement methods loop forever in the presence of invisible text sometimes */
	GtkWidget *text_view = gtk_text_view_new();
	GtkTextViewClass *plain = GTK_TEXT_VIEW_GET_CLASS(text_view);
	k->extend_selection = plain->extend_selection;
	k->move_cursor = plain->move_cursor;
	gtk_widget_destroy(text_view);

	
	//printf("%08lX %08lX",k->extend_selection,GTK_TEXT_VIEW_CLASS(&k->parent_class)->extend_selection);
	
	//k->extend_selection = GTK_TEXT_VIEW_CLASS(&k->parent_class)->extend_selection;
	//g_signal_connect(gobj(),"copy-clipboard",G_CALLBACK(notebook_copy_clipboard),NULL);
	
	active_state=NB_STATE_NOTHING;
	update_cursor=false;
	attention_ewma=0.0;
	
	/* create tags for style aspects that the syntax highlighter doesn't handle */
	tag_extra_space = sbuffer->create_tag();
	tag_extra_space->property_pixels_below_lines().set_value(8);
	tag_extra_space->property_pixels_above_lines().set_value(8);
	
	tag_blockquote = sbuffer->create_tag();
	tag_blockquote->property_left_margin().set_value(16);
	tag_blockquote->property_indent().set_value(-16);
	tag_blockquote->property_accumulative_margin().set_value(true);
	Gdk::RGBA fg = get_style_context()->get_color();
	fg.set_alpha(0.75);
	tag_blockquote->property_foreground_rgba().set_value(fg);
	
	tag_invisible = sbuffer->create_tag();
	tag_invisible->property_foreground_rgba().set_value(Gdk::RGBA("rgba(0,0,0,0)"));
	
	tag_hidden = sbuffer->create_tag();
	tag_hidden->property_invisible().set_value(true);
	
	tag_mono = sbuffer->create_tag();
	tag_mono->property_family().set_value("monospace");
	tag_mono->property_scale().set_value(0.8);
	/*tag_extra_space->property_left_margin().set_value(32);
	tag_extra_space->property_indent().set_value(-32);*/
	
	set_wrap_mode(Gtk::WRAP_WORD_CHAR);
}

void CNotebook::SetCursor(Glib::RefPtr<Gdk::Cursor> c)
{
	if(auto w=get_window(Gtk::TEXT_WINDOW_TEXT)) w->set_cursor(c);
}

void CNotebook::on_action(std::string name,int type,int param)
{
	switch(type) {
	case NB_ACTION_CMODE:
		devicemodes[last_device]=param;
		update_cursor=true;
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
		
		// size changed; need to recreate Cairo surface
		Cairo::RefPtr<Cairo::Surface> newptr = overlay.get_window()->create_similar_surface(Cairo::CONTENT_COLOR_ALPHA,a.get_width(),a.get_height());
		overlay_ctx = Cairo::Context::create(newptr);
		// copy old surface
		if(overlay_image) {
			overlay_ctx->save();
			overlay_ctx->set_source(overlay_image,0,0);
			overlay_ctx->rectangle(0,0,a.get_width(),a.get_height());
			overlay_ctx->set_operator(Cairo::OPERATOR_SOURCE);
			overlay_ctx->fill();
			overlay_ctx->restore();
		}
		// replace surface
		overlay_image = newptr;
	}
}

void CStroke::Render(const Cairo::RefPtr<Cairo::Context> &ctx, float basex, float basey, int start_index)
{
	ctx->translate(-basex,-basey);
	ctx->set_source_rgba(r,g,b,a);
	ctx->set_line_cap(Cairo::LINE_CAP_ROUND);
	for(unsigned int i=start_index;i<xcoords.size();++i) {
		ctx->set_line_width(pcoords[i-1]);
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
	latest_keyval = k->keyval;
	return Gsv::View::on_key_press_event(k);
}

void CNotebook::on_changed()
{
	last_modified = g_get_real_time();
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
	/* it seems that this can theoretically be called before the first on_allocate; 
	 * since we don't know our document view's size yet, we have no choice but to bail */
	if(!overlay_image) {
		return true;
	}
	//active.Render(ctx,bx,by);
	ctx->set_source(overlay_image,0,0);
	ctx->paint();
	//ctx->rectangle(0,0,get_width(),get_height());
	ctx->fill();
	
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
	//printf("relight %d %d\n",start.get_offset(),end.get_offset());
	std::pair<Glib::ustring,Glib::RefPtr<Gtk::TextTag> > extratags[]
		= { {"extra-space", tag_extra_space},
			{"blockquote-text", tag_blockquote},
			{"hline", tag_invisible},
			{"mono", tag_mono} };
	for(auto &[cclass,ttag] : extratags) {
		Gtk::TextBuffer::iterator i = start;
		do {
			Gtk::TextBuffer::iterator next = i;
			if(!sbuffer->iter_forward_to_context_class_toggle(next, cclass)) next=end;
			if(sbuffer->iter_has_context_class(i, cclass)) {
				sbuffer->apply_tag(ttag, i, next);
			} else {
				sbuffer->remove_tag(ttag, i, next);
			}
			i=next;
		} while(i<end);
	}
	
	/* TODO: this makes some widgets flicker when they shouldn't. */
	/* We really shouldn't clean up a span unless it changed, 
	 * but need to solve the *asdf*ghjk* middle * being part of
	 * both prox tags problem */
	/* probably:
	 * (1) walk the whole region, clear prox tags that no longer
	 *     are overlapped by an exactly matching prox context;
	 * (2) walk the whole region, instantiate all prox contexts
	 *     that are not already overlapped by an exactly matching
	 *     prox tag. 
	 * This ignores the case that a prox region has been 
	 * EXACTLY replaced by an incompatible one, but whatevs. */
	CleanUpSpan(start,end);
	
	Gtk::TextBuffer::iterator i = start;
	do {
		Gtk::TextBuffer::iterator next = i;
		if(!sbuffer->iter_forward_to_context_class_toggle(next, "prox")) next=end;
		
		if(sbuffer->iter_has_context_class(i, "prox")) {
			if(!i.has_tag(tag_proximity)) {
				/* initialise tag by leaving if necessary */
				Gtk::TextIter old_iter = sbuffer->get_iter_at_mark(last_position);
				if(old_iter.compare(i)<0 || old_iter.compare(next)>0) {
					PushIter(start);
					PushIter(end);
					on_leave_region(i,next);
					end=PopIter();
					start=PopIter();
				}
			}
			sbuffer->apply_tag(tag_proximity, i, next);
		} else {
			/* nothing to do \o/ */
		}
		i=next;
	} while(i<end);
}

Glib::RefPtr<Gtk::TextMark> CNotebook::PushIter(Gtk::TextIter i)
{
	iter_stack.push(sbuffer->create_mark(i,true));
	return iter_stack.top();
}

Gtk::TextIter CNotebook::PopIter()
{
	Glib::RefPtr<Gtk::TextMark> m = iter_stack.top();
	iter_stack.pop();
	Gtk::TextIter ret = sbuffer->get_iter_at_mark(m);
	sbuffer->delete_mark(m);
	return ret;
}

void CNotebook::RenderToWidget(Glib::ustring wtype, Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end) 
{
	if(wtype=="checkbox") {
		Gtk::TextIter i=start;
		++i;
		if(i.get_char()=='[') ++i;
		
		bool is_checked = (i.get_char()=='X');
		
		/* create a mark to save the control's starting position */
		Glib::RefPtr<Gtk::TextMark> mstart = sbuffer->create_mark(start,true);
		PushIter(end);
		
		Glib::RefPtr<Gtk::TextBuffer::ChildAnchor> anch;
		if(!(anch=start.get_child_anchor())) {
			/* we haven't set up a child anchor yet, so we need to queue the creation of one */
			
			/* defer creation of anchor to prevent invalidating iters. */
			/* once the anchor is created, syntax highlighting will recalculate, eventually
			 * calling this function again from the start */
			auto s = Glib::IdleSource::create();
			s->connect(sigc::slot<bool>( [mstart,this]() {
				Gtk::TextIter start = sbuffer->get_iter_at_mark(mstart);
				sbuffer->delete_mark(mstart);
				
				if(!start.get_child_anchor()) {
					sbuffer->create_child_anchor(start);
				}
				return false;
			})); 
			s->attach();
			// anch = sbuffer->create_child_anchor(start);
		} else {
			auto j = start; ++j;
			sbuffer->remove_tag(tag_hidden,start,j);
			
			Gtk::CheckButton *b = new Gtk::CheckButton();
			if(is_checked) b->set_active(true);
			b->property_can_focus().set_value(false);
			
			b->property_active().signal_changed().connect(sigc::slot<void>( [b,mstart,this]() {
				Gtk::TextIter i = sbuffer->get_iter_at_mark(mstart);
				++i; ++i;
				if(b->get_active())
					i=sbuffer->insert(i,"X");
				else
					i=sbuffer->insert(i," ");
				Gtk::TextIter j=i; ++j;
				sbuffer->erase(i,j);
			} ));
			
			/* destroy the mark if the widget is unmapped */
			b->signal_unmap().connect(sigc::slot<void>( [mstart,this]() {
				/* need to defer the destruction due to mysterious interactions with signal chain */
				auto s = Glib::IdleSource::create();
				s->connect(sigc::slot<bool>( [mstart,this]() {
					sbuffer->delete_mark(mstart);
					return false;
				})); 
				s->attach();
			}));
			
			Gtk::manage(b);
			add_child_at_anchor(*b,anch);
			b->show();
		}
		
		end=PopIter();
		start=sbuffer->get_iter_at_mark(mstart);
	}
#if defined (HAVE_LASEM) || defined (HAVE_CLATEXMATH)
	else if(wtype=="latex") {
		
		Glib::RefPtr<Gtk::TextBuffer::ChildAnchor> anch;
		
		if(!(anch=start.get_child_anchor())) {
			/* we haven't set up a child anchor yet, so we need to queue the creation of one */
			/* defer creation of anchor to prevent invalidating iters. */
			/* once the anchor is created, syntax highlighting will recalculate, eventually
			 * calling this function again from the start */
			Glib::RefPtr<Gtk::TextMark> mstart = sbuffer->create_mark(start,true);
			auto s = Glib::IdleSource::create();
			s->connect( [mstart,this]() {
				Gtk::TextIter start = sbuffer->get_iter_at_mark(mstart);
				sbuffer->delete_mark(mstart);
				
				if(!start.get_child_anchor()) {
					sbuffer->create_child_anchor(start);
				}
				return false;
			}); 
			s->attach();
			// anch = sbuffer->create_child_anchor(start);
		} else {
			auto j = start; ++j;
			sbuffer->remove_tag(tag_hidden,start,j);
			
			CLatexWidget *d = new CLatexWidget(get_window(Gtk::TEXT_WINDOW_TEXT),sbuffer->get_text(start,end,true),get_style_context()->get_color());
			Gtk::manage(d); 
			sbuffer->apply_tag(GetBaselineTag(d->GetBaseline()),start,j);
			add_child_at_anchor(*d,anch);
			d->show();
		}
		
	}
#endif
}

Glib::RefPtr<Gtk::TextTag> CNotebook::GetBaselineTag(int baseline)
{
	if(!baseline_tags.count(baseline)) {
		auto t = sbuffer->create_tag();
		t->property_rise().set_value(-PANGO_SCALE*baseline);
		baseline_tags[baseline]=t;
	}
	return baseline_tags[baseline];
}

void CNotebook::UnrenderWidgets(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end)
{
	if(start.get_child_anchor()) {
		auto ws = start.get_child_anchor()->get_widgets();
		for(Gtk::Widget *w : ws) {
			delete w;
		}
		auto j = start; ++j;
		sbuffer->apply_tag(tag_hidden,start,j);
		
		/*PushIter(end);
		Gtk::TextBuffer::iterator start_old = start;
		++start;
		start=sbuffer->erase(start_old,start);
		end=PopIter();*/
	}
}

/* Remove widget child anchors in span. */
void CNotebook::CleanUpSpan(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end)
{
	PushIter(start);
	Gtk::TextBuffer::iterator i = start;
	
	do {
		Gtk::TextBuffer::iterator next = i;
		if(!next.forward_to_tag_toggle(tag_proximity) || next>end) next=end;
		
		Gtk::TextIter ic=next, nextc=i;
		sbuffer->iter_backward_to_context_class_toggle(ic,"prox");
		sbuffer->iter_forward_to_context_class_toggle(nextc,"prox");
		
		if(i.has_tag(tag_proximity) && (ic!=i || nextc!=next)) {
			//printf("clean %d %d\n",i.get_offset(),next.get_offset());
			std::pair<Glib::ustring,Glib::RefPtr<Gtk::TextTag> > volatile_tags[]
				= { {"invisible", tag_hidden} };
				
			for(auto &[cclass,ttag] : volatile_tags) {
				sbuffer->remove_tag(ttag, i, next);
			}
			sbuffer->remove_tag(tag_proximity,i,next);
			
			if(i.get_child_anchor()) {
				PushIter(end);
				PushIter(next);
				next=i;
				++next;
				start=sbuffer->erase(i,next);
				next=PopIter();
				end=PopIter();
			}
		}
		i=next;
	} while(i<end);
	start=PopIter();
}

void CNotebook::on_enter_region(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end)
{
	//printf("enter: %d %d\n",start.get_offset(),end.get_offset());
	
	std::pair<Glib::ustring,Glib::RefPtr<Gtk::TextTag> > volatile_tags[]
		= { {"invisible", tag_hidden} };
		
	for(auto &[cclass,ttag] : volatile_tags) {
		sbuffer->remove_tag(ttag, start, end);
	}
	
	Glib::ustring renderable_tags[]
		= { "checkbox", 
#if defined (HAVE_LASEM) || defined (HAVE_CLATEXMATH)
		"latex"
#endif
		};
		
	for(auto &s : renderable_tags) {
		if(sbuffer->iter_has_context_class(start, s)) {
			UnrenderWidgets(start,end);
		}
	}
}

void CNotebook::on_leave_region(Gtk::TextBuffer::iterator &start, Gtk::TextBuffer::iterator &end)
{
	//printf("leave: %d %d\n",start.get_offset(),end.get_offset());
	
	std::pair<Glib::ustring,Glib::RefPtr<Gtk::TextTag> > volatile_tags[]
		= { {"invisible", tag_hidden} };
	
	for(auto &[cclass,ttag] : volatile_tags) {
		Gtk::TextBuffer::iterator i = start;
		do {
			Gtk::TextBuffer::iterator next = i;
			if(!sbuffer->iter_forward_to_context_class_toggle(next, cclass)) next=end;
			if(sbuffer->iter_has_context_class(i, cclass)) {
				sbuffer->apply_tag(ttag, i, next);
			} else {
				sbuffer->remove_tag(ttag, i, next);
			}
			i=next;
		} while(i<end);
	}
	
	Glib::ustring renderable_tags[]
		= { "checkbox", 
#if defined (HAVE_LASEM) || defined (HAVE_CLATEXMATH)
		"latex"
#endif
		};
		
	for(auto &s : renderable_tags) {
		if(sbuffer->iter_has_context_class(start, s)) {
			UnrenderWidgets(start,end); // just in case
			RenderToWidget(s,start,end);
		}
	}
}


bool GetTagExtents(Gtk::TextIter t, Glib::RefPtr<Gtk::TextTag> tag, Gtk::TextIter &left, Gtk::TextIter &right)
{
	if(t.starts_tag(tag)) {
		left=t;
		t.forward_to_tag_toggle(tag);
		right=t;
		return true;
	}
	if(t.ends_tag(tag)) {
		right=t;
		t.backward_to_tag_toggle(tag);
		left=t;
		return true;
	}
	if(t.has_tag(tag)) {
		left=t;
		left.backward_to_tag_toggle(tag);
		right=t;
		right.forward_to_tag_toggle(tag);
		return true;
	}
	return false;
}

void CNotebook::on_move_cursor()
{
	auto s = Glib::IdleSource::create();
	s->connect(sigc::slot<bool>( [this]() { 
		Gtk::TextIter new_iter = sbuffer->get_iter_at_mark(sbuffer->get_insert());
		Gtk::TextIter old_iter = sbuffer->get_iter_at_mark(last_position);
		//printf("old: %d, new: %d\n", old_iter.get_offset(), new_iter.get_offset());
		/*auto v = new_iter.get_marks();
		printf("%d marks here: ",v.size());
		for(auto &m : v) printf("%s ",m->get_name().c_str());
		printf("\n");*/
		
		Gtk::TextIter old_left, old_right, new_left, new_right;
		
		bool old_valid = GetTagExtents(old_iter,tag_proximity,old_left,old_right);
		bool new_valid = GetTagExtents(new_iter,tag_proximity,new_left,new_right);
		bool from_right = new_valid && new_iter.compare(new_left)>0//old_iter.compare(new_iter)>0
				&& (!latest_keyval || latest_keyval == GDK_KEY_Left ||
					latest_keyval == GDK_KEY_Up || latest_keyval == GDK_KEY_Down);
		Glib::RefPtr<Gtk::TextMark> move_to;
		
		if(old_valid) {
			if(new_valid) {
				if(new_iter.compare(old_left)<0 || new_iter.compare(old_right)>0) {
					if(from_right) {
						if(new_iter.has_tag(tag_hidden)) {
							new_iter.forward_to_tag_toggle(tag_hidden);
							move_to = sbuffer->create_mark(new_iter,false);
						}
					}
					/* moved into a different prox region. signal both. */
					PushIter(new_left);
					PushIter(new_right);
					on_leave_region(old_left,old_right);
					new_right=PopIter();
					new_left=PopIter();
					on_enter_region(new_left,new_right);
				} else if(new_iter==old_iter) {
					/* we may have deleted right into a prox region's boundary. signal to be safe. */
					on_enter_region(new_left,new_right);
				}
				/* otherwise, we stayed in the same one. no signals needed */
			} else {
				on_leave_region(old_left,old_right);
			}
		} else if(new_valid) {
			if(from_right) {
				if(new_iter.has_tag(tag_hidden)) {
					new_iter.forward_to_tag_toggle(tag_hidden);
					move_to = sbuffer->create_mark(new_iter,false);
				}
			}
			on_enter_region(new_left,new_right);
		}

		if(move_to) {
			//printf("move right: %d\n",latest_keyval);
			Gtk::TextIter t = sbuffer->get_iter_at_mark(move_to);
			Gtk::TextIter ins = sbuffer->get_iter_at_mark(sbuffer->get_insert());
			Gtk::TextIter sb = sbuffer->get_iter_at_mark(sbuffer->get_mark("selection_bound"));
			sbuffer->move_mark_by_name("insert",t);
			if(sb==ins) sbuffer->move_mark_by_name("selection_bound",t);
			sbuffer->delete_mark(move_to);
			/*auto s = Glib::IdleSource::create();
			s->connect(sigc::slot<bool>( [move_to,this]() {
				Gtk::TextIter t = sbuffer->get_iter_at_mark(move_to);
				Gtk::TextIter ins = sbuffer->get_iter_at_mark(sbuffer->get_insert());
				Gtk::TextIter sb = sbuffer->get_iter_at_mark(sbuffer->get_mark("selection_bound"));
				sbuffer->move_mark_by_name("insert",t);
				if(sb==ins) sbuffer->move_mark_by_name("selection_bound",t);
				sbuffer->delete_mark(move_to);
				return false;
			})); 
			s->attach();*/
		} 
		
		sbuffer->move_mark(last_position, sbuffer->get_iter_at_mark(sbuffer->get_insert()));//new_iter);
		
		return false;
	}));
	s->attach();
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
	latest_keyval=0;
	
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
		
			//printf("sig: %f %f %f %f\n",e->x, e->y, x,y);
		
			active.Reset();
			//active.r=active.g=active.b=0; active.a=1;
			active.Append(x,y, stroke_width*ReadPressure(e->device));
			
			active_state = NB_STATE_DRAW;
			
			gdk_window_set_event_compression(e->window,false);
			
			grab_focus();
		
			return true;
		}
		case NB_MODE_ERASE: {
			active_state = NB_STATE_ERASE;
			
			grab_focus();
			
			return true;
		}
		default: return false;
	}
	
}

bool CNotebook::on_button_release(GdkEventButton *e)
{
	gdk_window_set_event_compression(e->window,true);
	
	if(active_state!=NB_STATE_NOTHING) {
		if(active_state==NB_STATE_DRAW) {
			sbuffer->begin_not_undoable_action();
			active.Simplify();
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
		double x,y,p;
		Widget2Doc(e->x,e->y,x,y); 
		p = stroke_width*ReadPressure(e->device);
		
		/* dynamically toggle event compression */
		float xprev = 0.0f;
		float yprev = 0.0f;
		active.GetHead(xprev,yprev);
		float curve = abs(active.GetHeadCurvatureWrt(x,y));
		float delta = sqrt ( (xprev-x)*(xprev-x)+(yprev-y)*(yprev-y) );
		
		if(delta<0.6) {
			/* reject if we are too close to previous head */
			gdk_window_set_event_compression(e->window,true);
			return false;
		}
		
		attention_ewma = sqrt(curve)+(delta>4?0.2:0);
		if(attention_ewma<0.32) {
			//printf("compression on\n");
			//p=1;
			gdk_window_set_event_compression(e->window,true);
		} else if(attention_ewma>0.32){
			//printf("compression off\n");
			//%%p=3;
			gdk_window_set_event_compression(e->window,false);
		}
		//p=attention_ewma;
		
		active.Append(x,y,p);
		
		int bx, by;
		window_to_buffer_coords(Gtk::TEXT_WINDOW_WIDGET,0,0,bx,by);
		active.Render(overlay_ctx,bx,by,active.xcoords.size()-1);
		
		overlay.queue_draw_area(e->x-delta-p,e->y-delta-p,2*delta+4*p,2*delta+4*p);
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
			if(CBoundDrawing* d = CBoundDrawing::TryUpcast(w[0])) {
				/* found a region to erase from; we can adjust for its real location,
				 * but that's relative to the text body, so need to fixup */
				int bx, by;
				window_to_buffer_coords(Gtk::TEXT_WINDOW_TEXT,0,0,bx,by);
				d->EraseAt(x-(d->get_allocation().get_x())-bx,y-(d->get_allocation().get_y())-by,stroke_width,true);
				
				on_changed();
			}
		}
	}
}

void CNotebook::CommitStroke()
{
	float x0,y0,x1,y1;
	active.GetBBox(x0,x1,y0,y1);
	
	CBoundDrawing *d = NULL;
	
	/* clear buffer */
	overlay_ctx->save();
	overlay_ctx->set_source_rgba(0,0,0,0);
	overlay_ctx->rectangle(0,0,get_width(),get_height());
	overlay_ctx->set_operator(Cairo::OPERATOR_SOURCE);
	overlay_ctx->fill();
	overlay_ctx->restore();
	
	/* search for preceding regions we can merge with */
	Gtk::TextBuffer::iterator i,k;
	get_iter_at_location(i,x0,y0);
	k=i; // for later
	do {
		if(i.get_child_anchor()) {
			auto w = i.get_child_anchor()->get_widgets();
			if(w.size()) {
				//printf("match?\n");
				d = CBoundDrawing::TryUpcast(w[0]);
				break;
			}
		}
		if(i==sbuffer->begin()) break; // nothing more to the left
		Gtk::TextBuffer::iterator j = i;
		--j;
		gunichar prev_char = j.get_char();
		if(!g_unichar_isspace(prev_char) && prev_char!=0xFFFC) {
			break;
		}
		i=j;
	} while(true);
	
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
					//printf("match below?\n");
					d = CBoundDrawing::TryUpcast(w[0]);
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
			
			//printf("get rect: %d %d %d %d\n",r.get_x(),r.get_y(),r.get_width(),r.get_height());
			
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
			if(i.has_tag(tag_hidden)) {
				i.forward_to_tag_toggle(tag_hidden);
				++i; // TODO: Is this safe?
			}
			
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
	
	on_changed(); // update changed flag
}

guint8* CNotebook::on_serialize(const Glib::RefPtr<Gtk::TextBuffer>& content_buffer, const Gtk::TextBuffer::iterator& start, const Gtk::TextBuffer::iterator& end, gsize& length, bool render_images)
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
				if(CBoundDrawing *d = CBoundDrawing::TryUpcast(w[0])) {
					if(render_images)
						buf += d->SerializePNG();
					else
						buf += d->Serialize();
				}
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
			//printf("before: %d\n",iter.get_offset());
			auto mk = content_buffer->create_mark(iter,false);
			auto anch = content_buffer->create_child_anchor(iter);
			iter = content_buffer->get_iter_at_mark(mk);
			//printf("after: %d\n",iter.get_offset());
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

void CStroke::GetHead(float &x, float &y)
{
	if(!xcoords.size()) return;
	x = xcoords.back();
	y = ycoords.back();
}

float CStroke::GetHeadCurvatureWrt(float x, float y)
{
	if(xcoords.size()<2) return 0.0f;
	
	int n=xcoords.size()-1;
	float dx0=xcoords[n]-xcoords[n-1], dy0=ycoords[n]-ycoords[n-1], len0=sqrt(dx0*dx0+dy0*dy0);
	float dx1=x-xcoords[n], dy1=y-ycoords[n], len1=sqrt(dx1*dx1+dy1*dy1);
	float cross=dx1*dy0-dy1*dx0;
	return cross/len0/len1;
}

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

