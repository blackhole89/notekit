#include <gtkmm.h>

class AboutDiag : public Gtk::AboutDialog {
	public:
		AboutDiag();
	protected:
		/* modified version of
		 * ```sh
		 * echo "std::vector<Glib::ustring> contributers {" && \
		 * git shortlog -sn | awk '{$1 = ""; print "\t\""substr($0, 2)"\"," }' && \
		 * echo "};"
		 * ```
		 * Modifications:
		 * 	- Merged my (sp1rit) names into one
		 */
		std::vector<Glib::ustring> contributers {
			"Matvey Soloviev",
			"Florian \"sp1rit\"",
			"noasakurajin",
			"Bilal Elmoussaoui",
			"Lyes Saadi",
			"proxi",
			"大黄老鼠",
		};
		std::vector<Glib::ustring> artists {
			"Thomas Kole"
		};
};
