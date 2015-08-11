bin_PROGRAMS = ahoviewer
noinst_PROGRAMS = file2string

CLEANFILES = ui.cc version.h

file2string_SOURCES = file2string.cc

ahoviewer_SOURCES =       \
	archive/archive.cc    \
	archive/image.cc      \
	archive/rar.cc        \
	archive/zip.cc        \
	booru/browser.cc      \
	booru/curler.cc       \
	booru/image.cc        \
	booru/imagefetcher.cc \
	booru/page.cc         \
	booru/site.cc         \
	booru/tagentry.cc     \
	booru/tagview.cc      \
	image.cc              \
	imagebox.cc           \
	imagelist.cc          \
	keybindingeditor.cc   \
	main.cc               \
	mainwindow.cc         \
	preferences.cc        \
	settings.cc           \
	siteeditor.cc         \
	statusbar.cc          \
	thumbnailbar.cc       \
	ui.cc                 \
	version.cc

ui.cc: ui.glade file2string
	@./file2string $< $@ ahoviewer_ui

PHONY: version.h

version.h:
	@cd .. && ./version.sh > /dev/null 2>&1

ahoviewer_CPPFLAGS = @CPPFLAGS@ @LIBCURL_CPPFLAGS@ -DDATADIR=\"$(datadir)\"
ahoviewer_CXXFLAGS = @CXXFLAGS@ @gtkmm_CFLAGS@ @gstreamer_CFLAGS@ @libzip_CFLAGS@
ahoviewer_LDADD = @LIBS@ @gtkmm_LIBS@ @libconfig_LIBS@ @LIBCURL@ \
				@pugixml_LIBS@ @gstreamer_LIBS@ @libunrar_LIBS@ @libzip_LIBS@
