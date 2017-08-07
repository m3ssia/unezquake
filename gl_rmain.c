/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "vx_stuff.h"
#include "vx_vertexlights.h"
#include "utils.h"
#include "qsound.h"
#include "hud.h"
#include "hud_common.h"
#include "gl_bloom.h"
#include "rulesets.h"
#include "teamplay.h"

void CI_Init(void);
void OnChange_gl_clearColor(cvar_t *v, char *s, qbool *cancel);
void SCR_OnChangeMVHudPos(cvar_t *var, char *newval, qbool *cancel);
void OnChange_r_drawflat(cvar_t *v, char *skyname, qbool *cancel);
void OnChange_r_skyname(cvar_t *v, char *s, qbool *cancel);
void R_MarkLeaves(void);
void R_InitBubble(void);

extern msurface_t *alphachain;
#ifndef CLIENTONLY
extern cvar_t     maxclients;
#define IsLocalSinglePlayerGame() (com_serveractive && cls.state == ca_active && !cl.deathmatch && maxclients.value == 1)
#else
#define IsLocalSinglePlayerGame() (0)
#endif
extern vec3_t     lightcolor;
extern vec3_t     lightspot;
extern float      bubblecolor[NUM_DLIGHTTYPES][4];

// precalculated dot products for quantized angles
byte      r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS] =
#include "anorm_dots.h"
;
float     r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "anorms.h"
};
byte      *shadedots = r_avertexnormal_dots[0];


texture_t *r_notexture_mip = NULL;
refdef2_t r_refdef2;                          // screen size info
refdef_t  r_refdef;                           // screen size info
entity_t  r_worldentity;
entity_t  *currententity;
mplane_t  frustum[4];
mleaf_t   *r_viewleaf;
mleaf_t   *r_oldviewleaf;
mleaf_t   *r_viewleaf2;                       // for watervis hack
mleaf_t   *r_oldviewleaf2;                    // for watervis hack
vec3_t    dlight_color;
vec3_t    modelorg, r_entorigin;
vec3_t    vup, vpn, vright;                   // view origin
vec3_t    r_origin; // view origin
vec3_t    vertexlight;
vec3_t    shadevector;
float     gldepthmin, gldepthmax;
float     r_world_matrix[16];
float     r_base_world_matrix[16];
float     r_framelerp;
float     r_modelalpha;
float     r_lerpdistance;
float     r_modelcolor[3];
float     r_shellcolor[3];
float     shadelight;
float     ambientlight;
float     apitch;
float     ayaw;
float     clearColor[3] = {0, 0, 0};
qbool     r_cache_thrash;                     // compatability
qbool     full_light;
int       lastposenum;
GLuint    shelltexture = 0;
int       r_visframecount;                    // bumped when going to a new PVS
int       r_framecount;                       // used for dlight push checking
int       c_brush_polys;
int       c_alias_polys;
int       lightmode = 2;
int       sceneblur_texture;                  // motion blur.
int       d_lightstylevalue[256];             // 8.8 fraction of base light value
GLuint    particletexture;                    // little dot for particles
GLuint    playernmtextures[MAX_CLIENTS];
GLuint    playerfbtextures[MAX_CLIENTS];
GLuint    skyboxtextures[MAX_SKYBOXTEXTURES];
GLuint    underwatertexture, detailtexture;

void OnSquareParticleChange(cvar_t *var, char *value, qbool *cancel)
{
	extern void Classic_LoadParticleTexures();

	Classic_LoadParticleTexures();
}

cvar_t cl_multiview                        = {"cl_multiview", "0" };
cvar_t cl_mvdisplayhud                     = {"cl_mvdisplayhud", "1"};
cvar_t cl_mvhudvertical                    = {"cl_mvhudvertical", "0"};
cvar_t cl_mvhudflip                        = {"cl_mvhudflip", "0"};
cvar_t cl_mvhudpos                         = {"cl_mvhudpos", "bottom center"};
cvar_t cl_mvinset                          = {"cl_mvinset", "0"};
cvar_t cl_mvinsetcrosshair                 = {"cl_mvinsetcrosshair", "1"};
cvar_t cl_mvinsethud                       = {"cl_mvinsethud", "1"};
cvar_t r_drawentities                      = {"r_drawentities", "1"};
cvar_t r_lerpframes                        = {"r_lerpframes", "1"};
cvar_t r_lerpmuzzlehack                    = {"r_lerpmuzzlehack", "1"};
cvar_t r_drawflame                         = {"r_drawflame", "1"};
cvar_t r_speeds                            = {"r_speeds", "0"};
cvar_t r_fullbright                        = {"r_fullbright", "0"};
cvar_t r_lightmap                          = {"r_lightmap", "0"};
cvar_t gl_shaftlight                       = {"gl_shaftlight", "1"};
cvar_t r_shadows                           = {"r_shadows", "0"};
cvar_t r_wateralpha                        = {"gl_turbalpha", "1"};
cvar_t r_dynamic                           = {"r_dynamic", "1"};
cvar_t r_novis                             = {"r_novis", "0"};
cvar_t r_netgraph                          = {"r_netgraph", "0"};
cvar_t r_netstats                          = {"r_netstats", "0"};
cvar_t r_fullbrightSkins                   = {"r_fullbrightSkins", "1", 0, Rulesets_OnChange_r_fullbrightSkins};
cvar_t r_enemyskincolor                    = {"r_enemyskincolor", "", CVAR_COLOR};
cvar_t r_teamskincolor                     = {"r_teamskincolor",  "", CVAR_COLOR};
cvar_t r_skincolormode                     = {"r_skincolormode",  "0"};
cvar_t r_skincolormodedead                 = {"r_skincolormodedead",  "-1"};
cvar_t r_fastsky                           = {"r_fastsky", "0"};
cvar_t r_fastturb                          = {"r_fastturb", "0"};
cvar_t r_skycolor                          = {"r_skycolor", "40 80 150", CVAR_COLOR};
cvar_t r_telecolor                         = {"r_telecolor", "255 60 60", CVAR_COLOR};
cvar_t r_lavacolor                         = {"r_lavacolor", "80 0 0", CVAR_COLOR};
cvar_t r_slimecolor                        = {"r_slimecolor", "10 60 10", CVAR_COLOR};
cvar_t r_watercolor                        = {"r_watercolor", "10 50 80", CVAR_COLOR};
cvar_t r_drawflat                          = {"r_drawflat", "0", 0, OnChange_r_drawflat};
cvar_t r_wallcolor                         = {"r_wallcolor", "255 255 255", CVAR_COLOR, OnChange_r_drawflat};
cvar_t r_floorcolor                        = {"r_floorcolor", "50 100 150", CVAR_COLOR, OnChange_r_drawflat};
cvar_t gl_textureless                      = {"gl_textureless", "0", 0, OnChange_r_drawflat}; //Qrack
cvar_t r_farclip                           = {"r_farclip", "8192"}; // previous default was 4096. 8192 helps some TF players in big maps
cvar_t r_skyname                           = {"r_skyname", "", 0, OnChange_r_skyname};
cvar_t gl_detail                           = {"gl_detail","0"};
cvar_t gl_brush_polygonoffset              = {"gl_brush_polygonoffset", "2.0"}; // This is the one to adjust if you notice flicker on lift @ e1m1 for instance, for z-fighting
cvar_t gl_caustics                         = {"gl_caustics", "0"}; // 1
cvar_t gl_waterfog                         = {"gl_turbfog", "0"}; // 2
cvar_t gl_waterfog_density                 = {"gl_turbfogDensity", "1"};
cvar_t gl_waterfog_color_water             = {"gl_turbfog_color_water", "32 64 128", CVAR_COLOR};
cvar_t gl_waterfog_color_lava              = {"gl_turbfog_color_lava", "255 64 0", CVAR_COLOR};
cvar_t gl_waterfog_color_slime             = {"gl_turbfog_color_slime", "128 255 0", CVAR_COLOR};
cvar_t gl_lumaTextures                     = {"gl_lumaTextures", "1"};
cvar_t gl_subdivide_size                   = {"gl_subdivide_size", "64"};
cvar_t gl_clear                            = {"gl_clear", "0"};
cvar_t gl_clearColor                       = {"gl_clearColor", "0 0 0", CVAR_COLOR, OnChange_gl_clearColor};
cvar_t gl_cull                             = {"gl_cull", "1"};
cvar_t gl_smoothmodels                     = {"gl_smoothmodels", "1"};
cvar_t gl_affinemodels                     = {"gl_affinemodels", "0"};
cvar_t gl_polyblend                        = {"gl_polyblend", "1"}; // 0
cvar_t gl_flashblend                       = {"gl_flashblend", "0"};
cvar_t gl_rl_globe                         = {"gl_rl_globe", "0"};
cvar_t gl_playermip                        = {"gl_playermip", "0"};
cvar_t gl_nocolors                         = {"gl_nocolors", "0"};
cvar_t gl_finish                           = {"gl_finish", "0"};
cvar_t gl_fb_bmodels                       = {"gl_fb_bmodels", "1"};
cvar_t gl_fb_models                        = {"gl_fb_models", "1"};
cvar_t gl_lightmode                        = {"gl_lightmode", "2"};
cvar_t gl_loadlitfiles                     = {"gl_loadlitfiles", "1"};
cvar_t gl_colorlights                      = {"gl_colorlights", "1"};
cvar_t gl_solidparticles                   = {"gl_solidparticles", "0"}; // 1
cvar_t gl_squareparticles                  = {"gl_squareparticles", "0", 0, OnSquareParticleChange };
cvar_t gl_part_explosions                  = {"gl_part_explosions", "0"}; // 1
cvar_t gl_part_trails                      = {"gl_part_trails", "0"}; // 1
cvar_t gl_part_tracer1_color               = {"gl_part_tracer1_color", "0 124 0", CVAR_COLOR};
cvar_t gl_part_tracer2_color               = {"gl_part_tracer2_color", "255 77 0", CVAR_COLOR};
cvar_t gl_part_tracer1_size                = {"gl_part_tracer1_size", "3.75", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 3.75f, 10.f, 0.f};
cvar_t gl_part_tracer1_time                = {"gl_part_tracer1_time", "0.5", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 0.5f, 3.f, 0.f};
cvar_t gl_part_tracer2_size                = {"gl_part_tracer2_size", "3.75", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 3.75f, 10.f, 0.f};
cvar_t gl_part_tracer2_time                = {"gl_part_tracer2_time", "0.5", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 0.5f, 3.f, 0.f};
cvar_t gl_part_spikes                      = {"gl_part_spikes", "0"}; // 1
cvar_t gl_part_gunshots                    = {"gl_part_gunshots", "0"}; // 1
cvar_t gl_part_blood                       = {"gl_part_blood", "0"}; // 1
cvar_t gl_part_telesplash                  = {"gl_part_telesplash", "0"}; // 1
cvar_t gl_part_blobs                       = {"gl_part_blobs", "0"}; // 1
cvar_t gl_part_lavasplash                  = {"gl_part_lavasplash", "0"}; // 1
cvar_t gl_part_inferno                     = {"gl_part_inferno", "0"}; // 1
cvar_t gl_particle_style                   = {"gl_particle_style", "0"}; // 0 - round, 1 - square (sw style)
cvar_t gl_part_detpackexplosion_fire_color = {"gl_part_detpackexplosion_fire_color", "", CVAR_COLOR};
cvar_t gl_part_detpackexplosion_ray_color  = {"gl_part_detpackexplosion_ray_color", "", CVAR_COLOR};
cvar_t gl_powerupshells                    = {"gl_powerupshells", "1"};
cvar_t gl_powerupshells_style              = {"gl_powerupshells_style", "0"};
cvar_t gl_powerupshells_size               = {"gl_powerupshells_size", "5"};
cvar_t gl_powerupshells_effect1level       = {"gl_powerupshells_effect1level", "0.75"};
cvar_t gl_powerupshells_base1level         = {"gl_powerupshells_base1level", "0.05"};
cvar_t gl_powerupshells_effect2level       = {"gl_powerupshells_effect2level", "0.4"};
cvar_t gl_powerupshells_base2level         = {"gl_powerupshells_base2level", "0.1"};
cvar_t gl_fogenable                        = {"gl_fog", "0"};
cvar_t gl_fogstart                         = {"gl_fogstart", "50.0"};
cvar_t gl_fogend                           = {"gl_fogend", "800.0"};
cvar_t gl_fogred                           = {"gl_fogred", "0.6"};
cvar_t gl_foggreen                         = {"gl_foggreen", "0.5"};
cvar_t gl_fogblue                          = {"gl_fogblue", "0.4"};
cvar_t gl_fogsky                           = {"gl_fogsky", "1"};
cvar_t gl_simpleitems                      = {"gl_simpleitems", "0"};
cvar_t gl_simpleitems_size                 = {"gl_simpleitems_size", "16"};
cvar_t gl_simpleitems_orientation          = {"gl_simpleitems_orientation", "2"};
cvar_t gl_motion_blur                      = {"gl_motion_blur", "0"};
cvar_t gl_motion_blur_fps                  = {"gl_motion_blur_fps", "77"};
cvar_t gl_motion_blur_norm                 = {"gl_motion_blur_norm", "0.5"};
cvar_t gl_motion_blur_hurt                 = {"gl_motion_blur_hurt", "0.5"};
cvar_t gl_motion_blur_dead                 = {"gl_motion_blur_dead", "0.5"};
cvar_t gl_gammacorrection                  = {"gl_gammacorrection", "0", CVAR_LATCH};
cvar_t gl_modulate                         = {"gl_modulate", "1"};
cvar_t gl_outline                          = {"gl_outline", "0"};
cvar_t gl_outline_width                    = {"gl_outline_width", "2"};
cvar_t gl_meshdraw                         = {"gl_meshdraw", "1"};

typedef struct custom_model_color_s {
	cvar_t color_cvar;
	cvar_t fullbright_cvar;
	cvar_t* amf_cvar;
	int model_hint;
} custom_model_color_t;

custom_model_color_t custom_model_colors[] = {
	// LG beam
	{
		{ "gl_custom_lg_color", "", CVAR_COLOR },
		{ "gl_custom_lg_fullbright", "1" },
		&amf_lightning,
		MOD_THUNDERBOLT
	},
	// Rockets
	{
		{ "gl_custom_rocket_color", "", CVAR_COLOR },
		{ "gl_custom_rocket_fullbright", "1" },
		NULL,
		MOD_ROCKET
	},
	// Grenades
	{
		{ "gl_custom_grenade_color", "", CVAR_COLOR },
		{ "gl_custom_grenade_fullbright", "1" },
		NULL,
		MOD_GRENADE
	},
	// Spikes
	{
		{ "gl_custom_spike_color", "", CVAR_COLOR },
		{ "gl_custom_spike_fullbright", "1" },
		&amf_part_spikes,
		MOD_SPIKE
	}
};

custom_model_color_t* custom_model = NULL;

void GL_PolygonOffset(float factor, float units)
{
	if (factor || units)
	{
		glEnable (GL_POLYGON_OFFSET_FILL);
		glEnable (GL_POLYGON_OFFSET_LINE);

		glPolygonOffset(factor, units);
	}
	else
	{
		glDisable (GL_POLYGON_OFFSET_FILL);
		glDisable (GL_POLYGON_OFFSET_LINE);
	}
}

//Returns true if the box is completely outside the frustom
qbool R_CullBox(vec3_t mins, vec3_t maxs)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (BOX_ON_PLANE_SIDE (mins, maxs, &frustum[i]) == 2)
			return true;
	}
	return false;
}

//Returns true if the sphere is completely outside the frustum
qbool R_CullSphere(vec3_t centre, float radius)
{
	int i;
	mplane_t *p;

	for (i = 0, p = frustum; i < 4; i++, p++) {
		if (PlaneDiff(centre, p) <= -radius)
			return true;
	}

	return false;
}

void R_RotateForEntity(entity_t *e)
{
	GL_Translate(GL_MODELVIEW, e->origin[0], e->origin[1], e->origin[2]);

	GL_Rotate(GL_MODELVIEW, e->angles[1], 0, 0, 1);
	GL_Rotate(GL_MODELVIEW, -e->angles[0], 0, 1, 0);
	GL_Rotate(GL_MODELVIEW, e->angles[2], 1, 0, 0);
}


mspriteframe_t *R_GetSpriteFrame(entity_t *e, msprite2_t *psprite)
{
	mspriteframe_t  *pspriteframe;
	mspriteframe2_t *pspriteframe2;
	int i, numframes, frame, offset;
	float fullinterval, targettime, time;

	frame = e->frame;

	if (frame >= psprite->numframes || frame < 0) {
		Com_DPrintf ("R_GetSpriteFrame: no such frame %d (model %s)\n", frame, e->model->name);
		return NULL;
	}

	offset    = psprite->frames[frame].offset;
	numframes = psprite->frames[frame].numframes;

	if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
		Com_Printf ("R_GetSpriteFrame: wrong sprite\n");
		return NULL;
	}

	if (psprite->frames[frame].type == SPR_SINGLE) {
		pspriteframe  = (mspriteframe_t* )((byte*)psprite + offset);
	}
	else {
		pspriteframe2 = (mspriteframe2_t*)((byte*)psprite + offset);

		fullinterval = pspriteframe2[numframes-1].interval;

		time = r_refdef2.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++)
			if (pspriteframe2[i].interval > targettime)
				break;

		pspriteframe = &pspriteframe2[i].frame;
	}

	return pspriteframe;
}

void R_DrawSpriteModel (entity_t *e)
{
	vec3_t point, right, up;
	mspriteframe_t *frame;
	msprite2_t *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	psprite = (msprite2_t*)Mod_Extradata (e->model);	//locate the proper data
	frame = R_GetSpriteFrame (e, psprite);

	if (!frame)
		return;

	if (psprite->type == SPR_ORIENTED) {
		// bullet marks on walls
		AngleVectors (e->angles, NULL, right, up);
	} else if (psprite->type == SPR_FACING_UPRIGHT) {
		VectorSet (up, 0, 0, 1);
		right[0] = e->origin[1] - r_origin[1];
		right[1] = -(e->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast (right);
	} else if (psprite->type == SPR_VP_PARALLEL_UPRIGHT) {
		VectorSet (up, 0, 0, 1);
		VectorCopy (vright, right);
	} else {	// normal sprite
		VectorCopy (vup, up);
		VectorCopy (vright, right);
	}

	GL_Bind(frame->gl_texturenum);

	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	glVertex3fv (point);

	glEnd ();
}


static int GL_GenerateShellTexture(void)
{
	int x, y, d;
	byte data[32][32][4];
	for (y = 0;y < 32;y++)
	{
		for (x = 0;x < 32;x++)
		{
			d = (sin(x * M_PI / 8.0f) + cos(y * M_PI / 8.0f)) * 64 + 64;
			if (d < 0)
				d = 0;
			if (d > 255)
				d = 255;
			data[y][x][0] = data[y][x][1] = data[y][x][2] = d;
			data[y][x][3] = 255;
		}
	}

	return GL_LoadTexture("shelltexture", 32, 32, &data[0][0][0], TEX_MIPMAP, 4);
}

void GL_DrawAliasOutlineFrame (aliashdr_t *paliashdr, int pose1, int pose2) 
{
	int *order, count;
	vec3_t interpolated_verts;
	float lerpfrac;
	trivertx_t *verts1, *verts2;

	GL_PolygonOffset(1, 1);

	glCullFace (GL_BACK);
	glPolygonMode (GL_FRONT, GL_LINE);

	// limit outline width, since even width == 3 can be considered as cheat.
	glLineWidth (bound(0.1, gl_outline_width.value, 3.0));


	glColor4f (0.0f, 0.0f, 0.0f, 1.0f);
	glEnable (GL_LINE_SMOOTH);
	glDisable (GL_TEXTURE_2D);

	lerpfrac = r_framelerp;
	lastposenum = (lerpfrac >= 0.5) ? pose2 : pose1;    

	verts2 = verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	for ( ;; )
	{
		count = *order++;

		if (!count) {
			break;
		}

		if (count < 0) {
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else {
			glBegin(GL_TRIANGLE_STRIP);
		}

		do {
			order += 2;

			if ((currententity->renderfx & RF_LIMITLERP))
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;

			VectorInterpolate(verts1->v, lerpfrac, verts2->v, interpolated_verts);
			glVertex3fv(interpolated_verts);

			verts1++;
			verts2++;
		} 
		while (--count);

		glEnd();
	}

	glColor4f (1, 1, 1, 1);    
	glPolygonMode (GL_FRONT, GL_FILL);
	glDisable (GL_LINE_SMOOTH);
	glCullFace (GL_FRONT);
	glEnable (GL_TEXTURE_2D);    

	GL_PolygonOffset(0, 0);
}

static glm_program_t drawShellPolyProgram;
static GLint drawShell_modelViewMatrix;
static GLint drawShell_projectionMatrix;
static GLint drawShell_shellSize;
static GLint drawShell_color;
static GLint drawShell_materialTex;
static GLint drawShell_time;

// Shell adds normals
void GLM_DrawShellPoly(GLenum type, byte* color, float shellSize, unsigned int vao, int start, int vertices)
{
	if (!drawShellPolyProgram.program) {
		const char* vertexShaderText =
			"#version 430\n"
			"\n"
			"layout(location = 0) in vec3 position;\n"
			"layout(location = 1) in vec2 tex;\n"
			"layout(location = 2) in vec3 normal;\n"
			"\n"
			"out vec2 TextureCoord;\n"
			"\n"
			"uniform mat4 modelViewMatrix;\n"
			"uniform mat4 projectionMatrix;\n"
			"uniform float shellSize;\n"
			"uniform float time;\n"
			"\n"
			"void main()\n"
			"{\n"
			"    gl_Position = projectionMatrix * modelViewMatrix * vec4(position + normal * shellSize, 1.0);\n"
			"    TextureCoord = vec2(tex.x * 2.0 + cos(time * 1.5), tex.y * 2.0 + sin(time * 1.1));\n"
			"}\n";
		const char* fragmentShaderText =
			"#version 430\n"
			"\n"
			"uniform vec4 color;\n"
			"uniform sampler2D materialTex;\n"
			"\n"
			"in vec2 TextureCoord;\n"
			"out vec4 frag_colour;\n"
			"\n"
			"void main()\n"
			"{\n"
			"    vec4 texColor;\n"
			"\n"
			"    texColor = texture(materialTex, TextureCoord);\n"
			"\n"
			"    frag_colour = texColor * color;\n"
			"}\n";

		// Initialise program for drawing image
		GLM_CreateSimpleProgram("drawShell poly", vertexShaderText, fragmentShaderText, &drawShellPolyProgram);

		drawShell_modelViewMatrix = glGetUniformLocation(drawShellPolyProgram.program, "modelViewMatrix");
		drawShell_projectionMatrix = glGetUniformLocation(drawShellPolyProgram.program, "projectionMatrix");
		drawShell_shellSize = glGetUniformLocation(drawShellPolyProgram.program, "shellSize");
		drawShell_color = glGetUniformLocation(drawShellPolyProgram.program, "color");
		drawShell_materialTex = glGetUniformLocation(drawShellPolyProgram.program, "materialTex");
		drawShell_time = glGetUniformLocation(drawShellPolyProgram.program, "time");
	}

	if (drawShellPolyProgram.program && vao) {
		float modelViewMatrix[16];
		float projectionMatrix[16];

		GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
		GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

		glUseProgram(drawShellPolyProgram.program);
		glUniformMatrix4fv(drawShell_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
		glUniformMatrix4fv(drawShell_projectionMatrix, 1, GL_FALSE, projectionMatrix);
		glUniform1f(drawShell_shellSize, shellSize);
		glUniform4f(drawShell_color, color[0] * 1.0f / 255, color[1] * 1.0f / 255, color[2] * 1.0f / 255, color[3] * 1.0f / 255);
		glUniform1i(drawShell_materialTex, 0);
		glUniform1f(drawShell_time, cl.time);

		glBindVertexArray(vao);
		glDrawArrays(type, start, vertices);
	}
}

void GL_DrawPowerupShell(aliashdr_t* paliashdr, int pose, trivertx_t* verts1, trivertx_t* verts2, float lerpfrac, qbool scrolldir)
{
	int *order, count;
	float scroll[2];
	float v[3];
	float shell_size = bound(0, gl_powerupshells_size.value, 20);
	byte color[4];
	int vertIndex = pose * paliashdr->vertsPerPose;

	// LordHavoc: set the state to what we need for rendering a shell
	if (!shelltexture) {
		shelltexture = GL_GenerateShellTexture();
	}
	GL_Bind(shelltexture);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	if (gl_powerupshells_style.integer) {
		glBlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	else {
		glBlendFunc(GL_ONE, GL_ONE);
	}

	if (scrolldir) {
		scroll[0] = cos(cl.time * -0.5); // FIXME: cl.time ????
		scroll[1] = sin(cl.time * -0.5);
	}
	else {
		scroll[0] = cos(cl.time * 1.5);
		scroll[1] = sin(cl.time * 1.1);
	}

	if (GL_ShadersSupported()) {
		color[0] = r_shellcolor[0] * 255;
		color[1] = r_shellcolor[1] * 255;
		color[2] = r_shellcolor[2] * 255;
		color[3] = bound(0, gl_powerupshells.value, 1) * 255;
	}

	// get the vertex count and primitive type
	order = (int *)((byte *)paliashdr + paliashdr->commands);
	for (;;) {
		GLenum drawMode = GL_TRIANGLE_STRIP;

		count = *order++;
		if (!count) {
			break;
		}

		if (count < 0) {
			count = -count;
			drawMode = GL_TRIANGLE_FAN;
		}

		if (GL_ShadersSupported()) {
			order += 2 * count;

			GLM_DrawShellPoly(drawMode, color, shell_size, paliashdr->vao, vertIndex, count);

			vertIndex += count;
		}
		else {
			// alpha so we can see colour underneath still
			glColor4f(r_shellcolor[0], r_shellcolor[1], r_shellcolor[2], bound(0, gl_powerupshells.value, 1));

			glBegin(drawMode);
			do {
				glTexCoord2f(((float *)order)[0] * 2.0f + scroll[0], ((float *)order)[1] * 2.0f + scroll[1]);

				order += 2;

				v[0] = r_avertexnormals[verts1->lightnormalindex][0] * shell_size + verts1->v[0];
				v[1] = r_avertexnormals[verts1->lightnormalindex][1] * shell_size + verts1->v[1];
				v[2] = r_avertexnormals[verts1->lightnormalindex][2] * shell_size + verts1->v[2];
				v[0] += lerpfrac * (r_avertexnormals[verts2->lightnormalindex][0] * shell_size + verts2->v[0] - v[0]);
				v[1] += lerpfrac * (r_avertexnormals[verts2->lightnormalindex][1] * shell_size + verts2->v[1] - v[1]);
				v[2] += lerpfrac * (r_avertexnormals[verts2->lightnormalindex][2] * shell_size + verts2->v[2] - v[2]);

				glVertex3f(v[0], v[1], v[2]);

				verts1++;
				verts2++;
			} while (--count);
			glEnd();
		}
	}

	// LordHavoc: reset the state to what the rest of the renderer expects
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static glm_program_t aliasFrameProgram;

void GL_DrawSimpleAliasFrame(aliashdr_t* paliashdr, int pose1, qbool scrolldir)
{
	extern void GLM_DrawPolygonByType(GLenum type, byte* color, unsigned int vao, int start, int vertices, qbool apply_lightmap, qbool apply_texture, qbool alpha_texture);

	int vertIndex = pose1 * paliashdr->vertsPerPose;
	byte color[4];
	float l;
	qbool texture = custom_model == NULL;

	if (r_shellcolor[0] || r_shellcolor[1] || r_shellcolor[2]) {
		GL_DrawPowerupShell(paliashdr, pose1, NULL, NULL, 0.0f, false);
		return;
	}
	else if (r_modelcolor[0] < 0) {
		color[0] = color[1] = color[2] = 255;
	}
	else {
		color[0] = r_modelcolor[0] * 255;
		color[1] = r_modelcolor[1] * 255;
		color[2] = r_modelcolor[2] * 255;
	}
	color[3] = r_modelalpha * 255;

	if (paliashdr->vao) {
		int* order = (int *) ((byte *) paliashdr + paliashdr->commands);
		int count;

		while (count = *order++) {
			GLenum drawMode = GL_TRIANGLE_STRIP;

			if (count < 0) {
				count = -count;
				drawMode = GL_TRIANGLE_FAN;
			}

			// texture coordinates now stored in the VBO
			order += 2 * count;

			// TODO: model lerping between frames
			// TODO: Vertex lighting etc
			// TODO: shadedots[vert.l] ... need to copy shadedots to program somehow
			// TODO: Coloured lighting per-vertex?
			l = shadedots[0] / 127.0;
			l = (l * shadelight + ambientlight) / 256.0;
			l = min(l, 1);

			if (custom_model == NULL) {
				if (r_modelcolor[0] < 0) {
					// normal color
					color[0] = color[1] = color[2] = l * 255;
				}
				else {
					// forced
					color[0] *= l;
					color[1] *= l;
					color[2] *= l;
				}
			}
			else {
				color[0] = custom_model->color_cvar.color[0];
				color[1] = custom_model->color_cvar.color[1];
				color[2] = custom_model->color_cvar.color[2];
			}

			GLM_DrawPolygonByType(drawMode, color, paliashdr->vao, vertIndex, count, false, texture, false);

			vertIndex += count;
		}
	}
}

void GL_DrawAliasFrame(aliashdr_t *paliashdr, int pose1, int pose2, qbool mtex, qbool scrolldir)
{
	int *order, count;
	vec3_t interpolated_verts;
	float l, lerpfrac;
	trivertx_t *verts1, *verts2;
	//VULT COLOURED MODEL LIGHTS
	int i;
	vec3_t lc;

	if (GL_ShadersSupported()) {
		GL_DrawSimpleAliasFrame(paliashdr, (r_framelerp >= 0.5) ? pose2 : pose1, scrolldir);
		return;
	}

	lerpfrac = r_framelerp;
	lastposenum = (lerpfrac >= 0.5) ? pose2 : pose1;

	verts2 = verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	if (r_shellcolor[0] || r_shellcolor[1] || r_shellcolor[2]) {
		GL_DrawPowerupShell(paliashdr, pose1, verts1, verts2, lerpfrac, scrolldir);
	}
	else {
		if (r_modelalpha < 1) {
			GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		}

		if (custom_model) {
			glDisable(GL_TEXTURE_2D);
			glColor4ub(custom_model->color_cvar.color[0], custom_model->color_cvar.color[1], custom_model->color_cvar.color[2], r_modelalpha * 255);
		}

		for ( ; ; ) {
			count = *order++;
			if (!count) {
				break;
			}

			if (count < 0) {
				count = -count;
				glBegin(GL_TRIANGLE_FAN);
			}
			else {
				glBegin(GL_TRIANGLE_STRIP);
			}

			do {
				// texture coordinates come from the draw list
				if (mtex) {
					qglMultiTexCoord2f(GL_TEXTURE0, ((float *)order)[0], ((float *)order)[1]);
					qglMultiTexCoord2f(GL_TEXTURE1, ((float *)order)[0], ((float *)order)[1]);
				}
				else {
					glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
				}

				order += 2;

				if ((currententity->renderfx & RF_LIMITLERP)) {
					lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;
				}

				// VULT VERTEX LIGHTING
				if (amf_lighting_vertex.value && !full_light) {
					l = VLight_LerpLight(verts1->lightnormalindex, verts2->lightnormalindex, lerpfrac, apitch, ayaw);
				}
				else {
					l = FloatInterpolate(shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]) / 127.0;
					l = (l * shadelight + ambientlight) / 256.0;
				}
				l = min(l, 1);

				//VULT COLOURED MODEL LIGHTS
				if (amf_lighting_colour.value && !full_light) {
					for (i = 0;i < 3;i++) {
						lc[i] = lightcolor[i] / 256 + l;
					}

					if (r_modelcolor[0] < 0) {
						glColor4f(lc[0], lc[1], lc[2], r_modelalpha); // normal color
					}
					else {
						glColor4f(r_modelcolor[0] * lc[0], r_modelcolor[1] * lc[1], r_modelcolor[2] * lc[2], r_modelalpha); // forced
					}
				}
				else if (custom_model == NULL) {
					if (r_modelcolor[0] < 0) {
						glColor4f(l, l, l, r_modelalpha); // normal color
					}
					else {
						glColor4f(r_modelcolor[0] * l, r_modelcolor[1] * l, r_modelcolor[2] * l, r_modelalpha); // forced
					}
				}

				VectorInterpolate(verts1->v, lerpfrac, verts2->v, interpolated_verts);
				glVertex3fv(interpolated_verts);

				verts1++;
				verts2++;
			} while (--count);

			glEnd();
		}

		if (r_modelalpha < 1) {
			GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		}

		if (custom_model) {
			glEnable(GL_TEXTURE_2D);
			custom_model = NULL;
		}
	}
}

void R_SetupAliasFrame(maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr, qbool mtex, qbool scrolldir, qbool outline)
{
	int oldpose, pose, numposes;
	float interval;

	oldpose = oldframe->firstpose;
	numposes = oldframe->numposes;
	if (numposes > 1) {
		interval = oldframe->interval;
		oldpose += (int) (r_refdef2.time / interval) % numposes;
	}

	pose = frame->firstpose;
	numposes = frame->numposes;
	if (numposes > 1) {
		interval = frame->interval;
		pose += (int) (r_refdef2.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, oldpose, pose, mtex, scrolldir);
	if (outline) {
		GL_DrawAliasOutlineFrame(paliashdr, oldpose, pose);
	}
}

void GL_DrawAliasShadow(aliashdr_t *paliashdr, int posenum)
{
	int *order, count;
	vec3_t point;
	float lheight = currententity->origin[2] - lightspot[2], height = 1 - lheight;
	trivertx_t *verts;

	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		} else {
			glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			//no texture for shadows
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0] * (point[2] +lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;
			//height -= 0.001;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}	
}

void R_AliasSetupLighting(entity_t *ent)
{
	int minlight, lnum;
	float add, fbskins;
	unsigned int i;
	unsigned int j;
	unsigned int k;
	vec3_t dist;
	model_t *clmodel;

	//VULT COLOURED MODEL LIGHTING
	float radiusmax = 0;

	clmodel = ent->model;

	custom_model = NULL;
	for (i = 0; i < sizeof (custom_model_colors) / sizeof (custom_model_colors[0]); ++i) {
		custom_model_color_t* test = &custom_model_colors[i];
		if (test->model_hint == clmodel->modhint) {
			if (test->color_cvar.string[0] && (test->amf_cvar == NULL || test->amf_cvar->integer == 0)) {
				custom_model = &custom_model_colors[i];
			}
			break;
		}
	}

	if (custom_model && custom_model->fullbright_cvar.integer) {
		ambientlight = 4096;
		shadelight = 0;
		full_light = true;
		return;
	}

	// make thunderbolt and torches full light
	if (clmodel->modhint == MOD_THUNDERBOLT) {
		ambientlight = 60 + 150 * bound(0, gl_shaftlight.value, 1);
		shadelight = 0;
		full_light = true;
		return;
	} else if (clmodel->modhint == MOD_FLAME) {
		ambientlight = 255;
		shadelight = 0;
		full_light = true;
		return;
	}

	//normal lighting
	full_light = false;
	ambientlight = shadelight = R_LightPoint (ent->origin);


/* FIXME: dimman... cache opt from fod */
	//VULT COLOURED MODEL LIGHTS
	if (amf_lighting_colour.value)
	{
		for (i = 0; i < MAX_DLIGHTS/32; i++) {
			if (cl_dlight_active[i]) {
				for (j = 0; j < 32; j++) {
					if ((cl_dlight_active[i]&(1<<j)) && i*32+j < MAX_DLIGHTS) {
						lnum = i*32 + j;

						VectorSubtract (ent->origin, cl_dlights[lnum].origin, dist);
						add = cl_dlights[lnum].radius - VectorLength(dist);

						if (add > 0)
						{
							//VULT VERTEX LIGHTING
							if (amf_lighting_vertex.value)
							{
								if (!radiusmax)
								{
									radiusmax = cl_dlights[lnum].radius;
									VectorCopy(cl_dlights[lnum].origin, vertexlight);
								}
								else if (cl_dlights[lnum].radius > radiusmax)
								{
									radiusmax = cl_dlights[lnum].radius;
									VectorCopy(cl_dlights[lnum].origin, vertexlight);
								}
							}

							if (cl_dlights[lnum].type == lt_custom) {
								VectorCopy(cl_dlights[lnum].color, dlight_color);
								VectorScale(dlight_color, (1.0/255), dlight_color); // convert color from byte to float
							}
							else
								VectorCopy(bubblecolor[cl_dlights[lnum].type], dlight_color);

							for (k=0;k<3;k++)
							{
								lightcolor[k] = lightcolor[k] + (dlight_color[k]*add)*2;
								if (lightcolor[k] > 256)
								{
									switch (k)
									{
										case 0:
											lightcolor[1] = lightcolor[1] - (1 * lightcolor[1]/3); 
											lightcolor[2] = lightcolor[2] - (1 * lightcolor[2]/3); 
											break;
										case 1:
											lightcolor[0] = lightcolor[0] - (1 * lightcolor[0]/3); 
											lightcolor[2] = lightcolor[2] - (1 * lightcolor[2]/3); 
											break;
										case 2:
											lightcolor[1] = lightcolor[1] - (1 * lightcolor[1]/3); 
											lightcolor[0] = lightcolor[0] - (1 * lightcolor[0]/3); 
											break;
									}
								}
							}
						}
					}
				}
			}
		}
	} else {
		for (i = 0; i < MAX_DLIGHTS/32; i++) {
			if (cl_dlight_active[i]) {
				for (j = 0; j < 32; j++) {
					if ((cl_dlight_active[i]&(1<<j)) && i*32+j < MAX_DLIGHTS) {
						lnum = i*32 + j;

						VectorSubtract (ent->origin, cl_dlights[lnum].origin, dist);
						add = cl_dlights[lnum].radius - VectorLength(dist);

						if (add > 0)
						{
							//VULT VERTEX LIGHTING
							if (amf_lighting_vertex.value)
							{
								if (!radiusmax)
								{
									radiusmax = cl_dlights[lnum].radius;
									VectorCopy(cl_dlights[lnum].origin, vertexlight);
								}
								else if (cl_dlights[lnum].radius > radiusmax)
								{
									radiusmax = cl_dlights[lnum].radius;
									VectorCopy(cl_dlights[lnum].origin, vertexlight);
								}
							}
							ambientlight += add;
						}
					}
				}
			}
		}
	}
	//calculate pitch and yaw for vertex lighting
	if (amf_lighting_vertex.value)
	{
		vec3_t dist, ang;
		apitch = currententity->angles[0];
		ayaw = currententity->angles[1];

		if (!radiusmax)
		{
			vlight_pitch = 45;
			vlight_yaw = 45;
		}
		else
		{
			VectorSubtract (vertexlight, currententity->origin, dist);
			vectoangles(dist, ang);
			vlight_pitch = ang[0];
			vlight_yaw = ang[1];
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// always give the gun some light
	if ((ent->renderfx & RF_WEAPONMODEL) && ambientlight < 24)
		ambientlight = shadelight = 24;

	// never allow players to go totally black
	if (clmodel->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL) {
		if (ambientlight < 8)
			ambientlight = shadelight = 8;
	}


	if (clmodel->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL) {
		fbskins = bound(0, r_fullbrightSkins.value, r_refdef2.max_fbskins);
		if (fbskins == 1 && gl_fb_models.value == 1) {
			ambientlight = shadelight = 4096;
			full_light = true;
		}
		else if (fbskins == 0) {
			ambientlight = max(ambientlight, 8);
			shadelight = max(shadelight, 8);
			full_light = false;
		}
		else if (fbskins) {
			ambientlight = max(ambientlight, 8 + fbskins * 120);
			shadelight = max(shadelight, 8 + fbskins * 120);
			full_light = true;
		}
	}
	else if (
			!((clmodel->modhint == MOD_EYES || clmodel->modhint == MOD_BACKPACK) && strncasecmp(Rulesets_Ruleset(), "default", 7)) &&
			(gl_fb_models.integer == 1 && clmodel->modhint != MOD_GIB && clmodel->modhint != MOD_VMODEL && !IsLocalSinglePlayerGame())
		) {
		ambientlight = shadelight = 4096;
	}

	minlight = cl.minlight;

	if (ambientlight < minlight)
		ambientlight = shadelight = minlight;
}

void R_DrawPowerupShell(int effects, int layer_no, float base_level, float effect_level,
		maliasframedesc_t *oldframe, maliasframedesc_t *frame, aliashdr_t *paliashdr)
{
	base_level = bound(0, base_level, 1);
	effect_level = bound(0, effect_level, 1);

	r_shellcolor[0] = r_shellcolor[1] = r_shellcolor[2] = base_level;

	if (effects & EF_RED)
		r_shellcolor[0] += effect_level;
	if (effects & EF_GREEN)
		r_shellcolor[1] += effect_level;
	if (effects & EF_BLUE)
		r_shellcolor[2] += effect_level;

	GL_DisableMultitexture();
	GL_TextureEnvMode(GL_MODULATE);
	R_SetupAliasFrame (oldframe, frame, paliashdr, false, layer_no == 1, false);
}

void R_DrawAliasModel(entity_t *ent)
{
	int i, anim, skinnum, texture, fb_texture, playernum = -1, local_skincolormode;
	float scale;
	vec3_t mins, maxs;
	aliashdr_t *paliashdr;
	model_t *clmodel;
	maliasframedesc_t *oldframe, *frame;
	cvar_t *cv = NULL;
	byte *color32bit = NULL;
	qbool outline = false;
	float oldMatrix[16];

	//	entity_t *self;
	//static sfx_t *step;//foosteps sounds, commented out
	//static int setstep;

	extern	cvar_t r_viewmodelsize, cl_drawgun;

	local_skincolormode=r_skincolormode.integer;

	VectorCopy (ent->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//TODO: use modhints here? 
	//VULT CORONAS	
	if (
			(!strcmp (ent->model->name, "progs/flame.mdl") || 
			 !strcmp (ent->model->name, "progs/flame0.mdl") || 
			 !strcmp (ent->model->name, "progs/flame3.mdl") ) && amf_coronas.value ) {
		//FIXME: This is slow and pathetic as hell, really we should just check the entity
		//alternativley add some kind of permanent client side TE for the torch
		NewStaticLightCorona (C_FIRE, ent->origin, ent);
	}

	if (ent->model->modhint == MOD_TELEPORTDESTINATION && amf_coronas.value) {
		NewStaticLightCorona (C_LIGHTNING, ent->origin, ent);
	}

	clmodel = ent->model;
	paliashdr = (aliashdr_t *) Mod_Extradata (ent->model);	//locate the proper data

	if (ent->frame >= paliashdr->numframes || ent->frame < 0) {
		if (ent->model->modhint != MOD_EYES) {
			Com_DPrintf("R_DrawAliasModel: no such frame %d\n", ent->frame);
		}

		ent->frame = 0;
	}
	if (ent->oldframe >= paliashdr->numframes || ent->oldframe < 0) {
		if (ent->model->modhint != MOD_EYES) {
			Com_DPrintf("R_DrawAliasModel: no such oldframe %d\n", ent->oldframe);
		}

		ent->oldframe = 0;
	}

	frame = &paliashdr->frames[ent->frame];
	oldframe = &paliashdr->frames[ent->oldframe];

	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame) {
		r_framelerp = 1.0;
	}
	else {
		r_framelerp = min(ent->framelerp, 1);
	}

	//culling
	if (!(ent->renderfx & RF_WEAPONMODEL)) {
		if (ent->angles[0] || ent->angles[1] || ent->angles[2]) {
			if (R_CullSphere(ent->origin, max(oldframe->radius, frame->radius))) {
				return;
			}
		}
		else {
			if (r_framelerp == 1) {	
				VectorAdd(ent->origin, frame->bboxmin, mins);
				VectorAdd(ent->origin, frame->bboxmax, maxs);
			}
			else {
				for (i = 0; i < 3; i++) {
					mins[i] = ent->origin[i] + min (oldframe->bboxmin[i], frame->bboxmin[i]);
					maxs[i] = ent->origin[i] + max (oldframe->bboxmax[i], frame->bboxmax[i]);
				}
			}
			if (R_CullBox(mins, maxs)) {
				return;
			}
		}
	}

	GL_EnableFog();

	//get lighting information
	R_AliasSetupLighting(ent);
	shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	//draw all the triangles
	c_alias_polys += paliashdr->numtris;
	GL_PushMatrix(GL_MODELVIEW, oldMatrix);
	R_RotateForEntity (ent);

	if (clmodel->modhint == MOD_EYES) {
		GL_Translate(GL_MODELVIEW, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		GL_Scale(GL_MODELVIEW, paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else if (ent->renderfx & RF_WEAPONMODEL) {
		scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;
		GL_Translate(GL_MODELVIEW, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		GL_Scale(GL_MODELVIEW, paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
	}
	else {
		GL_Translate(GL_MODELVIEW, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		GL_Scale(GL_MODELVIEW, paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}

	anim = (int) (r_refdef2.time * 10) & 3;
	skinnum = ent->skinnum;
	if (skinnum >= paliashdr->numskins || skinnum < 0) {
		Com_DPrintf ("R_DrawAliasModel: no such skin # %d\n", skinnum);
		skinnum = 0;
	}

	texture = paliashdr->gl_texturenum[skinnum][anim];
	fb_texture = paliashdr->fb_texturenum[skinnum][anim];

	r_modelalpha = ((ent->renderfx & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	//VULT MOTION TRAILS
	if (ent->alpha)
		r_modelalpha = ent->alpha;

	if(ent->scoreboard)
		playernum = ent->scoreboard - cl.players;

	// we can't dynamically colormap textures, so they are cached separately for the players.  Heads are just uncolored.
	if (!gl_nocolors.value) {
		if (playernum >= 0 && playernum < MAX_CLIENTS) {
			if (!ent->scoreboard->skin)
				CL_NewTranslation(playernum);
			texture    = playernmtextures[playernum];
			fb_texture = playerfbtextures[playernum];
		}
	}
	if (full_light || !gl_fb_models.value)
		fb_texture = 0;

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	if ((ent->model->modhint == MOD_PLAYER || ent->renderfx & RF_PLAYERMODEL)
			&& playernum >= 0 && playernum < MAX_CLIENTS)
	{
		if (cl.teamplay && strcmp(cl.players[playernum].team, TP_SkinForcingTeam()) == 0)
			cv = &r_teamskincolor;
		else 
			cv = &r_enemyskincolor;

		if (ISDEAD(ent->frame) && r_skincolormodedead.integer != -1)
			local_skincolormode=r_skincolormodedead.integer;

	}

	if (cv && cv->string[0])
		color32bit = cv->color;

	r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture

	// Check for outline on models.
	// We don't support outline for transparent models,
	// and we also check for ruleset, since we don't want outline on eyes.
	outline = ((gl_outline.integer & 1) && r_modelalpha == 1 && !RuleSets_DisallowModelOutline(clmodel));

	if (gl_meshdraw.integer) {
		glDisable(GL_CULL_FACE);
	}
	if (color32bit) {
		//
		// seems we select force some color for such model
		//

		for (i = 0; i < 3; i++) {
			r_modelcolor[i] = (float)color32bit[i] / 255.0;
			r_modelcolor[i] = bound(0, r_modelcolor[i], 1);
		}

		GL_DisableMultitexture();
		GL_Bind (local_skincolormode ? texture : particletexture); // particletexture is just solid white texture

		//
		// we may use different methods for filling model surfaces, mixing(modulate), replace, add etc..
		//	
		switch(local_skincolormode) {
			case 1:		GL_TextureEnvMode(GL_REPLACE);	break;
			case 2:		GL_TextureEnvMode(GL_BLEND);		break;
			case 3:		GL_TextureEnvMode(GL_DECAL);		break;
			case 4:		GL_TextureEnvMode(GL_ADD);		break;
			default:	GL_TextureEnvMode(GL_MODULATE);	break;
		}

		R_SetupAliasFrame (oldframe, frame, paliashdr, false, false, outline);

		r_modelcolor[0] = -1;  // by default no solid fill color for model, using texture
	}
	else
	{
		if (fb_texture && gl_mtexable) {
			GL_DisableMultitexture ();

			GL_Bind (texture);
			GL_TextureEnvMode(GL_MODULATE);

			GL_EnableMultitexture ();
			GL_Bind (fb_texture);

			GL_TextureEnvMode(GL_DECAL);

			R_SetupAliasFrame (oldframe, frame, paliashdr, true, false, outline);

			GL_DisableMultitexture ();
		} 
		else 
		{
			GL_DisableMultitexture();
			GL_Bind (texture);

			GL_TextureEnvMode(GL_MODULATE);

			R_SetupAliasFrame (oldframe, frame, paliashdr, false, false, outline);

			if (fb_texture) {
				GL_TextureEnvMode(GL_REPLACE);
				GL_Bind (fb_texture);

				GL_AlphaBlendFlags(GL_BLEND_ENABLED);

				R_SetupAliasFrame (oldframe, frame, paliashdr, false, false, false);

				GL_AlphaBlendFlags(GL_BLEND_DISABLED);
			}
		}
	}
	if (gl_meshdraw.integer) {
		glEnable(GL_CULL_FACE);
	}

	// FIXME: think need put it after caustics
	if (bound(0, gl_powerupshells.value, 1))
	{
		// always allow powerupshells for specs or demos.
		// do not allow powerupshells for eyes in other cases
		if ( ( cls.demoplayback || cl.spectator ) || ent->model->modhint != MOD_EYES )
		{
			if ((ent->effects & EF_RED) || (ent->effects & EF_GREEN) || (ent->effects & EF_BLUE)) {
				R_DrawPowerupShell(ent->effects, 0, gl_powerupshells_base1level.value,
						gl_powerupshells_effect1level.value, oldframe, frame, paliashdr);
				R_DrawPowerupShell(ent->effects, 1, gl_powerupshells_base2level.value,
						gl_powerupshells_effect2level.value, oldframe, frame, paliashdr);
			}

			memset(r_shellcolor, 0, sizeof(r_shellcolor));
		}
	}

	// Underwater caustics on alias models of QRACK -->
#define GL_RGB_SCALE 0x8573

	// MEAG: GLM-FIXME
	if (!GL_ShadersSupported() && (gl_caustics.value) && (underwatertexture && gl_mtexable && ISUNDERWATER(TruePointContents(ent->origin))))
	{
		GL_EnableMultitexture ();
		glBindTexture (GL_TEXTURE_2D, underwatertexture);

		glMatrixMode (GL_TEXTURE);
		glLoadIdentity ();
		glScalef (0.5, 0.5, 1);
		glRotatef (r_refdef2.time * 10, 1, 0, 0);
		glRotatef (r_refdef2.time * 10, 0, 1, 0);
		glMatrixMode (GL_MODELVIEW);

		GL_Bind (underwatertexture);

		GL_TextureEnvMode(GL_DECAL);        
		glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);

		R_SetupAliasFrame (oldframe, frame, paliashdr, true, false, false);

		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);

		GL_SelectTexture(GL_TEXTURE1);
		//glTexEnvi (GL_TEXTURE_ENV, GL_RGB_SCALE, 1); FIXME
		GL_TextureEnvMode(GL_REPLACE);
		glDisable (GL_TEXTURE_2D);

		glMatrixMode (GL_TEXTURE);
		glLoadIdentity ();
		glMatrixMode (GL_MODELVIEW);

		GL_DisableMultitexture ();
	}
	// <-- Underwater caustics on alias models of QRACK

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);

	//VULT MOTION TRAILS - No shadows on motion trails
	if ((r_shadows.value && !full_light && !(ent->renderfx & RF_NOSHADOW)) && !ent->alpha) {
		float theta;
		static float shadescale = 0;

		if (!shadescale)
			shadescale = 1 / sqrt(2);
		theta = -ent->angles[1] / 180 * M_PI;

		VectorSet(shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);

		GL_PushMatrix(GL_MODELVIEW, oldMatrix);
		glTranslatef (ent->origin[0],  ent->origin[1],  ent->origin[2]);
		glRotatef (ent->angles[1],  0, 0, 1);

		glDisable (GL_TEXTURE_2D);
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		glColor4f (0, 0, 0, 0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glEnable (GL_TEXTURE_2D);
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		GL_PopMatrix(GL_MODELVIEW, oldMatrix);
	}

	glColor3ubv (color_white);

	GL_DisableFog();
}

static void GLM_DrawSimpleItem(int texture, vec3_t origin, vec3_t angles, float scale)
{
	static GLuint simpleItemVBO;
	static GLuint simpleItemVAO;

	float oldMatrix[16];
	byte color[4] = { 255, 255, 255, 255 };

	if (!simpleItemVBO) {
		float verts[4][VERTEXSIZE] = { 0 };

		VectorSet(verts[0], 0, -1, -1);
		verts[0][3] = 1;
		verts[0][4] = 1;

		VectorSet(verts[1], 0, -1, 1);
		verts[1][3] = 1;
		verts[1][4] = 0;

		VectorSet(verts[2], 0, 1, 1);
		verts[2][3] = 0;
		verts[2][4] = 0;

		VectorSet(verts[3], 0, 1, -1);
		verts[3][3] = 0;
		verts[3][4] = 1;

		glGenBuffers(1, &simpleItemVBO);
		glBindBufferExt(GL_ARRAY_BUFFER, simpleItemVBO);
		glBufferDataExt(GL_ARRAY_BUFFER, 4 * VERTEXSIZE * sizeof(float), verts, GL_STATIC_DRAW);
	}

	if (!simpleItemVAO) {
		glGenVertexArrays(1, &simpleItemVAO);
		glBindVertexArray(simpleItemVAO);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glBindBufferExt(GL_ARRAY_BUFFER, simpleItemVBO);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * VERTEXSIZE, (void*) (sizeof(float) * 3));
	}

	GL_PushMatrix(GL_MODELVIEW, oldMatrix);

	GL_PopMatrix(GL_MODELVIEW, r_world_matrix);
	GL_Translate(GL_MODELVIEW, origin[0], origin[1], origin[2]);
	{
		float tempMatrix[16];

		GL_PushMatrix(GL_MODELVIEW, tempMatrix);
		// x = -y
		tempMatrix[0] = 0;
		tempMatrix[4] = -scale;
		tempMatrix[8] = 0;

		// y = z
		tempMatrix[1] = 0;
		tempMatrix[5] = 0;
		tempMatrix[9] = scale;

		// z = -x
		tempMatrix[2] = -scale;
		tempMatrix[6] = 0;
		tempMatrix[10] = 0;

		GL_PopMatrix(GL_MODELVIEW, tempMatrix);
	}

	glActiveTexture(GL_TEXTURE0);
	GL_Bind(texture);
	GLM_DrawTexturedPoly(color, simpleItemVAO, 0, 4, false, true);

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
}

static qbool R_DrawTrySimpleItem(void)
{
	int sprtype = gl_simpleitems_orientation.integer;
	float sprsize = bound(1, gl_simpleitems_size.value, 16), autorotate;
	int simpletexture;
	vec3_t point, right, up, org, offset, angles;

	if (!currententity || !currententity->model) {
		return false;
	}

	if (currententity->skinnum < 0 || currententity->skinnum >= MAX_SIMPLE_TEXTURES) {
		simpletexture = currententity->model->simpletexture[0]; // ah...
	}
	else {
		simpletexture = currententity->model->simpletexture[currententity->skinnum];
	}

	if (!simpletexture) {
		return false;
	}

	autorotate = anglemod(100 * cl.time);

	if (sprtype == SPR_ORIENTED) {
		// bullet marks on walls
		angles[0] = angles[2] = 0;
		angles[1] = anglemod(autorotate);
		AngleVectors(angles, NULL, right, up);
	}
	else if (sprtype == SPR_FACING_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		right[0] = currententity->origin[1] - r_origin[1];
		right[1] = -(currententity->origin[0] - r_origin[0]);
		right[2] = 0;
		VectorNormalizeFast(right);
		vectoangles(right, angles);
	}
	else if (sprtype == SPR_VP_PARALLEL_UPRIGHT) {
		VectorSet(up, 0, 0, 1);
		VectorCopy(vright, right);
		vectoangles(right, angles);
	}
	else {
		// normal sprite
		VectorCopy(vup, up);
		VectorCopy(vright, right);
		vectoangles(right, angles);
	}

	VectorCopy(currententity->origin, org);
	// brush models require some additional centering
	if (currententity->model->type == mod_brush) {
		extern cvar_t cl_model_bobbing;

		VectorSubtract(currententity->model->maxs, currententity->model->mins, offset);
		offset[2] = 0;
		VectorMA(org, 0.5, offset, org);

		if (cl_model_bobbing.value) {
			org[2] += sin(autorotate / 90 * M_PI) * 5 + 5;
		}
	}
	org[2] += sprsize;

	if (GL_ShadersSupported()) {
		glDisable(GL_CULL_FACE);
		GLM_DrawSimpleItem(simpletexture, org, angles, sprsize);
		glEnable(GL_CULL_FACE);
	}
	else {
		glPushAttrib(GL_ENABLE_BIT);

		glDisable(GL_CULL_FACE);
		GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);

		GL_Bind(simpletexture);

		glBegin (GL_QUADS);

		glTexCoord2f (0, 1);
		VectorMA (org, -sprsize, up, point);
		VectorMA (point, -sprsize, right, point);
		glVertex3fv (point);

		glTexCoord2f (0, 0);
		VectorMA (org, sprsize, up, point);
		VectorMA (point, -sprsize, right, point);
		glVertex3fv (point);

		glTexCoord2f (1, 0);
		VectorMA (org, sprsize, up, point);
		VectorMA (point, sprsize, right, point);
		glVertex3fv (point);

		glTexCoord2f (1, 1);
		VectorMA (org, -sprsize, up, point);
		VectorMA (point, sprsize, right, point);
		glVertex3fv (point);

		glEnd ();

		glPopAttrib();
	}
	return true;
}

void R_DrawEntitiesOnList(visentlist_t *vislist)
{
	int i;

	if (!r_drawentities.value || !vislist->count)
		return;

	if (vislist->alpha) {
		GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED);
	}

	// draw sprites separately, because of alpha_test
	for (i = 0; i < vislist->count; i++) 
	{
		currententity = &vislist->list[i];

		if (gl_simpleitems.value && R_DrawTrySimpleItem()) {
			continue;
		}

		switch (currententity->model->type) 
		{
			case mod_alias:
				// VULT NAILTRAIL - Hidenails
				if (amf_hidenails.value && currententity->model->modhint == MOD_SPIKE)
					break;
				// VULT ROCKETTRAILS - Hide rockets
				if (amf_hiderockets.value && currententity->model->flags & EF_ROCKET)
					break;
				// VULT CAMERAS - Show/Hide playermodel
				if (currententity->alpha == -1)
				{
					if (cameratype == C_NORMAL)
						break;
					else
						currententity->alpha = 1;
				}
				// VULT MOTION TRAILS
				if (currententity->alpha < 0)
					break;

				// Handle flame/flame0 model changes
				if (qmb_initialized)
				{
					if (!amf_part_fire.value && !strcmp(currententity->model->name, "progs/flame0.mdl"))
					{
						currententity->model = cl.model_precache[cl_modelindices[mi_flame]];
					}
					else if (amf_part_fire.value)
					{
						if (!strcmp(currententity->model->name, "progs/flame0.mdl"))
						{
							if (!ISPAUSED)
								ParticleFire (currententity->origin);
						}
						else if (!strcmp(currententity->model->name, "progs/flame.mdl")
								&& cl_flame0_model /* do we have progs/flame0.mdl? */)
						{
							if (!ISPAUSED)
								ParticleFire (currententity->origin);
							currententity->model = cl_flame0_model;
						}
						else if (!strcmp(currententity->model->name, "progs/flame2.mdl") || !strcmp(currententity->model->name, "progs/flame3.mdl"))
						{
							if (!ISPAUSED)
								ParticleFire (currententity->origin);
							continue;
						}
					}
				}

				R_DrawAliasModel (currententity);

				break;
			case mod_alias3:
				R_DrawAlias3Model (currententity);
				break;
			case mod_brush:

				// Get rid of Z-fighting for textures by offsetting the
				// drawing of entity models compared to normal polygons.
				// dimman: disabled for qcon
				if(gl_brush_polygonoffset.value > 0 && Ruleset_AllowPolygonOffset(currententity)) {
					GL_PolygonOffset(0.05, bound(0, (float)gl_brush_polygonoffset.value, 25.0));
					R_DrawBrushModel(currententity);
					GL_PolygonOffset(0, 0);
				} else {
					R_DrawBrushModel(currententity);
				}

				break;
			case mod_sprite:
				R_DrawSpriteModel (currententity);
				break;
				// not handled
			case mod_spr32:
				break;
		}
	}

	if (vislist->alpha) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	}
}

void R_DrawViewModel(void)
{
	centity_t *cent;
	static entity_t gun;

	//VULT CAMERA - Don't draw gun in external camera
	if (cameratype != C_NORMAL)
		return;

	if (!r_drawentities.value || !cl.viewent.current.modelindex)
		return;

	memset(&gun, 0, sizeof(gun));
	cent = &cl.viewent;
	currententity = &gun;

	if (!(gun.model = cl.model_precache[cent->current.modelindex]))
		Host_Error ("R_DrawViewModel: bad modelindex");

	VectorCopy(cent->current.origin, gun.origin);
	VectorCopy(cent->current.angles, gun.angles);
	gun.colormap = vid.colormap;
	gun.renderfx = RF_WEAPONMODEL | RF_NOSHADOW;
	if (r_lerpmuzzlehack.value) {
		if (cent->current.modelindex != cl_modelindices[mi_vaxe] &&
				cent->current.modelindex != cl_modelindices[mi_vbio] &&
				cent->current.modelindex != cl_modelindices[mi_vgrap] &&
				cent->current.modelindex != cl_modelindices[mi_vknife] &&
				cent->current.modelindex != cl_modelindices[mi_vknife2] &&
				cent->current.modelindex != cl_modelindices[mi_vmedi] &&
				cent->current.modelindex != cl_modelindices[mi_vspan])
		{
			gun.renderfx |= RF_LIMITLERP;			
			r_lerpdistance =  135;
		}
	}

	gun.effects |= (cl.stats[STAT_ITEMS] & IT_QUAD) ? EF_BLUE : 0;
	gun.effects |= (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) ? EF_RED : 0;
	gun.effects |= (cl.stats[STAT_ITEMS] & IT_SUIT) ? EF_GREEN : 0;

	gun.frame = cent->current.frame;
	if (cent->frametime >= 0 && cent->frametime <= r_refdef2.time) {
		gun.oldframe = cent->oldframe;
		gun.framelerp = (r_refdef2.time - cent->frametime) * 10;
	} else {
		gun.oldframe = gun.frame;
		gun.framelerp = -1;
	}


	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));

	switch(currententity->model->type)
	{
		case mod_alias:
			R_DrawAliasModel (currententity);
			break;
		case mod_alias3:
			R_DrawAlias3Model (currententity);
			break;
		default:
			Com_Printf("Not drawing view model of type %i\n", currententity->model->type);
			break;
	} 
	glDepthRange (gldepthmin, gldepthmax);
}


void R_PolyBlend(void)
{
	extern cvar_t gl_hwblend;

	if (vid_hwgamma_enabled && gl_hwblend.value && !cl.teamfortress) {
		return;
	}
	if (!v_blend[3]) {
		return;
	}

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	if (GL_ShadersSupported()) {
		color_t v_blend_color = RGBA_TO_COLOR(
			bound(0, v_blend[0], 1) * 255,
			bound(0, v_blend[1], 1) * 255,
			bound(0, v_blend[2], 1) * 255,
			bound(0, v_blend[3], 1) * 255
		);

		Draw_AlphaRectangleRGB(r_refdef.vrect.x, r_refdef.vrect.y, r_refdef.vrect.width, r_refdef.vrect.height, 0.0f, true, v_blend_color);
	}
	else {
		glDisable(GL_TEXTURE_2D);

		glColor4fv(v_blend);

		glBegin(GL_QUADS);
		glVertex2f(r_refdef.vrect.x, r_refdef.vrect.y);
		glVertex2f(r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y);
		glVertex2f(r_refdef.vrect.x + r_refdef.vrect.width, r_refdef.vrect.y + r_refdef.vrect.height);
		glVertex2f(r_refdef.vrect.x, r_refdef.vrect.y + r_refdef.vrect.height);
		glEnd();

		glEnable(GL_TEXTURE_2D);
		glColor3ubv(color_white);
	}
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
}

void R_BrightenScreen(void)
{
	extern float vid_gamma;
	float f;

	if (vid_hwgamma_enabled)
		return;
	if (v_contrast.value <= 1.0)
		return;

	f = min (v_contrast.value, 3);
	f = pow (f, vid_gamma);

	glDisable (GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	glBlendFunc (GL_DST_COLOR, GL_ONE);
	glBegin (GL_QUADS);
	while (f > 1) 
	{
		if (f >= 2)
		{
			glColor3ubv (color_white);
		}
		else
		{
			glColor3f (f - 1, f - 1, f - 1);
		}

		glVertex2f (0, 0);
		glVertex2f (vid.width, 0);
		glVertex2f (vid.width, vid.height);
		glVertex2f (0, vid.height);

		f *= 0.5;
	}
	glEnd ();
	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable (GL_TEXTURE_2D);
	GL_AlphaBlendFlags(GL_BLEND_DISABLED);
	glColor3ubv (color_white);
}

int SignbitsForPlane(mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test
	bits = 0;
	for (j = 0; j < 3; j++) {
		if (out->normal[j] < 0)
			bits |= 1 << j;
	}
	return bits;
}


void R_SetFrustum(void)
{
	int i;

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );

	for (i = 0; i < 4; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

void R_SetupFrame(void)
{
	vec3_t testorigin;
	mleaf_t	*leaf;

	R_AnimateLight ();

	r_framecount++;

	// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_oldviewleaf2 = r_viewleaf2;

	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);
	r_viewleaf2 = NULL;

	// check above and below so crossing solid water doesn't draw wrong
	if (r_viewleaf->contents <= CONTENTS_WATER && r_viewleaf->contents >= CONTENTS_LAVA) {
		// look up a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] += 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents == CONTENTS_EMPTY)
			r_viewleaf2 = leaf;
	} else if (r_viewleaf->contents == CONTENTS_EMPTY) {
		// look down a bit
		VectorCopy (r_origin, testorigin);
		testorigin[2] -= 10;
		leaf = Mod_PointInLeaf (testorigin, cl.worldmodel);
		if (leaf->contents <= CONTENTS_WATER &&	leaf->contents >= CONTENTS_LAVA)
			r_viewleaf2 = leaf;
	}

	V_SetContentsColor (r_viewleaf->contents);
	V_AddWaterfog (r_viewleaf->contents);	 
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;
}

void MYgluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar)
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan(fovy * M_PI / 360.0);
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	if (cl_multiview.value == 2 && !cl_mvinset.value && cls.mvdplayback) {
		GL_Frustum(xmin, xmax, ymin + (ymax - ymin)*0.25, ymax - (ymax - ymin)*0.25, zNear, zFar);
	}
	else if (CL_MultiviewActiveViews() == 3) {
		if (CL_MultiviewCurrentView() == 2) {
			GL_Frustum(xmin, xmax, ymin + (ymax - ymin)*0.25, ymax - (ymax - ymin)*0.25, zNear, zFar);
		}
		else {
			GL_Frustum(xmin, xmax, ymin, ymax, zNear, zFar);
		}
	}
	else {
		GL_Frustum(xmin, xmax, ymin, ymax, zNear, zFar);
	}
}

void R_SetViewports(int glx, int x, int gly, int y2, int w, int h, float max) 
{
	//
	// Setup Multiview-viewports
	//
	if (max == 1) 
	{
		glViewport (glx + x, gly + y2, w, h);
		return;
	}
	else if (max == 2 && cl_mvinset.value) 
	{
		if (CL_MultiviewCurrentView() == 2)
			glViewport (glx + x, gly + y2, w, h);
		else if (CL_MultiviewCurrentView() == 1 && !cl_sbar.value)
			glViewport (glx + x + (glwidth/3)*2 + 2, gly + y2 + (glheight/3)*2, w/3, h/3);
		else if (CL_MultiviewCurrentView() == 1 && cl_sbar.value)
			glViewport (glx + x + (glwidth/3)*2 + 2, gly + y2 + (h/3)*2, w/3, h/3);
		else 
			Com_Printf("ERROR!\n");
		return;
	}
	else if (max == 2 && !cl_mvinset.value) 
	{
		if (CL_MultiviewCurrentView() == 2)
			glViewport (0, h/2, w, h/2);
		else if (CL_MultiviewCurrentView() == 1)
			glViewport (0, 0, w, h/2-1);
		else 
			Com_Printf("ERROR!\n");
		return;

	}
	else if (max == 3) 
	{
		if (CL_MultiviewCurrentView() == 2)
			glViewport (0, h/2, w, h/2);
		else if (CL_MultiviewCurrentView() == 3)
			glViewport (0, 0, w/2, h/2-1);
		else
			glViewport (w/2, 0, w/2, h/2-1);
		return;
	}
	else 
	{
		if (CL_MultiviewCurrentView() == 2)
			glViewport (0, h/2, w/2, h/2);
		else if (CL_MultiviewCurrentView() == 3)
			glViewport (w/2, h/2, w/2, h/2);
		else if (CL_MultiviewCurrentView() == 4)
			glViewport (0, 0, w/2, h/2-1);
		else if (CL_MultiviewCurrentView() == 1)
			glViewport (w/2, 0, w/2, h/2-1);
	}

	return;
} 

void Debug_ProjectionMatrix(void);

void R_SetupGL(void)
{
	float screenaspect;
	extern int glwidth, glheight;
	int x, x2, y2, y, w, h, farclip;

	// set up viewpoint
	x = r_refdef.vrect.x * glwidth / vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
	y = (vid.height - r_refdef.vrect.y) * glheight / vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight / vid.height;

	// fudge around because of frac screen scale
	if (x > 0) {
		x--;
	}
	if (x2 < glwidth) {
		x2++;
	}
	if (y2 < 0) {
		y2--;
	}
	if (y < glheight) {
		y++;
	}

	w = x2 - x;
	h = y - y2;

	// Multiview
	GL_IdentityProjectionView();
	if (CL_MultiviewCurrentView() != 0 && CL_MultiviewEnabled()) {
		R_SetViewports(glx, x, gly, y2, w, h, cl_multiview.value);
	}
	if (!CL_MultiviewEnabled()) {
		glViewport(glx + x, gly + y2, w, h);
	}

	farclip = max((int)r_farclip.value, 4096);
	screenaspect = (float)r_refdef.vrect.width / r_refdef.vrect.height;
	MYgluPerspective(r_refdef.fov_y, screenaspect, r_nearclip.value, farclip);
	glCullFace(GL_FRONT);

	GL_IdentityModelView();
	GL_Rotate(GL_MODELVIEW, -90, 1, 0, 0);	    // put Z going up
	GL_Rotate(GL_MODELVIEW, 90, 0, 0, 1);	    // put Z going up
	GL_Rotate(GL_MODELVIEW, -r_refdef.viewangles[2], 1, 0, 0);
	GL_Rotate(GL_MODELVIEW, -r_refdef.viewangles[0], 0, 1, 0);
	GL_Rotate(GL_MODELVIEW, -r_refdef.viewangles[1], 0, 0, 1);
	GL_Translate(GL_MODELVIEW, -r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);
	GL_GetMatrix(GL_MODELVIEW_MATRIX, r_world_matrix);

	// set drawing parms
	if (gl_cull.value) {
		glEnable(GL_CULL_FACE);
	}
	else {
		glDisable(GL_CULL_FACE);
	}

	if (CL_MultiviewEnabled()) {
		glClear(GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc(GL_LEQUAL);
	}

	glDepthRange(gldepthmin, gldepthmax);

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);

	if (!GL_ShadersSupported()) {
		glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		glHint(GL_FOG_HINT, GL_NICEST);
	}

	glEnable(GL_DEPTH_TEST);

	if (gl_gammacorrection.integer) {
		glEnable(GL_FRAMEBUFFER_SRGB);
	}
	else {
		glDisable(GL_FRAMEBUFFER_SRGB);
	}
}

void R_Init(void)
{
	int i;

	Cmd_AddCommand ("loadsky", R_LoadSky_f);
	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);
#ifndef CLIENTONLY
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);
#endif
	Cmd_AddCommand ("gl_checkmodels", CheckModels_f);
	Cmd_AddCommand ("gl_inferno", InfernoFire_f);
	Cmd_AddCommand ("gl_setmode", Amf_SetMode_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register (&r_bloom);
	Cvar_Register (&r_bloom_darken);
	Cvar_Register (&r_bloom_alpha);
	Cvar_Register (&r_bloom_diamond_size);
	Cvar_Register (&r_bloom_intensity);
	Cvar_Register (&r_bloom_sample_size);
	Cvar_Register (&r_bloom_fast_sample);
	Cvar_Register (&r_drawentities);
	Cvar_Register (&r_lerpframes);
	Cvar_Register (&r_lerpmuzzlehack);
	Cvar_Register (&r_drawflame);
	Cvar_Register (&gl_detail);
	Cvar_Register (&gl_powerupshells);
	Cvar_Register (&gl_powerupshells_style);
	Cvar_Register (&gl_powerupshells_size);
	Cvar_Register (&gl_powerupshells_base1level);
	Cvar_Register (&gl_powerupshells_effect1level);
	Cvar_Register (&gl_powerupshells_base2level);
	Cvar_Register (&gl_powerupshells_effect2level);

	Cvar_Register (&gl_simpleitems);
	Cvar_Register (&gl_simpleitems_size);
	Cvar_Register (&gl_simpleitems_orientation);

	Cvar_Register (&gl_motion_blur);
	Cvar_Register (&gl_motion_blur_fps);
	Cvar_Register (&gl_motion_blur_norm);
	Cvar_Register (&gl_motion_blur_hurt);
	Cvar_Register (&gl_motion_blur_dead);

	Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
	Cvar_Register (&gl_solidparticles);
	Cvar_Register (&gl_squareparticles);
	Cvar_Register (&gl_part_explosions);
	Cvar_Register (&gl_part_trails);
	Cvar_Register (&gl_part_tracer1_color);
	Cvar_Register (&gl_part_tracer1_size);
	Cvar_Register (&gl_part_tracer1_time);
	Cvar_Register (&gl_part_tracer2_color);
	Cvar_Register (&gl_part_tracer2_size);
	Cvar_Register (&gl_part_tracer2_time);
	Cvar_Register (&gl_part_spikes);
	Cvar_Register (&gl_part_gunshots);
	Cvar_Register (&gl_part_blood);
	Cvar_Register (&gl_part_telesplash);
	Cvar_Register (&gl_part_blobs);
	Cvar_Register (&gl_part_lavasplash);
	Cvar_Register (&gl_part_inferno);
	Cvar_Register (&gl_particle_style);

	Cvar_Register (&gl_part_detpackexplosion_fire_color);
	Cvar_Register (&gl_part_detpackexplosion_ray_color);

	Cvar_SetCurrentGroup(CVAR_GROUP_TURB);
	Cvar_Register (&r_skyname);
	Cvar_Register (&r_fastsky);
	Cvar_Register (&r_skycolor);
	Cvar_Register (&r_fastturb);

	Cvar_Register (&r_telecolor);
	Cvar_Register (&r_lavacolor);
	Cvar_Register (&r_slimecolor);
	Cvar_Register (&r_watercolor);

	Cvar_Register (&r_novis);
	Cvar_Register (&r_wateralpha);
	Cvar_Register (&gl_caustics);
	if (!COM_CheckParm ("-nomtex")) {
		Cvar_Register (&gl_waterfog);
		Cvar_Register (&gl_waterfog_density);
		Cvar_Register (&gl_waterfog_color_water);
		Cvar_Register (&gl_waterfog_color_lava);
		Cvar_Register (&gl_waterfog_color_slime);
	}

	Cvar_Register (&gl_fogenable); 
	Cvar_Register (&gl_fogstart); 
	Cvar_Register (&gl_fogend); 
	Cvar_Register (&gl_fogsky);
	Cvar_Register (&gl_fogred); 
	Cvar_Register (&gl_fogblue);
	Cvar_Register (&gl_foggreen);

	Cvar_SetCurrentGroup(CVAR_GROUP_BLEND);
	Cvar_Register (&gl_polyblend);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&r_fullbrightSkins);
	Cvar_Register (&r_enemyskincolor);
	Cvar_Register (&r_teamskincolor);
	Cvar_Register (&r_skincolormode);
	Cvar_Register (&r_skincolormodedead);

	Cvar_SetCurrentGroup(CVAR_GROUP_LIGHTING);
	Cvar_Register (&r_dynamic);
	Cvar_Register (&gl_fb_bmodels);
	Cvar_Register (&gl_fb_models);
	Cvar_Register (&gl_lightmode);
	Cvar_Register (&gl_flashblend);
	Cvar_Register (&gl_rl_globe);
	Cvar_Register (&r_shadows);
	Cvar_Register (&r_fullbright);
	Cvar_Register (&r_lightmap);
	Cvar_Register (&gl_shaftlight);
	Cvar_Register (&gl_loadlitfiles);
	Cvar_Register (&gl_colorlights);

	Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
	Cvar_Register (&gl_playermip);
	Cvar_Register (&gl_subdivide_size);
	Cvar_Register (&gl_lumaTextures);
	Cvar_Register (&r_drawflat);
	Cvar_Register (&r_wallcolor);
	Cvar_Register (&r_floorcolor);
	Cvar_Register (&gl_textureless); //Qrack

	for (i = 0; i < sizeof (custom_model_colors) / sizeof (custom_model_colors[0]); ++i) {
		Cvar_Register (&custom_model_colors[i].color_cvar);
		Cvar_Register (&custom_model_colors[i].fullbright_cvar);
	}

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register (&r_farclip);
	Cvar_Register (&gl_smoothmodels);
	Cvar_Register (&gl_affinemodels);
	Cvar_Register (&gl_clear);
	Cvar_Register (&gl_clearColor);
	Cvar_Register (&gl_cull);

	Cvar_Register(&gl_brush_polygonoffset);

	Cvar_Register (&gl_nocolors);
	Cvar_Register (&gl_finish);
	Cvar_Register (&gl_gammacorrection);
	Cvar_Register (&gl_modulate);

	Cvar_Register (&gl_outline);
	Cvar_Register (&gl_outline_width);
	Cvar_Register (&gl_meshdraw);

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&r_speeds);
	Cvar_Register (&r_netgraph);
	Cvar_Register (&r_netstats);

	Cvar_Register(&cl_multiview);
	Cvar_Register(&cl_mvdisplayhud);
	Cvar_Register(&cl_mvhudvertical);
	Cvar_Register(&cl_mvhudflip);
	Cvar_Register(&cl_mvhudpos);
	cl_mvhudpos.OnChange = SCR_OnChangeMVHudPos;
	Cvar_Register(&cl_mvinset);
	Cvar_Register(&cl_mvinsetcrosshair);
	Cvar_Register(&cl_mvinsethud);

	Cvar_ResetCurrentGroup();

	if (!hud_netgraph)
		hud_netgraph = HUD_Register("netgraph", /*"r_netgraph"*/ NULL, "Shows your network conditions in graph-form. With netgraph you can monitor your latency (ping), packet loss and network errors.",
				HUD_PLUSMINUS | HUD_ON_SCORES, ca_onserver, 0, SCR_HUD_Netgraph,
				"0", "top", "left", "bottom", "0", "0", "0", "0 0 0", NULL,
				"swap_x",       "0",
				"swap_y",       "0",
				"inframes",     "0",
				"scale",        "256",
				"ploss",        "1",
				"width",        "256",
				"height",       "32",
				"lostscale",    "1",
				"full",         "0",
				"alpha",        "1",
				NULL);

	// this minigl driver seems to slow us down if the particles are drawn WITHOUT Z buffer bits 
	if (gl_vendor && !strcmp(gl_vendor, "METABYTE/WICKED3D")) 
		Cvar_SetDefault(&gl_solidparticles, 1); 

	R_InitTextures ();	// FIXME: not sure is this safe re-init
	R_InitBubble ();	// safe re-init
	R_InitParticles (); // safe re-init imo
	CI_Init ();			// safe re-init

	//VULT STUFF
	if (qmb_initialized)
	{
		InitVXStuff(); // safe re-init imo
	}
	else
		; // FIXME: hm, in case of vid_restart, what we must do if before vid_restart qmb_initialized was true?

	InitTracker();

	R_InitOtherTextures (); // safe re-init

	R_InitBloomTextures();
}

void R_RenderScene(void)
{
	extern void Skins_PreCache(void);

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_Check_R_FullBright(); // check for changes in r_fullbright

	R_MarkLeaves ();	// done here so we know if we're in water

	Skins_PreCache ();  // preache skins if needed

	R_DrawWorld ();		// adds static entities to the list

	R_DrawEntitiesOnList(&cl_visents);
	R_DrawEntitiesOnList(&cl_alphaents);

	R_DrawWaterSurfaces();

	if (!GL_ShadersSupported()) {
		GL_DisableMultitexture();

		GL_ConfigureFog();
	}
}

void OnChange_gl_clearColor(cvar_t *v, char *s, qbool *cancel) {
	byte *color;
	char buf[MAX_COM_TOKEN];

	strlcpy(buf,s,sizeof(buf));
	color = StringToRGB(buf);

	clearColor[0] = color[0] / 255.0;
	clearColor[1] = color[1] / 255.0;
	clearColor[2] = color[2] / 255.0;

	glClearColor (clearColor[0], clearColor[1], clearColor[2], 1.0);
}

void R_Clear(void)
{
	int clearbits = 0;

	// meag: temp
	if (GL_ShadersSupported() && !cl_multiview.value) {
		clearbits |= GL_COLOR_BUFFER_BIT;
	}

	// This used to cause a bug with some graphics cards when
	// in multiview mode. It would clear all but the last
	// drawn views.
	if (!cl_multiview.value && (gl_clear.value || (!vid_hwgamma_enabled && v_contrast.value > 1)))
	{
		clearbits |= GL_COLOR_BUFFER_BIT;
	}

	if (gl_clear.value) {
		if (gl_fogenable.value) {
			glClearColor(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 0.5);//Tei custom clear color
		}
		else {
			glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.0);
		}
	}

	clearbits |= GL_DEPTH_BUFFER_BIT;
	glClear (clearbits);
	gldepthmin = 0;
	gldepthmax = 1;
	glDepthFunc (GL_LEQUAL);

	glDepthRange (gldepthmin, gldepthmax);
}

// player velocity is drawn on screen
// as 3d vector and its projections
static void draw_velocity_3d(void)
{
	extern cvar_t show_velocity_3d_offset_forward;
	extern cvar_t show_velocity_3d_offset_down;
	extern cvar_t show_velocity_3d;

	vec3_t *origin = &r_refdef.vieworg;
	vec3_t *angles = &r_refdef.viewangles;

	const float vx = cl.simvel[0];
	const float vy = cl.simvel[1];
	const float vz = cl.simvel[2];

	const float yaw_degrees = (*angles)[YAW];
	const float yaw = DEG2RAD(yaw_degrees);

	const double c = cos(yaw);
	const double s = sin(yaw);

	const double scale_factor = 0.04;
	const float v_side = (float) (scale_factor * (-vx * s + vy * c));
	const float v_forward = (float) (scale_factor * (vx * c + vy * s));
	const float v_up = (float) (scale_factor * vz);

	const float line_width = 10.f;
	const float stipple_line_width = 5.f;
	const float stipple_line_colour[3] = { 0.5f, 0.5f, 0.5f };
	const vec3_t v3_zero = {0.f, 0.f, 0.f };
	float oldMatrix[16];

	GL_PushMatrix(GL_MODELVIEW, oldMatrix);

	GL_Translate(GL_MODELVIEW, (*origin)[0], (*origin)[1], (*origin)[2]);
	GL_Rotate(GL_MODELVIEW, yaw_degrees, 0.f, 0.f, 1.f);
	GL_Translate(GL_MODELVIEW, show_velocity_3d_offset_forward.value, 0.f, -show_velocity_3d_offset_down.value);

	glPushAttrib(GL_LINE_BIT | GL_TEXTURE_BIT);

	glDisable(GL_TEXTURE_2D);

	switch (show_velocity_3d.integer)
	{
		case 1:                    //show vertical
			glEnable(GL_LINE_STIPPLE);
			glLineStipple(1, 0xFF00);
			glLineWidth(stipple_line_width);

			glColor3fv(stipple_line_colour);
			glBegin(GL_LINES);
			glVertex3f(v_forward, v_side, 0.f);
			glVertex3f(v_forward, v_side, v_up);
			glEnd();

			glDisable(GL_LINE_STIPPLE);
			glLineWidth(line_width);
			glColor3f(0.f, 1.f, 0.f);

			glBegin(GL_LINES);
			glVertex3fv(v3_zero);
			glVertex3f(v_forward, v_side, v_up);
			glEnd();
			//no break here

		case 2:                    //show horizontal velocity only
			glColor3f(1.f, 0.f, 0.f);
			glLineWidth(line_width);
			glBegin(GL_LINES);
			glVertex3fv(v3_zero);
			glVertex3f(v_forward, v_side, 0.f);
			glEnd();

			glEnable(GL_LINE_STIPPLE);
			glLineStipple(1, 0xFF00);
			glColor3fv(stipple_line_colour);
			glLineWidth(stipple_line_width);

			glBegin(GL_LINE_LOOP);
			glVertex3fv(v3_zero);
			glVertex3f(0.f, v_side, 0.f);
			glVertex3f(v_forward, v_side, 0.f);
			glVertex3f(v_forward, 0.f, 0.f);
			glEnd();

		default:
			break;
	}

	glPopAttrib();
	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
}

/*
   Motion blur effect.
   Stolen from FTE engine.
   */
static void R_RenderSceneBlurDo(float alpha)
{
	static double last_time;
	double current_time = Sys_DoubleTime(), diff_time = current_time - last_time;
	double fps = gl_motion_blur_fps.value > 0 ? gl_motion_blur_fps.value : 77;
	qbool draw = (alpha >= 0); // negative alpha mean we don't draw anything but copy screen only.
	float oldProjectionMatrix[16];
	float oldModelviewMatrix[16];

	int vwidth = 1, vheight = 1;
	float vs, vt, cs, ct;

	// Remember all attributes.
	glPushAttrib(GL_ALL_ATTRIB_BITS);

	// alpha more than 0.5 are wrong.
	alpha = bound(0.1, alpha, 0.5);

	if (gl_support_arb_texture_non_power_of_two)
	{	//we can use any size, supposedly
		vwidth = glwidth;
		vheight = glheight;
	}
	else
	{	//limit the texture size to square and use padding.
		while (vwidth < glwidth)
			vwidth *= 2;
		while (vheight < glheight)
			vheight *= 2;
	}

	glViewport (0, 0, glwidth, glheight);

	GL_Bind(sceneblur_texture);

	// go 2d
	GL_PushMatrix(GL_PROJECTION, oldProjectionMatrix);
	GL_PushMatrix(GL_MODELVIEW, oldModelviewMatrix);
	GL_OrthographicProjection(0, glwidth, 0, glheight, -99999, 99999);
	GL_IdentityModelView();

	//blend the last frame onto the scene
	//the maths is because our texture is over-sized (must be power of two)
	cs = vs = (float)glwidth / vwidth * 0.5;
	ct = vt = (float)glheight / vheight * 0.5;
	// qqshka: I don't get what is gl_motionblurscale, so simply removed it.
	vs *= 1;//gl_motionblurscale.value;
	vt *= 1;//gl_motionblurscale.value;

	glDisable(GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);

	glColor4f(1, 1, 1, alpha);

	if (draw)
	{
		glBegin(GL_QUADS);
		glTexCoord2f(cs-vs, ct-vt);
		glVertex2f(0, 0);
		glTexCoord2f(cs+vs, ct-vt);
		glVertex2f(glwidth, 0);
		glTexCoord2f(cs+vs, ct+vt);
		glVertex2f(glwidth, glheight);
		glTexCoord2f(cs-vs, ct+vt);
		glVertex2f(0, glheight);
		glEnd();
	}

	// Restore matrices.
	GL_PopMatrix(GL_PROJECTION, oldProjectionMatrix);
	GL_PopMatrix(GL_MODELVIEW, oldModelviewMatrix);

	// With high frame rate frames difference is soo smaaaal, so motion blur almost unnoticeable,
	// so I copy frame not every frame.
	if (diff_time >= 1.0 / fps)
	{
		last_time = current_time;

		//copy the image into the texture so that we can play with it next frame too!
		glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, vwidth, vheight, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}

	// Restore attributes.
	glPopAttrib();
}

static void R_RenderSceneBlur(void)
{
	if (!gl_motion_blur.integer)
	{
		// Motion blur disabled entirely.
		return;
	}

	// FIXME: Actually here should be some smoothing code for transaction from one case to another,
	// since for example if we turned off blur for everything but hurt, when we feel pain we use blur, but when
	// pain is ended we saddenly turning blur off, that does not look natural.

	if (gl_motion_blur_dead.value && cl.stats[STAT_HEALTH] < 1)
	{
		// We are dead.
		R_RenderSceneBlurDo (gl_motion_blur_dead.value);
	}
	// We are alive, lets check different cases.
	else if (gl_motion_blur_hurt.value && cl.hurtblur > cl.time)
	{
		// Hurt.
		R_RenderSceneBlurDo (gl_motion_blur_hurt.value);
	}
	else if (gl_motion_blur_norm.value)
	{
		// Plain case.
		R_RenderSceneBlurDo (gl_motion_blur_norm.value);
	}
	else
	{
		// We do not really blur anything, just copy image, so if we start bluring it will be smooth transaction.
		R_RenderSceneBlurDo (-1);
	}
}

void R_RenderPostProcess (void)
{
	R_RenderSceneBlur();
	R_BloomBlend();
}

void R_RenderView(void)
{
	extern void DrawCI (void);

	double time1 = 0, time2;
	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value) {
		glFinish ();
		time1 = Sys_DoubleTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	if (gl_finish.value)
		glFinish ();

	R_Clear();

	// render normal view
	R_RenderScene();

	R_RenderDlights();
	R_DrawParticles();

	if (!GL_ShadersSupported()) {
		DrawCI();

		//VULT: CORONAS
		//Even if coronas gets turned off, let active ones fade out
		if (amf_coronas.value || CoronaCount) {
			R_DrawCoronas();
		}
	}

	R_DrawViewModel();

	if (!GL_ShadersSupported()) {
		extern cvar_t show_velocity_3d;
		if (show_velocity_3d.integer) {
			draw_velocity_3d();
		}
	}

	SCR_SetupAutoID();

	if (r_speeds.value) {
		time2 = Sys_DoubleTime();
		Print_flags[Print_current] |= PR_TR_SKIP;
		Com_Printf("%3i ms  %4i wpoly %4i epoly\n", (int)((time2 - time1) * 1000), c_brush_polys, c_alias_polys);
	}
}

