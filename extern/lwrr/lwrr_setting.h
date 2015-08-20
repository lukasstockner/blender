/******************************************************************************\

  Copyright 2012 KAIST (Korea Advanced Institute of Science and Technology)
  All Rights Reserved.

  Permission to use, copy, modify and distribute this software and its
  documentation for educational, research and non-profit purposes, without
  fee, and without a written agreement is hereby granted, provided that the
  above copyright notice and the following three paragraphs appear in all
  copies. Any use in a commercial organization requires a separate license.

IN NO EVENT SHALL KAIST BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, 
SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, 
ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF 
KAIST HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.

KAIST SPECIFICALLY DISCLAIM ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, 
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND KAIST 
HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
OR MODIFICATIONS.

   ---------------------------------
  |Please send all BUG REPORTS to:  |
  |                                 |
  |     moonbochang@gmail.com       |
  |                                 |
   ---------------------------------

  The authors may be contacted via:

Mail:         Bochang Moon or Sung-Eui Yoon
 			Dept. of Computer Science, E3-1 
KAIST 
291 Daehak-ro(373-1 Guseong-dong), Yuseong-gu 
DaeJeon, 305-701 
Republic of Korea
\*****************************************************************************/

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) <= (b) ? (a) : (b))
#endif

////////////////////////////////////////////
// SAMPLER
#define LD_SAMPLER 1
#define RANDOM_SAMPLER 2
#define MY_SAMPLER LD_SAMPLER
////////////////////////////////////////////

#define OUTLIER_TRICK	// our outlier heuristic
#define BLOCKDIM 8 
#define MAX_HALF_WINDOW 9
#define MAX_HALF_WINDOW_INTER 5
#define NUM_TEST 5

// Uncomment it for the pool scene 
// It tests whether or not our method can support a naively selected feature 
//#define FEATURE_MOTION

#ifndef FEATURE_MOTION
#define nDimens 9		// basic 2D buffer - 2 images, 1 depth, 3 normal, 3 teuxtre
#else
#define nDimens 10		// additional buffer - 1 additional moving texture (grey)
#endif