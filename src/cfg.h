/***************************************************************************
 *            cfg.h
 *
 *  Thu Mar 10 15:02:49 2005
 *  Copyright  2005  Simon Morlat
 *  Email simon.morlat@linphone.org
 ****************************************************************************/

/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
 
#ifndef __UTILSCONFIG_H__
#define __UTILSCONFIG_H__
#include <stdio.h>

/**
 * The LpConfig object is used to manipulate a configuration file.
 * 
 * @ingroup misc
 * The format of the configuration file is a .ini like format:
 * - sections are defined in []
 * - each section contains a sequence of key=value pairs.
 *
 * Example:
 * @code
 * [sound]
 * echocanceler=1
 * playback_dev=ALSA: Default device
 *
 * [video]
 * enabled=1
 * @endcode
**/

#define MAX_LEN 16384

#if defined(WIN32) || defined(_WIN32_WCE)
//#define strncasecmp _strnicmp
//#define strcasecmp _stricmp
#define snprintf _snprintf
#endif

typedef struct _CfgList {
	struct _CfgList *next;
	struct _CfgList *prev;
	void *data;
}CfgList;


typedef struct _CfgItem{
	char *key;
	char *value;
} CfgItem;

typedef struct _CfgSection{
	char *name;
	CfgList *items;
} CfgSection;

struct _Cfg{
	FILE *file;
	char *filename;
	CfgList *sections;
	int modified;
};

typedef struct _Cfg Cfg;


#ifdef __cplusplus
extern "C" {
#endif

Cfg * cfg_new(const char *filename, const char separator);
int cfg_read_file(Cfg *cfg, const char *filename, const char separator);
/**
 * Retrieves a configuration item as a string, given its section, key, and default value.
 * 
 * @ingroup misc
 * The default value string is returned if the config item isn't found.
**/
char *cfg_get_string(Cfg *cfg, const char *section, const char *key, char *default_string);

/**
 * Retrieves a configuration item as an integer, given its section, key, and default value.
 * 
 * @ingroup misc
 * The default integer value is returned if the config item isn't found.
**/
int cfg_get_int(Cfg *cfg,const char *section, const char *key, int default_value);

/**
 * Retrieves a configuration item as a float, given its section, key, and default value.
 * 
 * @ingroup misc
 * The default float value is returned if the config item isn't found.
**/
float cfg_get_float(Cfg *cfg,const char *section, const char *key, float default_value);
/**
 * Sets a string config item 
 *
 * @ingroup misc
**/
void cfg_set_string(Cfg *cfg,const char *section, const char *key, const char *value);
/**
 * Sets an integer config item
 *
 * @ingroup misc
**/
void cfg_set_int(Cfg *cfg,const char *section, const char *key, int value);
/**
 * Sets a float config item
 *
 * @ingroup misc
**/
void cfg_set_float(Cfg *cfg,const char *section, const char *key, float value);	
/**
 * Writes the config file to disk.
 * 
 * @ingroup misc
**/
int cfg_sync(Cfg *cfg);
/**
 * Returns 1 if a given section is present in the configuration.
 *
 * @ingroup misc
**/
int cfg_has_section(Cfg *cfg, const char *section);
/**
 * Removes every pair of key,value in a section and remove the section.
 *
 * @ingroup misc
**/
void cfg_clean_section(Cfg *cfg, const char *section);


/*tells whether uncommited (with cfg_sync()) modifications exist*/
int cfg_needs_commit(const Cfg *cfg);
void cfg_destroy(Cfg *cfg);


#ifdef __cplusplus
}
#endif

#endif

