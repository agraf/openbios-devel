/*
 *   Creation Date: <2002/10/23 20:26:40 samuel>
 *   Time-stamp: <2004/01/07 19:39:15 samuel>
 *
 *	<video.c>
 *
 *	Mac-on-Linux display node
 *
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *
 */

#include "config.h"
#include "libopenbios/bindings.h"
#include "libc/diskio.h"
#include "libopenbios/ofmem.h"
#include "drivers/drivers.h"
#include "packages/video.h"
#include "libopenbios/video.h"
#include "libopenbios/console.h"
#include "drivers/vga.h"


/************************************************************************/
/*	OF methods							*/
/************************************************************************/

DECLARE_NODE( video, INSTALL_OPEN, 0, "Tdisplay" );

/* ( -- width height ) (?) */
static void
video_dimensions( void )
{
	int w, h;
	(void) video_get_res( &w, &h );
	PUSH( w );
	PUSH( h );
}

/* ( table start count -- ) (?) */
static void
video_set_colors( void )
{
	int count = POP();
	int start = POP();
	unsigned char *p = (unsigned char*)cell2pointer(POP());
	int i;

	for( i=0; i<count; i++, p+=3 ) {
		unsigned long col = (p[0] << 16) | (p[1] << 8) | p[2];
		set_color( i + start, col );
	}
	refresh_palette();
}

/* ( r g b index -- ) */
static void
video_color_bang( void )
{
	int index = POP();
	int b = POP();
	int g = POP();
	int r = POP();
	unsigned long col = ((r << 16) & 0xff0000) | ((g << 8) & 0x00ff00) | (b & 0xff);
	/* printk("color!: %08lx %08lx %08lx %08lx\n", r, g, b, index ); */
	set_color( index, col );
	refresh_palette();
}

/* ( color_ind x y width height -- ) (?) */
static void
video_fill_rect( void )
{
	int h = POP();
	int w = POP();
	int y = POP();
	int x = POP();
	int color_ind = POP();

	fill_rect( color_ind, x, y, w, h );
}

/* ( addr len -- actual ) */
static void
video_write(void)
{
    char *addr;
    int len;

    len = POP();
    addr = (char *)cell2pointer(POP());

    console_draw_fstr(addr, len);
    PUSH(len);
}

NODE_METHODS( video ) = {
	{"dimensions",		video_dimensions	},
	{"set-colors",		video_set_colors	},
	{"fill-rectangle",	video_fill_rect		},
	{"color!",		video_color_bang	},
	{"write",		video_write		},
};


/************************************************************************/
/*	init 								*/
/************************************************************************/

void
molvideo_init(void)
{
	REGISTER_NODE( video );
}
