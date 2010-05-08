#ifndef FDSETCONV_H
#define FDSETCONV_H

#include <stdio.h>
#include <string.h>

#include "opentts/opentts_types.h"

char *voice2str(SPDVoiceType voice);

SPDVoiceType str2voice(char *str);

char *punct2str(SPDPunctuation punct);

SPDPunctuation str2punct(char *str);

char *spell2str(SPDSpelling spell);

SPDSpelling str2spell(char *str);

char *recogn2str(SPDCapitalLetters recogn);

SPDCapitalLetters str2recogn(char *str);

SPDPriority str2priority(char *str);

#endif
