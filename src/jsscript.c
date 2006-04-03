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

#include <string.h>

#define XP_UNIX
#define JS_THREADSAFE
#include <jsapi.h>

#include <gtk/gtk.h>

#include "debug.h"
#include "jsscript.h"
#include "status.h"
#include "hand.h"
#include "warlock.h"
#include "warlockview.h"
#include "warlocktime.h"
#include "preferences.h"

#define JS_SCRIPT_TIMEOUT       100000
#define JS_HEAP_SIZE            1024L * 1024L
#define JS_CONTEXT_SIZE         (8L * 1024L)

/*** data types ***/
typedef struct _JSScriptData JSScriptData;
typedef struct _JSCallbackData JSCallbackData;

struct _JSScriptData {
	char *filename;
        guint id;
        gboolean running;
        gboolean suspended;
        GMutex *lock;                   // must hold this to use the global obj
        GCond *suspend_cond;            // signaled when the script is resumed
        JSContext *context;        // context for the main thread
        GAsyncQueue *line_queue;        // queue of lines coming from the server
        GSList *callbacks;      // functions to be called on every incoming line
        int callback_pos;       // current callback function
        int argc;
        char **argv;
        JSObject *global;       // global environment for the script
};

struct _JSCallbackData {
        JSFunction *function;
        JSObject *obj; // the object representing "this"
        jsval *argv;
	int argc;
	JSContext *context;	// context to which these objects are bound
	int32 id;
};

/*** global variables ***/

/* list of the data for each running script */
static GSList *script_data_list = NULL;

/* mutex that must be locked for any access to script_data_list */
static GMutex *js_script_mutex = NULL;

/* condition signaled when the player has moved */
static GCond *js_move_cond = NULL;

/* JS runtime all scripts are executed within */
static JSRuntime *js_runtime = NULL;

/*** local functions headers ***/
/* functions to use in javascript */
static JSBool js_warlock_send (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);
static JSBool js_warlock_move (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);
static JSBool js_warlock_nextRoom (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);
static JSBool js_warlock_addCallback (JSContext *cx, JSObject *obj,
                uintN argc, jsval *argv, jsval *rval);
static JSBool js_warlock_removeCallback (JSContext *cx, JSObject *obj,
                uintN argc, jsval *argv, jsval *rval);
static JSBool js_warlock_rightHandContents (JSContext *cx, JSObject *obj,
                uintN argc, jsval *argv, jsval *rval);
static JSBool js_warlock_leftHandContents (JSContext *cx, JSObject *obj,
                uintN argc, jsval *argv, jsval *rval);
static JSBool js_warlock_echo (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);
static JSBool js_warlock_debug (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);
static JSBool js_warlock_pause (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);
static JSBool js_warlock_getRoundtime (JSContext *cx, JSObject *obj,
                uintN argc, jsval *argv, jsval *rval);
static JSBool js_warlock_waitRT (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);

/* other functions */
static gpointer callback_thread_loop (gpointer user_data);
static void js_script_toggle_real (JSScriptData *data);
static guint next_id (void);
static gpointer js_script_thread (gpointer user_data);
static JSScriptData* get_data_by_id (guint id);
static gint compare_id (gconstpointer a, gconstpointer b);
static void js_script_stop_real (JSScriptData* data);
static JSBool js_queue_push (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);
static JSBool js_queue_pop (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval);
static gboolean remove_callback (JSScriptData *data, int id);

#define JS_STATUS_METHOD(status) \
        static JSBool \
js_warlock_##status (JSContext *cx, JSObject *obj, uintN argc, \
                jsval *argv, jsval *rval) \
{ \
        *rval = is_##status () ? JSVAL_TRUE : JSVAL_FALSE; \
        return JS_TRUE; \
}

/* status functions create from macros */
JS_STATUS_METHOD(standing);
JS_STATUS_METHOD(sitting);
JS_STATUS_METHOD(kneeling);
JS_STATUS_METHOD(lying);
JS_STATUS_METHOD(dead);
JS_STATUS_METHOD(bleeding);
JS_STATUS_METHOD(stunned);
JS_STATUS_METHOD(webbed);
JS_STATUS_METHOD(hidden);
JS_STATUS_METHOD(joined);

// JS global method implementation table
static JSFunctionSpec warlock_methods[] = {
        {"send", js_warlock_send, 1, 0, 0},
        {"put", js_warlock_send, 1, 0, 0},
        {"move", js_warlock_move, 1, 0, 0},
        {"nextRoom", js_warlock_nextRoom, 0, 0, 0},
        {"addCallback", js_warlock_addCallback, 1, 0, 0},
        {"removeCallback", js_warlock_removeCallback, 1, 0, 0},
        {"rightHandContents", js_warlock_rightHandContents, 0, 0, 0},
        {"leftHandContents", js_warlock_leftHandContents, 0, 0, 0},
        {"standing", js_warlock_standing, 0, 0, 0},
        {"sitting", js_warlock_sitting, 0, 0, 0},
        {"kneeling", js_warlock_kneeling, 0, 0, 0},
        {"lying", js_warlock_lying, 0, 0, 0},
        {"dead", js_warlock_dead, 0, 0, 0},
        {"bleeding", js_warlock_bleeding, 0, 0, 0},
        {"stunned", js_warlock_stunned, 0, 0, 0},
        {"webbed", js_warlock_webbed, 0, 0, 0},
        {"hidden", js_warlock_hidden, 0, 0, 0},
        {"joined", js_warlock_joined, 0, 0, 0},
        {"echo", js_warlock_echo, 1, 0, 0},
        {"debug", js_warlock_debug, 1, 0, 0},
        {"pause", js_warlock_pause, 0, 0, 0},
        {"getRoundtime", js_warlock_getRoundtime, 0, 0, 0},
        {"waitRT", js_warlock_waitRT, 0, 0, 0},
        {NULL, NULL, 0, 0, 0}
};

// JS queue method table
static JSFunctionSpec queue_methods[] = {
        {"push", js_queue_push, 1, 0, 0},
        {"pop", js_queue_pop, 0, 0, 0},
        {NULL, NULL, 0, 0, 0}
};

// global class signature
static JSClass global_class = {
        "global",
        0,
        JS_PropertyStub,
        JS_PropertyStub,
        JS_PropertyStub,
        JS_PropertyStub,
        JS_EnumerateStub,
        JS_ResolveStub,
        JS_ConvertStub,
        JS_FinalizeStub,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0
};

// queue class signature
static JSClass queue_class = {
        "Queue",
        JSCLASS_HAS_PRIVATE,
        JS_PropertyStub,
        JS_PropertyStub,
        JS_PropertyStub,
        JS_PropertyStub,
        JS_EnumerateStub,
        JS_ResolveStub,
        JS_ConvertStub,
        JS_FinalizeStub,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	0
};

/*** global function definitions ***/

/* Initialize the javascript runtime (call during program init) */
gboolean
js_script_init (void)
{
        js_runtime = JS_NewRuntime (JS_HEAP_SIZE);

        if(!js_runtime)
                return FALSE;

        js_move_cond = g_cond_new ();
        js_script_mutex = g_mutex_new ();

        return TRUE;
}

/* start a script */
void
js_script_load (const char *filename, int argc, const char **argv)
{
        JSScriptData *data;

        debug ("js_script_load(%s, %d, ...)\n", filename, argc);

        data = g_new (JSScriptData, 1);

        data->suspended = FALSE;
	data->filename = g_strdup (filename);
        data->id = next_id ();
        data->running = TRUE;
        data->line_queue = g_async_queue_new ();
        data->callbacks = NULL;
        data->callback_pos = 0;
        data->argc = 0;
        data->argv = NULL;
        data->suspend_cond = g_cond_new ();
        data->lock = g_mutex_new ();

        g_mutex_lock (js_script_mutex);
        script_data_list = g_slist_append (script_data_list, data);
        g_mutex_unlock (js_script_mutex);

        g_thread_create (js_script_thread, data, FALSE, NULL);
}

/* send a line to all running scripts */
void
js_script_got_line (const char *line)
{
        GSList *cur;

        g_mutex_lock (js_script_mutex);
        for (cur = script_data_list; cur != NULL; cur = cur->next) {
                JSScriptData *data;

                data = cur->data;
                if (!data->suspended && data->running) {
                        g_async_queue_push (data->line_queue, g_strdup (line));
                }
        }
        g_mutex_unlock (js_script_mutex);
}

/* return the JS status part of the prompt */
char*
js_script_get_prompt_string (void)
{
        GString* string;
        char* str;
        GSList* cur;

        g_mutex_lock (js_script_mutex);
        if (script_data_list == NULL) {
                str = NULL;
        } else {
                string = g_string_new ("[jsscript");
                for (cur = script_data_list; cur != NULL; cur = cur->next) {
                        g_string_append_printf (string,
                                        " %d", ((JSScriptData*)cur->data)->id);
                }

                g_string_append_c (string, ']');
                str = string->str;
                g_string_free (string, FALSE);
        }
        g_mutex_unlock (js_script_mutex);
        return str;
}

/* signal anyone wondering that we've moved */
void
js_script_moved (void)
{
        g_mutex_lock (js_script_mutex);
        g_cond_broadcast (js_move_cond);
        g_mutex_unlock (js_script_mutex);
}

/* stop all scripts */
void
js_script_stop_all (void)
{
        GSList *cur;

        g_mutex_lock (js_script_mutex);
        for (cur = script_data_list; cur != NULL; cur = cur->next) {
                js_script_stop_real (cur->data);
        }
        g_mutex_unlock (js_script_mutex);
}

/* stop a script by its id */
gboolean
js_script_stop (guint id)
{
        JSScriptData *data;
        gboolean rv;

        g_mutex_lock (js_script_mutex);

        data = get_data_by_id (id);
        if (data != NULL) {
                js_script_stop_real (data);
                rv = TRUE;
        } else {
                rv = FALSE;
        }

        g_mutex_unlock (js_script_mutex);

        return rv;
}

/* suspend/resume all scripts */
void
js_script_toggle_all (void)
{
        GSList *cur;

        g_mutex_lock (js_script_mutex);
        for (cur = script_data_list; cur != NULL; cur = cur->next) {
                js_script_toggle_real (cur->data);
        }
        g_mutex_unlock (js_script_mutex);
}

/* suspend/resume a script by its id */
gboolean
js_script_toggle (guint id)
{
        GSList *link;
        gboolean rv;

        g_mutex_lock (js_script_mutex);
        link = g_slist_find_custom (script_data_list, GINT_TO_POINTER (id),
                        compare_id);
        if (link != NULL) {
                js_script_toggle_real (link->data);
                rv = TRUE;
        } else {
                rv = FALSE;
        }
        g_mutex_unlock (js_script_mutex);

        return rv;
}

/*** local functions definitions ***/

/** general functions **/

/* send all javascript errors to the main window */
static void
js_error_reporter (JSContext *cx, const char *message, JSErrorReport *report)
{
        JSScriptData *data;
        const char *str;

        data = JS_GetContextPrivate (cx);

        str = ((cx == data->context) ? "main context" : "callback context");
        gdk_threads_enter ();
        echo_f ("JS script #%d (%s) error: %s\n", data->id, str, message);
        gdk_threads_leave ();
}

/* stop a script */
static void
js_script_stop_real (JSScriptData* data)
{
        if (data->suspended) {
                js_script_toggle_real (data);
                return;
        }

        if (data->running) {
                g_mutex_unlock (js_script_mutex);
                echo_f ("*** killed JS script %d ***\n", data->id);
                g_mutex_lock (js_script_mutex);
                data->running = FALSE;
        }
}

/* suspend/resume a script
 * this function must be called from inside a js_script_mutex lock and from
 * within the gdk thread context */
static void
js_script_toggle_real (JSScriptData *data)
{
        if (data->suspended) {
                g_mutex_unlock (js_script_mutex);
                echo_f ("*** JS Script %d resumed ***\n", data->id);
                g_mutex_lock (js_script_mutex);
                data->suspended = FALSE;
                g_cond_broadcast (data->suspend_cond);
        } else {
                g_mutex_unlock (js_script_mutex);
                echo_f ("*** JS Script %d paused ***\n", data->id);
                g_mutex_lock (js_script_mutex);
                data->suspended = TRUE;
        }
}

/* function to return false when a script has been stopped or to pause it when
 * it has been suspended */
static JSBool
branch_callback (JSContext *cx, JSScript *script)
{
        JSScriptData *data;
        jsrefcount depth;

        data = JS_GetContextPrivate (cx);

        // Make sure we should still be running
        if (!data->running) {
                return JS_FALSE;
        }

        depth = JS_SuspendRequest (cx);
        //g_mutex_unlock (data->lock);
        g_mutex_lock (js_script_mutex);
        while (data->suspended) {
                g_cond_wait (data->suspend_cond, js_script_mutex);
        }
        g_mutex_unlock (js_script_mutex);
        //g_mutex_lock (data->lock);
        JS_ResumeRequest (cx, depth);

        return JS_TRUE;
}

/* constructor for our JS async queue */
static JSBool
js_queue_constructor (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        GAsyncQueue *queue;

        queue = g_async_queue_new ();

        JS_BeginRequest (cx);
        if (!JS_SetPrivate (cx, obj, queue)) {
                return JS_FALSE;
        }
        JS_EndRequest (cx);

        *rval = OBJECT_TO_JSVAL (obj);
        return JS_TRUE;
}

/* push method for our JS async queue */
static JSBool
js_queue_push (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        GAsyncQueue *queue;
        JSObject *data;

	debug ("js_queue_push\n");

        JS_BeginRequest (cx);
        if (!JS_ConvertArguments (cx, argc, argv, "o", data)) {
                debug ("error parsing arguments to put\n");
                return JS_FALSE;
        }
        queue = JS_GetPrivate (cx, obj);

	debug ("queue: %p", queue);

	g_assert (queue != NULL);
        JS_EndRequest (cx);

        g_async_queue_push (queue, data);

        return JS_TRUE;
}

/* pop method for our JS async queue */
static JSBool
js_queue_pop (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        GAsyncQueue *queue;
        JSObject *queued_obj;
        JSScriptData *sdata;
        GTimeVal time;
        jsrefcount depth;

        debug ("js_queue_pop");
	
        JS_BeginRequest (cx);
        sdata = JS_GetContextPrivate (cx);

        queue = JS_GetPrivate (cx, obj);

	debug ("queue: %p", queue);

	g_assert (queue != NULL);
        JS_EndRequest (cx);

        depth = JS_SuspendRequest (cx);
        //g_mutex_unlock (data->lock);

        g_get_current_time (&time);
        g_time_val_add (&time, JS_SCRIPT_TIMEOUT);
        do {
                queued_obj = g_async_queue_timed_pop (queue, &time);
                if (!sdata->running) {
                        debug ("js_queue_pop: no longer running\n");
                        return JS_FALSE;
                }
        } while (queued_obj == NULL);
        //g_mutex_lock (data->lock);
        JS_ResumeRequest (cx, depth);

        *rval = OBJECT_TO_JSVAL (queued_obj);

        return JS_TRUE;
}

/* main thread/function for executing our script
 * TODO look into merging this with js_script_load */
static gpointer
js_script_thread (gpointer user_data)
{
        JSScriptData *data;
        GThread *callback_thread;
        char *script, *filename;
        gsize script_len;
        JSObject *queue_object;
        JSBool builtins;
        GError *err;
        gboolean natural_ending;
        jsval rval;

        gdk_threads_enter ();
        echo ("*** starting JavaScript script ***\n");
        gdk_threads_leave ();

        data = user_data;

        debug ("Loading JS %s\n", data->filename);

        /* create the script context */
        data->context = JS_NewContext (js_runtime, JS_CONTEXT_SIZE);
        JS_BeginRequest (data->context);
        //g_mutex_lock (data->lock);

        /* define script environment */
        JS_SetErrorReporter (data->context, js_error_reporter);
        data->global = JS_NewObject(data->context, &global_class, NULL, NULL);
        builtins = JS_InitStandardClasses(data->context, data->global);
        JS_DefineFunctions (data->context, data->global, warlock_methods);
        JS_SetBranchCallback (data->context, branch_callback);
	JS_SetContextPrivate (data->context, data);

        /* TODO set argv and argc here */

        /* initialize queue object */
        queue_object = JS_InitClass (data->context, data->global, NULL, &queue_class,
                        js_queue_constructor, 0, NULL, queue_methods, NULL,
                        NULL);

        /* initialize simple JS library */
	filename = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, "warlock.js",
			NULL);
        err = NULL;
        if (g_file_get_contents (filename, &script, &script_len, &err)) {
                if (!JS_EvaluateScript (data->context, data->global, script, script_len,
                                        data->filename, 0, &rval)) {
                        debug ("there was a problem in the Warlock JS library\n");
                        // TODO do something here
                }
                g_free (script);
        } else {
                print_error (err);
                // TODO do something here
        }

	g_free (filename);

        //g_mutex_unlock(data->lock);

	JS_AddRoot (data->context, data->global);

        /* start up the callback thread */
        err = NULL;
        callback_thread = g_thread_create (callback_thread_loop, data,
                        TRUE, &err);
        print_error (err);

        /* load and run the requested script */
        err = NULL;
        if (g_file_get_contents (data->filename, &script, &script_len, &err)) {
                if (!JS_EvaluateScript (data->context, data->global, script, script_len,
                                        data->filename, 0, &rval)) {
                        debug ("script returned false\n");
                }
                g_free (script);
        } else {
                print_error (err);
        }

        /* if running was previously set to false, the script didn't come
         * to a natural conclusion */
        natural_ending = data->running;
        data->running = FALSE;

        JS_RemoveRoot(data->context, data->global);

	g_mutex_lock (js_script_mutex);
	while (data->callbacks != NULL) {
		g_assert (remove_callback (data,
				((JSCallbackData*)data->callbacks->data)->id));
	}
	g_mutex_unlock (js_script_mutex);

        /* destroy the context */
        JS_DestroyContext (data->context);

        /* make sure the callback_thread has exited so we don't print the
         * finished method while there's a chance that something might still
         * happen behind us */
        g_thread_join (callback_thread);

        /* remove this script from the list */
        g_mutex_lock (js_script_mutex);
        script_data_list = g_slist_remove (script_data_list, data);
        g_mutex_unlock (js_script_mutex);

        if (natural_ending) {
                gdk_threads_enter ();
                echo ("*** finished JS script ***\n");
                gdk_threads_leave ();
        }

        return NULL;
}

/* find our data by an id
 * js_script_mutex must be held when this function is called */
static JSScriptData*
get_data_by_id (guint id)
{
        GSList *link;
        link = g_slist_find_custom (script_data_list, GINT_TO_POINTER (id),
                        compare_id);
        if (link != NULL) {
                return link->data;
        }

        return NULL;
}

/* generate an id */
static guint
next_id (void)
{
        int id;

        g_mutex_lock (js_script_mutex);
        for (id = 1; ; id++) {
                GSList *cur;

                for (cur = script_data_list; cur != NULL; cur = cur->next) {
                        if (((JSScriptData*)cur->data)->id == id) {
                                break;
                        }
                }
                if (cur == NULL) {
                        break;
                }
        }
        g_mutex_unlock (js_script_mutex);

        return id;
}

/* helper function to compare ids */
static gint
compare_id (gconstpointer a, gconstpointer b)
{
        return ((JSScriptData*)a)->id == GPOINTER_TO_INT (b);
}

/* function/thread where callbacks for a script are executed */
static gpointer
callback_thread_loop (gpointer user_data)
{
        JSScriptData *data;
        JSContext *cx;

        data = user_data;

        /* create callback context */
        cx = JS_NewContext (js_runtime, JS_CONTEXT_SIZE);
        JS_BeginRequest (cx);

        /* setup the callback enviroment to be the same as for the main
         * context */
        JS_SetBranchCallback (cx, branch_callback);
        JS_AddRoot(cx, data->global);
        JS_SetGlobalObject (cx, data->global);
	JS_SetContextPrivate (cx, data);

        /* loop until the script is done */
        for (;;) {
                char *line;
                GTimeVal time;

                /* wait a short time for a line */
                g_get_current_time (&time);
                g_time_val_add (&time, JS_SCRIPT_TIMEOUT);
                do {
                        line = g_async_queue_timed_pop (data->line_queue,
                                        &time);
                } while (line == NULL && data->running);

                /* if the script ended before we got the line, quit */
                if (!data->running) {
                        break;
                }

                /* execute each callback with the line we just got */
                debug ("doing Callbacks\n");
                for (;;) {
                        JSCallbackData *callback;
                        jsval rval;

                        /* find our callback in the list */
                        g_mutex_lock (js_script_mutex);
                        if (data->callback_pos >= g_slist_length
                                        (data->callbacks)) {
                                g_mutex_unlock (js_script_mutex);
                                break;
                        }
                        callback = g_slist_nth_data (data->callbacks,
                                        data->callback_pos);
                        g_assert (callback != NULL);
                        g_mutex_unlock (js_script_mutex);

                        data->callback_pos++;

                        /* call our callback */
                        //g_mutex_lock (data->lock);
                        JS_BeginRequest (cx);

                        callback->argv[0] = STRING_TO_JSVAL
				(JS_NewStringCopyZ (cx, line));
                        JS_CallFunction (cx, callback->obj, callback->function,
                                        callback->argc, callback->argv, &rval);

			// if the function returned false, remove the callback
			if (JSVAL_IS_BOOLEAN (rval) &&
					!JSVAL_TO_BOOLEAN (rval)) {
				g_assert (remove_callback (data, callback->id));
			}
                        JS_EndRequest (cx);
                        //g_mutex_unlock (data->lock);

                        debug ("executed function\n");
                }
                debug ("done with Callbacks\n");

                data->callback_pos = 0;

                g_free (line);
        }

        JS_RemoveRoot(cx, data->global);
        JS_EndRequest (cx);
        // FIXME for some reason the following line segfaults
        //JS_DestroyContext (cx);

        return NULL;
}

/** helpers for script functions **/

static void
echo_real (JSContext *cx, char *str)
{
        jsrefcount depth;

        depth = JS_SuspendRequest (cx);
        debug ("echo_real depth: %d\n", depth);
        gdk_threads_enter ();
        echo (str);	
        gdk_threads_leave ();
        JS_ResumeRequest (cx, depth);
}

static void
send_real (JSContext *cx, char *str)
{
        jsrefcount depth;

        depth = JS_SuspendRequest (cx);
        gdk_threads_enter ();
        warlock_send ("%s\n", str);	
        gdk_threads_leave ();
        JS_ResumeRequest (cx, depth);
}

static void
js_warlock_next_room_wait (JSContext *cx)
{
        gboolean moved;
        JSScriptData *data;
        jsrefcount depth;

        data = JS_GetContextPrivate (cx);

        depth = JS_SuspendRequest (cx);
        do {
                GTimeVal time;

                g_get_current_time (&time);
                g_time_val_add (&time, JS_SCRIPT_TIMEOUT);
                g_mutex_lock (js_script_mutex);
                moved = g_cond_timed_wait (js_move_cond, js_script_mutex,
                                &time);
                g_mutex_unlock (js_script_mutex);
        } while (!moved && data->running);
        JS_ResumeRequest (cx, depth);
}

/** script functions **/

static JSBool
js_warlock_send (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        char *command;

        JS_BeginRequest (cx);
        if (!JS_ConvertArguments (cx, argc, argv, "s", &command)) {
                debug ("send: parameter mismatch\n");
                return JS_FALSE;
        }
        JS_EndRequest (cx);

        send_real (cx, command);

        *rval = JSVAL_TRUE;
        return JS_TRUE;
}

static JSBool
js_warlock_move (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        char *to_move;

        JS_BeginRequest (cx);
        if (!JS_ConvertArguments (cx, argc, argv, "s", &to_move)) {
                debug ("move: parameter mismatch\n");
                return JS_FALSE;
        }
        JS_EndRequest (cx);

        send_real (cx, to_move);

        js_warlock_next_room_wait (cx);

        *rval = JSVAL_TRUE;
        return JS_TRUE;
}

static JSBool
js_warlock_nextRoom (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        js_warlock_next_room_wait (cx);
        *rval = JSVAL_TRUE;
        return TRUE;
}

static JSBool
js_warlock_echo (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        char *to_echo;

        JS_BeginRequest (cx);
        if (!JS_ConvertArguments (cx, argc, argv, "s", &to_echo)) {
                debug ("echo: parameter mismatch\n");
                return JS_FALSE;
        }
        JS_EndRequest (cx);

        echo_real (cx, to_echo);

        *rval = JSVAL_TRUE;
        return JS_TRUE;
}

static JSBool
js_warlock_debug (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        char *to_echo;

        JS_BeginRequest (cx);
        if (!JS_ConvertArguments (cx, argc, argv, "s", &to_echo)) {
                debug ("echo: parameter mismatch\n");
                return JS_FALSE;
        }
        JS_EndRequest (cx);

        g_printerr ("javascript debug: %s", to_echo);

        *rval = JSVAL_TRUE;
        return JS_TRUE;
}

static JSBool
js_warlock_pause (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        int seconds;
        JSScriptData *data;
        jsrefcount depth;

        JS_BeginRequest (cx);
        if (!JS_ConvertArguments (cx, argc, argv, "i", &seconds)) {
                debug ("pause: parameter mismatch\n");
                return JS_FALSE;
        }
        JS_EndRequest (cx);

        data = JS_GetContextPrivate (cx);

        depth = JS_SuspendRequest (cx);
        if (seconds > 0) {
                warlock_pause_wait (seconds, &data->running);
        }
        warlock_roundtime_wait (&data->running);
        JS_ResumeRequest (cx, depth);

        *rval = JSVAL_TRUE;
        return JS_TRUE;
}

static JSBool
js_warlock_waitRT (JSContext *cx, JSObject *obj, uintN argc, jsval *argv,
                jsval *rval)
{
        JSScriptData *data;
        jsrefcount depth;

        data = JS_GetContextPrivate (cx);

        depth = JS_SuspendRequest (cx);
        warlock_roundtime_wait (&data->running);
        JS_ResumeRequest (cx, depth);

        *rval = JSVAL_TRUE;
        return JS_TRUE;
}

static JSBool
js_warlock_getRoundtime (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval)
{
        int time_left;

        time_left = warlock_get_roundtime_left ();
        if (INT_FITS_IN_JSVAL (time_left)) {
                *rval = INT_TO_JSVAL (time_left);
                return JS_TRUE;
        } else {
                return JS_FALSE;
        }
}

static JSBool
js_warlock_addCallback (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval)
{
        JSCallbackData *callback;
        JSScriptData *data;
        JSFunction *function;
	int i;

        debug ("js_addCallback\n");
        JS_BeginRequest (cx);
        if (!JS_ConvertArguments (cx, argc, argv, "f", &function)) {
                debug ("addCallback: parameter mismatch\n");
                return JS_FALSE;
        }

        if (!JS_AddRoot (cx, function) || !JS_AddRoot (cx, obj)) {
                debug ("addCallback: could not add root\n");
                // FIXME we leak if any of these fail
                return JS_FALSE;
        }

        callback = g_new (JSCallbackData, 1);
	callback->argv = g_new (jsval, argc);

	for (i = 1; i < argc; i++) {
		if (JSVAL_IS_GCTHING (argv[i])) {
			if (!JS_AddRoot (cx, (void*)argv[i])) {
				debug ("addCallback: could not add root\n");
				// FIXME we leak here
				return JS_FALSE;
			}
		}

		callback->argv[i] = argv[i];
	}

        JS_EndRequest (cx);

        callback->function = function;
        callback->obj = obj;
	callback->argc = argc;
	callback->context = cx;

        data = JS_GetContextPrivate (cx);
	if (data == NULL) {
		*rval = JSVAL_FALSE;
		return JS_TRUE;
	}

        g_mutex_lock (js_script_mutex);
        data->callbacks = g_slist_append (data->callbacks, callback);
        // TODO: update the position here if necessary
        g_mutex_unlock (js_script_mutex);

        *rval = JSVAL_TRUE;
        return JS_TRUE;
}

/* this function cannot be called from a callback on a callback besides
 * itself. we'll probably get a crash if that is the case. */
static JSBool
js_warlock_removeCallback (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval)
{
        int32 callback_id;
        JSScriptData *data;

        debug ("js_removeCallback\n");
        JS_BeginRequest (cx);
        if (!JS_ConvertArguments (cx, argc, argv, "i", &callback_id)) {
                debug ("removeCallback: parameter mismatch\n");
                return JS_FALSE;
        }

        data = JS_GetContextPrivate (cx);

        g_mutex_lock (js_script_mutex);
	if (remove_callback (data, callback_id)) {
                *rval = JSVAL_TRUE;
	} else {
                *rval = JSVAL_FALSE;
	}
        g_mutex_unlock (js_script_mutex);
        JS_EndRequest (cx);

        return JS_TRUE;
}

static gint
compare_callback_with_id (gconstpointer a, gconstpointer b)
{
	int id1, id2;

	id1 = ((JSCallbackData*)a)->id;
	id2 = GPOINTER_TO_INT (b);

	if (id1 > id2) {
		return 1;
	} else if (id1 < id2) {
		return -1;
	} else {
		return 0;
	}
}

static gboolean
remove_callback (JSScriptData *data, int id)
{
        GSList *l;

        /* TODO adjust the position, so if we removed the current one or an
         * earlier one, we decrement the position */
        l = g_slist_find_custom (data->callbacks, GINT_TO_POINTER (id),
			compare_callback_with_id);
        if (l != NULL) {
		int i;

		JSCallbackData *callback = l->data;
                JS_RemoveRoot (callback->context, callback->function);
                JS_RemoveRoot (callback->context, callback->obj);
		for (i = 1; i < callback->argc; i++) {
			if (JSVAL_IS_GCTHING (callback->argv[i])) {
				JS_RemoveRoot (callback->context,
						(void*)callback->argv[i]);
			}
		}
		g_free (callback->argv);
                g_free (callback);
                data->callbacks = g_slist_delete_link (data->callbacks, l);
                debug ("callback removed\n");
		return TRUE;
        } else {
                debug ("callback not found\n");
		return FALSE;
        }
}

static JSBool
js_warlock_rightHandContents (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval)
{
        char *str;

        str = hand_get_contents (RIGHT_HAND);
        *rval = STRING_TO_JSVAL (str);
        return JS_TRUE;
}

static JSBool
js_warlock_leftHandContents (JSContext *cx, JSObject *obj, uintN argc,
                jsval *argv, jsval *rval)
{
        char *str;

        str = hand_get_contents (LEFT_HAND);
        *rval = STRING_TO_JSVAL (str);
        return JS_TRUE;
}
