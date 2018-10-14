README-OSX

In order to compile g7ctrl there are a number of dependencies that must
be satisfied. The best way is to first install "brew" or as it is more
affectionally known "the missing package manager for OS X". For instructions
for how to do this see https://brew.sh/.

Once "brew" is installed pls execute the following installs in order to compile the
core daemon.

brew install autoconf
brew install automake
brew install libharu
brew install readline
brew install pcre
brew install sqlite
brew install libusb

If you also want to re-build the documentation (written in an XML dialect called
Docbook) the following additional ibraries and utilities must be installed

brew install libxslt
brew install docbook-xsl
brew cask install java
brew install fop
brew install gawk
brew install cloc

You will also need to add/set the following environment variables (e.g. in your
.bashrc or .bash_profile)

export XML_CATALOG_FILES="/usr/local/etc/xml/catalog"
export LC_CTYPE=en_US.UTF-8

Once this is done you should be able to rebuild the build environment with
the following command

autoreconf --install --symlink

It is then possible to do the standard

configure && make

A convenient way to do the same thing with some useful additional options is
to use the buildscript

bldscript/stdbuild.sh
