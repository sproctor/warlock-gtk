#ifndef _SCRIPT_H
#define _SCRIPT_H

/* local data types */
typedef struct _ScriptData ScriptData;
typedef struct _ScriptCommand ScriptCommand;

typedef enum {
	SCRIPT_TYPE_INTEGER,
	SCRIPT_TYPE_STRING,
	SCRIPT_TYPE_VARIABLE,
	NUM_SCRIPT_TYPES
} ScriptType;

struct _ScriptData {
	ScriptType type;
	union {
		int as_integer;
		char *as_string;
	} value;
};

struct _ScriptCommand {
	GList *command;
	guint line_number;
	const char *depends_on;
};

void script_load (const char *filename, int argc, const char **argv);
void script_save_label (const char *label);
void script_moved (void);
void script_match_string (const char *string);
void script_kill (void);
void script_toggle_suspend (void);
void script_got_prompt (void);
void script_init (void);

#endif
