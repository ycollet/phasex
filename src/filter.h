/*****************************************************************************
 *
 * filter.h
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
#ifndef _PHASEX_FILTER_H_
#define _PHASEX_FILTER_H_

#include "phasex.h"
#include "engine.h"
#include "patch.h"


#define FILTER_TYPE_DIST                0
#define FILTER_TYPE_RETRO               1
#define FILTER_TYPE_MOOG_DIST           2
#define FILTER_TYPE_MOOG_CLEAN          3
#define FILTER_TYPE_EXPERIMENTAL_DIST   4
#define FILTER_TYPE_EXPERIMENTAL_CLEAN  5

#define NUM_FILTER_TYPES                6

#define FILTER_MODE_LP                  0
#define FILTER_MODE_HP                  1
#define FILTER_MODE_BP                  2
#define FILTER_MODE_BS                  3
#define FILTER_MODE_LP_PLUS             4
#define FILTER_MODE_LP_PLUS_BP          4
#define FILTER_MODE_HP_PLUS             5
#define FILTER_MODE_HP_PLUS_BP          5
#define FILTER_MODE_BP_PLUS             6
#define FILTER_MODE_LP_PLUS_HP          6
#define FILTER_MODE_BS_PLUS             7
#define FILTER_MODE_BS_PLUS_BP          7
#define FILTER_MODE_EXPERIMENTAL        8

#define NUM_FILTER_MODES                8


extern sample_t     filter_res[NUM_FILTER_TYPES][128];
extern sample_t     filter_table[TUNING_RESOLUTION * 648];
extern sample_t     filter_dist_1[32];
extern sample_t     filter_dist_2[32];
extern sample_t     filter_dist_3[32];
extern sample_t     filter_dist_4[32];
extern sample_t     filter_dist_5[32];
extern sample_t     filter_dist_6[32];
extern int          filter_limit;

/* waveshaper_table_11 is used by engine.c's oscillator modulation
   waveshaper; keep the size/scale in sync with filter.c's table build. */
#define WAVESHAPER_TABLE_SIZE    2048
#define WAVESHAPER_TABLE_MAX     4.0
extern sample_t     waveshaper_table_09[WAVESHAPER_TABLE_SIZE];
extern sample_t     waveshaper_table_11[WAVESHAPER_TABLE_SIZE];


/* Looks up the rational waveshaper curve for |x| = t (t is expected to
   already be non-negative, e.g. the result of MATH_ABS()), clamped to the
   table's domain. */
static inline sample_t
waveshaper_lookup(const sample_t *table, sample_t t)
{
	int index;

	if (t > (sample_t) WAVESHAPER_TABLE_MAX) {
		t = (sample_t) WAVESHAPER_TABLE_MAX;
	}
	index = (int) ((t / (sample_t) WAVESHAPER_TABLE_MAX) * (WAVESHAPER_TABLE_SIZE - 1));

	return table[index];
}


void build_filter_tables(void);
#ifdef FILTER_WAVETABLE_12DB
void filter_osc_table_12dB(int wave_num, int num_cycles, double octaves);
#endif
void filter_osc_table_24dB(int wave_num, int num_cycles, double octaves, sample_t scale);
void run_filter(VOICE *voice, PART *part, PATCH_STATE *state);
void run_moog_filter(VOICE *voice, PART *part, PATCH_STATE *state);
void run_experimental_filter(VOICE *voice, PART *part, PATCH_STATE *state);


#endif /* _PHASEX_FILTER_H_ */
