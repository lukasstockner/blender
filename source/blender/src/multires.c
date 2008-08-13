/*
 * $Id$
 *
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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Implements the multiresolution modeling tools.
 *
 * multires.h
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"
#include "DNA_view3d_types.h"

#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"

#include "BIF_editmesh.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BDR_editobject.h"
#include "BDR_sculptmode.h"

#include "BLI_editVert.h"

#include "BSE_edit.h"
#include "BSE_view.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#include "blendef.h"
#include "editmesh.h"
#include "multires.h"
#include "mydevice.h"
#include "parametrizer.h"

#include <math.h>

int multires_test()
{
	Mesh *me= get_mesh(OBACT);
	if(me && me->mr) {
		error("Unable to complete action with multires enabled.");
		return 1;
	}
	return 0;
}
int multires_level1_test()
{
	Mesh *me= get_mesh(OBACT);
	if(me && me->mr && me->mr->current != 1) {
		error("Operation only available for multires level 1.");
		return 1;
	}
	return 0;
}

/* Sculptmode */

void multires_check_state()
{
	if(G.f & G_SCULPTMODE && !G.obedit)
		sculptmode_correct_state();
}
