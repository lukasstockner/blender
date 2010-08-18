#ifndef PTEX_H
#define PTEX_H

#ifdef __cplusplus
extern "C" {
#endif


typedef struct PtexTextureHandle PtexTextureHandle;
typedef struct PtexFaceInfoHandle PtexFaceInfoHandle;
typedef struct PtexResHandle PtexResHandle;

typedef enum {
	PTEX_DT_UINT8,
	PTEX_DT_UINT16,
	PTEX_DT_FLOAT,
	PTEX_DT_UNSUPPORTED
} PtexDataType;

/* PtexTexture class */
extern PtexTextureHandle *ptex_open(const char *path, int print_error, int premultiply);
extern void ptex_texture_release(PtexTextureHandle *ptex_texture_handle);
extern PtexDataType ptex_texture_data_type(PtexTextureHandle *ptex_texture_handle);
extern int ptex_texture_num_channels(PtexTextureHandle *ptex_texture_handle);
extern PtexFaceInfoHandle *ptex_texture_get_face_info(PtexTextureHandle* ptex_texture_handle, int faceid);
extern void ptex_texture_get_data(PtexTextureHandle *ptex_texture_handle, int faceid, void *buffer, int stride, PtexResHandle *res_handle);
extern void ptex_texture_get_pixel(PtexTextureHandle *ptex_texture_handle, int faceid, int u, int v, float *result, int firstchan, int nchannels, PtexResHandle res_handle);

/* FaceInfo struct */
extern PtexResHandle *ptex_face_get_res(PtexFaceInfoHandle* face_info_handle);
extern int ptex_face_info_is_subface(PtexFaceInfoHandle* face_info_handle);

/* Res struct */
extern int ptex_res_u(PtexResHandle* ptex_res_handle);
extern int ptex_res_v(PtexResHandle* ptex_res_handle);

/* Utils */
int ptex_data_size(PtexDataType type);


#ifdef __cplusplus
}
#endif

#endif
