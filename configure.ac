#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([OpenTTS], [0.1], [opentts-dev@lists.opentts.org])
AM_INIT_AUTOMAKE([foreign])
LT_PREREQ([2.2])
LT_INIT([dlopen])
AC_CONFIG_SRCDIR([src/audio/alsa.c])
AC_CONFIG_HEADERS([config.h])

# libtool version numbers for the API:
#
# The steps below come from the libtool manual and describe how to
# change these numbers.
# It is an algorithm, so follow all of the steps.
#
# 1. Only change these numbers as the last step before a new public release.
# 2. If the library source has changed since the last public release,
# increment revision.
# 3. If any interfaces have been added, removed or changed since the
# last public release, increment current and set revision to 0.
# 4. If any interfaces have been added since the last public release,
# increment age.
# 5. If any interfaces have been removed since the last public release,
# set age to 0.
#
LIBOPENTTS_CURRENT=0
LIBOPENTTS_REVISION=0
LIBOPENTTS_AGE=0
AC_SUBST(LIBOPENTTS_CURRENT)
AC_SUBST(LIBOPENTTS_REVISION)
AC_SUBST(LIBOPENTTS_AGE)

# command line arguments
AC_ARG_ENABLE([python],
	[AS_HELP_STRING([--disable-python], [do not install python bindings])],
	[],
	[enable_python=check])

AC_ARG_WITH([alsa],
	[AS_HELP_STRING([--with-alsa], [include ALSA support])],
	[],
	[with_alsa=check])

AC_ARG_WITH([libao],
	[AS_HELP_STRING([--with-libao], [include libao support])],
	[],
	[with_libao=check])

AC_ARG_WITH([nas],
	[AS_HELP_STRING([--with-nas], [include network audio system (nas) support])],
	[],
	[with_nas=check])

AC_ARG_WITH([oss],
	[AS_HELP_STRING([--with-oss], [include oss support])],
	[],
	[with_oss=check])

AC_ARG_WITH([pulse],
	[AS_HELP_STRING([--with-pulse], [include pulseaudio support])],
	[],
	[with_pulse=check])

AC_ARG_WITH([cicero],
	[AS_HELP_STRING([--with-cicero], [include cicero support])],
	[],
	[with_cicero=no])

AC_ARG_WITH([dummy],
	[AS_HELP_STRING([--without-dummy], [do not include dummy synthesizer support])],
	[],
	[with_dummy=yes])

AC_ARG_WITH([espeak],
	[AS_HELP_STRING([--with-espeak], [include espeak support])],
	[],
	[with_espeak=check])

AC_ARG_WITH([festival],
	[AS_HELP_STRING([--with-festival], [include festival support])],
	[],
	[with_festival=no])

AC_ARG_WITH([flite],
	[AS_HELP_STRING([--with-flite], [include Festival Lite (Flite) support])],
	[],
	[with_flite=check])

AC_ARG_WITH([generic],
	[AS_HELP_STRING([--without-generic], [do not include generic synthesizer support])],
	[],
	[with_generic=yes])

AC_ARG_WITH([ibmtts],
	[AS_HELP_STRING([--with-ibmtts], [include IBM TTS support])],
	[],
	[with_ibmtts=check])

AC_ARG_WITH([ivona],
	[AS_HELP_STRING([--with-ivona], [include ivona support])],
	[],
	[with_ivona=check])

AC_ARG_WITH([pico],
	[AS_HELP_STRING([--with-pico], [include pico SVOX support])],
	[],
	[with_pico=check])

AC_ARG_WITH([opentts-user],
	[AS_HELP_STRING([--with-opentts-user], [user for OpenTTS system service (opentts)])],
	[opentts_user="$withval"],
	[opentts_user=opentts])
AC_SUBST(opentts_user)

AC_ARG_WITH([opentts-group],
	[AS_HELP_STRING([--with-opentts-group], [group for OpenTTS system service (opentts)])],
	[opentts_group="$withval"],
	[opentts_group=opentts])
AC_SUBST(opentts_group)

AC_ARG_WITH([system-log-path],
	[AS_HELP_STRING([--with-system-log-path],[path for logs when running as system service (/var/log/opentts)])],
	[system_log_path="$withval"],
	[system_log_path=/var/log/opentts])
AC_SUBST(system_log_path)

AC_ARG_WITH([system-pid-path],
	[AS_HELP_STRING([--with-system-pid-path],[path of pid file for system service (/var/run)])],
	[system_pid_path="$withval"],
	[system_pid_path=/var/run])
AC_SUBST(system_pid_path)

AC_ARG_WITH([system-socket-path],
	[AS_HELP_STRING([--with-system-socket-path],[path to socket for system service (/var/run/openttsd)])],
	[system_socket_path="$withval"],
	[system_socket_path=/var/run/openttsd])
AC_SUBST(system_socket_path)

#
# Setup GETTEXT
#
GETTEXT_PACKAGE=opentts
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Gettext package])
AM_GLIB_GNU_GETTEXT
IT_PROG_INTLTOOL([0.40.0])

# checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h langinfo.h limits.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/ioctl.h sys/socket.h sys/time.h unistd.h wchar.h wctype.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2 gethostbyname gettimeofday memmove memset mkdir select socket strcasecmp strchr strcspn strdup strerror strncasecmp strndup strstr strcasestr strtol])

# Checks for libraries.
AC_SEARCH_LIBS([sqrt], [m], [],
	[AC_MSG_FAILURE([Math library missing])])
AC_SEARCH_LIBS([pthread_create], [pthread], [],
	[AC_MSG_FAILURE([Threads library missing])])
AC_SEARCH_LIBS([lt_dlopen], [ltdl], [],
	[AC_MSG_FAILURE([ltdl library missing])])

PKG_CHECK_MODULES([GLIB], [glib-2.0])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES([GTHREAD], [gthread-2.0])
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

PKG_CHECK_MODULES([GMODULE], [gmodule-2.0])
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

PKG_CHECK_MODULES([DOTCONF], [dotconf])
AC_SUBST(DOTCONF_CFLAGS)
AC_SUBST(DOTCONF_LIBS)

# Check for libsndfile.
PKG_CHECK_MODULES([SNDFILE], [sndfile >= 1.0.2], [ac_cv_sndfile=1], [ac_cv_sndfile=0])

AC_DEFINE_UNQUOTED([HAVE_SNDFILE],${ac_cv_sndfile},
        [Set to 1 if you have libsndfile.])

AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)

# Python support
# Python 2.4 or greater should be acceptable.
AS_IF([test $enable_python != "no"],
	[AM_PATH_PYTHON([2.4],
		[enable_python=yes],
		[AS_IF([test $enable_python = "yes"],
		[AC_MSG_FAILURE([python is not available])])])])
AM_CONDITIONAL([HAVE_PYTHON], [test "$enable_python" = 'yes'])

# Audio subsystem part
AUDIO_DLOPEN_MODULES="-dlopen force"

AS_IF([test $with_alsa != "no"],
	[PKG_CHECK_MODULES([ALSA], [alsa],
		[with_alsa=yes;
		AUDIO_DLOPEN_MODULES="${AUDIO_DLOPEN_MODULES} -dlopen ../audio/otts_alsa.la"],
		[AS_IF([test $with_alsa = "yes"],
			[AC_MSG_FAILURE([ALSA support not available on this system])])])])

AC_SUBST(ALSA_CFLAGS)
AC_SUBST(ALSA_LIBS)
AM_CONDITIONAL(alsa_support, test $with_alsa = "yes")

AS_IF([test $with_libao != "no"],
	[PKG_CHECK_MODULES([LIBAO], [ao],
		[with_libao=yes;
		AUDIO_DLOPEN_MODULES="${AUDIO_DLOPEN_MODULES} -dlopen ../audio/otts_libao.la"],
		[AS_IF([test $with_libao = "yes"],
			[AC_MSG_FAILURE([libao is not available])])])])

AC_SUBST(LIBAO_CFLAGS)
AC_SUBST(LIBAO_LIBS)
AM_CONDITIONAL(libao_support, test $with_libao = "yes")

AS_IF([test $with_nas != "no"],
	[AC_CHECK_LIB([audio],
		[AuOpenServer],
		[with_nas=yes;
		NAS_LIBS="-L/usr/X11R6/lib -lXau -laudio";
		AUDIO_DLOPEN_MODULES="${AUDIO_DLOPEN_MODULES} -dlopen ../audio/otts_nas.la"],
		[AS_IF([test $with_nas = "yes"],
			[AC_MSG_FAILURE([nas is not available])])],
		[-L/usr/X11R6/lib -lXau])])

AC_SUBST(NAS_LIBS)
AM_CONDITIONAL(nas_support, test $with_nas = "yes")

AS_IF([test $with_oss != "no"],
	[AC_CHECK_HEADER([sys/soundcard.h],
		[with_oss=yes;
		AUDIO_DLOPEN_MODULES="${AUDIO_DLOPEN_MODULES} -dlopen ../audio/otts_oss.la"],
		[AS_IF([test $with_oss = "yes"],
			[AC_MSG_FAILURE([oss is not available])])])])

AM_CONDITIONAL(oss_support, test $with_oss = "yes")

AS_IF([test $with_pulse != "no"],
	[PKG_CHECK_MODULES([PULSE], [libpulse-simple],
		[with_pulse=yes;
		AUDIO_DLOPEN_MODULES="${AUDIO_DLOPEN_MODULES} -dlopen ../audio/otts_pulse.la"],
		[AS_IF([test $with_pulse = "yes"],
			[AC_MSG_FAILURE([pulseaudio is not available on this system])])])])

AC_SUBST(PULSE_CFLAGS)
AC_SUBST(PULSE_LIBS)
AM_CONDITIONAL(pulse_support, test $with_pulse = "yes")

AC_SUBST(AUDIO_DLOPEN_MODULES)

AS_IF([test "$AUDIO_DLOPEN_MODULES" = "-dlopen force"],
	[AC_MSG_WARN([openttsd is being built without audio support])])

# speech synthesizers

# check for cicero support
AM_CONDITIONAL(cicero_support, test $with_cicero = "yes")

# check for dummy support
AM_CONDITIONAL(dummy_support, test $with_dummy = "yes")

# check for espeak support
#
# Additional library check added by Willie Walker to account
# for how the SunStudio CC compiler works.
#
if test "$GCC" != yes; then
	EXTRA_ESPEAK_LIBS="-lCstd -lCrun"
fi
AC_SUBST(EXTRA_ESPEAK_LIBS)

AS_IF([test $with_espeak != "no"],
	[AC_CHECK_LIB([espeak],
		[espeak_Synth],
		[with_espeak=yes],
		[AS_IF([test $with_espeak = "yes"],
			[AC_MSG_FAILURE([Espeak is not available])])]
		[$EXTRA_ESPEAK_LIBS])])

AM_CONDITIONAL(espeak_support, test $with_espeak = "yes")

# check for festival support
AM_CONDITIONAL(festival_support, test $with_festival = "yes")

AS_IF([test $with_flite != "no"],
	[AC_CHECK_LIB([flite],
		[flite_init],
		[flite_basic="-lflite"],
		[AS_IF([test $with_flite = "yes"],
			[AC_MSG_FAILURE([libflite is not available])])],
		[-lm])
	AC_CHECK_LIB([flite_usenglish],
		[usenglish_init],
		[flite_basic="-lflite_usenglish ${flite_basic}"],
		[AS_IF([test $with_flite = "yes"],
			[AC_MSG_FAILURE([libflite_usenglish is not available])])],
		[-lflite -lm])
	AC_CHECK_LIB([flite_cmulex],
		[cmu_lex_init],
		[flite_basic="-lflite_cmulex ${flite_basic}"],
		[AS_IF([test $with_flite = "yes"],
			[AC_MSG_FAILURE([libflite_cmulex is not available])])],
		[-lflite -lm])
	AC_CHECK_LIB([flite_cmu_us_kal16],
		[register_cmu_us_kal],
		[with_flite=yes;
		flite_kal="-lflite_cmu_us_kal16"],
		[AC_CHECK_LIB([flite_cmu_us_kal],
			[register_cmu_us_kal],
			[with_flite=yes;
			flite_kal="-lflite_cmu_us_kal"],
			[AS_IF([test $with_flite = "yes"],
				[AC_MSG_FAILURE([no flite voices found])])],
		[-lflite_cmulex -lflite_usenglish -lflite -lm])],
	[-lflite_cmulex -lflite_usenglish -lflite -lm])])

AC_SUBST(flite_basic)
AC_SUBST(flite_kal)
AM_CONDITIONAL(flite_support, test $with_flite = "yes")

# check for generic support
AM_CONDITIONAL(generic_support, test $with_generic = "yes")

# Check for IBM TTS
AS_IF([test $with_ibmtts != "no"],
	[AC_CHECK_LIB([ibmeci],
		[eciStop],
		[with_ibmtts=yes;
		ibmtts_include="-I/opt/IBM/ibmtts/inc/"],
		[AS_IF([test $with_ibmtts = "yes"],
			[AC_MSG_FAILURE([IBMTTS is not available])])])])

AC_SUBST(ibmtts_include)
AM_CONDITIONAL(ibmtts_support, test $with_ibmtts = "yes")

# check for dumbtts support
AS_IF([test $with_ivona != "no"],
	[AC_CHECK_LIB([dumbtts],
		[dumbtts_TTSInit],
		[with_ivona=yes],
		[AS_IF([test $with_ivona = "yes"],
			[AC_MSG_FAILURE([ivona is not available])])])])

AM_CONDITIONAL(ivona_support, test $with_ivona = "yes")

# check for pico SVOX support
AS_IF([test $with_pico != "no"],
	[AC_CHECK_LIB([ttspico],
		[pico_initialize],
		[with_pico=yes],
		[AS_IF([test $with_pico = "yes"],
			[AC_MSG_FAILURE([pico SVOX is not available])])])])

AM_CONDITIONAL(pico_support, test $with_pico = "yes")

AC_SUBST(datadir)

spdconfdir='${sysconfdir}/opentts'
AC_SUBST(spdconfdir)
moduleconfdir='${spdconfdir}/modules' 
AC_SUBST(moduleconfdir)
clientconfdir='${spdconfdir}/clients'
AC_SUBST(clientconfdir)

spdconforigdir='${datadir}/opentts/conf'
AC_SUBST(spdconforigdir)
moduleconforigdir='${spdconforigdir}/modules'
AC_SUBST(moduleconforigdir)
clientconforigdir='${spdconforigdir}/clients'
AC_SUBST(clientconforigdir)
spddesktopconforigdir='${spdconforigdir}/desktop'
AC_SUBST(spddesktopconforigdir)

snddatadir='${datadir}/sounds/opentts'
AC_SUBST(snddatadir)
pvtlibdir='${libdir}/opentts/libs'
AC_SUBST(pvtlibdir)
modulebindir='${libdir}/opentts/modules'
AC_SUBST(modulebindir)
audiodir='${libdir}/opentts/audio'
AC_SUBST(audiodir)
localedir=${datadir}/locale
AC_SUBST(localedir)

# Extra libraries for sockets added by Willie Walker
# based upon how SunStudio libraries work.  Also
# conditionally set other compiler/linker flags.
#
if test "$GCC" = yes; then
    EXTRA_SOCKET_LIBS=""
    ERROR_CFLAGS="-Wall"
    RPATH="-rpath"
    RDYNAMIC="-rdynamic"
else
    EXTRA_SOCKET_LIBS="-lsocket -lnsl"
    ERROR_CFLAGS="-errwarn=%all -errtags=yes -erroff=E_STATEMENT_NOT_REACHED"
    RPATH="-R"
    RDYNAMIC=""
fi
AC_SUBST(EXTRA_SOCKET_LIBS)
AC_SUBST(ERROR_CFLAGS)
AC_SUBST(RPATH)
AC_SUBST(RDYNAMIC)

AC_CONFIG_FILES([Makefile
                 opentts.pc
                 config/Makefile
                 config/clients/Makefile
                 config/modules/Makefile
                 data/Makefile
                 data/desktop/Makefile
                 data/modules/Makefile
                 doc/Makefile
                 include/Makefile
                 po/Makefile.in
                 src/Makefile
                 src/api/Makefile
                 src/api/c/Makefile
                 src/api/python/Makefile
                 src/api/python/opentts/Makefile
                 src/api/python/opentts_config/Makefile
                 src/audio/Makefile
                 src/clients/Makefile
                 src/clients/say/Makefile
                 src/clients/send/Makefile
                 src/common/Makefile
                 src/modules/Makefile
                 src/server/Makefile
                 src/tests/Makefile])
AC_OUTPUT

