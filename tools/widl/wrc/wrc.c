/*
 * Copyright 1994 Martin von Loewis
 * Copyright 1998 Bertho A. Stultiens (BS)
 * Copyright 2003 Dimitrie O. Paun
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 */

#include "../config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>

#include "../tools.h"
#include "wrc.h"
#include "utils.h"
#include "newstruc.h"
#include "parser.h"
#include "wpp_private.h"

/*
 * Set if compiling in 32bit mode (default).
 */
int win32 = 1;

/*
 * Recognize win32 keywords if set (-w 32 enforces this),
 * otherwise set with -e option.
 */
int extensions = 1;

/*
 * Language setting for resources (-l option)
 */
language_t currentlanguage = 0;

/*
 * Set when _only_ to run the preprocessor (-E option)
 */
int preprocess_only = 0;

/*
 * Set when _not_ to run the preprocessor (-P cat option)
 */
int no_preprocess = 0;

int utf8_input = 0;

int check_utf8 = 1;  /* whether to check for valid utf8 */

const char *input_name = NULL;	/* The name given on the command-line */

const char *nlsdirs[3] = { NULL, DATADIR "/wine/nls", NULL };

int line_number = 1;		/* The current line */
int char_number = 1;		/* The current char pos within the line */

int parser_debug, yy_flex_debug;

resource_t *resource_top;	/* The top of the parsed resources */
