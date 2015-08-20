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

#include "lwrr_setting.h"
#include "lwrr_mem.h"

extern "C" 
void allocTextureMemory(int xSize, int ySize);

extern "C"
void initDeviceMemory(const float* _img, const float* _var_img, const float* _texture, const float* _var_texture, 
                      const float* _normal, const float* _var_normal, const float* _depth, const float* _var_depth, 
					  const float* _texture_moving, const float* _var_texture_moving,
					  const int* _mapSPP, int xSize, int ySize,
					  const int* _mapMovingSPP = NULL);

extern "C"
void freeDeviceMemory();

extern "C"
void localFitShared(float* _dbgImg,
					int xSize, int ySize, const int MAX_HALF,
					float* _dbg_hessians,
					float** _fit_map, float** _var_map, float** _bias_map, float* _ranks,
					float** _width_guess,
					LWR_cuda_mem& gloMemory, const int* _spp);

extern "C"
void localFitSharedFinal(float* _out, 
						 int xSize, int ySize, const int MAX_HALF,	
						 float* _bandwidth, 
						 LWR_cuda_mem& gloMemory,
						 float* _opt_var);


extern "C"
void localGuassian2(float* _img, float* _d_in_mem, float* _d_out_mem, int xSize, int ySize, float h, bool isColor, bool isIntegral = false);

extern "C"
void localGuassianFillHoles(float* _img, const int* _still_spp, int xSize, int ySize, int halfWidth, bool isColor);