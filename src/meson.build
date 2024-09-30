gnome = import('gnome')

conf = configuration_data()
conf.set_quoted('AHOVIEWER_DATADIR', datadir)
conf.set_quoted('AHOVIEWER_PREFIX', prefix)
conf.set_quoted('AHOVIEWER_VERSION', '@VCS_TAG@')
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_URL', project_url)
conf.set_quoted('PACKAGE_BUGREPORT', project_bugreport)

ahoviewer_cpp_args = [ '-Wno-attributes' ]

deps = [
  threads, glibmm, sigcpp, gtkmm, libconfig, libxml, curl,
  gstreamer, gstaudio, gstvideo, libpeas, libsecret, libunrar, libzip, libnsgif,
]
incdirs = [ ]
sources = [ ]

incdirs += include_directories('../ext/entities')
incdirs += include_directories('../ext/date/include')
incdirs += include_directories('../ext/json')

if host_machine.system() != 'windows'
  ahoviewer_cpp_args += [
    '-DHAS_REMOTE_API=1',
  ]
endif
ahoviewer_cpp_args += [
  '-DHAS_UNCAUGHT_EXCEPTIONS=1',
]

if curl_openssl and openssl.found()
  conf.set('USE_OPENSSL', 1)
  deps += openssl
endif

if curl_gnutls and libgcrypt.found()
  conf.set('USE_GNUTLS', 1)
  deps += libgcrypt
endif

if gstreamer.found()
  conf.set('HAVE_GSTREAMER', 1)
endif

if libpeas.found()
  conf.set('HAVE_LIBPEAS', 1)

  plugin_sources = [
    'plugin/siteactivatable.c',
    'plugin/siteactivatable.h',
    'plugin/windowabstract.c',
    'plugin/windowabstract.h',
  ]

  # Dummy executable that gir needs
  if host_machine.system() == 'windows'
    gir_dummy = executable(
      'gir_dummy',
      sources : [
        plugin_sources,
        'plugin/gir-dummy.c',
      ],
      c_args : [ '-DGIR_DUMMY' ],
      dependencies : [ libpeas, gtk ],
      link_args: [ '-Wl,--export-all-symbols' ],
      install : false,
    )
  else
    gir_dummy = executable(
      'gir_dummy',
      sources : [
        plugin_sources,
        'plugin/gir-dummy.c',
      ],
      c_args : [ '-DGIR_DUMMY' ],
      dependencies : [ libpeas, gtk ],
      install : false,
    )
  endif

  ahoviewer_gir = gnome.generate_gir(
    gir_dummy,
    sources : [
      plugin_sources,
      'plugin/gir-dummy.c',
    ],
    namespace : 'Ahoviewer',
    nsversion : '1.0',
    includes : [
      'GObject-2.0',
      'Gio-2.0',
      'Gtk-3.0',
    ],
    symbol_prefix : 'ahoviewer',
    identifier_prefix : 'Ahoviewer',
    install : true,
    extra_args: [
      '--warn-all',
    ],
  )

  sources += [
    ahoviewer_gir,
    'plugin/siteactivatable.c',
    'plugin/windowabstract.c',
    'plugin/manager.cc',
    'plugin/siteplugin.cc',
    'plugin/windowplugin.cc',
    'plugin/mainwindowinterface.cc',
  ]
endif

if libsecret.found()
  conf.set('HAVE_LIBSECRET', 1)
endif

if libunrar_found
  conf.set('HAVE_LIBUNRAR', 1)
  incdirs += include_directories(unrar_incdir)
endif

if libzip.found()
  conf.set('HAVE_LIBZIP', 1)
endif

config_h_in = configure_file(
  output : 'config.h.in',
  configuration : conf,
)

config_h = declare_dependency(
  sources: vcs_tag(
    command : ['git', 'describe', '--dirty'],
    input : config_h_in,
    output : 'config.h',
    fallback : meson.project_version(),
  )
)
deps += config_h

resources = gnome.compile_resources(
  'resources', 'ahoviewer.gresource.xml'
)

if host_machine.system() == 'windows'
  ahoviewer_rc = configure_file(
      input : 'ahoviewer.rc.in',
      output : 'ahoviewer.rc',
      configuration : version_data,
    )
  resources += import('windows').compile_resources(ahoviewer_rc)
endif

sources += [
  resources,
  '../ext/date/src/tz.cpp',
  '../ext/entities/entities.c',
  'archive/archive.cc',
  'archive/image.cc',
  'archive/rar.cc',
  'archive/zip.cc',
  'booru/browser.cc',
  'booru/curler.cc',
  'booru/image.cc',
  'booru/imagefetcher.cc',
  'booru/imagelist.cc',
  'booru/infobox.cc',
  'booru/page.cc',
  'booru/site.cc',
  'booru/tagentry.cc',
  'booru/tagview.cc',
  'application.cc',
  'image.cc',
  'imagebox.cc',
  'imageboxnote.cc',
  'imagelist.cc',
  'keybindingeditor.cc',
  'main.cc',
  'mainwindow.cc',
  'preferences.cc',
  'recentmenu.cc',
  'settings.cc',
  'siteeditor.cc',
  'statusbar.cc',
  'thumbnailbar.cc',
  'util.cc',
  'videobox.cc',
]

if not libnsgif.found()
  sources += [
    '../ext/libnsgif/gif.c',
    '../ext/libnsgif/lzw.c',
  ]
  incdirs += include_directories('../ext/libnsgif')
endif

subsystem = 'console'
if get_option('buildtype').startswith('release')
  subsystem = 'windows'
endif

ahoviewer = executable(
  meson.project_name(),
  sources,
  cpp_args : ahoviewer_cpp_args,
  dependencies : deps,
  include_directories : incdirs,
  win_subsystem : subsystem,
  install : true,
)
