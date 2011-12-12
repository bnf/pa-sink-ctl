#ifndef CONFIG_H
#define CONFIG_H

struct config {
	GKeyFile *keyfile;

	GList *priorities;
};

struct priority {
	gchar *match, *value;
	gint priority;
};

int
config_init(struct config *cfg);

void
config_uninit(struct config *cfg);

#endif /* CONFIG_H */
