
#include "openttsd.h"

#ifndef ALLOC_H
#define ALLOC_H

/* Like free(), but don't try to free NULL data. After freeing
   the data, fill the pointer with NULL. */
void spd_free(void *data);

TSpeechDQueue *speechd_queue_alloc();

/* Copy a message */
TSpeechDMessage *spd_message_copy(TSpeechDMessage * old);

/* Free a message */
void mem_free_message(TSpeechDMessage * msg);

/* Free a settings element */
void mem_free_fdset(TFDSetElement * set);

#endif
