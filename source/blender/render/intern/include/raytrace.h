
#ifndef __RENDER_RAYTRACE_H__
#define __RENDER_RAYTRACE_H__

struct LampRen;
struct ObjectInstanceRen;
struct RayObject;
struct Render;
struct RenderDB;
struct ShadeInput;
struct ShadeResult;
struct World;

/* RayTree Create/Free */

void raytree_create(struct Render *re);
void raytree_free(struct RenderDB *rdb);

struct RayObject* raytree_create_object(struct Render *re, struct ObjectInstanceRen *obi);

/* Raytraced Shading */

void ray_shadow_single(float lashdw[3],
	struct Render *re, struct ShadeInput *shi, struct LampRen *lar,
	float from[3], float to[3]);

void ray_trace(struct Render *re, struct ShadeInput *, struct ShadeResult *);
void ray_ao(struct Render *re, struct ShadeInput *, float *, float *);
void ray_trace_mirror(struct Render *re, struct ShadeInput *shi, struct ShadeResult *shr);

void ray_path(struct Render *re, struct ShadeInput *shi);

#endif /* __RENDER_RAYTRACE_H__ */

