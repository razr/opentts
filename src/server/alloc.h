#ifndef ALLOC_H
#define ALLOC_H

#include "fdset.h"
#include "openttsd.h"

/* Copy a message */
openttsd_message *copy_message(openttsd_message * old);

/* Free a message */
void mem_free_message(openttsd_message * msg);

/* Free a settings element */
void mem_free_fdset(TFDSetElement * set);

#endif
