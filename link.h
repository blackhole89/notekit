#include <gtkmm.h>

class LinkButton : public Gtk::Bin {
	public:
		LinkButton(Glib::ustring uri, Glib::ustring alt, Glib::ustring title);
		//virtual ~NkLinkButton();

		Glib::ustring protocol;
		Glib::ustring alt;

	protected:
		Gtk::Button btn;
		Gtk::Box container;
		Gtk::Image img;
		Gtk::Label altl;

		void ResolveFileInfo(const Glib::RefPtr<Gio::AsyncResult>& result);

		void ClickHandler();
		bool PointerEnterEvent(GdkEventCrossing* event);

		Glib::RefPtr<Gio::File> file;

		typedef std::map<const Glib::ustring, const Glib::RefPtr<Gio::Icon>> protocolmap_t;
		protocolmap_t protocol_icons {
			{"http", Gio::Icon::create("text-html")},
			{"https", Gio::Icon::create("text-html")},
			{"ftp", Gio::Icon::create("emblem-web")}
		};
};
