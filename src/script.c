/* Warlock Front End
 * Copyright 2005 Sean Proctor, Marshall Culpepper
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */
                                                                                
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include <pcre.h>

#include "script.h"
#include "warlock.h"
#include "warlockview.h"
#include "preferences.h"
#include "warlocktime.h"
#include "debug.h"

/* macro definitions */
#define ltoa(x)         g_strdup_printf ("%ld", x)
#define itoa(x)         g_strdup_printf ("%d", x)
#define SCRIPT_TIMEOUT  100000

/* external variables */
extern int scriptparse (ScriptCommand **command, const char **label,
		int line_number);
extern int script_scan_string (const char*);

/* local data types */
/* argument list is of type ScriptData */
typedef void (*ScriptFunc) (GList *args);

typedef struct {
	char *string;
	char *label;
        pcre *regex;
        pcre_extra *regex_extra;
} MatchData;

/* local function definitions */
static void script_variable_set (const char *name, ScriptData *data);
static void script_command_call (ScriptCommand *command);
static gboolean script_variable_exists (const char *name);
static ScriptData *script_variable_lookup (const char *name);
static char *script_list_to_string (GList *list);
static char *script_data_to_string (ScriptData *data);
static long script_data_to_integer (ScriptData *data);
static void init_variables (const char **argv);
static gpointer script_run (gpointer data);
static gboolean check_conditional (ScriptConditional *conditional);
static void script_error (const char *fmt, ...);
static void script_warn (const char *fmt, ...);

static void script_move (GList *args);
static void script_goto (GList *args);
static void script_pause (GList *args);
static void script_shift (GList *args);
static void script_put (GList *args);
static void script_echo (GList *args);
static void script_match (GList *args);
static void script_matchre (GList *args);
static void script_matchwait (GList *args);
static void script_waitfor (GList *args);
static void script_waitforre (GList *args);
static void script_wait (GList *args);
static void script_save (GList *args);
static void script_nextroom (GList *args);
static void script_counter (GList *args);
static void script_exit (GList *args);
static void script_setvariable (GList *args);
static void script_deletevariable (GList *args);
static void script_call (GList *args);
static void script_return (GList *args);
static void script_random(GList *args);

/* local constants */
static const struct {
	const char *name;
	ScriptFunc func;
} predefined_functions[] = {
	{"move", script_move},
	{"goto", script_goto},
	{"pause", script_pause},
	{"shift", script_shift},
	{"match", script_match},
	{"matchre", script_matchre},
	{"matchwait", script_matchwait},
	{"waitfor", script_waitfor},
	{"waitforre", script_waitforre},
	{"put", script_put},
	{"echo", script_echo},
	{"wait", script_wait},
	{"exit", script_exit},
	{"save", script_save},
	{"nextroom", script_nextroom},
	{"counter", script_counter},
	{"setvariable", script_setvariable},
	{"deletevariable", script_deletevariable},
	{"call", script_call},
	{"gosub", script_call}, /* for Genie2 Pro Compatability */
	{"return", script_return},
	{"random", script_random},
	{"addtohighlightstrings", NULL},
	{"addtohighlightnames", NULL},
	{"deletefromhighlightstrings", NULL},
	{"deletefromhighlightnames", NULL},
	{NULL, NULL}
};

/* global variables */
gboolean script_running = FALSE;

/* local variables */
static GList *curr_command, *next_command;
static GHashTable *variables_table;
static gboolean suspended = FALSE;
static GHashTable *labels;
static GList *commands = NULL;

static GMutex *script_mutex = NULL;

static GCond *move_cond = NULL;
static GCond *wait_cond = NULL;
static GCond *suspend_cond = NULL;
static GAsyncQueue *script_line_queue = NULL;

static GList *match_list = NULL;
static GSList *position_stack = NULL;
static gboolean prompt_since_put = FALSE;

/* initialize script variables */
void
script_init (void)
{
        script_mutex = g_mutex_new ();

	move_cond = g_cond_new ();
	wait_cond = g_cond_new ();
        suspend_cond = g_cond_new ();
        script_line_queue = g_async_queue_new ();
}

/* end the script - must be called externally */
void
script_kill (void)
{
	g_mutex_lock (script_mutex);
        script_running = FALSE;
	g_mutex_unlock (script_mutex);
}

/* suspend or resume the script depending on the current state
 * must be called from inside gdk_threads
 */
void
script_toggle_suspend (void)
{
        g_mutex_lock (script_mutex);

        if (script_running) {
                if (suspended) {
                        echo ("*** Script resumed ***\n");
                        suspended = FALSE;
                        g_cond_broadcast (suspend_cond);
                } else {
                        echo ("*** Script paused ***\n");
                        suspended = TRUE;
                }
        }
        g_mutex_unlock (script_mutex);
}

/* associates the label with the current position in the command list */
static void
script_save_label (const char *label)
{
	char *real_label;

	real_label = g_ascii_strdown (label, -1);
	g_hash_table_insert (labels, real_label, g_list_last (commands));
}

/* loads the script into memory then runs it */
void
script_load (const char *filename, int argc, const char **argv)
{
	char *contents;
        GError *err;

	if (script_running) {
		echo ("*** A script is already running ***\n");
		return;
	}

	err = NULL;
	if (!g_file_get_contents (filename, &contents, NULL, &err)) {
		echo_f ("*** Couldn't open file: %s (Error %d: %s) ***\n",
                                filename, err->code, err->message);
		return;
	}
        g_assert (err == NULL);

        /* initialize misc variables */
	variables_table = g_hash_table_new (g_str_hash, g_str_equal);
	labels = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	init_variables (argv + 1);
        
	echo_f ("*** Script Started: %s ***\n", filename);

	g_thread_create (script_run, contents, TRUE, NULL);
}

/* called externally only, lets us match against a string */
void
script_match_string (const char *string)
{
	if (script_running) {
                g_async_queue_push (script_line_queue, g_ascii_strdown
                                (string, -1));
        }
}

/* call this externally only, signals us to stop waiting on a move */
void
script_moved (void)
{
        g_cond_broadcast (move_cond);
}

/* notify the script that we got a prompt */
void
script_got_prompt (void)
{
        g_mutex_lock (script_mutex);
        prompt_since_put = TRUE;
        g_cond_broadcast (wait_cond);
        g_mutex_unlock (script_mutex);
}

ScriptData *
string_to_script_data (char *str)
{
	ScriptData *data;

	data = g_new (ScriptData, 1);
	data->type = SCRIPT_TYPE_STRING;
	data->value.as_string = str;

	return data;
}

static ScriptData *
integer_to_script_data (int n)
{
	ScriptData *data;

	data = g_new (ScriptData, 1);
	data->type = SCRIPT_TYPE_INTEGER;
	data->value.as_integer = n;

	return data;
}

/* initializes the variable table with the arguments given to the script
 * "1" is the first arg, "2" is the second, etc.
 */
static void
init_variables (const char **argv)
{
        int i;

	for (i = 0; argv != NULL && argv[i] != NULL; i++) {
		ScriptData *var;
		char *name;
                
		name = itoa (i + 1);

		var = string_to_script_data (g_strdup (argv[i]));

		debug ("adding %s = %s\n", name, argv[i]);
		script_variable_set (name, var);
	}
}

/*** local functions ***/

static gboolean
check_binary_expr (ScriptBinaryExpr *expr)
{
	switch (expr->op) {
		case SCRIPT_OP_AND:
			return check_conditional (expr->lhs)
				&& check_conditional (expr->rhs);

		case SCRIPT_OP_OR:
			return check_conditional (expr->lhs)
				|| check_conditional (expr->rhs);

		default:
			g_assert_not_reached ();
			return FALSE;
	}
}

static gboolean
check_unary_expr (ScriptUnaryExpr *expr)
{
	switch (expr->op) {
		case SCRIPT_OP_NOT:
			return !check_conditional (expr->rhs);

		default:
			g_assert_not_reached ();
			return FALSE;
	}
}

static int
script_data_compare (GList *lhs, GList *rhs)
{
	char *l_str, *r_str, *end;
	long l_int, r_int;

	l_str = g_strstrip (script_list_to_string (lhs));
	r_str = g_strstrip (script_list_to_string (rhs));

	debug ("lhs: %s, rhs: %s\n", l_str, r_str);

	// test if we can convert our strings to integers
	errno = 0;
	l_int = strtol (l_str, &end, 10);
	if (errno == 0 && end != l_str) {
		r_int = strtol (r_str, &end, 10);
		if (errno == 0 && end != r_str) {
			// We have 2 integers
			if (l_int > r_int) return 1;
			if (l_int < r_int) return -1;
			return 0;
		}
	}
	errno = 0;

	// else we have 2 non-integers
	return strcmp (l_str, r_str);
}

static gboolean
check_compare_expr (ScriptCompareExpr *expr)
{
	int result;

	switch (expr->op) {
		case SCRIPT_OP_GT:
			result = script_data_compare (expr->lhs, expr->rhs);
			return result > 0;

		case SCRIPT_OP_GTE:
			result = script_data_compare (expr->lhs, expr->rhs);
			return result >= 0;

		case SCRIPT_OP_LT:
			result = script_data_compare (expr->lhs, expr->rhs);
			return result < 0;

		case SCRIPT_OP_LTE:
			result = script_data_compare (expr->lhs, expr->rhs);
			return result <= 0;

		case SCRIPT_OP_EQUAL:
			result = script_data_compare (expr->lhs, expr->rhs);
			return result == 0;

		case SCRIPT_OP_NOTEQUAL:
			result = script_data_compare (expr->lhs, expr->rhs);
			return result != 0;

		default:
			g_assert_not_reached ();
			return FALSE;
	}
}

static gboolean
check_test_expr (ScriptTestExpr *expr)
{
	switch (expr->op) {
		case SCRIPT_OP_EXISTS:
			return script_variable_exists (script_list_to_string
					(expr->arg));

		default:
			g_assert_not_reached ();
			return FALSE;
	}
}

static gboolean
check_conditional (ScriptConditional *conditional)
{
	switch (conditional->type) {
		case SCRIPT_BINARY_EXPR:
			return check_binary_expr (conditional->expr.binary);

		case SCRIPT_UNARY_EXPR:
			return check_unary_expr (conditional->expr.unary);

		case SCRIPT_COMPARE_EXPR:
			return check_compare_expr (conditional->expr.compare);

		case SCRIPT_TEST_EXPR:
			return check_test_expr (conditional->expr.test);

		default:
			g_assert_not_reached ();
			return FALSE;
	}
}

/* calls command_name with args and returns its return value */
static void
script_command_call (ScriptCommand *command)
{
	char *command_name;
	char *command_string;
	int i;
	int matches;
	int pmatch[6]; // only allow 2 matches (6 / 3)
	static pcre *command_regexp = NULL;
	static pcre *args_regexp = NULL;
	int len;
	GList *args;
	GList *curr_condition;

	if (!script_running) {
		return;
	}

	/* don't execute if there is no command */
	if (command->command == NULL) {
		return;
	}

	/* don't execute if any of our conditions fail */
	for (curr_condition = command->conditionals; curr_condition != NULL;
			curr_condition = curr_condition->next) {
		if (!check_conditional (curr_condition->data)) {
			return;
		}
	}

	command_string = script_list_to_string (command->command);
	len = strlen (command_string);

	/* compile the command name regexp if we haven't done it yet */
	if (command_regexp == NULL) {
		const char *err;
		int offset;

		command_regexp = pcre_compile ("^([^[:blank:]]+)[[:blank:]]*",
				0, &err, &offset, NULL);
		if (command_regexp == NULL) {
			// TODO change the following into a dialog
			g_warning ("Error compiling command_regexp at character"
					" %d: %s\n", offset, err);
			return;
		}
        }

	/* match out the command name */
	matches = pcre_exec (command_regexp, NULL, command_string, len, 0, 0,
			pmatch, 6);
	if (matches <= 1) {
		g_warning ("Error: no command found\n");
		return;
	}

	command_name = g_ascii_strdown (command_string + pmatch[2],
			pmatch[3] - pmatch[2]);

	/* compile the args regexp if we haven't done it yet */
	if (args_regexp == NULL) {
		const char *err;
		int offset;

		args_regexp = pcre_compile ("([^[:blank:]]+[[:blank:]]*)", 0,
				&err, &offset, NULL);
		if (args_regexp == NULL) {
			// TODO change the following into a dialog
			g_warning ("Error compiling args_regexp at character"
					" %d: %s\n", offset, err);
			return;
		}
	}

	/* create the argument list */
	args = NULL;
	while ((matches = pcre_exec (args_regexp, NULL, command_string, len,
					pmatch[1], 0, pmatch, 6)) > 0) {
		ScriptData *data;
		data = string_to_script_data (g_ascii_strdown (command_string
					+ pmatch[2], pmatch[3] - pmatch[2]));
		args = g_list_append (args, data);
	}
					
	/* lookup and call the function */
	for (i = 0; predefined_functions[i].name != NULL; i++) {
		if (strcmp (predefined_functions[i].name, command_name)
				== 0) {
			/* any calls to interface functions, etc need to be
			 * surrounded by gdk_threads_enter and _leave macros
			 */
			if (predefined_functions[i].func != NULL) {
				predefined_functions[i].func (args);
			}
			break;
		}
	}
}

static void
script_parse (char *position)
{
	int line_number;
	char *line;

	line = NULL;
	line_number = 0;

	// parse the script
	while (position != NULL && *position != '\0') {
		char *end;
		int len;
		ScriptCommand *command;
		const char *label;

		end = strstr (position, "\n");

		if (end != NULL) {
			// len is end - position + 1, because we want to include
			// the end
			len = end - position + 1;
		} else {
			len = strlen (position);
		}
		if (line == NULL || sizeof (line) < len + 1) {
			if (line != NULL) {
				g_free (line);
			}
			// len + 1 because we need rooom for the null
			line = g_new (char, len + 1);
		}
		memcpy (line, position, len);
		line[len] = '\0';
		if (end != NULL) {
			position = end + 1;
		} else {
			position = NULL;
		}
		line_number++;

		debug ("script line: %s", line);
		script_scan_string (line);
		label = NULL;
		if (script_running && scriptparse (&command, &label,
					line_number) != 0)
		{
			gdk_threads_enter ();
			echo_f ("*** Parse error in script at line: %d ***\n",
					line_number);
			gdk_threads_leave ();
			script_running = FALSE;
			break;
		}
		g_assert (command != NULL);
		g_mutex_lock (script_mutex);
		commands = g_list_append (commands, command);
		if (label != NULL)
			script_save_label (label);
		g_mutex_unlock (script_mutex);
	}
	g_free (line);
}

/* runs the script that has already been loaded */
static gpointer
script_run (gpointer data)
{

	script_running = TRUE;
	suspended = FALSE;

	script_parse (data);
	g_free (data);

        // wait until we're out of roundtime to start the script
	warlock_roundtime_wait (&script_running);	

	// execute the script
	for (curr_command = commands; curr_command != NULL;
			curr_command = next_command) {
		next_command = curr_command->next;
		if (!script_running) {
			break;
		}
		script_command_call (curr_command->data);

		// if we're suspended, wait until we aren't to continue
                g_mutex_lock (script_mutex);
                while (suspended && script_running) {
                        GTimeVal time;

                        g_get_current_time (&time);
                        g_time_val_add (&time, SCRIPT_TIMEOUT);
                        g_cond_timed_wait (suspend_cond, script_mutex, &time);
                }
                g_mutex_unlock (script_mutex);
	}

	debug ("Closing script\n");
	/* quits the currently running script */
	/* free up what we used, set things back to NULL */
	g_mutex_lock (script_mutex);
	script_running = FALSE;
	g_hash_table_destroy (labels);
	g_hash_table_destroy (variables_table);
	labels = NULL;
	variables_table = NULL;

	if (match_list != NULL) {
		g_list_free (match_list);
		match_list = NULL;
	}

	if (position_stack != NULL) {
		g_slist_free (position_stack);
		position_stack = NULL;
	}
        g_mutex_unlock (script_mutex);
	g_list_free (commands);
	commands = NULL;

	gdk_threads_enter ();
	echo ("*** Script Finished ***\n");
	gdk_threads_leave ();

        debug ("Script ended\n");
	return NULL;
}

/* lookup a variable in the global variable list */
static ScriptData *
script_variable_lookup (const char *name)
{
        char *real_name;
	ScriptData *var;

        real_name = g_strstrip (g_ascii_strdown (name, -1));

	var = g_hash_table_lookup (variables_table, real_name);
	
        g_free (real_name);

        return var;
}

/* does variable exist? */
static gboolean
script_variable_exists (const char *name)
{
	return script_variable_lookup (name) != NULL;
}

/* unset a variable at all levels */
static void
script_variable_unset (const char *name)
{
	// TODO the value should be freed here
	// if we do, make sure all callers expect the value to be freed
	g_hash_table_remove (variables_table, name);
}

/* name must be lowercase and not contain white-space, etc */
/* TODO make sure that we don't plan to free data anywhere this function is
 * called from. if we do, copy it here
 */
static void
script_variable_set (const char *name, ScriptData *data)
{
        script_variable_unset (name);
        g_hash_table_insert (variables_table, g_strdup (name), data);
}

/* helper for script_data_to_string, casts a list into a string */
static char *
script_list_to_string (GList *list)
{
	GString *buffer;

	buffer = g_string_new ("");
	while (list != NULL) {
		buffer = g_string_append (buffer, script_data_to_string
                                (list->data));
		list = list->next;
	}
	return buffer->str;
}

/* casts data to a string */
static char *
script_data_to_string (ScriptData *data)
{
	if (data == NULL) return g_strdup ("");

	switch (data->type) {
		case SCRIPT_TYPE_STRING:
			return g_strdup (data->value.as_string);

		case SCRIPT_TYPE_INTEGER:
			return ltoa (data->value.as_integer);

		case SCRIPT_TYPE_VARIABLE:
			return script_data_to_string (script_variable_lookup
					(data->value.as_string));

		default:
			g_assert_not_reached ();
			return g_strdup ("");
	}
}

/* casts data to an integer */
static long
script_data_to_integer (ScriptData *data)
{
	switch (data->type) {
		case SCRIPT_TYPE_STRING:
			{
				long result;
				result = strtol (data->value.as_string, NULL,
						10);
				if (errno != 0) {
					if (result == LONG_MAX
							|| result == LONG_MIN) {
						script_warn ("Number overflow");
					} else if (result == 0) {
						script_error ("Could not convert string to integer");
					} else {
						script_error ("Unknown error");
					}
				}
				return result;
			}

		case SCRIPT_TYPE_INTEGER:
			return data->value.as_integer;

		case SCRIPT_TYPE_VARIABLE:
			return script_data_to_integer (script_variable_lookup
					(data->value.as_string));

		default:
			g_assert_not_reached ();
			return 0;
	}
}

/*** script helper functions ***/

/* displays an error message when something goes wrong in a script and needs
 * to be reported to the player
 */
static void
script_error (const char *fmt, ...)
{
	va_list list;
	char *str;

	// Stop the script
	script_running = FALSE;

	va_start (list, fmt);
	g_vasprintf (&str, fmt, list);
	va_end (list);

	gdk_threads_enter ();
	echo_f ("*** Error in script at line %d: %s ***\n",
			((ScriptCommand*)curr_command->data)->line_number, str);
	gdk_threads_leave ();

	g_free (str);
}

/* displays an error message when something goes wrong in a script and needs
 * to be reported to the player
 */
static void
script_warn (const char *fmt, ...)
{
	va_list list;
	char *str;

	va_start (list, fmt);
	g_vasprintf (&str, fmt, list);
	va_end (list);

	gdk_threads_enter ();
	echo_f ("*** Warning in script at line %d: %s ***\n",
			((ScriptCommand*)curr_command->data)->line_number, str);
	gdk_threads_leave ();

	g_free (str);
}

/* set the buffer to label, or stop execution until we find label */
/* label must be lowercase and stripped of whitespace */
static gboolean
goto_label (const char *label)
{
	gpointer value;

	value = g_hash_table_lookup (labels, label);

	if (value != NULL) {
		next_command = value;
	} else {
		value = g_hash_table_lookup (labels, "labelerror");
		if (value == NULL) {
			script_error ("Could not find label \"%s\"", label);
			return FALSE;
		} else {
			next_command = value;
		}
	}

	return TRUE;
}

static char *
get_line (void)
{
        char *line;

        do {
                GTimeVal time;

                g_get_current_time (&time);
                g_time_val_add (&time, SCRIPT_TIMEOUT);
                line = g_async_queue_timed_pop (script_line_queue, &time);
        } while (line == NULL && script_running);

        return line;
}

/* Gets current line number */
guint script_get_linenum(void) {
        return (script_running && curr_command != NULL
			&& curr_command->data != NULL)
		? ((ScriptCommand*)curr_command->data)->line_number
		: 0;
}

static void
clear_line_queue (void)
{
        while (g_async_queue_try_pop (script_line_queue) != NULL) ;
}

/* you must hold mutex to call this function */
static void
next_room_wait (GMutex *mutex)
{
        gboolean moved;

        do {
                GTimeVal time;

                g_get_current_time (&time);
                g_time_val_add (&time, SCRIPT_TIMEOUT);
                moved = g_cond_timed_wait (move_cond, mutex, &time);
        } while (!moved && script_running);
}

/*** script commands ***/

/* shifts numbered variables down one, remove %1 */
static void
script_shift (GList *args)
{
	int i;

        for (i = 1; ; i++) {
		ScriptData *arg;

                arg = script_variable_lookup (itoa (i + 1));
                if (arg == NULL) {
			script_variable_unset (itoa (i));
                        break;
                }
		script_variable_set (itoa (i), arg);
        }
}

/* installs a match callback */
/* arg 1: label, arg 2+: string to match */
static void
script_match (GList *args)
{
	MatchData *match_data;

	match_data = g_new (MatchData, 1);

	match_data->label = g_strstrip (g_ascii_strdown (script_data_to_string
				(args->data), -1));
	match_data->string = g_strstrip (g_ascii_strdown (script_list_to_string
				(args->next), -1));

	g_mutex_lock (script_mutex);
	if (match_list == NULL) clear_line_queue ();

        match_data->regex = NULL;
        match_data->regex_extra = NULL;
	match_list = g_list_append (match_list, match_data);
	g_mutex_unlock (script_mutex);
}

/* installs a match regex callback */
/* arg 1: label, arg 2+: regex to match against */
static void
script_matchre (GList *args)
{
	MatchData *match_data;
        const char *err;
        int err_offset;

	match_data = g_new (MatchData, 1);

	match_data->label = script_data_to_string (args->data);
	match_data->string = script_list_to_string (args->next);

        err = NULL;
        match_data->regex = pcre_compile (match_data->string, 0, &err,
                        &err_offset, NULL);
        if (err != NULL) {
                script_error ("Invalid regex");
		return;
        }
        match_data->regex_extra = pcre_study (match_data->regex, 0, &err);
        if (err != NULL) {
                script_error ("Invalid regex");
		return;
        }
	g_mutex_lock (script_mutex);
	match_list = g_list_append (match_list, match_data);
	g_mutex_unlock (script_mutex);
}

/* match a line against a regexp */
static gboolean
regexp_match (pcre *regex, pcre_extra *regex_extra, char *line)
{
	int n_matches;
	int matches[32 * 3];

	n_matches = pcre_exec (regex, regex_extra, line, strlen (line), 0, 0,
			matches, 32 * 3);
	if (n_matches >= 0) {
		int i;
		for (i = 0; i <= n_matches; i++) {
			char name[10];
			ScriptData *data;
			g_snprintf (name, 10, "match%d", i);
			data = string_to_script_data (g_strndup (
						line + matches[i * 2],
						matches[i * 2 + 1]));
		}
		return TRUE;
	}
	return FALSE;
}

/* waits for a match callback to return, then clears them out */
static void
script_matchwait (GList *args)
{
	while (match_list != NULL && script_running) {
                GList *curr_match;
                char *line;

                line = get_line ();

                if (line == NULL) {
                        break;
                }

                g_mutex_lock (script_mutex);

                //debug ("Awoke script_matchwait, line: %s\n", line);
                curr_match = match_list;
                while (curr_match != NULL) {
                        MatchData *match_data;

                        match_data = curr_match->data;
                        if (match_data->regex == NULL) {
				//debug ("matching string: %s\n", match_data->string);
                                if (strstr (line, match_data->string) != NULL) {
                                        goto_label (match_data->label);
                                        g_list_free (match_list);
                                        match_list = NULL;
                                        //debug ("done with matchwait");
                                        break;
                                }
                        } else {
				if (regexp_match (match_data->regex,
							match_data->regex_extra,
							line)) {
                                        goto_label (match_data->label);
                                        g_list_free (match_list);
                                        match_list = NULL;
                                        break;
                                }
                        }
                        curr_match = curr_match->next;
                }
                g_mutex_unlock (script_mutex);
	}

	warlock_roundtime_wait (&script_running);
}

/* waits for a string to match */
static void
script_waitfor (GList *args)
{
	char *waitfor_string;
        char *line;

	g_mutex_lock (script_mutex);

	waitfor_string = g_strstrip (g_ascii_strdown (script_list_to_string
				(args), -1));
	debug ("waitfor string: %s\n", waitfor_string);

        clear_line_queue ();

	g_mutex_unlock (script_mutex);

	/* we get woken up to check each string */
        do {
                line = get_line ();
        } while (line != NULL && strstr (line, waitfor_string) == NULL);

        // wait for the roundtime to end
	warlock_roundtime_wait (&script_running);	
}

/* waits for a regexp to match */
static void
script_waitforre (GList *args)
{
	pcre *regex;
	pcre_extra *regex_extra;
	char *str;
        char *line;
        const char *err;
        int err_offset;

	str = g_strstrip (g_ascii_strdown (script_list_to_string (args), -1));
	debug ("waitforre regex: %s\n", str);

        err = NULL;
        regex = pcre_compile (str, 0, &err, &err_offset, NULL);
        if (err != NULL) {
                script_error ("Invalid regex");
		return;
        }
        regex_extra = pcre_study (regex, 0, &err);
        if (err != NULL) {
                script_error ("Invalid regex");
		return;
        }

	g_mutex_lock (script_mutex);
        clear_line_queue ();
	g_mutex_unlock (script_mutex);

	/* we get woken up to check each string */
        do {
                line = get_line ();
        } while (line != NULL && !regexp_match (regex, regex_extra, line));

        // wait for the roundtime to end
	warlock_roundtime_wait (&script_running);	
}

/* manipulate the %c variable (set,add,subtract,multiply,divide) */
static void
script_counter (GList *args)
{
	ScriptData *old_counter;
	int new_counter;
	char *operation;
	int rhs;

        if (args == NULL || args->data == NULL || args->next == NULL ||
			args->next->data == NULL) {
                script_error ("Not enough arguments to counter");
                return;
        }

	old_counter = script_variable_lookup ("c");

	if (old_counter == NULL) {
		new_counter = 0;
	} else {
                new_counter = script_data_to_integer (old_counter);
        }

        rhs = script_data_to_integer (args->next->data);
	operation = g_strstrip (g_ascii_strdown (script_data_to_string
				(args->data), -1));

        if (strcmp(operation, "set") == 0) {
                new_counter = rhs;
        } else if (strcmp(operation, "add") == 0) {
                new_counter += rhs;
        } else if (strcmp(operation, "subtract") == 0) {
                new_counter -= rhs;
        } else if (strcmp(operation, "multiply") == 0) {
                new_counter *= rhs;
        } else if (strcmp(operation, "divide") == 0) {
                new_counter /= rhs;
        } else {
                script_error ("Unrecognized counter operation \"%s\"",
				operation);
		return;
        }

        script_variable_set ("c", integer_to_script_data (new_counter));
}

/* send the strings to the server */
static void
script_put (GList *args)
{
	// Make sure we got a prompt since the last put
	g_mutex_lock (script_mutex);
	if (!prompt_since_put) {
		g_cond_wait(wait_cond,script_mutex);
	}
	prompt_since_put = FALSE;
	g_mutex_unlock (script_mutex);

	gdk_threads_enter ();
	warlock_send ("%s", script_list_to_string (args));
	gdk_threads_leave ();
}

/* position the running script at label */
static void
script_goto (GList *args)
{
	char *label;

	label = g_strstrip (g_ascii_strdown (script_list_to_string (args), -1));
        if (*label == '\0') {
                script_error ("GOTO with no label specified");
                return;
        }
        goto_label (label);
}

/* put direction, then wait for the player to move */
static void
script_move (GList *args)
{
	// Always acquire the gdk thread before the script_mutex
	gdk_threads_enter ();
	g_mutex_lock (script_mutex);
	warlock_send ("%s", script_list_to_string (args));
	gdk_threads_leave ();

        next_room_wait (script_mutex);
	g_mutex_unlock (script_mutex);

	warlock_roundtime_wait (&script_running);
}

/* pause for a number of seconds */
static void
script_pause (GList *args)
{
	int pause_time;

	if (args == NULL ||  args->data == NULL) {
		pause_time = 1;
	} else {
		pause_time = script_data_to_integer (args->data);
		if (pause_time < 1) {
			pause_time = 1;
		}
	}

        warlock_pause_wait (pause_time, &script_running);
	warlock_roundtime_wait (&script_running);	
}

/* prints the strings to the screen followed by a newline */
static void
script_echo (GList *args)
{
	gdk_threads_enter ();
	echo_f ("%s\n", script_list_to_string (args));
	gdk_threads_leave ();
}

/* wait for a status prompt */
static void
script_wait (GList *args)
{
        gboolean got_wait;

	g_mutex_lock (script_mutex);

        // wait for a prompt
        do {
                GTimeVal time;

                g_get_current_time (&time);
                g_time_val_add (&time, SCRIPT_TIMEOUT);
                got_wait = g_cond_timed_wait (wait_cond, script_mutex, &time);
        } while (!got_wait && script_running);

	g_mutex_unlock (script_mutex);

        // wait for the roundtime to end
	warlock_roundtime_wait (&script_running);	
}

static void
script_exit (GList *args)
{
	script_running = FALSE;
}

static void
script_nextroom (GList *args)
{
        g_mutex_lock (script_mutex);
        next_room_wait (script_mutex);
        g_mutex_unlock (script_mutex);
}

static void
script_save (GList *args)
{
        char *str, *orig;

        str = script_list_to_string (args);
	orig = str;
	/* if we have matching "s or 's at the beginning and end of the string,
	 * remove them */
        if (*str == '"' || *str == '\'') {
                char *last_char;

                last_char = &str[strlen (str) - 1];
                if (*last_char == *str) {
                        str++;
                        *last_char = '\0';
                }
        }

        script_variable_set ("s", string_to_script_data (str));
	g_free (orig);
}

/* Support for user-created variables */
static void
script_setvariable (GList *args)
{
	ScriptData *value;
	char *name;

	if (args == NULL || args->data == NULL || args->next == NULL ||
			args->next->data == NULL) {
		script_error ("Not enough arguments to SETVARIABLE");
		return;
	}

	name = g_strstrip (g_ascii_strdown (script_data_to_string (args->data),
				-1));

	value = string_to_script_data (g_strstrip (script_list_to_string
			(args->next)));
        script_variable_set (name, value);
}

/* delete user-created variables */
static void
script_deletevariable (GList *args)
{
	char *name;

	if (args == NULL || args->data == NULL) {
		script_error ("Not enough arguments to DELETEVARIABLE");
		return;
	}

	name = g_strstrip (g_ascii_strdown (script_data_to_string (args->data),
				-1));

        script_variable_unset (name);
}

/* call a function (like goto, but returnable) */
static void
script_call (GList *args)
{
	char *label;

	if (args == NULL || args->data == NULL) {
		script_error ("Not enough arguments to CALL.");
		return;
	}

	label = g_strstrip (g_ascii_strdown (script_data_to_string (args->data),
				-1));
        if (*label == '\0') {
                script_error ("CALL with no label specified");
                return;
        }
	g_mutex_lock (script_mutex);
	position_stack = g_slist_prepend (position_stack, next_command);
	g_mutex_unlock (script_mutex);

	goto_label (label);
}

/* return from a function. return to the place the function was called from */
static void
script_return (GList *args)
{
	char *operation;

	g_mutex_lock (script_mutex);
	if (args == NULL || args->data == NULL || args->next == NULL ||
			args->next->data == NULL) {
		next_command = position_stack->data;
		position_stack = g_slist_delete_link (position_stack, position_stack);
	} else {
		operation = g_strstrip (g_ascii_strdown (script_data_to_string
					(args->data), -1));
		if (strcmp(operation, "cancel") == 0) {
			position_stack = g_slist_delete_link (position_stack, position_stack);
		} else {
			/* Remove Lock Before Generating Error */
			g_mutex_unlock (script_mutex);
			script_error("Unknown Return Operation");
			return;
		}
	}
	g_mutex_unlock (script_mutex);
}

/* assign %r a random number in the range arg1 - arg2
 * If only given 1 parameter, range is 0 - arg1
 * if given no parameters 0 - 100
 */
static void
script_random (GList *args)
{
	static GRand *rand = NULL;
	ScriptData *randdata;
	int upper, lower;

	if (rand == NULL) {
		rand = g_rand_new ();
	}

	if (args == NULL || args->data == NULL) {
		lower = 0;
		upper = 100;
	} else if (args->next == NULL || args->next->data == NULL) {
		lower = 0;
		upper = script_data_to_integer (args->data);
        } else {
		lower = script_data_to_integer (args->data);
		upper = script_data_to_integer (args->next->data);
	}

	// 1 is added to upper so that it will include the upper number
	randdata = integer_to_script_data (g_rand_int_range (rand, lower,
				upper + 1));
	
	script_variable_set ("r", randdata);
}
