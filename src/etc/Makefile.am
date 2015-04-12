# ===============================================================================
# $Id: Makefile.am 939 2015-04-08 06:34:27Z ljp $
# Automake build script to $(INSTALL) system management shutdown/startup scripts
#
# ===============================================================================

SUBDIRS = .
EXTRA_DIST = deb.init.d.in suse.init.d.in deb.init.conf.in g7ctrl.service.in README g7ctrl.conf.template.in event-scripts presets logrotate.g7ctrl

presetsdir=${prefix}/share/@PACKAGE@/presets

presets_DATA=presets/alarm_off.preset.template presets/gprs_off.preset presets/gtrack_on.preset.template \
presets/rec_on.preset.template presets/alarm_on.preset.template presets/gprs_on.preset.template \
presets/rec_off.preset.template presets/sleep_wm.preset.template

scriptsdir=${prefix}/share/@PACKAGE@/event_scripts

scripts_DATA=event-scripts/0_action.sh.template event-scripts/1_action.sh.template event-scripts/34_action.sh.template \
event-scripts/40_action.sh.template event-scripts/4_action.sh.template event-scripts/100_action.sh.template \
event-scripts/2_action.sh.template event-scripts/37_action.sh.template event-scripts/48_action.sh.template \
event-scripts/tracker_conn.sh.template

confdir=${sysconfdir}/g7ctrl
conf_DATA=g7ctrl.conf.template

if have_logrotate
logrotatedir=$(sysconfdir)/logrotate.d
logrotate_DATA=logrotate.g7ctrl
endif

if have_debian_initd

if have_upstart_init

install-data-local:
	[ -d ${DESTDIR}$(sysconfdir)/init.d ] || $(INSTALL) -d -m 755 ${DESTDIR}$(sysconfdir)/init.d
	[ -d ${DESTDIR}$(sysconfdir)/init ] || $(INSTALL) -d -m 755 ${DESTDIR}$(sysconfdir)/init
	$(INSTALL) -m 644  deb.init.conf ${DESTDIR}$(sysconfdir)/init/g7ctrl.conf
	$(LN_S) -f /lib/init/upstart-job ${DESTDIR}$(sysconfdir)/init.d/g7ctrl

uninstall-local:
	rm -rf ${DESTDIR}$(sysconfdir)/init.d/g7ctrl
	rm -rf ${DESTDIR}$(sysconfdir)/init/g7ctrl.conf

else

install-data-local:
	[ -d ${DESTDIR}$(sysconfdir)/init.d ] || $(INSTALL) -d -m 755 ${DESTDIR}$(sysconfdir)/init.d
	$(INSTALL) -m 644  deb.init.d ${DESTDIR}$(sysconfdir)/init.d/g7ctrl

uninstall-local:
	rm -rf ${DESTDIR}$(sysconfdir)/init.d/g7ctrl

endif

else

install-data-local:
	[ -d ${DESTDIR}$(sysconfdir)/init.d ] || $(INSTALL) -d -m 755 ${DESTDIR}$(sysconfdir)/init.d
	$(INSTALL) -m 755  suse.init.d ${DESTDIR}$(sysconfdir)/init.d/g7ctrl

uninstall-local:
	rm -rf ${DESTDIR}$(sysconfdir)/init.d/g7ctrl

endif

# Also install scripts for systemd to let user afterwards select how to manage daemon
if HAVE_SYSTEMD
    systemdsystemunit_DATA = g7ctrl.service
endif
