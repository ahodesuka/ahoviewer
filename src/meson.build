conf = configuration_data()
conf.set_quoted('AHOVIEWER_DATADIR', datadir)
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_URL', project_url)
conf.set_quoted('PACKAGE_BUGREPORT', project_bugreport)

deps = [
  threads, glibmm, sigcpp, gtkmm, libconfig, libxml, curl,
  gstreamer, gstaudio, gstvideo, libsecret, libunrar, libzip, libnsgif
]
incdirs = [ ]

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
if libsecret.found()
  conf.set('HAVE_LIBSECRET', 1)
endif
if libunrar.found()
  conf.set('HAVE_LIBUNRAR', 1)
  if libunrar_dll_hpp
    conf.set('HAVE_LIBUNRAR_DLL_HPP', 1)
  elif unrar_dll_hpp
    conf.set('HAVE_UNRAR_DLL_HPP', 1)
  endif
endif
if libzip.found()
  conf.set('HAVE_LIBZIP', 1)
endif

configure_file(
  output: 'config.h',
  configuration: conf
)

vcs_tag(
  command: ['git', 'describe', '--dirty'],
  input: 'version.h.in',
  output: 'version.h'
)

gnome = import('gnome')

resources = gnome.compile_resources(
    'resources', 'ahoviewer.gresource.xml'
)

sources = [
  resources,
  'archive/archive.cc',
  'archive/image.cc',
  'archive/rar.cc',
  'archive/zip.cc',
  'booru/browser.cc',
  'booru/curler.cc',
  'booru/image.cc',
  'booru/imagefetcher.cc',
  'booru/imagelist.cc',
  'booru/page.cc',
  'booru/site.cc',
  'booru/tagentry.cc',
  'booru/tagview.cc',
  'application.cc',
  'image.cc',
  'imagebox.cc',
  'imagelist.cc',
  'keybindingeditor.cc',
  'main.cc',
  'mainwindow.cc',
  'preferences.cc',
  'settings.cc',
  'siteeditor.cc',
  'statusbar.cc',
  'thumbnailbar.cc',
  'version.cc'
]

if not libnsgif.found()
  sources += [
    '../ext/libnsgif/libnsgif.c',
    '../ext/libnsgif/lzw.c'
  ]
  incdirs += include_directories('../ext/libnsgif')
endif

executable(
  meson.project_name(),
  sources,
  dependencies : deps,
  include_directories : incdirs,
  gui_app : true,
  install : true
)
