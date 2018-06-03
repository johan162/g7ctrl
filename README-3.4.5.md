v3.4.5 (mar 2018)
=================

General notes for this release
------------------------------
This is purely a build environment updated to stay current with evolving
tool chains and improved Debian packaging. No user visible changes.


Detailed user visible changes
-----------------------------
- None


Bugs fixed
----------
- None


Internal changes (visible for developers and packagers)
-------------------------------------------------------
- Lockfile is now created directly under /var/run to better harmonize with 
  debian standard init files
- Some improvements in automation when building Debian package
- Remove deprecated link targets (syslog) in g7ctrl.service
- Enable all runlevels in init.d/ to comply with Debian packaging guidline
- Remove all existing SuSE specific scripts since they are not tested


v3.4.2 - 3.4.4
===============
Internally consumed versions. Not released.


v3.4.1 (feb 2017)
=================

General notes for this release
------------------------------
Maintenance release and build environment update. The primary build environment
now uses gcc v5.4.0 (was 4.8.4) as well as fop v2.1 (was 1.1). The base for the
build server is now LTS release Ubunti 16.04 LTS (flavor Linux Mint 18.1 Serena)

The environment update has forced some minor code updates (primarily some
instrumentation to shut down false positive warning for use of C99 variable
length arrays on the stack (VLA). The move to fop 2.1 requires a configuration
file to set the proper base URI for inclusion of external objects (e.g. images in
the manual).

Adds some more information on cache usage in mail templates for tracking updates.
Some further minor internal changes only visible for developers.


Detailed user visible changes
-----------------------------
- Added cache reporting in the mail template


Bugs fixed
----------
- Reported absolute fill of cache was wrong after cache wraparound


Internal changes (visible for developers and packagers)
-------------------------------------------------------
- Avoid warning from clang compiler with pragma when -Wgnu-folding-constant is enabled
- Added '>>>' prefix for debug messages to make them more easy to spot in the log
- Added "fop.cfg" for use with fop invocation.
- Minor update to the build configuration script


v3.4.0 (may 2016)
=================

General notes for this release
------------------------------
Minor maintenance release. Since the configuration file was updated with two
new settings to adjust the cache size the version was bumped to a new .dot release.
The focus of this release is primarily the geolocation cache handling with new
features and one important bug fix. Statistics is now also kept when the
daemon is restarted.


Detailed user visible changes
-----------------------------
- Geolocation cache size is now adjustable in the configuration file by two new
  settings.
- Cache statistics will now be saved on exit and re-read upon start of the daemon.
- Minor visual adjustments of the PDF report layout
- Geolocation cache for addresses now creates an automatic backup of previously
  saved caches.
- Some more information on cache size and current usage is now shown in
  the ".cachestat" server command.


Bugs fixed
----------
- Fixed a potential overflow if more cache locations was saved than we
  can read back at startup if the cache size has been lowered.
- The geolocation cache was not saved properly to file when terminating daemon
  when the cache index had wrapped around.


Internal changes (visible for developers and packagers)
-------------------------------------------------------
- Refactored the geo-location cache handling into its own module.
- Changed cache allocation from stack to heap
- the "mv_and_rename()" file function now accepts NULL argument for newname



v3.3.3 (dec 2015)
=================

General notes for this release
------------------------------
Happy X-mas! The last release for the year!

This release fixes one possible security issue. In the 3.x series it was not
possible to enable the password for command login via the config file. It had
no affect. A one-line check had been missing since the large refactoring in
the 3.0.0 release. Some minor changes in the report layout was also made.


Detailed user visible changes
-----------------------------
 - User password for command login can now be enabled again (regression in
   the 3.x series)
 - Some minor layout improvements in the PDF report.
 - Some minor update to mail template used for new connection


Bugs fixed
----------
 - Password could not be enabled for command connections. The setting in the
   config file were ignored.
 - The power saving section in the PDF report had an error in the status display
   for the wakeup report. It was also a bit too small.


Internal changes (visible for developers and packagers)
-------------------------------------------------------
 - Replaced some internal usage of unsafe string functions to corresponding
   safe versions in libxstr internal library.



v3.3.2 (nov 2015)
=================

General notes for this release
------------------------------

A minor maintenance release with some improvements in the handling of error
returns from the device and layout of the device report. In addition some internal
changes were made to make the build fully compatible with the latest OS X
release "El Capitan" (which felt the need to introduce some compatibility
breaks in the file system layout and permissions).

NOTE: Deprecation of support for OpenSuSE (and derivatives) based distributions.
As of this version no more active testing on those platforms will be made.
From 3.4.0 all OpenSuSE specific control scripts will be removed (init.d/ scripts)

From this release and onwards a Debian/Ubuntu package is available at the
PPA "ppa:johan162/g7ctrl"


Detailed user visible changes
-----------------------------
 - Give better error message when the "test" command indicates error.
 - Add command ".breport" (Basic-report) which is almost the same as ".report"
   but does not include the geo-fence events in the report. This is useful if the
   report is generated over GPRS since it saves a lot of time since the report
   does not have to question the state of all 50 possible geo-fence events.
 - Minor change in the report layout to improve human readability of the to/from date
   and time for stored locations.


Bugs fixed
----------
 - None (no reported bugs or remaining bugs in the backlog)


Internal changes (visible for developers and packagers)
-------------------------------------------------------
 - Retry up to a maximum of five times if we get a timeout from reading from
   the device. However, that rarely seems to help once the device is in a non
   responsive mode.
   If the device does not reply within a reasonable time no amount of re-reading
   seems to succeed in reading back data. Instead the only way seems to be to
   flush the serial buffer and re-issue the command.

 - Refactored rkey.[hc] to a proper dictionary API and renamed module to "dict"

 - Change the XSL stylesheet lookup to trust the current XML catalog instead of manually
   trying to locate the stylesheets in the config process.
   This was originally a workaround for faulty installations with wrong catalog setup.
   However, no more workarounds. If its broken you should fix your
   XML setup! This has the additional advantage of future proofing the Docbook build.

 - Minor updates to the bootstrap scripts to add the latest dependencies

 - Support for OpenSuSE and derivatives thereof is deprecated. This does not mean it
   doesn't work. It just means that no more official testing will be done on those system.
   In addition this means that no more RPM packages will be built. The reason has
   nothing to do with technical merits of the OpenSuSE platform but only as a mean
   to reduce the maintenance work. Starting with 3.4.0 all platform specific scripts
   (like init scripts to start the daemon) will also be removed since they will no
   longer be maintained.
   Version 3.4.0 and forward will only support Debian based derivatives (like Ubunto and
   Linux Mint)

 - Changed build options for GCC to include "--param=ssp-buffer-size=4 -Wstack-protector"
   which will decrease the default SSP limit of 8 down to 4 to make sure most functions
   using stack is protected.

 - Modify configuration process (configure.ac) so that an $prefix is not pre-prended
   to the DB directory in case the prefix is specified as either /usr or /usr/local
   Also fix a corner case when for bizarre setups the sysconfdir was not properly set.