#ifndef FDSETCONV_H
#define FDSETCONV_H

#include <stdio.h>
#include <string.h>

#include "opentts/opentts_types.h"
#include "fdset.h"

char *EVoice2str(EVoiceType voice);

EVoiceType str2EVoice(char *str);

char *punct2str(SPDPunctuation punct);

SPDPunctuation str2punct(char *str);

char *spell2str(SPDSpelling spell);

SPDSpelling str2spell(char *str);

char *recogn2str(SPDCapitalLetters recogn);

SPDCapitalLetters str2recogn(char *str);

EVoiceType str2intpriority(char *str);

#endif
