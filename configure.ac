#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([OpenTTS], [0.0], [opentts-dev@lists.opentts.org])
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

# checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PATH_PROG([PYTHON],[python])

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
AC_CHECK_FUNCS([dup2 gethostbyname gettimeofday memmove memset mkdir select socket strcasecmp strchr strcspn strdup strerror strncasecmp strndup strstr strtol])

# Checks for libraries.
AC_CHECK_LIB(m,
        sqrt, echo "",
        echo "*** Required math library missing! See INSTALL .";exit 1)
AC_CHECK_LIB(dl,
        dlopen, echo "",
        echo "*** Required dl library missing! See INSTALL .";exit 1)
AC_CHECK_LIB(pthread,
        pthread_create, echo "",
        echo "*** Required pthread library missing! See INSTALL .";exit 1)

PKG_CHECK_MODULES(GLIB,glib-2.0,,exit)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GTHREAD,gthread-2.0,,exit)
AC_SUBST(GTHREAD_CFLAGS)
AC_SUBST(GTHREAD_LIBS)

PKG_CHECK_MODULES(GMODULE,gmodule-2.0,,exit)
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

PKG_CHECK_MODULES(DOTCONF,dotconf,,exit)
AC_SUBST(DOTCONF_CFLAGS)
AC_SUBST(DOTCONF_LIBS)

# Check for libsndfile.
PKG_CHECK_MODULES(SNDFILE, sndfile >= 1.0.2, ac_cv_sndfile=1, ac_cv_sndfile=0)

AC_DEFINE_UNQUOTED([HAVE_SNDFILE],${ac_cv_sndfile},
        [Set to 1 if you have libsndfile.])

AC_SUBST(SNDFILE_CFLAGS)
AC_SUBST(SNDFILE_LIBS)

# oss included by default, why actually?
STATIC_AUDIO_PLUGINS_LIST="oss"
SPD_AUDIO_LIBS=

AC_CHECK_LIB(audio,
        AuOpenServer,
	nas_aud="true";echo "Compiling with NAS support.";,
        nas_aud="false";echo "*** NAS Audio library missing. Compiling without NAS support! See INSTALL.";,
	-L/usr/X11R6/lib -lXau)

AC_ARG_WITH(nas, AS_HELP_STRING(--with-nas, Compile with Network Audio System (NAS) support),
		 if test $withval = "no"; then 
		    echo "Forced compilation without NAS support.";
		    nas_aud="false";
		 else 
		    if test $nas_aud = "false"; then
		       echo "NAS support is not available. Sorry."; exit 1;
		    fi
                 fi
 		 echo "")

AM_CONDITIONAL(nas_support, test $nas_aud = "true")
if test $nas_aud = "true"; then
        STATIC_AUDIO_PLUGINS_LIST="$STATIC_AUDIO_PLUGINS_LIST nas";
        SPD_AUDIO_LIBS="$SPD_AUDIO_LIBS -laudio -L/usr/X11R6/lib"
fi

AC_CHECK_LIB(ao,
        ao_play,
	libao_aud="true";echo "Compiling with libao support.";,
        libao_aud="false";echo "*** libao library missing. Compiling without libao support! See INSTALL.";)

AC_ARG_WITH(libao, AS_HELP_STRING(--with-libao, Compile with libao support),
		 if test $withval = "no"; then 
		    echo "Forced compilation without libao support.";
		    libao_aud="false";
		 else 
		    if test $libao_aud = "false"; then
		       echo "libao support is not available. Sorry."; exit 1;
		    fi
                 fi
 		 echo "")

AM_CONDITIONAL(libao_support, test $libao_aud = "true")
if test $libao_aud = "true"; then
        STATIC_AUDIO_PLUGINS_LIST="$STATIC_AUDIO_PLUGINS_LIST libao";
        SPD_AUDIO_LIBS="$SPD_AUDIO_LIBS -lao"
fi


AC_CHECK_LIB(asound,
        snd_pcm_open,
	alsa_aud="true";echo "Compiling with ALSA support.";,
        alsa_aud="false";echo "*** ALSA C API library missing. Compiling without ALSA support! See INSTALL.";)

AC_ARG_WITH(alsa, AS_HELP_STRING(--with-alsa, Compile with ALSA support),
		 if test $withval = "no"; then 
		    echo "Forced compilation without ALSA support.";
		    alsa_aud="false";
		 else 
		    if test $alsa_aud = "false"; then
		       echo "ALSA support is not available. Sorry."; exit 1;
		    fi
                 fi
 		 echo "")

AM_CONDITIONAL(alsa_support, test $alsa_aud = "true")
if test $alsa_aud = "true"; then
        STATIC_AUDIO_PLUGINS_LIST="$STATIC_AUDIO_PLUGINS_LIST alsa";
        SPD_AUDIO_LIBS="$SPD_AUDIO_LIBS -lasound"
fi

AC_CHECK_LIB(pulse,
        pa_threaded_mainloop_new,
	pulse_aud="true";echo "Compiling with PulseAudio support.";,
        pulse_aud="false";echo "*** PulseAudio library missing. Compiling without PulseAudio support! See INSTALL.";,
	-lpulse)

AC_ARG_WITH(pulse, AS_HELP_STRING(--with-pulse, Compile with PulseAudio support),
		 if test $withval = "no"; then 
		    echo "Forced compilation without PulseAudio support.";
		    pulse_aud="false";
		 else 
		    if test $pulse_aud = "false"; then
		       echo "PulseAudio support is not available. Sorry."; exit 1;
		    fi
                 fi
 		 echo "")

AM_CONDITIONAL(pulse_support, test $pulse_aud = "true")
if test $pulse_aud = "true"; then
        STATIC_AUDIO_PLUGINS_LIST="$STATIC_AUDIO_PLUGINS_LIST pulse";
        SPD_AUDIO_LIBS="$SPD_AUDIO_LIBS -lpulse-simple"
fi

for name in $STATIC_AUDIO_PLUGINS_LIST; do
        STATIC_AUDIO_PLUGINS_GET="$STATIC_AUDIO_PLUGINS_GET${name}_plugin_get,";
        STATIC_AUDIO_PLUGINS_EXTERN="${STATIC_AUDIO_PLUGINS_EXTERN} extern spd_audio_plugin_t const * ${name}_plugin_get (void); "
done

# used in src/audio/static_plugins.c.in
AC_SUBST(STATIC_AUDIO_PLUGINS_EXTERN)
AC_SUBST(STATIC_AUDIO_PLUGINS_GET)
AC_SUBST(SPD_AUDIO_LIBS)

# Check for Flite (currently required)
AC_CHECK_LIB(flite,
        flite_init, echo "",
        echo "",
	-lm)

AC_CHECK_LIB(flite_cmulex,
        cmu_lex_init, echo "",
        echo "",
	-lflite -lm)

AC_CHECK_LIB(flite_usenglish,
        usenglish_init, echo "",
        echo "",
	-lflite -lm)

flite_basic="-lflite_cmulex -lflite_usenglish -lflite -lm";

AC_CHECK_LIB(flite_cmu_us_kal16,
        register_cmu_us_kal, flite_ok="true";flite_kal="-lflite_cmu_us_kal16";echo "16-bit Kal Flite voice found. Flite support enabled.", 
        	AC_CHECK_LIB(flite_cmu_us_kal,	
        	register_cmu_us_kal, flite_ok="true";flite_kal="-lflite_cmu_us_kal";
		echo "8-bit Kal Flite voice found. Flite support enabled.", 
        	flite_ok="false"; echo "Flite libraries missing. Compiling without Flite support!",
		-lflite_cmulex -lflite_usenglish -lflite -lm),
	-lflite_cmulex -lflite_usenglish -lflite -lm)

AC_SUBST(flite_basic)
AC_SUBST(flite_kal)

AC_ARG_WITH(flite, AS_HELP_STRING(--with-flite, Compile with Festival Lite (Flite) support),
		 if test $withval = "no"; then 
		    echo "Forced compilation without Flite support."; 
		    flite_ok="false";
		 else 
		    if test $flite_ok = "false"; then
		       echo "Flite support is not available. Sorry."; exit 1;
		    fi
                 fi
 		 echo "")

AM_CONDITIONAL(flite_support, test $flite_ok = "true")

# Check for IBM TTS
AC_CHECK_LIB(ibmeci,
        eciStop,
        ibmtts_ok="true";ibmtts_include="-I/opt/IBM/ibmtts/inc/"; echo "Compiling with IBM TTS support.",
        ibmtts_ok="false";ibmtts_include=""; echo "*** IBM TTS ECI library missing. Compiling without IBM TTS support! See INSTALL.";,
	-libmeci)

AC_ARG_WITH(ibmtts, AS_HELP_STRING(--with-ibmtts, Compile with IBM TTS support),
		 if test $withval = "no"; then 
		    echo "Forced compilation without IBMTTS support."; 
		    ibmtts_ok="false";
		 else 
		    if test $ibmtts_ok = "false"; then
		       echo "IBM TTS support is not available. Sorry."; exit 1;
		    fi
                 fi
 		 echo "")

AM_CONDITIONAL(ibmtts_support, test $ibmtts_ok = "true")

AC_SUBST(ibmtts_include)

# Additional library check added by Willie Walker to account
# for how the SunStudio CC compiler works.
#
# check for espeak support
if test "$GCC" = yes; then
    EXTRA_ESPEAK_LIBS="-lpthread -lm"
else
    EXTRA_ESPEAK_LIBS="-lpthread -lm -lCstd -lCrun"
fi
AC_SUBST(EXTRA_ESPEAK_LIBS)

AC_CHECK_LIB(espeak,
espeak_Synth,
		espeak_ok="true"; echo "Compiling with espeak support.",
		espeak_ok="false"; echo "Espeak not found.",
		$EXTRA_ESPEAK_LIBS)

AC_ARG_WITH(espeak, AS_HELP_STRING(--with-espeak, Compile with espeak support),
		 if test $withval = "no"; then 
		    echo "Forced compilation without espeak support."; 
		    espeak_ok="false";
		 else 
		    if test $espeak_ok = "false"; then
		       echo "Espeak support is not available. Sorry."; exit 1;
		    fi
                 fi
 		 echo "")

AM_CONDITIONAL(espeak_support, test $espeak_ok = "true")

# check for dumbtts support
AC_CHECK_LIB(dumbtts,
	dumbtts_TTSInit,
	ivona_ok="true"; echo "Compiling with Ivona support.";,
	ivona_ok="false"; echo "Dumbtts library not found. Not installing Ivona.";,
	-ldumbtts)

AC_ARG_WITH(ivona, AS_HELP_STRING(--with-ivona, Compile with ivona support),
		 if test $withval = "no"; then 
		    echo "Forced compilation without ivona support."; 
		    ivona_ok="false";
		 else 
		    if test $ivona_ok = "false"; then
		       echo "Ivona support is not available. Sorry."; exit 1;
		    fi
                 fi
 		 echo "")
	
AM_CONDITIONAL(ivona_support, test $ivona_ok = "true")

AC_SUBST(datadir)

AC_ARG_ENABLE([session-integration],
		AS_HELP_STRING([--enable-session-integration], [build with user session integration support]),
		[], [enable_session_integration=no])

AM_CONDITIONAL([ENABLE_SESSION], [test x$enable_session_integration = xyes])

AC_ARG_WITH([bash-profile-dir],
            AC_HELP_STRING([--with-bash-profile-dir=PATH],
                           [choose directory for session integration shell file (default=SYSCONFDIR/profile.d)]),
            with_bash_profile_dir=$withval,
            with_bash_profile_dir=$sysconfdir/profile.d)
BASH_PROFILE_DIR=$with_bash_profile_dir
AC_SUBST(BASH_PROFILE_DIR)

AC_ARG_WITH([system-speechd-start-cmd],
            AC_HELP_STRING([--with-system-speechd-start-cmd=COMMAND],
                           [Specify the command that you want used when spawning the speech-dispatcher server as a system process (default is /etc/init.d/speech-dispatcher start)]),
            with_system_speechd_start_cmd=$withval,
            with_system_speechd_start_cmd="/etc/init.d/speech-dispatcher start")
SPEECHD_SYSTEM_START_CMD=$with_system_speechd_start_cmd
AC_SUBST(SPEECHD_SYSTEM_START_CMD)

spdconfdir='${sysconfdir}/speech-dispatcher'
AC_SUBST(spdconfdir)
moduleconfdir='${spdconfdir}/modules' 
AC_SUBST(moduleconfdir)
clientconfdir='${spdconfdir}/clients'
AC_SUBST(clientconfdir)

spdconforigdir='${datadir}/speech-dispatcher/conf'
AC_SUBST(spdconforigdir)
moduleconforigdir='${spdconforigdir}/modules'
AC_SUBST(moduleconforigdir)
clientconforigdir='${spdconforigdir}/clients'
AC_SUBST(clientconforigdir)
spddesktopconforigdir='${spdconforigdir}/desktop'
AC_SUBST(spddesktopconforigdir)


snddatadir='${datadir}/sounds/speech-dispatcher'
AC_SUBST(snddatadir)
modulebindir='${libdir}/speech-dispatcher-modules'
AC_SUBST(modulebindir)
spdlibdir='${libdir}/speech-dispatcher' 
AC_SUBST(spdlibdir)
spdlib2dir='${libdir}/speech-dispatcher' 
AC_SUBST(spdlib2dir)


AC_ARG_VAR(pidpath, Absolute path to a pid file)

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
                 doc/Makefile
                 src/Makefile
                 src/api/Makefile
                 src/api/c/Makefile
                 src/api/python/Makefile
                 src/audio/Makefile
                 src/audio/static_plugins.c
                 src/clients/Makefile
                 src/clients/say/Makefile
                 src/clients/spdsend/Makefile
                 src/common/Makefile
                 src/modules/Makefile
                 src/server/Makefile
                 src/session/Makefile
                 src/tests/Makefile])
AC_OUTPUT

