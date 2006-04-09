#ifndef _SCRIPT_H
#define _SCRIPT_H

/* local data types */
typedef struct _ScriptData		ScriptData;
typedef struct _ScriptCommand		ScriptCommand;
typedef struct _ScriptConditional	ScriptConditional;
typedef struct _ScriptBinaryExpr	ScriptBinaryExpr;
typedef struct _ScriptUnaryExpr		ScriptUnaryExpr;
typedef struct _ScriptCompareExpr	ScriptCompareExpr;
typedef struct _ScriptTestExpr		ScriptTestExpr;

typedef enum {
	SCRIPT_TYPE_INTEGER,
	SCRIPT_TYPE_STRING,
	SCRIPT_TYPE_VARIABLE,
	NUM_SCRIPT_TYPES
} ScriptType;

struct _ScriptData {
	ScriptType type;
	union {
		long as_integer;
		char *as_string;
	} value;
};

struct _ScriptCommand {
	GList *command;
	guint line_number;
	ScriptConditional *conditional;
};

typedef enum {
	SCRIPT_OP_AND,
	SCRIPT_OP_OR,
} ScriptBinaryOp;

typedef enum {
	SCRIPT_OP_NOT
} ScriptUnaryOp;

typedef enum {
	SCRIPT_OP_GT,
	SCRIPT_OP_GTE,
	SCRIPT_OP_LT,
	SCRIPT_OP_LTE,
	SCRIPT_OP_EQUAL,
	SCRIPT_OP_NOTEQUAL,
	SCRIPT_OP_CONTAINS
} ScriptCompareOp;

typedef enum {
	SCRIPT_OP_EXISTS
} ScriptTestOp;

typedef enum {
	SCRIPT_BINARY_EXPR,
	SCRIPT_UNARY_EXPR,
	SCRIPT_COMPARE_EXPR,
	SCRIPT_TEST_EXPR
} ScriptCondType;

struct _ScriptConditional {
	ScriptCondType type;
	union {
		ScriptBinaryExpr	*binary;
		ScriptUnaryExpr		*unary;
		ScriptCompareExpr	*compare;
		ScriptTestExpr		*test;
	} expr;
};

struct _ScriptBinaryExpr {
	ScriptBinaryOp		 op;
	ScriptConditional	*lhs;
	ScriptConditional	*rhs;
};

struct _ScriptUnaryExpr {
	ScriptUnaryOp		 op;
	ScriptConditional	*rhs;
};

struct _ScriptCompareExpr {
	ScriptCompareOp	 op;
	ScriptData	*lhs;
	ScriptData	*rhs;
};

struct _ScriptTestExpr {
	ScriptTestOp	 op;
	ScriptData	*rhs;
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
