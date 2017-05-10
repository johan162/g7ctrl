#
# spec file for package vsftpd
#
# Copyright (c) 2013 SUSE LINUX Products GmbH, Nuernberg, Germany.
#
# All modifications and additions to the file contributed by third parties
# remain the property of their copyright owners, unless otherwise agreed
# upon. The license for this file, and modifications and additions to the
# file, is the same license as for the pristine package itself (unless the
# license for the pristine package is not an Open Source License, in which
# case the license is the MIT License). An "Open Source License" is a
# license that conforms to the Open Source Definition (Version 1.9)
# published by the Open Source Initiative.

# Please submit bugfixes or comments via http://bugs.opensuse.org/
#


Name:           vsftpd
BuildRequires:  gpg-offline
BuildRequires:  openssl-devel
BuildRequires:  pam-devel
%if 0%{?suse_version} < 1001
BuildRequires:  libcap
%else
BuildRequires:  libcap-devel
%endif
%if 0%{?suse_version} > 1140
BuildRequires:  systemd
%endif
Version:        3.0.2
Release:        0
Summary:        Very Secure FTP Daemon - Written from Scratch
License:        GPL-2.0+
Group:          Productivity/Networking/Ftp/Servers
Url:            https://security.appspot.com/vsftpd.html
Source0:        https://security.appspot.com/downloads/%{name}-%{version}.tar.gz
Source1:        %name.pam
Source2:        %name.logrotate
Source3:        %name.init
Source4:        README.SUSE
Source5:        %name.xml
Source6:        %name.firewall
Source7:        vsftpd.service
Source9:        %name.keyring
Source1000:     https://security.appspot.com/downloads/%{name}-%{version}.tar.gz.asc
Patch1:         vsftpd-2.0.4-lib64.diff
Patch3:         vsftpd-2.0.4-xinetd.diff
Patch4:         vsftpd-2.0.4-enable-ssl.patch
Patch5:         vsftpd-2.0.4-dmapi.patch
Patch6:         vsftpd-2.0.5-vuser.patch
Patch7:         vsftpd-2.0.5-enable-debuginfo.patch
Patch8:         vsftpd-2.0.5-utf8-log-names.patch
Patch9:         vsftpd-2.3.5-conf.patch
Patch10:        vsftpd-3.0.0_gnu_source_defines.patch
Patch11:        vsftpd-3.0.0-optional-seccomp.patch
Patch12:        vsftpd-allow-dev-log-socket.patch
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Provides:       ftp-server
PreReq:         %insserv_prereq /usr/sbin/useradd
%{?systemd_requires}
Requires:       logrotate

%description
Vsftpd is an FTP server, or daemon. The "vs" stands for Very Secure.
Obviously this is not a guarantee, but the entire codebase was written
with security in mind, and carefully designed to be resilient to
attack.

Recent evidence suggests that vsftpd is also extremely fast (and this
is before any explicit performance tuning!). In tests against wu-ftpd,
vsftpd was always faster, supporting over twice as many users in some
tests.

%prep
%gpg_verify %{S:1000}
%setup -q
%patch1
%patch3
%patch4
%patch5
%patch6
%patch7
%patch8
%patch9
%patch10 -p1
%patch11 -p1
%patch12 -p1

%build
%define seccomp_opts %{nil}
%if 0%{?suse_version} > 1030
%define seccomp_opts -D_GNU_SOURCE -DUSE_SECCOMP
%endif
rm -f dummyinc/sys/capability.h
make CFLAGS="$RPM_OPT_FLAGS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -fPIE -fstack-protector --param=ssp-buffer-size=4 %{seccomp_opts}" \
     LDFLAGS="-fPIE -pie -Wl,-z,relro -Wl,-z,now" LINK=

%install
mkdir -p $RPM_BUILD_ROOT/usr/share/empty
cp %SOURCE4 .
install -D -m 755 %name  $RPM_BUILD_ROOT/usr/sbin/%name
install -D -m 600 %name.conf $RPM_BUILD_ROOT/etc/%name.conf
install -D -m 600 xinetd.d/%name $RPM_BUILD_ROOT/etc/xinetd.d/%name
install -D -m 644 $RPM_SOURCE_DIR/%name.pam $RPM_BUILD_ROOT/etc/pam.d/%name
install -D -m 644 $RPM_SOURCE_DIR/%name.logrotate $RPM_BUILD_ROOT/etc/logrotate.d/%name
install -D -m 644 %name.conf.5 $RPM_BUILD_ROOT/%_mandir/man5/%name.conf.5
install -D -m 644 %name.8 $RPM_BUILD_ROOT/%_mandir/man8/%name.8
install -D -m 755 %SOURCE3 $RPM_BUILD_ROOT/etc/init.d/%name
ln -sf ../../etc/init.d/%name $RPM_BUILD_ROOT/%_prefix/sbin/rc%name
install -d $RPM_BUILD_ROOT/%_datadir/omc/svcinfo.d/
install -D -m 644 %SOURCE5 $RPM_BUILD_ROOT/%_datadir/omc/svcinfo.d/
install -d $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/SuSEfirewall2.d/services/
install -m 644 %{S:6} $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/SuSEfirewall2.d/services/%{name}
%if 0%{?suse_version} > 1140
install -D -m 0644 %SOURCE7 %{buildroot}/%{_unitdir}/%{name}.service
%endif

%pre
/usr/sbin/useradd -r -g nogroup -s /bin/false -c "Secure FTP User" -d /var/lib/empty ftpsecure 2> /dev/null || :
%if 0%{?suse_version} > 1140
%service_add_pre %{name}.service
%endif

%preun
%stop_on_removal %name
%if 0%{?suse_version} > 1140
%service_del_preun %{name}.service
%endif

%post
%{fillup_and_insserv -f %{name}}
%if 0%{?suse_version} > 1140
%service_add_post %{name}.service
%endif

%postun
%insserv_cleanup
%restart_on_update %name
%if 0%{?suse_version} > 1140
%service_del_postun %{name}.service
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%if 0%{?suse_version} > 1140
%{_unitdir}/%{name}.service
%endif
/usr/sbin/%name
/usr/sbin/rc%name
%config /etc/init.d/%name
%_datadir/omc/svcinfo.d/vsftpd.xml
%dir /usr/share/empty
%config(noreplace) /etc/xinetd.d/%name
%config(noreplace) /etc/%name.conf
%config /etc/pam.d/%name
%config(noreplace) /etc/logrotate.d/%name
%_mandir/man5/%name.conf.*
%_mandir/man8/%name.*
%doc BUGS AUDIT Changelog LICENSE README README.security
%doc REWARD SPEED TODO SECURITY TUNING SIZE FAQ EXAMPLE COPYING
%doc README.SUSE
%config %{_sysconfdir}/sysconfig/SuSEfirewall2.d/services/%{name}

%changelog
