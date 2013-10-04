Synge Installation
==================
This file describes the process of installing synge on several different platforms.

### Linux ###
You'll need the following dependencies (usually found in your distro's repos):
* [libgmp](http://gmplib.org/#DOWNLOAD)>=5.1.2
* [libmpfr](http://www.mpfr.org/mpfr-current/#download)>=3.1.2
* [gtk+2.0](http://www.gtk.org/download)>=2.24 (and all dependencies)
* [python3](http://www.python.org/download/)>=3.3.2

Download the code, compile, test and install:
```
git clone https://github.com/cyphar/synge.git
cd synge
make
make test
sudo make install
```

### Windows ###
There is a pre-built "installer" that has been written for windows. It pulls the latest release
executables and dependencies from a dropbox share. This is the prefered method for users.

#### Pre-built ####
This method requires both an internet connection and administrative privileges.
* Download the zip'd [repository](https://github.com/cyphar/synge/archive/master.zip)
* Unzip it, and navigate to `win/`
* Run `windows.bat`, and press `i` (or run `windows.bat -i`)
* Synge is now installed in `C:\Program Files\Synge`

#### Compiling ####
In order to compile Synge, you need a working [mingw](http://www.mingw.org/) toolchain.
* Install the [gtk+2.0 dev packages](http://www.gtk.org/download/win32.php) to your `mingw/bin` directory
* Install the [libgmp](http://sourceforge.net/projects/mingw/files/MinGW/Base/gmp/) ...
* ... and [libmpfr](http://sourceforge.net/projects/mingw/files/MinGW/Base/mpfr/) dlls to the `mingw/bin` directory
* Install [python 3](http://www.python.org/download/)
* Download code from [github](https://github.com/cyphar/synge)

Compile and test code:
```
cd synge
make
make test
```

#### Non-dropbox Solution ####
If the dropbox share ever goes down, you'll need these things:

* [gtk+2.0 windows installer](http://sourceforge.net/projects/gtk-win/)
* [libmpfr](http://sourceforge.net/projects/mingw/files/MinGW/Base/mpfr/) and [libgmp](http://sourceforge.net/projects/mingw/files/MinGW/Base/gmp/) windows ports

Install all of the above to a single `/bin` location, and ensure that your `%PATH%` contains this `/bin` location.

### OS X ###
Unfortunately, there is currently no easy, simple and documented way to compile, test and install Synge on OS X.

#### For the ambitious ####
You need the following:

* XCode Developer Tools (with the gcc/clang command line tools).
* [libgmp](http://gmplib.org/#DOWNLOAD)>=5.1.2 source
* [libmpfr](http://www.mpfr.org/mpfr-current/#download)>=3.1.2 source
* [python3](http://www.python.org/download/)>=3.3.2 (should be pre-installed)

You first need to compile libgmp, libmpfr (in that order). These commands will produce **huge** amounts of errors (because OS X is very bad when it comes to compilation), but it *should* still work:
```
# GMP
tar xvfz libgmp.tar.gz
cd libgmp
./configure && make && sudo make install

# MPFR
tar xvfz libmpfr.tar.gz
cd libmpfr
./configure && make && sudo make install

# Synge
cd synge
make
make test
sudo make install
```

The above instructions are far from comprehensive. While Synge has been successfully compiled on OS X and passed all of the tests, it has not been fully documented.
In addition, since GTK doesn't work on OS X, you can only really use the CLI version.

### Hurd ###
Really? Since Synge isn't GPL, you probably don't want to use it anyway. But the instructions are the same as Linux.
