/*
 * pa-sink-ctl - NCurses based Pulseaudio control client
 * Copyright (C) 2011  Benjamin Franzke <benjaminfranzke@googlemail.com>
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib.h>
#include <string.h>

#include "pa-sink-ctl.h"
#include "config.h"

#include "command.h"

static int
parse_priorities(struct config *cfg)
{
	gchar **groups;
	struct priority p;
	gsize i, length;
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
	g_strfreev(groups);

	return 0;

error:
	if (p.value)
		g_free(p.value);
	if (p.match)
		g_free(p.match);

	g_printerr("Failed to read property in prioritiy group '%s': %s\n",
		   groups[i], error->message);

	g_strfreev(groups);

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

static int
read_settings(struct config *cfg)
{
	GError *error = NULL;

	cfg->name_props =
		g_key_file_get_string_list(cfg->keyfile, "pa-sink-ctl",
					   "name-properties", NULL, &error);
	/* Can be ignored if not found. */
	if (error)
		error = NULL;

	return 0;
}

static unsigned int
read_int(const char *string)
{
	unsigned int value;

	if (string[1] == 'x')
		sscanf(string, "%x", &value);
	else
		sscanf(string, "%o", &value);

	return value;
}

static int
read_input_mappings(struct config *cfg)
{
	int i, j;
	gchar **list;
	GError *error = NULL;

	cfg->keymap = g_hash_table_new(g_direct_hash, g_direct_equal);
	if (cfg->keymap == NULL)
		return -1;

	for (i = 0; command_cbs[i].command; ++i) {
		const char *attrib = command_cbs[i].command;

		error = NULL;
		list = g_key_file_get_string_list(cfg->keyfile, "input",
						  attrib, NULL, &error);
		if (error) {
			g_printerr("error reading keymap for '%s': %s\n",
				   attrib, error->message);
			return -1;
		}

		for (j = 0; list[j]; ++j) {
			int key;
			if (strlen(list[j]) == 0)
				continue;

			if (strlen(list[j]) > 2 && list[j][0] == '0') {
				key = read_int(list[j]);
			} else
				key = list[j][0];
			g_hash_table_insert(cfg->keymap, GINT_TO_POINTER(key),
					    &command_cbs[i]);
		}
		g_strfreev(list);
	}

	return 0;
}

int
config_init(struct config *cfg)
{
	/* FIXME: Not a nicer method available in glib? */
	const gchar *home_dirs[] = { g_get_user_config_dir(), NULL };
	const gchar * const * dirs_array[] = { home_dirs, g_get_system_config_dirs() };
	GError *error;
	gsize i;

	memset(cfg, 0, sizeof *cfg);
	cfg->keyfile = g_key_file_new();
	cfg->priorities = NULL;

	for (i = 0; i < G_N_ELEMENTS(dirs_array); ++i) {
		error = NULL;
		if (g_key_file_load_from_dirs(cfg->keyfile,
					      "pa-sink-ctl/config",
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

	if (read_settings(cfg) < 0)
		return -1;

	if (parse_priorities(cfg) < 0)
		return -1;
	if (read_input_mappings(cfg) < 0)
		return -1;

	return 0;
}

void
config_uninit(struct config *cfg)
{
	g_list_free_full(cfg->priorities, destroy_priority);

	g_hash_table_destroy(cfg->keymap);
	g_strfreev(cfg->name_props);
	g_key_file_free(cfg->keyfile);
}
