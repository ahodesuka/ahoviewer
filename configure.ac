AC_PREREQ([2.69])
AC_INIT([ahoviewer], m4_esyscmd_s([sh version.sh]),
        [https://github.com/ahodesuka/ahoviewer/issues],
        [ahoviewer],
        [https://github.com/ahodesuka/ahoviewer])
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([src/main.cc])
AC_CONFIG_HEADER([src/config.h])
AM_SILENT_RULES([yes])

AC_SUBST(LIBCURL_CPPFLAGS)
AC_SUBST(LIBCURL)
AC_SUBST(gstreamer_CFLAGS)
AC_SUBST(gstreamer_LIBS)
AC_SUBST(libsecret_CFLAGS)
AC_SUBST(libsecret_LIBS)
AC_SUBST(libunrar_LIBS)
AC_SUBST(libzip_CFLAGS)
AC_SUBST(libzip_LIBS)

AC_PROG_CXX
AC_PROG_INSTALL

AC_LANG([C++])
AX_CXX_COMPILE_STDCXX(14, [noext])

# Make sure glibmm is up to date for Glib::Threads::Thread::create
PKG_CHECK_MODULES([glibmm], [glibmm-2.4 >= 2.36.0])

PKG_CHECK_MODULES([gtkmm], [gtkmm-2.4 >= 2.20.0])

PKG_CHECK_MODULES([libconfig], [libconfig++ >= 1.4])

LIBCURL_CHECK_CONFIG(, 7.32.0,, [AC_MSG_ERROR([libcurl >= 7.32.0 not found])])

PKG_CHECK_MODULES([libxml2], [libxml-2.0])

# Windows specific
case "$target" in
    *-*-cygwin* | *-*-mingw*)
        CXXFLAGS+=" -mwindows"
        AM_CONDITIONAL([WINDOWS], [true])
        windows="true"
    ;;
    *)
        AM_CONDITIONAL([WINDOWS], [false])
    ;;
esac

if test x"$windows" != xtrue; then
    AC_ARG_ENABLE([gst],
        AS_HELP_STRING([--enable-gst],
            [enable webm support through GStreamer @<:@default=auto@:>@]
        ),
        [enable_gst=$enableval],
        [enable_gst=auto]
    )

    if test x"$enable_gst" != xno; then
            PKG_CHECK_MODULES([gstreamer], [gstreamer-1.0 gstreamer-video-1.0 gstreamer-plugins-base-1.0],
                [
                    AC_DEFINE(HAVE_GSTREAMER, 1, [Define to have webm support through GStreamer])
                ],
                [
                    if test x"$enable_gst" = xyes; then
                        AC_MSG_ERROR([gstreamer not found, but --enable-gst=yes])
                    fi
                ]
            )
    fi
fi

AC_ARG_ENABLE([libsecret],
    AS_HELP_STRING([--enable-libsecret],
        [enable libsecret support @<:@default=auto@:>@]
    ),
    [enable_libsecret=$enableval],
    [enable_libsecret=auto]
)

if test x"$enable_libsecret" != xno; then
    PKG_CHECK_MODULES([libsecret], [libsecret-1],
        [
            AC_DEFINE(HAVE_LIBSECRET, 1, [Define to have libsecret support])
        ],
        [
            if test x"$enable_libsecret" = xyes; then
                AC_MSG_ERROR([libsecret not found, but --enable-libsecret=yes])
            fi
        ]
    )
fi

AC_ARG_ENABLE([rar],
    AS_HELP_STRING([--enable-rar],
        [enable rar archive support @<:@default=auto@:>@]
    ),
    [enable_rar=$enableval],
    [enable_rar=auto]
)

if test x"$enable_rar" != xno; then
    AC_CHECK_HEADERS([unrar/dll.hpp libunrar/dll.hpp], [break], [],
        [
        #if defined(__linux__) || defined(_APPLE) || defined(__MINGW32__)
          #define _UNIX
        #endif
        ]
    )
    if test x"$ac_cv_header_unrar_dll_hpp" != xyes -a x"$ac_cv_header_libunrar_dll_hpp" != xyes
    then
        if test x"$enable_rar" = xyes
        then
            AC_MSG_ERROR([libunrar's dll.hpp not found, but --enable-rar=yes])
        fi
    else
        AC_CHECK_LIB([unrar], [RARGetDllVersion],
            [
                AC_DEFINE(HAVE_LIBUNRAR, 1, [Define to have rar archive support])
                libunrar_LIBS="-lunrar"
            ],
            [
                if test x"$enable_rar" = xyes; then
                    AC_MSG_ERROR([libunrar not found, but --enable-rar=yes])
                fi
            ]
        )
    fi
fi

AC_ARG_ENABLE([zip],
    AS_HELP_STRING([--enable-zip],
        [enable zip archive support @<:@default=auto@:>@]
    ),
    [enable_zip=$enableval],
    [enable_zip=auto]
)

if test x"$enable_zip" != xno; then
    PKG_CHECK_MODULES([libzip], [libzip],
        [
            AC_DEFINE(HAVE_LIBZIP, 1, [Define to have zip archive support])
        ],
        [
            if test x"$enable_zip" = xyes; then
                AC_MSG_ERROR([libzip not found, but --enable-zip=yes])
            fi
        ]
    )
fi

AC_CONFIG_FILES([
     Makefile
     pixmaps/Makefile
     po/Makefile
     src/Makefile
])
AC_OUTPUT
