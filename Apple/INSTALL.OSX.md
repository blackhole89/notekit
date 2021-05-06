![Screenshot on OS X](/screenshots/osx-notekit-example.png?raw=true)

# OS X - NoteKit install instructions

To install notekit on OS X, you need to have a working installation of [Homebrew](https://brew.sh).

If you don't have homebrew installed, you can do so by pasting `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"` into a shell promt.

Once you have homebrew, you can install NoteKit by typing
```sh
brew install --HEAD sp1ritCS/tap/notekit
```

if you want to launch NoteKit from your deskop (if you don't know what this means, you probably want to) you have to additionally type
```sh
ln -s /usr/local/opt/notekit/NoteKit.app /Applications/
```

---

Please keep in mind, that the OS X version is less tested, because we have no iMac to test NoteKit on. If something breaks just create an [Issue](https://github.com/blackhole89/notekit/issues/new) and ping @sp1ritCS.
