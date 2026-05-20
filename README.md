# RetroShare 0.6 Git Plugin

This is a Git plugin

# Dependencies 

### Install package dependencies:

#### Debian / Ubuntu / Linux Mint
	sudo apt-get install libgit2

#### Windows
	pacman -S mingw-w64-x86_64-libgit2


# build & install:

put/clone `RetroGit` to `RetroShare/plugins/` recommend

	cd ${YOUR_DIR}/RetroShare/plugins/RetroGit/
	mkdir build
	cd build
	qmake ..
	make 

Copy your RetroGit.dll to "Data/extensions6" (Windows)
Then restart your RetroShare. You'll see a git logo in you main tool-bar.

# Usage:

