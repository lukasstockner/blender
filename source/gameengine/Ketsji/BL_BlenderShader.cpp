
#include "DNA_customdata_types.h"

#include "BL_BlenderShader.h"

#include "GPU_extensions.h"
#include "GPU_material.h"

#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"

const bool BL_BlenderShader::Ok()const
{
	return (mGPUMat != 0);
}

BL_BlenderShader::BL_BlenderShader(struct Material *ma)
:
	mGPUMat(0),
	mBound(false)
{
	if(ma)
		mGPUMat = GPU_material_from_blender(ma, GPU_PROFILE_GAME);
}

BL_BlenderShader::~BL_BlenderShader()
{
	if(mGPUMat) {
		GPU_material_unbind(mGPUMat);
		GPU_material_free(mGPUMat);
		mGPUMat = 0;
	}
}

void BL_BlenderShader::ApplyShader()
{
}

void BL_BlenderShader::SetProg(bool enable)
{
	if(mGPUMat) {
		if(enable) {
			GPU_material_bind(mGPUMat);
			mBound = true;
		}
		else {
			GPU_material_unbind(mGPUMat);
			mBound = false;
		}
	}
}

void BL_BlenderShader::SetTexCoords(RAS_IRasterizer* ras)
{
	GPUVertexAttribs attribs;
	int i;

	if(!mGPUMat)
		return;

	GPU_material_vertex_attributes(mGPUMat, &attribs);

    for(i = 0; i < attribs.totlayer; i++) {
		if(attribs.layer[i].type == CD_MTFACE)
            ras->SetTexCoords(RAS_IRasterizer::RAS_TEXCO_UV1, i);
		else if(attribs.layer[i].type == CD_TANGENT)
            ras->SetTexCoords(RAS_IRasterizer::RAS_TEXTANGENT, i);
		else if(attribs.layer[i].type == CD_ORCO)
            ras->SetTexCoords(RAS_IRasterizer::RAS_TEXCO_ORCO, i);
		else if(attribs.layer[i].type == CD_NORMAL)
            ras->SetTexCoords(RAS_IRasterizer::RAS_TEXCO_NORM, i);
        else
            ras->SetTexCoords(RAS_IRasterizer::RAS_TEXCO_DISABLE, i);
	}
}

void BL_BlenderShader::Update( const KX_MeshSlot & ms, RAS_IRasterizer* rasty )
{
	float obmat[4][4], viewmat[4][4];

	if(!mGPUMat || !mBound)
		return;

	MT_Matrix4x4 model;
	model.setValue(ms.m_OpenGLMatrix);
	MT_Matrix4x4 view;
	rasty->GetViewMatrix(view);

	model.getValue((float*)obmat);
	view.getValue((float*)viewmat);

	GPU_material_bind_uniforms(mGPUMat, obmat, viewmat);
}

// eof
