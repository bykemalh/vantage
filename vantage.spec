Name:           LenovoVantage
Version:        1.0
Release:        1%{?dist}
Summary:        Lenovo Vantage

License:        GPLv3+
URL:            http://bykemalh.me
Source0:        Lenovo-Vantage-1.0.tar.gz

BuildRequires:  gtk4-devel
Requires:       gtk4
BuildRequires:  adwaita-icon-theme
Requires:       adwaita-icon-theme


%description
Lenovo Vantage for Linux

%prep
%autosetup

%build
%meson
%ninja_build

%install
%ninja_install

%files
%license LICENSE
%doc README.md
/usr/bin/myapp
/usr/share/applications/myapp.desktop
/usr/share/icons/hicolor/32x32/apps/icon.png

%changelog
* Mon Aug 12 2024 Your Name <you@example.com> - 1.0-1
- Initial package
