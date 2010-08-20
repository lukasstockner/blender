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
PtexFaceInfoHandle *ptex_face_info_new(int u, int v, int adjfaces[4], int adjedges[4], int isSubface)
{
	int ulog2 = ptex_res_to_log2(u);
	int vlog2 = ptex_res_to_log2(v);

	return (PtexFaceInfoHandle*)(new Ptex::FaceInfo(Ptex::Res(ulog2, vlog2), adjfaces, adjedges, isSubface));
}

PtexResHandle *ptex_face_info_get_res(PtexFaceInfoHandle *face_info_handle)
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



/**** PtexWriter class ****/
PtexWriterHandle *ptex_writer_open(const char *path, PtexDataType dt, int nchannels, int alphachan, int nfaces, int genmipmaps)
{
	Ptex::DataType ptex_data_type;
	Ptex::String error;

	switch(dt) {
	case PTEX_DT_UINT8:
		ptex_data_type = Ptex::dt_uint8;
		break;
	case PTEX_DT_UINT16:
		ptex_data_type = Ptex::dt_uint16;
		break;
	case PTEX_DT_FLOAT:
		ptex_data_type = Ptex::dt_float;
		break;
	default:
		return NULL;
	}

	return (PtexWriterHandle*)PtexWriter::open(path, Ptex::mt_quad, ptex_data_type, nchannels, alphachan, nfaces, error, genmipmaps);
}

void ptex_writer_write_face(PtexWriterHandle *ptex_writer_handle, int faceid, PtexFaceInfoHandle *info, const void *data, int stride)
{
	((PtexWriter*)ptex_writer_handle)->writeFace(faceid, *(Ptex::FaceInfo*)info, data, stride);
}

void ptex_writer_release(PtexWriterHandle *ptex_writer_handle)
{
	((PtexWriter*)ptex_writer_handle)->release();
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

int ptex_res_to_log2(int res)
{
	switch(res) {
	case (1<<0): return 0;
	case (1<<1): return 1;
	case (1<<2): return 2;
	case (1<<3): return 3;
	case (1<<4): return 4;
	case (1<<5): return 5;
	case (1<<6): return 6;
	case (1<<7): return 7;
	case (1<<8): return 8;
	case (1<<9): return 9;
	case (1<<10): return 10;
	case (1<<11): return 11;
	case (1<<12): return 12;
	case (1<<13): return 13;
	case (1<<14): return 14;
	case (1<<15): return 15;
	case (1<<16): return 16;
	case (1<<17): return 17;
	case (1<<18): return 18;
	case (1<<19): return 19;
	case (1<<20): return 20;
	case (1<<21): return 21;
	case (1<<22): return 22;
	case (1<<23): return 23;
	case (1<<24): return 24;
	case (1<<25): return 25;
	case (1<<26): return 26;
	case (1<<27): return 27;
	case (1<<28): return 28;
	case (1<<29): return 29;
	case (1<<30): return 30;
	case (1<<31): return 31;
	default: return 0;
	}
}
