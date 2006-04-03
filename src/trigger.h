typedef struct {
	char *match;
	char *command;
} Trigger;

GPtrArray *get_triggers_from_file(char *);
void save_triggers_to_file(GPtrArray*,char *);
