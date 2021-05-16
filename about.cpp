#include "about.h"

AboutDiag::AboutDiag() {
    set_program_name("NoteKit");
    set_logo_icon_name("com.github.blackhole89.notekit");
    set_comments("A GTK3 hierarchical markdown notetaking\napplication with tablet support.");
    set_version("0.0.1");
    set_website("https://github.com/blackhole89/notekit");
    set_website_label("GitHub: @blackhole89/notekit");
    set_copyright("Maintainer:  Matvey Soloviev");
    set_license_type(Gtk::LICENSE_GPL_3_0);

    set_authors(contributers);
    set_artists(artists);
}
