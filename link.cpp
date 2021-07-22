#include "link.h"

LinkButton::LinkButton(Glib::ustring uri, Glib::ustring altt, Glib::ustring title) {
	std::size_t de = uri.find("://");
	if (de != std::string::npos) {
		protocol = uri.substr(0, de);
		file = Gio::File::create_for_uri(uri);
	} else {
		file = Gio::File::create_for_path(uri);
	}

	file->query_info_async(sigc::mem_fun(*this, &LinkButton::ResolveFileInfo), g_strdup_printf("%s,%s", G_FILE_ATTRIBUTE_STANDARD_ICON,G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME));
	alt=altt;

	if (title != "") {
		if (title[0] == '"' && title[title.size() - 1] == '"') {
			title.erase(title.size() - 1);
			title.erase(title.begin());
			set_tooltip_text(title);
		} else {
			fprintf(stderr, "Malformed title: %s, make sure it starts and ends with \"\n", title.c_str());
		}
	}

	btn.signal_clicked().connect(sigc::mem_fun(*this, &LinkButton::ClickHandler));
	btn.signal_enter_notify_event().connect(sigc::mem_fun(*this, &LinkButton::PointerEnterEvent));

	Glib::RefPtr<Gtk::StyleContext> ctx = get_style_context();
	ctx->add_class("link-card");

	container.set_spacing(8);
	container.set_margin_top(12);
	container.set_margin_start(12);
	container.set_margin_end(12);
	container.set_margin_bottom(12);

	img.set_from_icon_name("application-x-generic", Gtk::IconSize(6));
	altl.set_label(altt);

	Glib::RefPtr<Gtk::StyleContext> lctx = altl.get_style_context();
	lctx->add_class("large-title");

	container.pack_start(img, false, false);
	container.pack_end(altl, true, false);

	btn.add(container);
	btn.show_all();

	add(btn);
}

void LinkButton::ResolveFileInfo(const Glib::RefPtr<Gio::AsyncResult>& result) {
	try {
		Glib::RefPtr<Gio::FileInfo> info = file->query_info_finish(result);
		if (alt == "") {
			altl.set_label(info->get_display_name());
		}
		Glib::RefPtr<const Gio::Icon> icon = info->get_icon();
		img.set(icon, Gtk::IconSize(6));
	} catch (Gio::Error const &e) {
		switch(e.code()) {
			case Gio::Error::Code::FAILED: // no network connection
				fprintf(stderr, "General gio error: %s\n", e.what().c_str());
				// fall-through
			case Gio::Error::Code::NOT_MOUNTED: // ftp:// uses this
			// fall-through
		case Gio::Error::Code::NOT_SUPPORTED: {
			fprintf(stderr, "Fallback to protocol\n");
			protocolmap_t::iterator protocol_icon = protocol_icons.find(protocol);
			if (protocol_icon != protocol_icons.end()) {
				Glib::RefPtr<const Gio::Icon> p_icon = protocol_icon->second;
				img.set(p_icon, Gtk::IconSize(6));
			} else {
				fprintf(stderr, "%s has not icon", protocol.c_str());
			}
			}
			break;
		case Gio::Error::NOT_FOUND:
			btn.set_sensitive(false);
			// I'd think an "File Missing" icon would be more appropriate, but Adwaita doesn't provide one.
			img.set_from_icon_name("image-missing", Gtk::IconSize(6));
			break;
		default:
			fprintf(stderr, "Failed to realize icon of link: %s (Code: %d)\n", e.what().c_str(), e.code());
		}
	}
}

void LinkButton::ClickHandler() {
	try {
		file->query_default_handler()->launch(file);
	} catch (Gio::Error const &e) {
		switch(e.code()) {
			case Gio::Error::Code::NOT_SUPPORTED:
				// TODO: telling the user graphically would be nice.
				fprintf(stderr, "[Open Link] No application for that kind of file found/set.\n");
				break;
			case Gio::Error::NOT_FOUND:
				// button should be set to unsensetive, from self::ResolveFileInfo
				// so this should not have been called. Just in case, wel'll set it
				// to unsensitive again.
				btn.set_sensitive(false);
				break;
			default:
				fprintf(stderr, "[Open link] Unhandled GIO Exception: %s (Code %d)\n", e.what().c_str(), e.code());
		}
	}
}

bool LinkButton::PointerEnterEvent(GdkEventCrossing* event) {
	if(get_realized()) {
		Glib::RefPtr<Gdk::Cursor> cur = Gdk::Cursor::create(get_display(), "pointer");
		Glib::RefPtr<Gdk::Window> win = get_root_window();
		win->set_cursor(cur);
	}
	return true;
}
