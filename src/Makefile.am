bin_PROGRAMS = ahoviewer

CLEANFILES = resources.c version.h

BUILT_SOURCES = version

ahoviewer_SOURCES =       \
	archive/archive.cc    \
	archive/image.cc      \
	archive/rar.cc        \
	archive/zip.cc        \
	booru/browser.cc      \
	booru/curler.cc       \
	booru/image.cc        \
	booru/imagefetcher.cc \
	booru/imagelist.cc    \
	booru/page.cc         \
	booru/site.cc         \
	booru/tagentry.cc     \
	booru/tagview.cc      \
	application.cc        \
	image.cc              \
	imagebox.cc           \
	imagelist.cc          \
	keybindingeditor.cc   \
	main.cc               \
	mainwindow.cc         \
	preferences.cc        \
	resources.c           \
	settings.cc           \
	siteeditor.cc         \
	statusbar.cc          \
	thumbnailbar.cc       \
	version.cc

if WINDOWS
ahoviewer_SOURCES += ahoviewer.rc
SUFFIXES: .rc
.rc.o:
	$(target)-windres $^ -o $@
endif
resources.c: ahoviewer.gresource.xml ui/ahoviewer.ui
	@glib-compile-resources --target=$@ --generate-source $<

if !SYSTEM_LIBNSGIF
ahoviewer_SOURCES += \
	../ext/libnsgif/libnsgif.c \
	../ext/libnsgif/lzw.c
endif

PHONY: version

version:
	@cd .. && ./version.sh > /dev/null 2>&1

ahoviewer_CFLAGS = @CFLAGS@ @glib_CFLAGS@
ahoviewer_CPPFLAGS = @CPPFLAGS@ @LIBCURL_CPPFLAGS@ -DAHOVIEWER_DATADIR=\"$(datadir)\"
ahoviewer_CXXFLAGS = @CXXFLAGS@ @gtkmm_CFLAGS@ @libconfig_CFLAGS@ @libxml2_CFLAGS@ \
				@gstreamer_CFLAGS@ @libsecret_CFLAGS@ @libzip_CFLAGS@ \
				@OpenSSL_CFLAGS@ @GnuTLS_CFLAGS@ @libnsgif_CFLAGS@
ahoviewer_LDADD = @LIBS@ @gtkmm_LIBS@ @libconfig_LIBS@ @LIBCURL@ @libxml2_LIBS@ \
				@gstreamer_LIBS@ @libsecret_LIBS@ @libunrar_LIBS@ @libzip_LIBS@ \
				@OpenSSL_LIBS@ @GnuTLS_LIBS@ @libnsgif_LIBS@
