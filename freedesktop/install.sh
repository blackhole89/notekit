#!/bin/bash
if [ "$EUID" -ne 0 ]; then
	echo "This script wont work without superuser privileges"
	exit
fi

install -Dm644 com.github.blackhole89.notekit.png -t /usr/share/icons/hicolor/128x128/apps/
install -Dm644 com.github.blackhole89.notekit.svg -t /usr/share/icons/hicolor/scalable/apps/
install -Dm644 com.github.blackhole89.notekit.desktop -t /usr/share/applications

gtk-update-icon-cache -t /usr/share/icons/hicolor
update-desktop-database
