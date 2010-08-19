#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_dmgrid.h"
#include "BKE_subsurf.h"

#include "BLI_math.h"

DerivedMesh *quad_dm_create_from_derived(DerivedMesh *dm)
{
	DerivedMesh *ccgdm;
	SubsurfModifierData smd;
	GridKey gridkey;
	
	memset(&smd, 0, sizeof(SubsurfModifierData));
	smd.levels = 1;
	smd.subdivType = ME_SIMPLE_SUBSURF;
	GRIDELEM_KEY_INIT(&gridkey, 1, 0, 0, 1);
	ccgdm = subsurf_make_derived_from_derived(dm, &smd, &gridkey,
						  0, NULL, 0, 0);

	return ccgdm;
}
