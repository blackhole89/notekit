#include <gtkmm.h>

class AboutDiag : public Gtk::AboutDialog {
	public:
		AboutDiag();
	protected:
		std::vector<Glib::ustring> contributers {
			"Matvey Soloviev",
			"Florian \"sp1rit\"",
			"Lyes Saadi"
		};
		std::vector<Glib::ustring> artists {
			"Thomas Kole"
		};
};
