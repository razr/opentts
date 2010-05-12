if ENABLE_SESSION
speechd_default_port = $(shell grep SPEECHD_DEFAULT_PORT ../common/def.h | awk '{print $$3}')
speechd_system_start_cmd = @SPEECHD_SYSTEM_START_CMD@
dist_man1_MANS = speechd-server-spawn.1
bin_SCRIPTS = speechd-server-spawn
bashprofile_SCRIPTS = speechd-user-port.sh
CLEANFILES = speechd-server-spawn speech-user-port.sh

speechd-user-port.sh: speechd-user-port.sh.in
	sed -e "s/[@]SPEECHD_DEFAULT_PORT[@]/$(speechd_default_port)/" speechd-user-port.sh.in > speechd-user-port.sh

speechd-server-spawn: speechd-server-spawn.in
	sed -e "s#[@]SPEECHD_SYSTEM_STARTUP_CMD[@]#$(speechd_system_start_cmd)#" speechd-server-spawn.in > speechd-server-spawn
endif

EXTRA_DIST = speechd-server-spawn.in speechd-user-port.sh.in
