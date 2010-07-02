# Copyright (C) 2008 Brailcom, o.p.s.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import sys
import os
import shutil
import fileinput
import socket
import time
import datetime

from optparse import OptionParser


from opentts_config.i18n import _
# Configuration and sound data paths
import paths

def report(msg):
    """Output information messages for the user on stdout
    and if desired, by espeak synthesis"""
    print msg
    if options.use_espeak_synthesis:
        os.system("espeak \"" + msg + "\"")

def input_audio_icon():
    """Produce a sound for the event 'input requested' used in question()"""
    if options.use_espeak_synthesis:
        os.system("espeak \"Type in\"");

def question(text, default):
    """Ask a simple question and suggest the default value"""

    while 1:
        if isinstance(default, bool):
            if default == True:
                default_str = _("yes")
            else:
                default_str = _("no")
        else:
            default_str = str(default)
        report(text + " ["+default_str+"] :")
        input_audio_icon()

        if not options.dont_ask:
            str_inp = raw_input(">") 

        # On plain denter, return default
        if options.dont_ask or (len(str_inp) == 0):
            return default
        # If a value was typed, check it and convert it
        elif isinstance(default, bool):
            # TRANSLATORS: The following string contains correct input
            # Sequences for a positive (true) response like
            # yes, y, Y, 1, etc.
            # Please separate possible responses with a space
            # and try to aproximate as good as you can the options of this string
            # for your locale.
            yes = _("yes y Y true t 1")

            # TRANSLATORS: The following string contains correct input
            # Sequences for a negative (false) response like
            # no, n, N, false 0, etc.
            # Please separate possible responses with a space
            # and try to aproximate as good as you can the options of this string
            # for your locale.
            no = _("no n N false f 0")
            if str_inp in yes.split(" "):
                return True;
            elif str_inp in no.split(" "):
                return False;
            else:
                report (_("Unknown answer (type 'yes' or 'no')"))
                continue
        elif isinstance(default, int):
            return int(str_inp)
        elif isinstance(default, str):
            return str_inp

def question_with_suggested_answers(text, default, suggest):
    """Ask a question with suggested answers. If the answer typed is not
    in 'suggest', the user is notified and given an opportunity to correct
    his choice"""
    
    reply = question(text, default)
    while reply not in suggest:
        report(_("""The value you have chosen is not among the suggested values.
You have chosen '%s'.""") % reply)
        report(_("The suggested values are %s") % str(suggest))
        correct = question(_("Do you want to correct your answer?"), True)
        if correct == True:
            reply = question(text, default)
        else:
            return reply
    return reply

def question_with_required_answers(text, default, required):
    """Ask a question and repeat it until the answer typed in is in 'required'"""
    
    reply = question(text, default)
    while reply not in required:
        report(_("You have chosen '%s'. Please choose one of %s") % (reply, str(required)))
        reply = question(text, default)
    return reply

class Options(object):
    """Configuration for spdconf"""

    _conf_options = \
        {
        'create_user_configuration':
            {
            'descr' : _("Create OpenTTS configuration for the given user"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('-u', '--create-user-conf'),
            },
        'config_basic_settings_user':
            {
            'descr' : _("Configure basic settings in user configuration"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('-c', '--config-basic-settings-user'),
            },
        'config_basic_settings_system':
            {
            'descr' : _("Configure basic settings in system-wide configuration"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('-C', '--config-basic-settings-system'),
            },
        'diagnostics':
            {
            'descr' : _("Diagnose problems with the current setup"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('-d', '--diagnostics'),
            },
        'test_otts_say':
            {
            'descr' : _("Test connection to OpenTTS using otts-say"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('-s', '--test-otts-say'),
            },
        'test_festival':
            {
            'descr' : _("Test whether Festival works as a server"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('', '--test-festival'),
            },
        'test_espeak':
            {
            'descr' : _("Test whether Espeak works as a standalone binary"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('', '--test-espeak'),
            },
        'test_alsa':
            {
            'descr' : _("Test ALSA audio"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('', '--test-alsa'),
            },

        'test_pulse':
            {
            'descr' : _("Test Pulse Audio"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('', '--test-pulse'),
            },

        'use_espeak_synthesis':
            {
            'descr' : _("Use espeak to synthesize messages"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('-e', '--espeak'),
            },
        'dont_ask':
            {
            'descr' : _("Do not ask any questions, allways use default values"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('-n', '--dont-ask'),
            },
        'debug':
            {
            'descr' : _("Debug a problem and generate a report"),
            'doc' : None,
            'type' : bool,
            'default' : False,
            'command_line' : ('-D', '--debug'),
            },
        }

    def __init__(self):
        usage = _("""A simple tool for basic configuration of OpenTTS
and problem diagnostics

usage: %prog [options]""")
        self.cmdline_parser = OptionParser(usage)

        for option, definition in self._conf_options.iteritems():
            # Set object attributes to default values
            def_val = definition.get('default', None)
            setattr(self, option, def_val)
            
            # Fill in the cmdline_parser object
            if definition.has_key('command_line'):
                descr = definition.get('descr', None)                
                type = definition.get('type', None)
                
                if definition.has_key('arg_map'):
                    type, map = definition['arg_map']
                if type == str:
                    type_str = 'string'
                elif type == int:
                    type_str = 'int'
                elif type == float:
                    type_str = 'float'
                elif type == bool:
                    type_str = None
                else:
                    raise "Unknown type"
                
                if type != bool:
                    self.cmdline_parser.add_option(type=type_str, dest=option,
                                                   help=descr,
                                                   *definition['command_line'])
                else: # type == bool
                    self.cmdline_parser.add_option(action="store_true", dest=option,
                                                   help=descr,
                                                   *definition['command_line'])
            
        # Set options according to command line flags
        (cmdline_options, args) = self.cmdline_parser.parse_args()

        for option, definition in self._conf_options.iteritems():
                val = getattr(cmdline_options, option, None)
                if val != None:
                    if definition.has_key('arg_map'):
                        former_type, map = definition['arg_map']
                        try:
                            val = map[val]
                        except KeyError:
                            raise "Invalid option value: "  + str(val)
                        
                    setattr(self, option, val)
        
        #if len(args) != 0:
           # raise "This command takes no positional arguments (without - or -- prefix)"

class Tests:
    """Tests of functionality of OpenTTS and its dependencies
    and methods for determination of proper paths"""

    def user_openttsd_dir(self):
        """Return user OpenTTS configuration and logging directory"""
        return os.path.expanduser(os.path.join('~', '.opentts'))

    def user_openttsd_dir_exists(self):
        """Determine whether user openttsd directory exists"""
        return os.path.exists(self.user_openttsd_dir())

    def user_conf_dir(self):
        """Return user configuration directory"""
        return os.path.join(self.user_openttsd_dir(), "conf")

    def system_conf_dir(self):
        """Determine system configuration directory"""
        return paths.SPD_CONF_PATH;

    def user_conf_dir_exists(self):
        """Determine whether user configuration directory exists"""
        return os.path.exists(self.user_conf_dir())

    def festival_connect(self, host="localhost", port=1314):
        """
        Try to connect to festival and determine whether it is possible.
        On success self.festival_socket is initialized with the openned socket.
        """
        self.festival_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            self.festival_socket.connect((socket.gethostbyname(host), port))
        except socket.error, (num, reson):
            report(_("""ERROR: It was not possible to connect to Festival on the
given host and port. Connection failed with error %d : %s .""") % (num, reson))
            report(_("""Hint: Most likely, your Festival server is not running now
(or not at the default port.
Try /etc/init.d/festival start or run 'festival --server' from the command line."""))
            return False

        return True
        
    def festival_with_freebsoft_utils(self):
        """Test whether festival contains working festival-freebsoft-utils.
        This test must be performed only after festival_connect().
        """
        self.festival_socket.send("(require 'speech-dispatcher)\n");
        reply = self.festival_socket.recv(1024);
        if "LP" in reply:
            return True
        else:
            report(_("""ERROR: Your Festival server is working but it doesn't seem
to load festival-freebsoft-utils. You need to install festival-freebsoft-utils
to be able to use Festival with OpenTTS."""))
            return False

    def python_openttsd_in_path(self):
        """Try whether python openttsd library is importable"""
        try:
            import opentts
        except:
            report(_("""Python can't find the OpenTTS library.
Is it installed? This won't prevent OpenTTS to work, but no
Python applications like Orca will find it.
Search for package like python-openttsd."""))
            return False
        return True

    def audio_try_play(self, type):
        """Try to play a sound through the standard playback utility for the
        given audio method."""
        wavfile = os.path.join(paths.SPD_SOUND_DATA_PATH,"test.wav")

        if type == 'alsa':
            cmd = "aplay" + " " + wavfile
        elif type == 'pulse':
            cmd = "paplay" + " " + wavfile
        elif type == 'oss':
            raise NotImplementedError

        try:
            ret = os.system(cmd)
        except:
            report(_("""Can't execute the %s command, The tool might just not be installed.
This doesn't mean that the sound system is not working, but we can't determine it.
Please test manually."""))
            reply = question(_("Are you sure that %s audio is working?") % type, False)
            return reply
    
        if ret:
            report(_("Can't play audio via %s") % cmd)
            report(_("""Your audio doesn't seem to work, please fix audio first or choose
a different method."""))
            return False


        reply = question(_("Did you hear the sound?"), True)
        
        if not reply:
            report(_("""Please examine the above output from the sound playback
utility. If everything seems right, are you sure your audio is loud enough and
not muted in the mixer? Please fix your audio system first or choose a different
audio output method in configuration."""))
            return False

    def test_otts_say(self):
        """Test OpenTTS using otts-say"""

        report(_("Testing OpenTTS using otts-say"))
        
        while True:
            try:
                ret = os.system("otts-say -P important \"OpenTTS seems to work\"")
            except:
                report(_("""Can't execute the otts-say binary,
it is likely that OpenTTS is not installed."""))
                return False
            hearing_test = question(_("Did you hear the message about OpenTTS working?"), True)
            if hearing_test:
                report(_("OpenTTS is working"))
                return True
            else:
                repeat = question(_("Do you want to repeat the test?"), True)
                if repeat:
                    continue
                else:
                    report(_("OpenTTS not working now"))
                    return False

    def test_festival(self):
        """Test whether Festival works as a server"""
        report(_("Testing whether Festival works as a server"))

        ret = self.festival_connect()
        while not ret:
            reply = question(_("Do you want to try again?"), True)
            if not reply:
                report(_("Festival server is not working now."))
                return False
        report(_("Festival server seems to work correctly"))
        return True

    def test_espeak(self):
        """Test the espeak utility"""

        report(_("Testing whether Espeak works"))
        
        while True:
            try:
                msg = _("Espeak seems to work")
                os.system("espeak \"%s\"" % msg)
            except:
                report(_("""Can't execute the espeak binary, it is likely that espeak
is not installed."""))
                return False

            hearing_test = question(_("Did you hear the message 'Espeak seems to work'?"),
                                    True)
            if hearing_test:
                report(_("Espeak is working"))
                return True
            else:
                repeat = question(_("Do you want to repeat the test?"), True)
                if repeat:
                    continue
                else:
                    report(_("""
Espeak is installed, but the espeak utility is not working now.
This doesn't necessarily mean that your espeak won't work with
OpenTTS."""))
                    return False

    def test_alsa(self):
        """Test ALSA sound output"""
        report(_("Testing ALSA sound output"))
        return self.audio_try_play(type='alsa')

    def test_pulse(self):
        """Test Pulse Audio sound output"""
        report(_("Testing PULSE sound output"))
        return self.audio_try_play(type='pulse')

    def diagnostics(self, openttsd_running = True, output_modules=[], audio_output=[]):

        """Perform a complete diagnostics"""
        working_modules = []
        working_audio = []

        if openttsd_running:
            # Test whether OpenTTS works
            if self.test_otts_say():
                opentts_working = True
                skip = question(_("OpenTTS works. Do you want to skip other tests?"),
                                True)
                if skip:
                    return {'opentts_working': True}
            else:
                opentts_working = False
        else:
            opentts_working = False

        if not opentts_working:
            if not question(_("""
OpenTTS isn't running or we can't connect to it (see above),
do you want to proceed with other tests? (They can help to determine
what is wrong)"""), True):
                return {'opentts_working': False}


        def decide_to_test(identifier, name, listing):
            """"Ask the user whether to test a specific capability"""
            if ((identifier in listing)
                or (not len(listing)
                    and question(_("Do you want to test the %s now?") % name, True))):
                return True
            else:
                return False

        if decide_to_test('festival', _("Festival synthesizer"), output_modules):
            if self.test_festival():
                working_modules += ["festival"]

        if decide_to_test('espeak', _("Espeak synthesizer"), output_modules):
            if self.test_espeak():
                working_modules += ["espeak"]

        if decide_to_test('alsa', _("ALSA sound system"), audio_output):
            if not self.test_alsa():
                working_audio += ["alsa"]

        if decide_to_test('pulse', _("Pulse Audio sound system"), audio_output):
            if not self.test_pulse():
                working_audio += ["pulse"]

        report(_("Testing whether Python OpenTTS library is in path and importable"))
        python_openttsd_working = test.python_openttsd_in_path()
        
        # TODO: Return results of the diagnostics
        return  {'opentts_working': opentts_working,
                 'audio': working_audio,
                 'synthesizers': working_modules,
                 'python_openttsd' : python_openttsd_working}

    def write_diagnostics_results(self, results):
        """Write out diagnostics results using report()"""

        report(_("""

Diagnostics results:"""))
        if results.has_key('opentts_working'):
            if results['opentts_working']:
                report(_("OpenTTS is working"))
            else:
                report(_("OpenTTS not working"))
        if results.has_key('synthesizers'):
            report(_("Synthesizers that were tested and work: %s") %
                   str(results['synthesizers']))
        if results.has_key('audio'):
            report(_("Audio systems that were tested and work: %s") %
                   str(results['audio']))
        if results.has_key('python_openttsd'):
            if(results['python_openttsd']):
                report(_("Python OpenTTS module is importable"))
            else:
                report(_("""Python OpenTTS module not importable.
Either not installed or not in path."""))
        report(_("End of diagnostics results"))

    def user_configuration_seems_complete(self):
        """Decide if the user configuration seems reasonably complete"""
        if not os.path.exists(os.path.join(self.user_conf_dir(), "openttsd.conf")):
            return False

        if not len(os.listdir(self.user_conf_dir())) > 2:
            return False

        if not os.path.exists(os.path.join(self.user_conf_dir(), "modules")):
            return False

        if not os.path.exists(os.path.join(self.user_conf_dir(), "clients")):
            return False

        return True

    def debug_and_report(self, openttsd_port=6560, type = None):
        """Start OpenTTS in debugging mode, collect the debugging output
        and offer to send it to the developers"""

        report(_("Starting collecting debugging output, configuration and logfiles"))

        if not type:
            type = question_with_required_answers(_("""
Do you want to debug 'system' or  'user' OpenTTS?"""),
                                                  'user', ['user', 'system'])

        # Try to kill running OpenTTS
        reply = question(_("""It is necessary to kill the currently running OpenTTS
processes. Do you want to do it now?"""), True);
        if reply:
            os.system("killall openttsd")
        else:
            report(_("""
You decided not to kill running OpenTTS  processes.
Please make sure your OpenTTS is not running now."""))
            reply = question(_("Is your OpenTTS not running now?"), True);
            if not reply:
                report(_("Can't continue, please stop your OpenTTS and try again"))

        time.sleep(2)

        # All debugging files are written to TMPDIR/openttsd-debug/
        if os.environ.has_key('TMPDIR'):
            tmpdir = os.environ['TMPDIR']
        else:
            tmpdir = "/tmp/"
        debugdir_path = os.path.join(tmpdir, "openttsd-debug")
        date = datetime.date.today()
        debugarchive_path = os.path.join(tmpdir, "openttsd-debug-%d-%d-%d.tar.gz" %
                                         (date.day, date.month, date.year))

        # Start OpenTTS with debugging enabled
        if type == 'user':
            report(_("OpenTTS will be started now in debugging mode"))
            openttsd_started = not os.system("openttsd -D")
            configure_directory = test.user_conf_dir()
        else:
            report(_("Warning: You must be root or under sudo to do this."))
            report(_("""
Please start your system OpenTTS now with parameter '-D'"""))
            reply = question(_("Is your OpenTTS running now?"), True);
            if reply:
                openttsd_started = True
            else:
                report(_("Can't continue"))
            configure_directory = test.system_conf_dir()
        time.sleep(2)

        if not openttsd_started:
            reply = question(_("OpenTTS failed to start, continuing anyway"));

        report(_("Trying to speak some messages"))
        ret = os.system("otts-say \"OpenTTS debugging 1\"")
        if ret:
            report(_("Can't test OpenTTS connection, can't connect"))

        os.system("otts-say \"OpenTTS debugging 2\"")
        os.system("otts-say \"OpenTTS debugging 3\"")

        report(_("Please wait (about 5 seconds)"))
        time.sleep(5)

        report(_("Collecting debugging output and your configuration information"))

        os.system("umask 077");
        os.system("tar -cz %s %s > %s" % 
                  (debugdir_path, configure_directory, debugarchive_path))   
        os.system("killall openttsd")
        os.system("rm -rf %s" % debugdir_path)

        report(_("""
Please send %s to opentts-dev@lists.opentts.org with
a short description of what you did. We will get in touch with you soon
and suggest a solution.""") % debugarchive_path)
        
test = Tests()

class Configure:

    """Setup user configuration and/or set basic options in user/system configuration"""

    default_output_module = None
    default_language = None
    default_audio_method = None
    
    def remove_user_configuration(self):
        """Remove user configuration tree"""
        shutil.rmtree(test.user_conf_dir())

    def options_substitute(self, configfile, options):
        """Substitute the given options with given values.

        Arguments:
        configfile -- the file path of the configuration file as a string
        options -- a list of tuples (option_name, value)"""

        # Parse config file in-place and replace the desired options+values
        for line in fileinput.input(configfile, inplace=True, backup=".bak"):
            # Check if the current line contains any of the desired options
            for opt, value in options.iteritems():
                if opt in line:
                    # Now count unknown words and try to judge if this is
                    # real configuration or just a comment
                    unknown = 0
                    for word in line.split():
                        if word =='#' or word == '\t':
                            continue
                        elif word == opt:
                            # If a foreign word went before our option identifier,
                            # we are not in code but in comments
                            if unknown != 0:
                                unknown = 2;
                                break;
                        else:
                            unknown += 1

                    # Only consider the line as the actual code when the keyword
                    # is followed by exactly one word value. Otherwise consider this
                    # line as plain comment and leave intact
                    if unknown == 1:
                        # Convert value into string representation in spd_val
                        if isinstance(value, bool):
                            if value == True:
                                spd_val = "1"
                            elif value == False:
                                spd_val = "2"
                        elif isinstance(value, int):
                            spd_val = str(value)
                        else:
                            spd_val = str(value)
                            
                        print opt + "   " + spd_val
                        break
            
            else:
                print line,
                
    def create_user_configuration(self):
        """Create user configuration in the standard location"""

        # Ask before touching things that we do not have to!
        if test.user_openttsd_dir_exists():
            if test.user_conf_dir_exists():
                if test.user_configuration_seems_complete():
                    reply = question(
                        _("""User configuration already exists.
Do you want to rewrite it with a new one?"""), False)
                    if reply == False:
                        report(_("Keeping configuration intact and continuing with settings."))
                        return
                    else:
                        self.remove_user_configuration()
                else:
                    reply = question(
                        _("""User configuration already exists, but it seems to be incomplete.
Do you want to keep it?"""), False)
                    if reply == False:
                        self.remove_user_configuration()
                    else:
                        report(_("Keeping configuration intact and aborting."))
                        return

            # TODO: Check for permissions on logfiles and pid
        else:
            report(_("Creating ") + test.user_openttsd_dir())
            os.mkdir(test.user_openttsd_dir())

        # Copy the original intact configuration files
        # creating a conf/ subdirectory
        shutil.copytree(paths.SPD_CONF_ORIG_PATH, test.user_conf_dir())
        
        report(_("User configuration created in %s") % test.user_conf_dir())

    def configure_basic_settings(self, type='user'):
        """Ask for basic settings and rewrite them in the configuration file"""

        if type == 'user':
            report(_("Configuring user settings for OpenTTS"))
        elif type == 'system':
            report(_("Warning: You must be root or under sudo to do this."))
            report(_("Configuring system settings for OpenTTS"))
        else:
            assert(0);

        # Now determine the most important config option
        self.default_output_module = question_with_suggested_answers(
            _("Default output module"),
            "espeak",
            ["espeak", "flite", "festival", "cicero", "ibmtts"])

        self.default_language = question(
            _("Default language (two-letter iso language code like \"en\" or \"cs\")"),
            "en")

        self.default_audio_method = question_with_suggested_answers(
            _("Default audio output method"),
            "pulse,alsa",
            ["pulse", "alsa", "oss", "pulse,alsa"])

        self.default_speech_rate = question(
            _("Default speech rate (on the scale of -100..100, 0 is default, 50 is faster, -50 is slower)"),
            "0")

        self.default_speech_pitch = question(
            _("Default speech pitch (on the scale of -100..100, 0 is default, 50 is higher, -50 is lower)"),
            "0")

        # Substitute given configuration options
        if type == 'user':
            configfile = os.path.join(test.user_conf_dir(), "openttsd.conf")
        elif type == 'system':
            configfile = os.path.join(test.system_conf_dir(), "openttsd.conf")

        self.options_substitute(configfile, 
                                {"DefaultModule": self.default_output_module,
                                 "DefaultLanguage": self.default_language,
                                 "AudioOutputMethod": self.default_audio_method,
                                 "DefaultRate": self.default_speech_rate,
                                 "DefaultPitch": self.default_speech_pitch,
                                 "DefaultLanguage": self.default_language,
                                 })
        if type == 'user':
            self.setup_autostart = question(
                _("Do you want to have OpenTTS automatically started from ~/.config/autostart ?"),
                True)
            if self.setup_autostart:
                os.system("""cp %s ~/.config/autostart/""" % os.path.join(paths.SPD_DESKTOP_CONF_PATH,
                                                                          "openttsd.desktop")) 
                         
                report(_("""
Configuration written to %s
Basic configuration now complete. You might still need to fine tune it by
manually editing the configuration above file. Especially if you need to
use special audio settings, non-standard synthesizer ports etc.""") % configfile)

    def openttsd_start_user(self):
        """Start OpenTTS in user-mode"""

        report(_("Starting OpenTTS in user-mode"))

        not_started = True
        while not_started:
            not_started = os.system("openttsd");
            if not_started:
                report(_("Can't start OpenTTS. Exited with status %d") %
                       not_started)
                reply = question(_("""
Perhaps this is because your OpenTTS is already running.
Do you want to kill all running OpenTTS processes and try again?"""), True)
                if reply:
                    os.system("killall openttsd")
                else:
                    report(_("Can't start OpenTTS"))
                    return False
        return True

    def openttsd_start_system(self):
        """Start OpenTTS in system-mode"""

        report(_("Warning: You must be root or under sudo to do this."))
        report(_("Starting OpenTTS in system-mode"))
        
        reply = question(_("Is your system using an /etc/init.d/openttsd script?"),
                         True)
        if reply:
            report(_("Stopping OpenTTS in case any is running already"))
            os.system("/etc/init.d/openttsd stop") 
            report(_("Starting OpenTTS via /etc/init.d/openttsd"))
            ret = os.system("/etc/init.d/openttsd start")
            if ret:
                report(_("Can't start OpenTTS. Exited with status %d") % ret)
                return False
        else:
            report(_("""Do not know how to start system OpenTTS,
you have to start it manually to continue."""))
            reply = question(_("Have you started OpenTTS now?"), True)
            if not reply:
                report(_("Can't continue"))
                return False
        return True

    def complete_config(self):
        """Create a complete configuration, run diagnosis and if necessary, debugging"""

        openttsd_type = question_with_required_answers(
            _("Do you want to create/setup a 'user' or 'system' configuration"),
            'user', ['user', 'system'])

        if openttsd_type == 'user':
            self.create_user_configuration()
            self.configure_basic_settings(type='user')
        elif openttsd_type == 'system':
            self.configure_basic_settings(type='system')
        else:
            assert(False)

        reply = question(_("Do you want to start/restart OpenTTS now and run some tests?"), True)
        if not reply:
            report(_("Your configuration is now done but not tested"))
            return
        else:
            if openttsd_type == 'user':
                started = self.openttsd_start_user()
            elif openttsd_type == 'system':
                started= self.openttsd_start_system()

        if not started:
            report(_("Your OpenTTS is not running"))
            
        result = test.diagnostics(openttsd_running = started,
                                  audio_output=[self.default_audio_method],
                                  output_modules=[self.default_output_module]);
        test.write_diagnostics_results(result)

        reply = question(_("Do you want to run debugging now and send a request for help to the developers?"),
                         False)
        if reply:
            test.debug_and_report(type=openttsd_type)


# Basic objects
options = Options()
configure = Configure()
test = Tests()


def main():

    report(_("OpenTTS configuration tool"))
    
    if options.create_user_configuration:
        # Check for and/or create basic user configuration
        configure.create_user_configuration()
        reply = question(_("Do you want to continue with basic settings?"), True)
        if reply:
            configure.configure_basic_settings(type='user')
    elif options.config_basic_settings_user:
        configure.configure_basic_settings(type='user')

    elif options.config_basic_settings_system:
        configure.configure_basic_settings(type='system')

    elif options.test_festival:
        test.test_festival()

    elif options.test_otts_say:
        test.test_otts_say()

    elif options.test_espeak:
        test.test_espeak()

    elif options.test_alsa:
        test.audio_try_play(type='alsa')

    elif options.test_pulse:
        test.audio_try_play(type='pulse')

    elif options.diagnostics:
        ret = test.diagnostics()
        test.write_diagnostics_results(ret)

    elif options.debug:
        test.debug_and_report()

    else:
        reply = question(_("Do you want to setup a completely new configuration?"), True)
        if reply:
            configure.complete_config()
        else:
            reply = question(_("Do you want to run diagnosis of problems?"), True)
            if reply:
                test.diagnostics()
                test.write_diagnostics_results(ret)
            else:
                report(_("""Please run this command again and select what you want to do
or read the quick help available through '-h' or '--help'."""))

if __name__ == "__main__":
    sys.exit(main())
else:
    main()
