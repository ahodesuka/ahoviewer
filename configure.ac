AC_PREREQ([2.69])
AC_INIT([ahoviewer], [0.1],
        [https://github.com/ahodesuka/ahoviewer/issues],
        [ahoviewer],
        [https://github.com/ahodesuka/ahoviewer])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/main.cc])
AC_CONFIG_HEADER([src/config.h])
AM_SILENT_RULES([yes])

AC_SUBST(gtkmm_CFLAGS)
AC_SUBST(gtkmm_LIBS)
AC_SUBST(LIBCURL_CPPFLAGS)
AC_SUBST(LIBCURL)
AC_SUBST(pugixml_LIBS)
AC_SUBST(libunrar_LIBS)
AC_SUBST(libzip_CFLAGS)
AC_SUBST(libzip_LIBS)

AC_PROG_CXX
AC_PROG_INSTALL

AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_11([noext])

# Make sure glibmm is up to date for Glib::Threads::Thread::create
PKG_CHECK_MODULES([glibmm], [glibmm-2.4 >= 2.36.0])

PKG_CHECK_MODULES([gtkmm], [gtkmm-2.4 >= 2.20.0])

PKG_CHECK_MODULES([libconfig], [libconfig++ >= 1.4])

LIBCURL_CHECK_CONFIG(, 7.16.0,, [AC_MSG_ERROR([libcurl >= 7.16.0 not found])])

AC_CHECK_HEADER([pugixml.hpp],, AC_MSG_ERROR([pugixml.hpp not found]))
AC_CHECK_LIB([pugixml], [main], [pugixml_LIBS="-lpugixml"],
    [AC_MSG_ERROR([pugixml not found])]
)

AC_ARG_ENABLE([rar],
    AS_HELP_STRING([--enable-rar],
        [enable rar archive support @<:@default=auto@:>@]
    ),
    [enable_rar=$enableval],
    [enable_rar=auto]
)

if test x$enable_rar != xno; then
    AC_CHECK_HEADER([libunrar/rar.hpp],,
        [
            if test x$enable_rar = xyes; then
                AC_MSG_ERROR([libunrar/rar.hpp not found, but --enable-rar=yes])
            fi
        ]
    )
    AC_CHECK_LIB([unrar], [RARGetDllVersion],
        [
            AC_DEFINE(HAVE_LIBUNRAR, 1, [Define to have rar archive support])
            libunrar_LIBS="-lunrar"
        ],
        [
            if test x$enable_rar = xyes; then
                AC_MSG_ERROR([libunrar not found, but --enable-rar=yes])
            fi
        ]
    )
fi

AC_ARG_ENABLE([zip],
    AS_HELP_STRING([--enable-zip],
        [enable zip archive support @<:@default=auto@:>@]
    ),
    [enable_zip=$enableval],
    [enable_zip=auto]
)

if test x$enable_zip != xno; then
    PKG_CHECK_MODULES([libzip], [libzip],
        [
            AC_DEFINE(HAVE_LIBZIP, 1, [Define to have zip archive support])
        ],
        [
            if test x$enable_zip = xyes; then
                AC_MSG_ERROR([libzip not found, but --enable-zip=yes])
            fi
        ]
    )
fi

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
