/*
 * ***** BEGIN GPL LICENSE BLOCK *****
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
 *
 * The Original Code is Copyright (C) 2012 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Jason Wilkins, Alexandr Kuznetsov
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file intern/gpu_deprecated.h
 *  \ingroup gpu
 */

#ifndef _GPU_DEPRECATED_H_
#define _GPU_DEPRECATED_H_

#undef glBegin
#define glBegin DO_NOT_USE_glBegin
#undef glEnd
#define glEnd DO_NOT_USE_glEnd

#undef glColor3b
#define glColor3b DO_NOT_USE_glColor3b
#undef glColor3bv
#define glColor3bv DO_NOT_USE_glColor3bv
#undef glColor3d
#define glColor3d DO_NOT_USE_glColor3d
#undef glColor3dv
#define glColor3dv DO_NOT_USE_glColor3dv
#undef glColor3f
#define glColor3f DO_NOT_USE_glColor3f
#undef glColor3fv
#define glColor3fv DO_NOT_USE_glColor3fv
#undef glColor3i
#define glColor3i DO_NOT_USE_glColor3i
#undef glColor3iv
#define glColor3iv DO_NOT_USE_glColor3iv
#undef glColor3s
#define glColor3s DO_NOT_USE_glColor3s
#undef glColor3sv
#define glColor3sv DO_NOT_USE_glColor3sv
#undef glColor3ub
#define glColor3ub DO_NOT_USE_glColor3ub
#undef glColor3ubv
#define glColor3ubv DO_NOT_USE_glColor3ubv
#undef glColor3ui
#define glColor3ui DO_NOT_USE_glColor3ui
#undef glColor3uiv
#define glColor3uiv DO_NOT_USE_glColor3uiv
#undef glColor3us
#define glColor3us DO_NOT_USE_glColor3us
#undef glColor3usv
#define glColor3usv DO_NOT_USE_glColor3usv
#undef glColor4b
#define glColor4b DO_NOT_USE_glColor4b
#undef glColor4bv
#define glColor4bv DO_NOT_USE_glColor4bv
#undef glColor4d
#define glColor4d DO_NOT_USE_glColor4d
#undef glColor4dv
#define glColor4dv DO_NOT_USE_glColor4dv
#undef glColor4f
#define glColor4f DO_NOT_USE_glColor4f
#undef glColor4fv
#define glColor4fv DO_NOT_USE_glColor4fv
#undef glColor4i
#define glColor4i DO_NOT_USE_glColor4i
#undef glColor4iv
#define glColor4iv DO_NOT_USE_glColor4iv
#undef glColor4s
#define glColor4s DO_NOT_USE_glColor4s
#undef glColor4sv
#define glColor4sv DO_NOT_USE_glColor4sv
#undef glColor4ub
#define glColor4ub DO_NOT_USE_glColor4ub
#undef glColor4ubv
#define glColor4ubv DO_NOT_USE_glColor4ubv
#undef glColor4ui
#define glColor4ui DO_NOT_USE_glColor4ui
#undef glColor4uiv
#define glColor4uiv DO_NOT_USE_glColor4uiv
#undef glColor4us
#define glColor4us DO_NOT_USE_glColor4us
#undef glColor4usv
#define glColor4usv DO_NOT_USE_glColor4usv

#undef glEvalCoord1d
#define glEvalCoord1d DO_NOT_USE_glEvalCoord1d
#undef glEvalCoord1dv
#define glEvalCoord1dv DO_NOT_USE_glEvalCoord1dv
#undef glEvalCoord1f
#define glEvalCoord1f DO_NOT_USE_glEvalCoord1f
#undef glEvalCoord1fv
#define glEvalCoord1fv DO_NOT_USE_glEvalCoord1fv
#undef glEvalCoord2d
#define glEvalCoord2d DO_NOT_USE_glEvalCoord2d
#undef glEvalCoord2dv
#define glEvalCoord2dv DO_NOT_USE_glEvalCoord2dv
#undef glEvalCoord2f
#define glEvalCoord2f DO_NOT_USE_glEvalCoord2f
#undef glEvalCoord2fv
#define glEvalCoord2fv DO_NOT_USE_glEvalCoord2fv
#undef glEvalMesh1
#define glEvalMesh1 DO_NOT_USE_glEvalMesh1
#undef glEvalMesh2
#define glEvalMesh2 DO_NOT_USE_glEvalMesh2
#undef glEvalPoint1
#define glEvalPoint1 DO_NOT_USE_glEvalPoint1
#undef glEvalPoint2
#define glEvalPoint2 DO_NOT_USE_glEvalPoint2

#undef glIndexd
#define glIndexd DO_NOT_USE_glIndexd
#undef glIndexdv
#define glIndexdv DO_NOT_USE_glIndexdv
#undef glIndexf
#define glIndexf DO_NOT_USE_glIndexf
#undef glIndexfv
#define glIndexfv DO_NOT_USE_glIndexfv
#undef glIndexi
#define glIndexi DO_NOT_USE_glIndexi
#undef glIndexiv
#define glIndexiv DO_NOT_USE_glIndexiv
#undef glIndexs
#define glIndexs DO_NOT_USE_glIndexs
#undef glIndexsv
#define glIndexsv DO_NOT_USE_glIndexsv
#undef glIndexub
#define glIndexub DO_NOT_USE_glIndexub
#undef glIndexubv
#define glIndexubv DO_NOT_USE_glIndexubv

#undef glNormal3b
#define glNormal3b DO_NOT_USE_glNormal3b
#undef glNormal3bv
#define glNormal3bv DO_NOT_USE_glNormal3bv
#undef glNormal3d
#define glNormal3d DO_NOT_USE_glNormal3d
#undef glNormal3dv
#define glNormal3dv DO_NOT_USE_glNormal3dv
#undef glNormal3f
#define glNormal3f DO_NOT_USE_glNormal3f
#undef glNormal3fv
#define glNormal3fv DO_NOT_USE_glNormal3fv
#undef glNormal3i
#define glNormal3i DO_NOT_USE_glNormal3i
#undef glNormal3iv
#define glNormal3iv DO_NOT_USE_glNormal3iv
#undef glNormal3s
#define glNormal3s DO_NOT_USE_glNormal3s
#undef glNormal3sv
#define glNormal3sv DO_NOT_USE_glNormal3sv

#undef glMaterialf
#define glMaterialf DO_NOT_USE_glMaterialf
#undef glMaterialfv
#define glMaterialfv DO_NOT_USE_glMaterialfv
#undef glMateriali
#define glMateriali DO_NOT_USE_glMateriali
#undef glMaterialiv
#define glMaterialiv DO_NOT_USE_glMaterialiv

#undef glTexCoord1d
#define glTexCoord1d DO_NOT_USE_glTexCoord1d
#undef glTexCoord1dv
#define glTexCoord1dv DO_NOT_USE_glTexCoord1dv
#undef glTexCoord1f
#define glTexCoord1f DO_NOT_USE_glTexCoord1f
#undef glTexCoord1fv
#define glTexCoord1fv DO_NOT_USE_glTexCoord1fv
#undef glTexCoord1i
#define glTexCoord1i DO_NOT_USE_glTexCoord1i
#undef glTexCoord1iv
#define glTexCoord1iv DO_NOT_USE_glTexCoord1iv
#undef glTexCoord1s
#define glTexCoord1s DO_NOT_USE_glTexCoord1s
#undef glTexCoord1sv
#define glTexCoord1sv DO_NOT_USE_glTexCoord1sv
#undef glTexCoord2d
#define glTexCoord2d DO_NOT_USE_glTexCoord2d
#undef glTexCoord2dv
#define glTexCoord2dv DO_NOT_USE_glTexCoord2dv
#undef glTexCoord2f
#define glTexCoord2f DO_NOT_USE_glTexCoord2f
#undef glTexCoord2fv
#define glTexCoord2fv DO_NOT_USE_glTexCoord2fv
#undef glTexCoord2i
#define glTexCoord2i DO_NOT_USE_glTexCoord2i
#undef glTexCoord2iv
#define glTexCoord2iv DO_NOT_USE_glTexCoord2iv
#undef glTexCoord2s
#define glTexCoord2s DO_NOT_USE_glTexCoord2s
#undef glTexCoord2sv
#define glTexCoord2sv DO_NOT_USE_glTexCoord2sv
#undef glTexCoord3d
#define glTexCoord3d DO_NOT_USE_glTexCoord3d
#undef glTexCoord3dv
#define glTexCoord3dv DO_NOT_USE_glTexCoord3dv
#undef glTexCoord3f
#define glTexCoord3f DO_NOT_USE_glTexCoord3f
#undef glTexCoord3fv
#define glTexCoord3fv DO_NOT_USE_glTexCoord3fv
#undef glTexCoord3i
#define glTexCoord3i DO_NOT_USE_glTexCoord3i
#undef glTexCoord3iv
#define glTexCoord3iv DO_NOT_USE_glTexCoord3iv
#undef glTexCoord3s
#define glTexCoord3s DO_NOT_USE_glTexCoord3s
#undef glTexCoord3sv
#define glTexCoord3sv DO_NOT_USE_glTexCoord3sv
#undef glTexCoord4d
#define glTexCoord4d DO_NOT_USE_glTexCoord4d
#undef glTexCoord4dv
#define glTexCoord4dv DO_NOT_USE_glTexCoord4dv
#undef glTexCoord4f
#define glTexCoord4f DO_NOT_USE_glTexCoord4f
#undef glTexCoord4fv
#define glTexCoord4fv DO_NOT_USE_glTexCoord4fv
#undef glTexCoord4i
#define glTexCoord4i DO_NOT_USE_glTexCoord4i
#undef glTexCoord4iv
#define glTexCoord4iv DO_NOT_USE_glTexCoord4iv
#undef glTexCoord4s
#define glTexCoord4s DO_NOT_USE_glTexCoord4s
#undef glTexCoord4sv
#define glTexCoord4sv DO_NOT_USE_glTexCoord4sv

#undef glVertex2d
#define glVertex2d DO_NOT_USE_glVertex2d
#undef glVertex2dv
#define glVertex2dv DO_NOT_USE_glVertex2dv
#undef glVertex2f
#define glVertex2f DO_NOT_USE_glVertex2f
#undef glVertex2fv
#define glVertex2fv DO_NOT_USE_glVertex2fv
#undef glVertex2i
#define glVertex2i DO_NOT_USE_glVertex2i
#undef glVertex2iv
#define glVertex2iv DO_NOT_USE_glVertex2iv
#undef glVertex2s
#define glVertex2s DO_NOT_USE_glVertex2s
#undef glVertex2sv
#define glVertex2sv DO_NOT_USE_glVertex2sv
#undef glVertex3d
#define glVertex3d DO_NOT_USE_glVertex3d
#undef glVertex3dv
#define glVertex3dv DO_NOT_USE_glVertex3dv
#undef glVertex3f
#define glVertex3f DO_NOT_USE_glVertex3f
#undef glVertex3fv
#define glVertex3fv DO_NOT_USE_glVertex3fv
#undef glVertex3i
#define glVertex3i DO_NOT_USE_glVertex3i
#undef glVertex3iv
#define glVertex3iv DO_NOT_USE_glVertex3iv
#undef glVertex3s
#define glVertex3s DO_NOT_USE_glVertex3s
#undef glVertex3sv
#define glVertex3sv DO_NOT_USE_glVertex3sv
#undef glVertex4d
#define glVertex4d DO_NOT_USE_glVertex4d
#undef glVertex4dv
#define glVertex4dv DO_NOT_USE_glVertex4dv
#undef glVertex4f
#define glVertex4f DO_NOT_USE_glVertex4f
#undef glVertex4fv
#define glVertex4fv DO_NOT_USE_glVertex4fv
#undef glVertex4i
#define glVertex4i DO_NOT_USE_glVertex4i
#undef glVertex4iv
#define glVertex4iv DO_NOT_USE_glVertex4iv
#undef glVertex4s
#define glVertex4s DO_NOT_USE_glVertex4s
#undef glVertex4sv
#define glVertex4sv DO_NOT_USE_glVertex4sv

#undef glMultiTexCoord1d
#define glMultiTexCoord1d DO_NOT_USE_glMultiTexCoord1d
#undef glMultiTexCoord1dv
#define glMultiTexCoord1dv DO_NOT_USE_glMultiTexCoord1dv
#undef glMultiTexCoord1f
#define glMultiTexCoord1f DO_NOT_USE_glMultiTexCoord1f
#undef glMultiTexCoord1fv
#define glMultiTexCoord1fv DO_NOT_USE_glMultiTexCoord1fv
#undef glMultiTexCoord1i
#define glMultiTexCoord1i DO_NOT_USE_glMultiTexCoord1i
#undef glMultiTexCoord1iv
#define glMultiTexCoord1iv DO_NOT_USE_glMultiTexCoord1iv
#undef glMultiTexCoord1s
#define glMultiTexCoord1s DO_NOT_USE_glMultiTexCoord1s
#undef glMultiTexCoord1sv
#define glMultiTexCoord1sv DO_NOT_USE_glMultiTexCoord1sv
#undef glMultiTexCoord2d
#define glMultiTexCoord2d DO_NOT_USE_glMultiTexCoord2d
#undef glMultiTexCoord2dv
#define glMultiTexCoord2dv DO_NOT_USE_glMultiTexCoord2dv
#undef glMultiTexCoord2f
#define glMultiTexCoord2f DO_NOT_USE_glMultiTexCoord2f
#undef glMultiTexCoord2fv
#define glMultiTexCoord2fv DO_NOT_USE_glMultiTexCoord2fv
#undef glMultiTexCoord2i
#define glMultiTexCoord2i DO_NOT_USE_glMultiTexCoord2i
#undef glMultiTexCoord2iv
#define glMultiTexCoord2iv DO_NOT_USE_glMultiTexCoord2iv
#undef glMultiTexCoord2s
#define glMultiTexCoord2s DO_NOT_USE_glMultiTexCoord2s
#undef glMultiTexCoord2sv
#define glMultiTexCoord2sv DO_NOT_USE_glMultiTexCoord2sv
#undef glMultiTexCoord3d
#define glMultiTexCoord3d DO_NOT_USE_glMultiTexCoord3d
#undef glMultiTexCoord3dv
#define glMultiTexCoord3dv DO_NOT_USE_glMultiTexCoord3dv
#undef glMultiTexCoord3f
#define glMultiTexCoord3f DO_NOT_USE_glMultiTexCoord3f
#undef glMultiTexCoord3fv
#define glMultiTexCoord3fv DO_NOT_USE_glMultiTexCoord3fv
#undef glMultiTexCoord3i
#define glMultiTexCoord3i DO_NOT_USE_glMultiTexCoord3i
#undef glMultiTexCoord3iv
#define glMultiTexCoord3iv DO_NOT_USE_glMultiTexCoord3iv
#undef glMultiTexCoord3s
#define glMultiTexCoord3s DO_NOT_USE_glMultiTexCoord3s
#undef glMultiTexCoord3sv
#define glMultiTexCoord3sv DO_NOT_USE_glMultiTexCoord3sv
#undef glMultiTexCoord4d
#define glMultiTexCoord4d DO_NOT_USE_glMultiTexCoord4d
#undef glMultiTexCoord4dv
#define glMultiTexCoord4dv DO_NOT_USE_glMultiTexCoord4dv
#undef glMultiTexCoord4f
#define glMultiTexCoord4f DO_NOT_USE_glMultiTexCoord4f
#undef glMultiTexCoord4fv
#define glMultiTexCoord4fv DO_NOT_USE_glMultiTexCoord4fv
#undef glMultiTexCoord4i
#define glMultiTexCoord4i DO_NOT_USE_glMultiTexCoord4i
#undef glMultiTexCoord4iv
#define glMultiTexCoord4iv DO_NOT_USE_glMultiTexCoord4iv
#undef glMultiTexCoord4s
#define glMultiTexCoord4s DO_NOT_USE_glMultiTexCoord4s
#undef glMultiTexcoord4sv
#define glMultiTexcoord4sv DO_NOT_USE_glMultiTexCoord4sv

#undef glMultiTexCoord1dARB
#define glMultiTexCoord1dARB DO_NOT_USE_glMultiTexCoord1dARB
#undef glMultiTexCoord1dvARB
#define glMultiTexCoord1dvARB DO_NOT_USE_glMultiTexCoord1dvARB
#undef glMultiTexCoord1fARB
#define glMultiTexCoord1fARB DO_NOT_USE_glMultiTexCoord1fARB
#undef glMultiTexCoord1fvARB
#define glMultiTexCoord1fvARB DO_NOT_USE_glMultiTexCoord1fvARB
#undef glMultiTexCoord1iARB
#define glMultiTexCoord1iARB DO_NOT_USE_glMultiTexCoord1iARB
#undef glMultiTexCoord1ivARB
#define glMultiTexCoord1ivARB DO_NOT_USE_glMultiTexCoord1ivARB
#undef glMultiTexCoord1sARB
#define glMultiTexCoord1sARB DO_NOT_USE_glMultiTexCoord1sARB
#undef glMultiTexCoord1svARB
#define glMultiTexCoord1svARB DO_NOT_USE_glMultiTexCoord1svARB
#undef glMultiTexCoord2dARB
#define glMultiTexCoord2dARB DO_NOT_USE_glMultiTexCoord2dARB
#undef glMultiTexCoord2dvARB
#define glMultiTexCoord2dvARB DO_NOT_USE_glMultiTexCoord2dvARB
#undef glMultiTexCoord2fARB
#define glMultiTexCoord2fARB DO_NOT_USE_glMultiTexCoord2fARB
#undef glMultiTexCoord2fvARB
#define glMultiTexCoord2fvARB DO_NOT_USE_glMultiTexCoord2fvARB
#undef glMultiTexCoord2iARB
#define glMultiTexCoord2iARB DO_NOT_USE_glMultiTexCoord2iARB
#undef glMultiTexCoord2ivARB
#define glMultiTexCoord2ivARB DO_NOT_USE_glMultiTexCoord2ivARB
#undef glMultiTexCoord2sARB
#define glMultiTexCoord2sARB DO_NOT_USE_glMultiTexCoord2sARB
#undef glMultiTexCoord2svARB
#define glMultiTexCoord2svARB DO_NOT_USE_glMultiTexCoord2svARB
#undef glMultiTexCoord3dARB
#define glMultiTexCoord3dARB DO_NOT_USE_glMultiTexCoord3dARB
#undef glMultiTexCoord3dvARB
#define glMultiTexCoord3dvARB DO_NOT_USE_glMultiTexCoord3dvARB
#undef glMultiTexCoord3fARB
#define glMultiTexCoord3fARB DO_NOT_USE_glMultiTexCoord3fARB
#undef glMultiTexCoord3fvARB
#define glMultiTexCoord3fvARB DO_NOT_USE_glMultiTexCoord3fvARB
#undef glMultiTexCoord3iARB
#define glMultiTexCoord3iARB DO_NOT_USE_glMultiTexCoord3iARB
#undef glMultiTexCoord3ivARB
#define glMultiTexCoord3ivARB DO_NOT_USE_glMultiTexCoord3ivARB
#undef glMultiTexCoord3sARB
#define glMultiTexCoord3sARB DO_NOT_USE_glMultiTexCoord3sARB
#undef glMultiTexCoord3svARB
#define glMultiTexCoord3svARB DO_NOT_USE_glMultiTexCoord3svARB
#undef glMultiTexCoord4dARB
#define glMultiTexCoord4dARB DO_NOT_USE_glMultiTexCoord4dARB
#undef glMultiTexCoord4dvARB
#define glMultiTexCoord4dvARB DO_NOT_USE_glMultiTexCoord4dvARB
#undef glMultiTexCoord4fARB
#define glMultiTexCoord4fARB DO_NOT_USE_glMultiTexCoord4fARB
#undef glMultiTexCoord4fvARB
#define glMultiTexCoord4fvARB DO_NOT_USE_glMultiTexCoord4fvARB
#undef glMultiTexCoord4iARB
#define glMultiTexCoord4iARB DO_NOT_USE_glMultiTexCoord4iARB
#undef glMultiTexCoord4ivARB
#define glMultiTexCoord4ivARB DO_NOT_USE_glMultiTexCoord4ivARB
#undef glMultiTexCoord4sARB
#define glMultiTexCoord4sARB DO_NOT_USE_glMultiTexCoord4sARB
#undef glMultiTexcoord4svARB
#define glMultiTexcoord4svARB DO_NOT_USE_glMultiTexCoord4svARB

#undef glFogCoordd
#define glFogCoordd DO_NOT_USE_glFogCoordd
#undef glFogCoorddv
#define glFogCoorddv DO_NOT_USE_glFogCoorddv
#undef glFogCoordf
#define glFogCoordf DO_NOT_USE_glFogCoordf
#undef glFogCoordfv
#define glFogCoordfv DO_NOT_USE_glFogCoordfv

#undef glSecondaryColor3b
#define glSecondaryColor3b DO_NOT_USE_glSecondaryColor3b
#undef glSecondaryColor3bv
#define glSecondaryColor3bv DO_NOT_USE_glSecondaryColor3bv
#undef glSecondaryColor3d
#define glSecondaryColor3d DO_NOT_USE_glSecondaryColor3d
#undef glSecondaryColor3dv
#define glSecondaryColor3dv DO_NOT_USE_glSecondaryColor3dv
#undef glSecondaryColor3f
#define glSecondaryColor3f DO_NOT_USE_glSecondaryColor3f
#undef glSecondaryColor3fv
#define glSecondaryColor3fv DO_NOT_USE_glSecondaryColor3fv
#undef glSecondaryColor3i
#define glSecondaryColor3i DO_NOT_USE_glSecondaryColor3i
#undef glSecondaryColor3iv
#define glSecondaryColor3iv DO_NOT_USE_glSecondaryColor3iv
#undef glSecondaryColor3s
#define glSecondaryColor3s DO_NOT_USE_glSecondaryColor3s
#undef glSecondaryColor3sv
#define glSecondaryColor3sv DO_NOT_USE_glSecondaryColor3sv
#undef glSecondaryColor3ub
#define glSecondaryColor3ub DO_NOT_USE_glSecondaryColor3ub
#undef glSecondaryColor3ubv
#define glSecondaryColor3ubv DO_NOT_USE_glSecondaryColor3ubv
#undef glSecondaryColor3ui
#define glSecondaryColor3ui DO_NOT_USE_glSecondaryColor3ui
#undef glSecondaryColor3uiv
#define glSecondaryColor3uiv DO_NOT_USE_glSecondaryColor3uiv
#undef glSecondaryColor3us
#define glSecondaryColor3us DO_NOT_USE_glSecondaryColor3us
#undef glSecondaryColor3usv
#define glSecondaryColor3usv DO_NOT_USE_glSecondaryColor3usv

#endif /* _GPU_DEPRECATED_H_ */
