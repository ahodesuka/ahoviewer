project('ahoviewer', ['c', 'cpp'],
    version : run_command('sh', 'version.sh', '--').stdout().strip(),
    license : 'MIT',
    meson_version : '>= 0.47.0',
    default_options : ['cpp_std=c++17'])

project_url = 'https://github.com/ahodesuka/ahoviewer'
project_bugreport = 'https://github.com/ahodesuka/ahoviewer/issues'

#################################
###   Required dependencies   ###
#################################
threads = dependency('threads')
glibmm = dependency('glibmm-2.4', version : ['>=2.46.0'])
# >= 2.6.0 since we use lambdas with return values
sigcpp = dependency('sigc++-2.0', version : ['>=2.6.0'])
gtk = [ dependency('gtk+-3.0', version : ['>=3.22.0']) ]
gtkmm = [ dependency('gtkmm-3.0', version : ['>=3.22.0']) ]
if host_machine.system() != 'windows' and not get_option('gstreamer').disabled()
  gtkmm += dependency('x11')
endif
libconfig = dependency('libconfig++', version : ['>=1.4'])
libxml = dependency('libxml-2.0')

# CURL_LOCK_DATA_CONNECT was implemented in 7.57, but had deadlocking issues that
# weren't fixed until 7.58
# 7.58 also added --ssl-backends to curl-config (this is used below)
curl = dependency('libcurl', version : '>= 7.58.0')

curl_ssl_backend = run_command(
  [get_option('curl-config'), '--ssl-backends']).stdout().strip().to_lower()

curl_openssl = false
curl_gnutls = false

if curl_ssl_backend.contains('openssl')
  curl_openssl = true
  # Check if we need to setup thread locks for older OpenSSL
  openssl = dependency('openssl', version : ['<1.1.0'], required : false)
  # USE_OPENSSL is defined in src/meson.build if above is found
endif
if curl_ssl_backend.contains('gnutls')
  curl_gnutls = true
  # Older versions of libgcrypt require some lock setup to be thread safe
  libgcrypt = dependency('libgcrypt', version : ['<1.6.0'], required : false)
  # USE_GNUTLS is defined in src/meson.build if above is found
endif

#################################
###   Optional dependencies   ###
#################################
gstreamer = dependency('gstreamer-1.0', required : get_option('gstreamer'))
gstaudio = dependency('gstreamer-audio-1.0', required : get_option('gstreamer'))
# GstVideoOverlay (rendering directly to the ahoviewer window)
gstvideo = dependency('gstreamer-video-1.0', required : get_option('gstreamer'))

# Plugin support
libpeas = dependency('libpeas-1.0', version : ['>=1.22.0'], required : get_option('libpeas'))

# Used to securely store passowrds/api keys
libsecret = dependency('libsecret-1', required : get_option('libsecret'))

libunrar = dependency('libunrar', required : false)
# libunrar doesn't package a .pc file so we need to find it manually
# worse yet not all distros put the header into the same directory
if not libunrar.found() and not get_option('libunrar').disabled()
  libunrar_dll_hpp = false
  unrar_dll_hpp = false
  cpp = meson.get_compiler('cpp')
  libunrar = cpp.find_library('libunrar', required : get_option('libunrar'))
  if cpp.has_header('dll.hpp', args : '-I/usr/include/libunrar')
    libunrar_dll_hpp = true
  elif cpp.has_header('dll.hpp', args : '-I/usr/include/unrar')
    unrar_dll_hpp = true
  endif
  if not libunrar_dll_hpp and not unrar_dll_hpp and get_option('libunrar').enabled()
    error('libunrar enabled but was not found')
  endif
endif
libzip = dependency('libzip', required : get_option('libzip'))

# If not found on system the version found in ext directory will be built into
# ahoviewer
libnsgif = dependency('libnsgif', required : false)

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))

subdir('data')
subdir('po')
subdir('src')
