
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

void ray_shadow(struct Render *re, struct ShadeInput *, struct LampRen *, float *);
void ray_trace(struct Render *re, struct ShadeInput *, struct ShadeResult *);
void ray_ao(struct Render *re, struct ShadeInput *, float *, float *);
void ray_trace_mirror(struct Render *re, struct ShadeInput *shi, struct ShadeResult *shr);
void init_jitter_plane(struct LampRen *lar);
void init_ao_sphere(struct World *wrld);

#endif /* __RENDER_RAYTRACE_H__ */

