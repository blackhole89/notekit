# NoteKit
This program is a structured notetaking application based on GTK+ 3. Write your notes in instantly-formatted Markdown, organise them in a tree of folders that can be instantly navigated from within the program, and add hand-drawn notes by mouse, touchscreen or digitiser.

![Screenshot](/screenshots/notekit.png?raw=true)

We have a [Discord server](https://discord.gg/WVas9aX6Ee) for questions and discussing the project's development.

## Why?

I figured it would be nice to have a free-software, platform-independent OneNote. While there is a remarkable number of free (speech or beer) notetaking applications out there, to my best knowledge, none of them simultaneously check the following boxes:

* note organisation
* text as a first-class object
* formatting
* simple, standard on-disk format
* tablet input

## How to install

![automated build](https://github.com/blackhole89/notekit/workflows/automated%20build/badge.svg)

You can download the following binary builds:

* [Linux x86_64](http://twilightro.kafuka.org/%7Eblackhole89/files/notekit-20210403.tar.gz) (Git version of 2021-04-03), or download the possibly unstable [automated build](https://github.com/blackhole89/notekit/releases).
* [Windows x86_64](http://twilightro.kafuka.org/~blackhole89/files/notekit-20210422.zip) (Git version of 2020-04-22). The Windows version is less tested and incapable of remembering which folders are opened due to mingw-w64's lack of support for xattrs.

Moreover, there is also

* a [Launchpad PPA](https://launchpad.net/~msoloviev/+archive/ubuntu/notekit) with a DEB package for Ubuntu 18.04 and many other modern Debian-family systems;
* a [Fedora COPR repository](https://copr.fedorainfracloud.org/coprs/lyessaadi/notekit/) (thanks to @LyesSaadi); 
* an [OpenSUSE Build Service package](https://software.opensuse.org/download.html?project=home%3Asp1rit&package=notekit), and
* an [Arch User Repository](https://aur.archlinux.org/packages/notekit-clatexmath-git/) (both thanks to @sp1ritCS).

To run the binary, you will in addition require at least the following packages: `libgtkmm-3.0-1v5 libgtksourceviewmm-3.0-0v5 libjsoncpp1 zlib1g libxml2`, where the version of `libgtkmm-3.0-1v5` is at least 3.20. (In particular, this means that Ubuntu 16.04 LTS (xenial) and derived distributions are too old.) If the binary does not work for you, it is recommended that you build from source, as described below.

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

### Note management

* To create a new note, doubleclick a `+` node in the tree view and enter a name.
* To create a new **folder**, doubleclick a `+` node in the tree view and enter a name ending in `/`, e.g. `new folder/`.
* Notes will be sorted alphabetically. 
* You can move notes and whole folders between folders by dragging and dropping.
* Files are saved automatically when the window is closed, when a different file is opened, and after a timeout when no user action is performed.

### Markdown

* Some markdown features are unsupported as a stylistic choice or because of parser limitations. If you are feeling adventurous, you can adjust the markdown parser by editing the GtkSourceView language definition in `sourceview/markdown.lang`.
* Add LaTeX math using single `$` signs, e.g. `$\int x dx$`.
* Some markdown will be hidden ("rendered") unless your cursor is next to it.

### Pen input

* Drawings can currently only be deleted whole. (This will be fixed eventually.)
* To edit the default colour palette, right-click any of the colour buttons on the right-hand toolbar.
* Due to present limitations, drawing clears the undo stack.
* When copypasting text into other applications, drawings will be automatically converted into data URL PNGs.

### Syntax highlighting

![Screenshot](/screenshots/notekit-syntax_highlighting.png?raw=true)

### Misc

* The program loads a custom Gtk+ stylesheet found in `data/stylesheet.css`. Clear it if parts of the UI look wonky.
* If something unexpected happens, it is often useful to run the program from a terminal and look at stdout.

## Project status
Late alpha. Creating and editing notes and drawing works well enough, but many basic quality-of-life features (such as resizing/moving drawings) are still missing.

## Planned features
* Selecting strokes, moving and transforming selections, etc.
* Floating figures (so drawings can exist on top of text rather than as blocks that text floats around).
* More Markdown rendering, e.g. actually formatting links.

