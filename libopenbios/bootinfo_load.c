/*
 *
 *       <bootinfo_load.c>
 *
 *       bootinfo file loader
 *
 *   Copyright (C) 2009 Laurent Vivier (Laurent@vivier.eu)
 *
 *   Original XML parser by Blue Swirl <blauwirbel@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *
 */

#include "config.h"
#include "libopenbios/bindings.h"
#include "libopenbios/bootinfo_load.h"
#include "libopenbios/ofmem.h"
#include "libc/vsprintf.h"

//#define DEBUG_BOOTINFO

#ifdef DEBUG_BOOTINFO
#define DPRINTF(fmt, args...) \
    do { printk("%s: " fmt, __func__ , ##args); } while (0)
#else
#define DPRINTF(fmt, args...) \
    do { } while (0)
#endif

static char *
get_device( const char *path )
{
	int i;
	static char buf[1024];

	for (i = 0; i < sizeof(buf) && path[i] && path[i] != ':'; i++)
		buf[i] = path[i];
	buf[i] = 0;

	return buf;
}

static char *
get_partition( const char *path )
{
	static char buf[2];

	buf[0] = '\0';
	buf[1] = '\0';

	while ( *path && *path != ':' )
		path++;

	if (!*path)
		return buf;
	path++;

	if (path[0] == ',' || !strchr(path, ',')) /* if there is not a ',' or no partition id then return */
		return buf;

	/* Must be a partition id */
	buf[0] = path[0];

	return buf;
}

static char *
get_filename( const char * path , char **dirname)
{
        static char buf[1024];
        char *filename;

        while ( *path && *path != ':' )
                path++;

        if (!*path) {
                *dirname = NULL;
                return NULL;
        }
        path++;

        while ( *path && isdigit(*path) )
                path++;

        if (*path == ',')
                path++;

        strncpy(buf, path, sizeof(buf));
        buf[sizeof(buf) - 1] = 0;

        filename = strrchr(buf, '\\');
        if (filename) {
                *dirname = buf;
                (*filename++) = 0;
        } else {
                *dirname = NULL;
                filename = buf;
        }

        return filename;
}

int
is_bootinfo(char *bootinfo)
{
	return (strncasecmp(bootinfo, "<chrp-boot", 10) ? 0 : -1);
}

int 
bootinfo_load(struct sys_info *info, const char *filename)
{
	// Currently not implemented
	return LOADER_NOT_SUPPORT;
}

/*
  Parse SGML structure like:
  <chrp-boot>
  <description>Debian/GNU Linux Installation on IBM CHRP hardware</description>
  <os-name>Debian/GNU Linux for PowerPC</os-name>
  <boot-script>boot &device;:\install\yaboot</boot-script>
  <icon size=64,64 color-space=3,3,2>
*/

void
bootinfo_init_program(void)
{
	char c;
	char buf[128];
	char *bootpath_prop, *bootpath, *directory, *device, *partition,
		 *filename, *bootinfo, *bootscript;
	int proplen, is_entity, is_script, is_tag, scriptvalid;
	phandle_t chosen;
	size_t filesize, scriptlen, scriptstart, scriptend, scriptind, buflen, current;

	chosen = find_dev("/chosen");

	/* Copy a C string of the boot path */
	bootpath_prop = get_property(chosen, "bootpath", &proplen);
	bootpath = malloc((proplen + 1) * sizeof (*bootpath));
	memcpy(bootpath, bootpath_prop, proplen);
	bootpath[proplen] = '\0';

	DPRINTF("bootpath: %s\n", bootpath);

	device = get_device(bootpath);
	partition = get_partition(bootpath);
	filename = get_filename(bootpath, &directory);

	feval("load-base");
	bootinfo = (char *)cell2pointer(POP());

	feval("load-size");
	filesize = (size_t)POP();

	DPRINTF("filesize: %zu\n", filesize);

	scriptstart = 0;
	scriptend = 0;
	scriptlen = 0;
	buflen = 0;
	is_entity = 0;
	is_script = 0;
	is_tag = 0;
	scriptvalid = 0;
	current = 0;

	/* Find the length of the bootscript (scriptlen) */
	while (current < filesize && bootinfo[current] != 0x04) {
		c = bootinfo[current++];

		if (c == '<') {
			is_script = 0;
			is_tag = 1;
			buflen = 0;
		} else if (is_tag) {
			if (c == '>') {
				is_tag = 0;
				buf[buflen] = '\0';
				if (strncasecmp(buf, "boot-script", 11) == 0) {
					is_script = 1;
					scriptstart = current;
				} else if (strncasecmp(buf, "/boot-script", 12) == 0) {
					is_script = 0;
					scriptend = current - 14;
					scriptvalid = 1;
					break;
				}
			} else {
				buf[buflen++] = c;
			}
		} else if (is_script) {
			if (is_entity) {
				switch (c) {
				case ';':
					is_entity = 0;
					buf[buflen] = '\0';
					if (strncasecmp(buf, "lt", 2) == 0 ||
							strncasecmp(buf, "gt", 2) == 0) {
						scriptlen += 1;
					} else if (strncasecmp(buf, "device", 6) == 0) {
						scriptlen += strlen(device);
					} else if (strncasecmp(buf, "partition", 9) == 0) {
						scriptlen += strlen(partition);
					} else if (strncasecmp(buf, "directory", 9) == 0) {
						scriptlen += strlen(directory);
					} else if (strncasecmp(buf, "filename", 8) == 0) {
						scriptlen += strlen(filename);
					} else if (strncasecmp(buf, "full-path", 9) == 0) {
						scriptlen += strlen(bootpath);
					} else {
						scriptlen += buflen + 2;
					}
					break;
				case '&':
					DPRINTF("nested entity");
					return;
				default:
					buf[buflen++] = c;
					break;
				}
			} else {
				scriptlen += 1;
			}
		} else if (c == '&') {
			is_entity = 1;
			buflen = 0;
		}
	}

	if (!scriptvalid) {
		DPRINTF("Unable to parse bootscript.\n");
		return;
	}

	bootscript = malloc(scriptlen * sizeof *bootscript);

	is_entity = 0;
	scriptind = 0;
	current = scriptstart;

	/* Copy the bootscript and translate entities */
	while (current < scriptend) {
		c = bootinfo[current++];

		if (c == '\r') c = '\n';
		putchar(c);

		if (is_entity) {
			if (c == ';') {
				is_entity = 0;
				buf[buflen] = '\0';
				if (strncasecmp(buf, "lt", 2) == 0) {
					bootscript[scriptind++] = '<';
				} else if (strncasecmp(buf, "gt", 2) == 0) {
					bootscript[scriptind++] = '>';
				} else if (strncasecmp(buf, "device", 6) == 0) {
					strcpy(bootscript + scriptind, device);
					scriptind += strlen(device);
				} else if (strncasecmp(buf, "partition", 9) == 0) {
					strcpy(bootscript + scriptind, partition);
					scriptind += strlen(partition);
				} else if (strncasecmp(buf, "directory", 9) == 0) {
					strcpy(bootscript + scriptind, directory);
					scriptind += strlen(directory);
				} else if (strncasecmp(buf, "filename", 8) == 0) {
					strcpy(bootscript + scriptind, filename);
					scriptind += strlen(filename);
				} else if (strncasecmp(buf, "full-path", 9) == 0) {
					strcpy(bootscript + scriptind, bootpath);
					scriptind += strlen(bootpath);
				} else {
					bootscript[scriptind++] = '&';
					strcpy(bootscript + scriptind, buf);
					scriptind += buflen;
					bootscript[scriptind++] = ';';
				}
			} else {
				buf[buflen++] = c;
			}
		} else if (c == '&') {
			is_entity = 1;
			buflen = 0;

		} else {
			bootscript[scriptind++] = c;
		}
	}
	bootscript[scriptlen] = '\0';

	feval(bootscript);
}
