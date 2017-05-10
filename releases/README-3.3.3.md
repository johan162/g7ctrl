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
