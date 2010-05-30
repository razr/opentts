#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "alloc.h"

queue_t *queue_alloc()
{
	queue_t *new;

	new = g_malloc(sizeof(queue_t));

	/* Initialize all the queues to be empty */
	new->p1 = NULL;
	new->p2 = NULL;
	new->p3 = NULL;
	new->p4 = NULL;
	new->p5 = NULL;

	return (new);
}

openttsd_message *copy_message(openttsd_message * old)
{
	openttsd_message *new = NULL;

	if (old == NULL)
		return NULL;

	new = (openttsd_message *) g_malloc(sizeof(openttsd_message));

	*new = *old;

	new->buf = g_malloc((old->bytes + 1) * sizeof(char));
	memcpy(new->buf, old->buf, old->bytes);
	new->buf[new->bytes] = 0;

	new->settings = old->settings;

	new->settings.language = g_strdup(old->settings.language);
	new->settings.synthesis_voice = g_strdup(old->settings.synthesis_voice);
	new->settings.client_name = g_strdup(old->settings.client_name);
	new->settings.output_module = g_strdup(old->settings.output_module);
	new->settings.index_mark = g_strdup(old->settings.index_mark);

	return new;
}

void mem_free_fdset(TFDSetElement * fdset)
{
	/* Don't forget that only these items are filled in
	   in a openttsd_message */
	g_free(fdset->client_name);
	g_free(fdset->language);
	g_free(fdset->synthesis_voice);
	g_free(fdset->output_module);
	g_free(fdset->index_mark);
}

void mem_free_message(openttsd_message * msg)
{
	if (msg == NULL)
		return;
	g_free(msg->buf);
	mem_free_fdset(&(msg->settings));
	g_free(msg);
}
