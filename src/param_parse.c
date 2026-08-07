/*****************************************************************************
 *
 * param_parse.c
 *
 * PHASEX:  [P]hase [H]armonic [A]dvanced [S]ynthesis [EX]periment
 *
 * Copyright (C) 1999-2013 William Weston <whw@linuxmail.org>
 *
 * PHASEX is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PHASEX is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PHASEX.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "phasex.h"
#include "settings.h"
#include "config.h"
#include "param.h"
#include "param_parse.h"
#include "param_strings.h"
#include "patch.h"
#include "engine.h"
#include "wave.h"
#include "debug.h"


/*****************************************************************************
 * get_rate_val()
 *
 * Given an input MIDI cltr value,
 * returns a time based rate to be used by the engine.
 *****************************************************************************/
sample_t
get_rate_val(int ctlr)
{
	if (ctlr <= 0) {
		return 32.0;
	}
	else if (ctlr <= 64) {
		return (1.0 / (((sample_t)(ctlr)) * 4.0 / 64.0));
	}
	else if (ctlr <= 111) {
		return (1.0 / (((sample_t)(ctlr - 64)) * 4.0 / 48.0));
	}
	else if (ctlr <= 127) {
		return (1.0 / (((sample_t)(ctlr - 111)) * 4.0));
	}

	return 0.25;
}


/*****************************************************************************
 * get_boolean()
 *
 * Given an input token, returns a boolean value of 1 or 0.
 *****************************************************************************/
int
get_boolean(char *token, char *UNUSED(filename), int UNUSED(line))
{
	if (strcmp(token, "1") == 0) {
		return 1;
	}
	else if (strcmp(token, "true") == 0) {
		return 1;
	}
	else if (strcmp(token, "yes") == 0) {
		return 1;
	}
	else if (strcmp(token, "on") == 0) {
		return 1;
	}
	return 0;
}
