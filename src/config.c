#include <glib.h>
#include <string.h>

#include "pa-sink-ctl.h"
#include "config.h"

static int
parse_priorities(struct config *cfg)
{
	gchar **groups;
	struct priority p;
	int i;
	gsize length;
	GError *error = NULL;

	groups = g_key_file_get_groups(cfg->keyfile, &length);

	for (i = 0; i < length; ++i) {
		if (strncmp(groups[i], "priority", 8) != 0)
			continue;

		memset(&p, 0, sizeof p);

		p.match = g_key_file_get_value(cfg->keyfile, groups[i],
					       "match", &error);
		if (error)
			goto error;
		p.value = g_key_file_get_value(cfg->keyfile, groups[i],
					       "value", &error);
		if (error)
			goto error;
		p.priority = g_key_file_get_integer(cfg->keyfile, groups[i],
						    "priority", &error);
		if (error)
			goto error;

		list_append_struct(cfg->priorities, p);
	}

	return 0;

error:
	if (p.value)
		g_free(p.value);
	if (p.match)
		g_free(p.match);

	g_printerr("Failed to read property in prioritiy group '%s': %s\n",
		   groups[i], error->message);

	return -1;
}

static void
destroy_priority(gpointer data)
{
	struct priority *p = data;

	g_free(p->value);
	g_free(p->match);
	g_free(p);
}

int
config_init(struct config *cfg)
{
	/* FIXME: Not a nicer method available in glib? */
	const gchar *home_dirs[] = { g_get_user_config_dir(), NULL };
	const gchar * const * dirs_array[] = { home_dirs, g_get_system_config_dirs() };
	GError *error;
	int i;

	memset(cfg, 0, sizeof *cfg);
	cfg->keyfile = g_key_file_new();
	cfg->priorities = NULL;

	for (i = 0; i < G_N_ELEMENTS(dirs_array); ++i) {
		error = NULL;
		if (g_key_file_load_from_dirs(cfg->keyfile,
					      "pa-sink-ctl/config.ini",
					      (const gchar **) dirs_array[i],
					      NULL, G_KEY_FILE_NONE, &error)
		    && !error)
			break;
	}
	if (error) {
			g_printerr("Failed to open config file: %s\n",
				   error->message);
			return -1;
	}

	if (parse_priorities(cfg) < 0)
		return -1;

	return 0;
}

void
config_uninit(struct config *cfg)
{
	g_list_free_full(cfg->priorities, destroy_priority);

	g_key_file_free(cfg->keyfile);
}
