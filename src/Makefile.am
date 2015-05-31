bin_PROGRAMS = ahoviewer
noinst_PROGRAMS = file2string

CLEANFILES = ui.cc

file2string_SOURCES = file2string.cc

file2string_LDADD = @CXXFLAGS@

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
	main.cc               \
	mainwindow.cc         \
	settings.cc           \
	statusbar.cc          \
	thumbnailbar.cc       \
	ui.cc

ui.cc: ui.glade file2string
	@./file2string $< $@ ahoviewer_ui

ahoviewer_CPPFLAGS = @CPPFLAGS@ @LIBCURL_CPPFLAGS@
ahoviewer_CXXFLAGS = @CXXFLAGS@ @gtkmm_CFLAGS@ @libzip_CFLAGS@
ahoviewer_LDADD = @LIBS@ @gtkmm_LIBS@ @libconfig_LIBS@ @LIBCURL@ \
				@pugixml_LIBS@ @libunrar_LIBS@ @libzip_LIBS@
