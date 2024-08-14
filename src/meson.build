lenovo_vantage_sources = [
  'main.c',
  'lenovo-vantage-application.c',
  'lenovo-vantage-window.c',
]

lenovo_vantage_deps = [
  dependency('gtk4'),
  dependency('libadwaita-1', version: '>= 1.4'),
  dependency('x11'),
  dependency('gio-2.0'),
]

lenovo_vantage_sources += gnome.compile_resources('lenovo-vantage-resources',
  'lenovo-vantage.gresource.xml',
  c_name: 'lenovo_vantage'
)

executable('lenovo-vantage', lenovo_vantage_sources,
  dependencies: lenovo_vantage_deps,
  install: true,
)