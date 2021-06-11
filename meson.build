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
  cpp = meson.get_compiler('cpp')
  libunrar_found = false
  libunrar = cpp.find_library('libunrar', required : get_option('libunrar'))
  search_paths = [ '/usr/include/unrar', '/usr/include/libunrar',
                   '/usr/include/x86_64-linux-gnu/unrar', '/usr/include/x86_64-linux-gnu/libunrar',
                   '/usr/local/include/unrar', '/usr/local/include/libunrar']
  # Can't easily access MINGW_PREFIX enviornment variable from meson so just search everywhere
  if host_machine.system() == 'windows'
    search_paths = [ '/mingw64/include/unrar', '/mingw32/include/unrar',
                     '/clang64/include/unrar', '/clang32/include/unrar',
                     '/ucrt64/include/unrar' ] + search_paths
  endif
  # Prepend manually set path
  if get_option('libunrar-path') != ''
    search_paths = [ get_option('libunrar-path') ] + search_paths
  endif
  if libunrar.found()
    foreach idir : search_paths
      inc_arg = '-I' + idir
      if cpp.has_header('dll.hpp', args: inc_arg)
        unrar_incdir = idir
        libunrar_found = true
        break
      endif
    endforeach
  endif
  if not (libunrar_found and libunrar.found()) and get_option('libunrar').enabled()
    error('libunrar enabled but was not found')
  endif
endif
libzip = dependency('libzip', required : get_option('libzip'))

# If not found on system the version found in ext directory will be built into
# ahoviewer
libnsgif = dependency('libnsgif', required : false)

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))

if host_machine.system() == 'windows'
  version_array = meson.project_version().split('.')
  version_major = version_array[0].to_int()
  version_minor = version_array[1].to_int()
  if version_array[2].contains('-')
    version_patch = version_array[2].split('-')[0].to_int()
  else
    version_patch = version_array[2].to_int()
  endif

  version_data = configuration_data()
  version_data.set('VERSION_MAJOR', version_major)
  version_data.set('VERSION_MINOR', version_minor)
  version_data.set('VERSION_PATCH', version_patch)

  version_data.set_quoted('VERSION', version_major.to_string() + '.' + version_minor.to_string() + '.' + version_patch.to_string())
  version_data.set_quoted('HOST_MACHINE_ARCH', host_machine.cpu_family() == 'x86' ? 'win32' : 'win64')
  version_data.set_quoted('PACKAGE', meson.project_name())
  installer_nsi = configure_file(
    input : 'installer.nsi.in',
    output : 'installer.nsi',
    configuration : version_data,
  )
endif

subdir('data')
subdir('po')
subdir('src')
