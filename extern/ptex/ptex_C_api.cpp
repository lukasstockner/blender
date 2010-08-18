#include "ptex.h"
#include "src/ptex/Ptexture.h"
#include <iostream>

/**** PtexTexture class ****/
PtexTextureHandle *ptex_open(const char *path, int print_error, int premultiply)
{
	PtexTextureHandle *ptex_texture_handle;

	Ptex::String error_string;

	ptex_texture_handle = (PtexTextureHandle*)PtexTexture::open(path, error_string, premultiply);

	if(!ptex_texture_handle && print_error)
		std::cout << "Ptex error: " << error_string << std::endl;

	return ptex_texture_handle;
}

void ptex_texture_release(PtexTextureHandle *ptex_texture_handle)
{
	((PtexTexture*)ptex_texture_handle)->release();
}

PtexDataType ptex_texture_data_type(PtexTextureHandle *ptex_texture_handle)
{
	Ptex::DataType type = ((PtexTexture*)ptex_texture_handle)->dataType();

	switch(type) {
	case Ptex::dt_uint8:
		return PTEX_DT_UINT8;
	case Ptex::dt_uint16:
		return PTEX_DT_UINT16;
	case Ptex::dt_float:
		return PTEX_DT_FLOAT;
	default:
		return PTEX_DT_UNSUPPORTED;
	}
}

int ptex_texture_num_channels(PtexTextureHandle *ptex_texture_handle)
{
	return ((PtexTexture*)ptex_texture_handle)->numChannels();
}

PtexFaceInfoHandle *ptex_texture_get_face_info(PtexTextureHandle *ptex_texture_handle, int faceid)
{
	return (PtexFaceInfoHandle*)(&((PtexTexture*)ptex_texture_handle)->getFaceInfo(faceid));
}

void ptex_texture_get_data(PtexTextureHandle *ptex_texture_handle, int faceid, void *buffer, int stride, PtexResHandle *res_handle)
{
	((PtexTexture*)ptex_texture_handle)->getData(faceid, buffer, stride, *((Ptex::Res*)res_handle));
}

void ptex_texture_get_pixel(PtexTextureHandle *ptex_texture_handle, int faceid, int u, int v, float *result, int firstchan, int nchannels, PtexResHandle *res_handle)
{
	((PtexTexture*)ptex_texture_handle)->getPixel(faceid, u, v, result, firstchan, nchannels, *((Ptex::Res*)res_handle));
}



/**** FaceInfo struct ****/
PtexResHandle *ptex_face_get_res(PtexFaceInfoHandle *face_info_handle)
{
	return (PtexResHandle*)(&((Ptex::FaceInfo*)face_info_handle)->res);
}

int ptex_face_info_is_subface(PtexFaceInfoHandle *face_info_handle)
{
	return ((Ptex::FaceInfo*)face_info_handle)->isSubface();
}



/**** Res struct ****/
int ptex_res_u(PtexResHandle *ptex_res_handle)
{
	return ((Ptex::Res*)ptex_res_handle)->u();
}

int ptex_res_v(PtexResHandle *ptex_res_handle)
{
	return ((Ptex::Res*)ptex_res_handle)->v();
}

/**** Utils ****/
int ptex_data_size(PtexDataType type)
{
	switch(type) {
	case PTEX_DT_UINT8:
		return 1;
	case PTEX_DT_UINT16:
		return 2;
	case PTEX_DT_FLOAT:
		return 4;
	default:
		return 0;
	}
}
