# NoteKit ![automated build](https://github.com/blackhole89/notekit/workflows/automated%20build/badge.svg)
This program is a structured notetaking application based on GTK+ 3. Write your notes in instantly-formatted Markdown, organise them in a tree of folders that can be instantly navigated from within the program, and add hand-drawn notes by mouse, touchscreen or digitiser.

![Screenshot](/screenshots/notekit-example.png?raw=true)

## Why?

I figured it would be nice to have a free-software, platform-independent OneNote. While there is a remarkable number of free (speech or beer) notetaking applications out there, to my best knowledge, none of them simultaneously check the following boxes:

* note organisation
* text as a first-class object
* formatting
* simple, standard on-disk format
* tablet input

## How to install 

You can download the following binary builds:

* [Linux x86_64](http://twilightro.kafuka.org/%7Eblackhole89/files/notekit-20200418.tar.gz) (Git version of 2020-04-18)
* [Windows x86_64](http://twilightro.kafuka.org/%7Eblackhole89/files/notekit-20200426.zip) (Git version of 2020-04-26). The Windows version is less tested and incapable of remembering which folders are opened to mingw-w64's lack of support for xattrs.

There is also a [Fedora COPR repository](https://copr.fedorainfracloud.org/coprs/lyessaadi/notekit/) (thanks to @LyesSaadi).

To run the binary, you will in addition require at least the following packages: `libgtkmm-3.0-1v5 libgtksourceviewmm-3.0-0v5 libjsoncpp1 zlib1g libxml2`, where the version of `libgtkmm-3.0-1v5` is at least 3.20. (In particular, this means that Ubuntu 16.04 LTS (xenial) and derived distributions are too old.) If the binary does not work for you, it is recommended that you build from source, as described below.

You can also install it using snap:
[![Get it from the Snap Store](https://snapcraft.io/static/images/badges/en/snap-store-black.svg)](https://snapcraft.io/notekit)

## How to build
Either invoke `cmake .` followed by `make` (which will build a binary at `cmake-build-Release/output/notekit`), or get [CodeLite](https://codelite.org/), open and build the workspace.

Required libraries:

* `cmake`.
* `libgtkmm-3.0-dev`>=3.20 (UI stuff)
* `libgtksourceviewmm-3.0-dev`>=3.18 (more UI stuff)
* `libjsoncpp-dev` ~ 1.7.4 (config files; older versions may work)
* `zlib1g-dev`
* `libfontconfig1-dev` ~ 2.13 (to use custom fonts)

If you want to enable LaTeX math rendering support, you moreover need:

* Set the CMAKE variable `HAVE_CLATEXMATH` to ON.
* Run `install-clatexmath.sh` to clone [cLaTeXMath](https://github.com/NanoMichael/cLaTeXMath) into a subfolder and build it as a static library.
* If you observe linker errors, make sure your `gcc` is sufficiently new.

For older LaTeX math support using [lasem](https://github.com/GNOME/lasem), you can proceed as follows:

* Set the CMAKE variable `HAVE_LASEM` to ON.
* Have checked out and compiled [lasem](https://github.com/GNOME/lasem) from git in the CMAKE variable `LASEM_PATH` (default: `./lasem`). (Remember to build it; just checking out is not enough.)
* `libxml2-dev` ~ 2.9 (older versions may work)

Development and testing was exclusively conducted on X11-based Linux. The one tested way of building on Windows involves [MSYS2](https://www.msys2.org/)'s mingw-w64 package family (following the `cmake` route outlined above). Since MSYS2's `coreutils` depend on its Cygwin fork, the released Windows binary packages instead include a subset of coreutils from [GnuWin32](http://gnuwin32.sourceforge.net/).

## Installation notes
* By default, configuration is saved in `$HOME/.config/notekit`, and notes are in `$HOME/.local/share/notekit`. This may depend on your `$XDG_` environmental variables, and the notes base path can be changed in the `config.json` file in the configuration folder.
* Resources (`data/` and `sourceview/`) are searched in `/notekit/` under `$XDG_DATA_DIRS` (default: `/usr/local/share:/usr/share`), followed by the current working directory `.`. If packaging Notekit or otherwise preparing it for system-wide installation, these two folders should probably be copied into `/usr/share/notekit/data` and `/usr/share/notekit/sourceview` respectively.

## Usage notes
* To create a new note, doubleclick a `+` node in the tree view and enter a name.
* To create a new **folder**, doubleclick a `+` node in the tree view and enter a name ending in `/`, e.g. `new folder/`.
* To edit the default colour palette, right-click a colour picker.
* Drawings can currently only be deleted whole. (This will be fixed eventually.)
* Files are saved automatically when the window is closed, or when a different file is opened.
* The document formatting is mostly based on standard `GtkSourceView` language and style files. If you want to change colours or syntax highlighting rules, you can edit them in the `sourceview/` subfolder.
* The program loads a custom Gtk+ stylesheet found in `data/stylesheet.css`. Clear it if parts of the UI look wonky.
* When copypasting text into other applications, drawings will be automatically converted into data URL PNGs.
* Some markdown will be hidden ("rendered") unless your cursor is next to it.

## Project status
Late alpha. Creating and editing notes and drawing works well enough, but many basic quality-of-life features (such as resizing/moving drawings) are still missing.

## Planned features
* Selecting strokes, moving and transforming selections, etc.
* Floating figures (so drawings can exist on top of text rather than as blocks that text floats around).
* More Markdown rendering, e.g. actually formatting links.

