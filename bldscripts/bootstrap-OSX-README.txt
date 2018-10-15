README-OSX
==========
In order to compile g7ctrl there are a number of dependencies that must
be satisfied. The best way is to first install "brew" or as it is more
affectionally known "the missing package manager for OS X". For instructions
for how to do this see https://brew.sh/.

Recreating the build environment
--------------------------------
We adhere to the principle that no generated files are stored in the repo. This
means that if the repo is cloned then it is necessary to create a new build
environment using the autoconf/automake toolchain as a first step. In contrast
the distributed tar-ball have such an environment setup so that end users only
have to type the usual configure && make && make install.

Installing dependencies
-----------------------
Once "brew" is installed the core dependencies can be installed using the following
commands:

brew install autoconf
brew install automake
brew install libharu
brew install readline
brew install pcre
brew install sqlite
brew install libusb
brew install xz

Note: the "xz" dependency is ony necessary to make a full release.

To re-build the documentation (written in an XML dialect called
Docbook) the following additional libraries and utilities must be installed

brew cask install java
brew install libxslt
brew install docbook-xsl
brew install fop
brew install gawk
brew install cloc


You will also need to add/set the following environment variables (e.g. in your
.bashrc or .bash_profile)

export XML_CATALOG_FILES="/usr/local/etc/xml/catalog"
export LC_CTYPE=en_US.UTF-8

Creating the build environment
------------------------------
Once this is done you should be able to create the build environment with
the following commands

autoreconf --install --symlink

the is also a convenient build script "bldscript/basic_bootstrap.sh" to do this.

Building the source
-------------------
Once the build environment is created the package can be rebuilt using the
usual:

configure && make -j4

A convenient way to do the same thing with some useful additional options is
to use the build script "bldscript/stdbuild.sh"

<EOF>
