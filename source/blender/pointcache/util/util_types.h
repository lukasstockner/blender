/*
 * Copyright 2013, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef PTC_UTIL_TYPES_H
#define PTC_UTIL_TYPES_H

typedef enum PTCReadSampleResult {
	PTC_READ_SAMPLE_INVALID = 0,	/* no valid result can be retrieved */
	PTC_READ_SAMPLE_EARLY,			/* request time before first sample */
	PTC_READ_SAMPLE_LATE,			/* request time after last sample */
	PTC_READ_SAMPLE_EXACT,			/* found sample for requested frame */
	PTC_READ_SAMPLE_INTERPOLATED	/* no exact sample, but found enclosing samples for interpolation */
} PTCReadSampleResult;

#endif  /* PTC_UTIL_TYPES_H */
