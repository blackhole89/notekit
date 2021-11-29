namespace widgets {

/* ** Widget API **
 * in: nb: Pointer to notebook to add widget to.
 * in: start, end: Iterators bracketing the textual representation of the widget. 
 *                 The first character will be a special code representing the anchor at which the
 *                 widget lands in the buffer. The actual textual representation begins at start+1.
 * return: Pointer to an unowned Gtk::Widget, or NULL. The caller will call Gtk::manage on it.
 * 
 * You may not modify the buffer in the widget API. However, you can apply and remove tags, such
 * as baseline modifiers, and signal callbacks you hook up to your widgets may modify the buffer contents.
 * */
	
Gtk::Widget *RenderCheckbox(CNotebook *nb, Gtk::TextBuffer::iterator start, Gtk::TextBuffer::iterator end)
{
	Glib::RefPtr<Gtk::TextMark> mstart = nb->sbuffer->create_mark(start,true);
	Gtk::TextIter i=start;
	++i;
	if(i.get_char()=='[') ++i;
	
	bool is_checked = (i.get_char()=='x'||i.get_char()=='X');
	
	auto j = start; ++j;
	// shift baseline for gtk checkbox widget
	nb->sbuffer->apply_tag(nb->GetBaselineTag(2),start,j);
	
	Gtk::CheckButton *b = new Gtk::CheckButton();
	if(is_checked) b->set_active(true);
	b->property_can_focus().set_value(false);
	
	b->property_active().signal_changed().connect(sigc::slot<void>( [b,mstart,nb]() {
		Gtk::TextIter i = nb->sbuffer->get_iter_at_mark(mstart);
		++i; ++i;
		if(b->get_active())
			i=nb->sbuffer->insert(i,"x");
		else
			i=nb->sbuffer->insert(i," ");
		Gtk::TextIter j=i; ++j;
		nb->sbuffer->erase(i,j);
	} ));
	
	/* set appropriate cursor when checkbox hovered */
	b->signal_enter().connect(sigc::slot<void>( [mstart,nb]() { nb->SetCursor(nb->pointer_cursor); } ));
	b->signal_leave().connect(sigc::slot<void>( [mstart,nb]() { nb->update_cursor = true; } ));
	
	/* destroy the mark when the widget is unmapped */
	b->signal_unmap().connect(sigc::slot<void>( [mstart,nb]() {
		/* need to defer the destruction due to mysterious interactions with signal chain */
		auto s = Glib::IdleSource::create();
		s->connect(sigc::slot<bool>( [mstart,nb]() {
			nb->sbuffer->delete_mark(mstart);
			return false;
		})); 
		s->attach();
	}));
	
	return b;
}
			

Gtk::Widget *RenderImage(CNotebook *nb, Gtk::TextBuffer::iterator start, Gtk::TextBuffer::iterator end)
{
	/* the worst thing that could happen is that we go to the end and URL becomes empty */
	auto urlstart = start;
	nb->sbuffer->iter_forward_to_context_class_toggle(urlstart,"url");
	
	if(urlstart < end) { 
		auto urlend = urlstart;
		nb->sbuffer->iter_forward_to_context_class_toggle(urlend,"url");
		
		// workaround for canonicalize_filename not being supported in glibmm<2.64
		Glib::RefPtr<Gio::File> file = Glib::wrap(g_file_new_for_commandline_arg_and_cwd(urlstart.get_text(urlend).c_str(), nb->document_path.c_str()));
		Gtk::Image *d = new Gtk::Image(file->get_path());

		return d;
	} else {
		// condition fails if url was empty, vis. () 
		Gtk::Image *d = new Gtk::Image(); 
		d->set_from_icon_name("action-unavailable",Gtk::ICON_SIZE_BUTTON);
		
		return d;
	}
}

#if defined (HAVE_LASEM) || defined (HAVE_CLATEXMATH)
Gtk::Widget *RenderLatex(CNotebook *nb, Gtk::TextBuffer::iterator start, Gtk::TextBuffer::iterator end)
{
	CLatexWidget *d = new CLatexWidget(nb->get_window(Gtk::TEXT_WINDOW_TEXT),nb->sbuffer->get_text(start,end,true),nb->get_style_context()->get_color());
		
	auto j = start; ++j;
	nb->sbuffer->apply_tag(nb->GetBaselineTag(d->GetBaseline()),start,j);
	
	return d;
}
#endif

}