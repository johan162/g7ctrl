# $Id$ 
# spec file for package g7ctrl
# Copyright 2014 Johan Persson <johan162@gmail.com> 


BuildRequires:  glibc-devel libiniparser-devel libxml2-devel pcre-devel libxslt docbook-xsl-stylesheets readline-devel sqlite3-devel
PreReq:         pwdutils coreutils xz /usr/sbin/useradd /usr/sbin/groupadd
Summary:        GM7 GPS Tracker controlling daemon
Name:           g7ctrl
Version:        1.0.0
Release:        1
License:        GPL-3.0
Group:          Productivity/Networking/Other
BuildRoot:      %{_tmppath}/%{name}-%{version}-build  
AutoReqProv:    on 
URL:            http://sourceforge.net/projects/tvpvrd
Distribution:	OpenSuSE
#Source0: https://downloads.sourceforge.net/project/g7ctrl/%{name}-%{version}.tar.xz
Source: %{name}-%{version}.tar.xz
BuildRequires: systemd
%{?systemd_requires}

%description
GM7 Is a multi function GPS tracker with an extremly good battery life (several months). It is perfect to use a theft amd tracking alarm for vehicles. It does howevre not come with any managing SW what so ever but everythign is handled by commands over a virtual serial line (or simple commands as text messages from a phone)

This daemon allows both configuration and setup of the tracker over a USB cable (and soon GPRS) with an extended command set which is much easier to use than the native raw commands. In addition tha daemon can act as a tracking server that receives the location updates from the device and takes user specified action upon certain events.

The daemon has a small footprint, is easy to install and comes with a command line shell as main interface. It is known to run on several GNU Linux distribution and is verified on OpenSuSE 12.3 and Mint 16. There is also experimental support for OSX.

Authors:
Johan Persson <johan162@gmail.com>

#---------------------------------------------------------------------------------
# PREP
%prep
%setup -q
%configure --with-systemdsystemunitdir=%{_unitdir}

# ---------------------------------------------------------------------------------
# BUILD 
# configure and build. The configure macro will automatically set the
# correct prefix and sysconfdir directories the smp macro will make sure the
# build is parallelized
%build
make %{?_smp_mflags}

#---------------------------------------------------------------------------------
# INSTALL
%install
%makeinstall

#---------------------------------------------------------------------------------
# CLEAN
%clean
%__rm -rf %{buildroot}

#---------------------------------------------------------------------------------
# PRE
%pre
%service_add_pre %{name}.service

#---------------------------------------------------------------------------------
# PREUN
%preun
%stop_on_removal %{name}
%service_del_preun %{name}.service

#---------------------------------------------------------------------------------
# POST
%post  
/usr/sbin/groupadd -r %{name} 2> /dev/null || : 
/usr/sbin/useradd -r -g %{name} -s /bin/false -c "GM7 tracker daemon user" -G dialout %{name} 2> /dev/null || : 
%service_add_post %{name}.service

#---------------------------------------------------------------------------------
# POSTUN
%postun
%insserv_cleanup
%restart_on_update %{name}
%service_del_postun %{name}.service

#---------------------------------------------------------------------------------
# FILES
%files
%defattr(-,root,root) 
%{_bindir}/*
%{_initrddir}/%{name}
%config %{_sysconfdir}/%{name}
%config %{_sysconfdir}/init.d/%{name}
%doc %attr(0444,root,root) %{_mandir}/man1/*
%doc COPYING AUTHORS README NEWS 
%doc docs/manpages/*.html
%doc docs/manpages/*.pdf
%doc docs/manual/out/html
%doc docs/manual/out/pdf
%{_unitdir}/%{name}.service

#---------------------------------------------------------------------------------
# CHANGELOG
%changelog
* Sat Jan 11 2014 Johan Persson <johan162@gmail.com> 
- Created spec file
