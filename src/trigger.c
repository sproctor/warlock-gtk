#include <glib.h>
#include <string.h>

#include "trigger.h"

extern char* file_readline(FILE*);

GPtrArray *get_triggers_from_file(char *filename)
{
	FILE *file;
	GPtrArray *triggers = g_ptr_array_new();

	file = fopen(filename, "r");

	if ( file != NULL )
	{
		char *line;
		while ((line = file_readline(file)) != NULL)
		{
			char *trig, *to_send;
			Trigger *trigger = g_new(Trigger, 1);

			trig = g_strdup(strtok(line, "\t"));
			to_send = g_strdup(strtok(NULL, "\t"));
			
			trigger->match = trig;
			trigger->command = to_send;

			g_ptr_array_add(triggers, trigger);
		}

		return triggers;
	} else return NULL;
}

void save_triggers_to_file(GPtrArray *triggers, char *filename)
{
	FILE *file;

	file = fopen(filename, "w+");

	if ( file != NULL )
	{
		int i = 0;
		for ( ; i < triggers->len; i++)
		{
			Trigger *trigger = (Trigger *) triggers->pdata[i];
			fprintf(file, "%s\t%s\n", trigger->match, trigger->command);
		}

		fclose(file);
	}
}
