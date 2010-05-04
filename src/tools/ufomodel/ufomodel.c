/**
 * @file ufomodel.c
 * @brief Starting point for model tool
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

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

#include "../../shared/ufotypes.h"
#include "../../common/mem.h"
#include "../../shared/shared.h"
#include "../../client/renderer/r_local.h"
#include "../../shared/images.h"

#define VERSION "0.1"

rstate_t r_state;
image_t *r_noTexture;

typedef enum {
	ACTION_NONE,

	ACTION_MDX,
	ACTION_SKINEDIT,
	ACTION_SKINCHECK,
	ACTION_SKINFIX
} ufoModelAction_t;

typedef struct modelConfig_s {
	qboolean overwrite;
	qboolean verbose;
	char fileName[MAX_QPATH];
	ufoModelAction_t action;
	float smoothness;
	char inputName[MAX_QPATH];
} modelConfig_t;

static modelConfig_t config;

struct memPool_s *com_genericPool;
struct memPool_s *com_fileSysPool;
struct memPool_s *vid_modelPool;
struct memPool_s *vid_imagePool;

static void Exit(int exitCode) __attribute__ ((__noreturn__));

static void Exit (int exitCode)
{
	Mem_Shutdown();

	exit(exitCode);
}

void Com_Printf (const char *format, ...)
{
	char out_buffer[4096];
	va_list argptr;

	va_start(argptr, format);
	Q_vsnprintf(out_buffer, sizeof(out_buffer), format, argptr);
	va_end(argptr);

	printf("%s", out_buffer);
}

void Com_DPrintf (int level, const char *fmt, ...)
{
	if (config.verbose) {
		char outBuffer[4096];
		va_list argptr;

		va_start(argptr, fmt);
		Q_vsnprintf(outBuffer, sizeof(outBuffer), fmt, argptr);
		va_end(argptr);

		Com_Printf("%s", outBuffer);
	}
}

image_t *R_LoadImageData (const char *name, byte * pic, int width, int height, imagetype_t type)
{
	image_t *image;
	size_t len;

	len = strlen(name);
	if (len >= sizeof(image->name))
		Com_Error(ERR_DROP, "R_LoadImageData: \"%s\" is too long", name);
	if (len == 0)
		Com_Error(ERR_DROP, "R_LoadImageData: name is empty");

	image = Mem_PoolAlloc(sizeof(*image), vid_imagePool, 0);
	image->has_alpha = qfalse;
	image->type = type;
	image->width = width;
	image->height = height;

	Q_strncpyz(image->name, name, sizeof(image->name));
	/* drop extension */
	if (len >= 4 && image->name[len - 4] == '.') {
		image->name[len - 4] = '\0';
		Com_Printf("Image with extension: '%s'\n", name);
	}

	return image;
}

#ifdef DEBUG
image_t *R_FindImageDebug (const char *pname, imagetype_t type, const char *file, int line)
#else
image_t *R_FindImage (const char *pname, imagetype_t type)
#endif
{
	char lname[MAX_QPATH];
	image_t *image;
	SDL_Surface *surf;

	if (!pname || !pname[0])
		Com_Error(ERR_FATAL, "R_FindImage: NULL name");

	/* drop extension */
	Com_StripExtension(pname, lname, sizeof(lname));

	if (Img_LoadImage(lname, &surf)) {
		image = R_LoadImageData(lname, surf->pixels, surf->w, surf->h, type);
		SDL_FreeSurface(surf);
	} else {
		image = NULL;
	}

	/* no fitting texture found */
	if (!image) {
		Com_Printf("  \\ - could not load skin '%s'\n", pname);
		image = r_noTexture;
	}

	return image;
}

/**
 * @note Both client and server can use this, and it will
 * do the appropriate things.
 */
void Com_Error (int code, const char *fmt, ...)
{
	va_list argptr;
	static char msg[1024];

	va_start(argptr, fmt);
	Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	fprintf(stderr, "Error: %s\n", msg);
	Exit(1);
}


/**
 * @brief Loads in a model for the given name
 * @param[in] name Filename relative to base dir and with extension (models/model.md2)
 */
static model_t *LoadModel (const char *name)
{
	model_t *mod;
	byte *buf;
	int modfilelen;

	/* load the file */
	modfilelen = FS_LoadFile(name, &buf);
	if (!buf) {
		Com_Printf("Could not load '%s'\n", name);
		return NULL;
	}

	mod = Mem_PoolAlloc(sizeof(*mod), vid_modelPool, 0);
	Q_strncpyz(mod->name, name, sizeof(mod->name));

	/* call the appropriate loader */
	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
		/* MD2 header */
		R_ModLoadAliasMD2Model(mod, buf, modfilelen, qfalse);
		break;

	case DPMHEADER:
		R_ModLoadAliasDPMModel(mod, buf, modfilelen);
		break;

	case IDMD3HEADER:
		/* MD3 header */
		R_ModLoadAliasMD3Model(mod, buf, modfilelen);
		break;

	default:
		if (!Q_strcasecmp(mod->name + strlen(mod->name) - 4, ".obj"))
			R_LoadObjModel(mod, buf, modfilelen);
		else
			Com_Error(ERR_FATAL, "R_ModForName: unknown fileid for %s", mod->name);
	}

	FS_FreeFile(buf);

	return mod;
}

static void WriteToFile (const model_t *mod, const mAliasMesh_t *mesh, const char *fileName)
{
	int i, frame;
	qFILE f;
	uint32_t version = MDX_VERSION;
	int32_t numIndexes, numVerts, idx;

	Com_Printf("  \\ - writing to file '%s'\n", fileName);

	FS_OpenFile(fileName, &f, FILE_WRITE);
	if (!f.f) {
		Com_Printf("  \\ - can not open '%s' for writing\n", fileName);
		return;
	}

	FS_Write(IDMDXHEADER, strlen(IDMDXHEADER), &f);
	version = LittleLong(version);
	FS_Write(&version, sizeof(version), &f);

	numIndexes = LittleLong(mesh->num_tris * 3);
	numVerts = LittleLong(mesh->num_verts);
	FS_Write(&mesh->num_verts, sizeof(int32_t), &f);
	FS_Write(&numIndexes, sizeof(int32_t), &f);

	for (i = 0; i < mesh->num_tris * 3; i++) {
		idx = LittleLong(mesh->indexes[i]);
		FS_Write(&idx, sizeof(int32_t), &f);
	}

	for (frame = 0; frame < mod->alias.num_frames; frame++) {
		int32_t offset = mesh->num_verts * frame;
		for (i = 0; i < mesh->num_verts; i++) {
			mAliasVertex_t *v = &mesh->vertexes[i + offset];
			int j;
			for (j = 0; j < 3; j++) {
				v->normal[j] = LittleFloat(v->normal[j]);
				v->tangent[j] = LittleFloat(v->tangent[j]);
			}
		}

		for (i = 0; i < mesh->num_verts; i++) {
			mAliasVertex_t *v = &mesh->vertexes[i + offset];
			FS_Write(v->normal, sizeof(vec3_t), &f);
			FS_Write(v->tangent, sizeof(vec4_t), &f);
		}
	}

	FS_CloseFile(&f);
}

static int PrecalcNormalsAndTangents (const char *filename)
{
	char mdxFileName[MAX_QPATH];
	model_t *mod;
	int i;
	int cntCalculated = 0;

	Com_Printf("- model '%s'\n", filename);

	Com_StripExtension(filename, mdxFileName, sizeof(mdxFileName));
	Q_strcat(mdxFileName, ".mdx", sizeof(mdxFileName));

	if (!config.overwrite && FS_CheckFile("%s", mdxFileName) != -1) {
		Com_Printf("  \\ - mdx already exists\n");
		return 0;
	}

	mod = LoadModel(filename);
	if (!mod)
		Com_Error(ERR_DROP, "Could not load %s", filename);

	Com_Printf("  \\ - # meshes '%i', # frames '%i'\n", mod->alias.num_meshes, mod->alias.num_frames);

	for (i = 0; i < mod->alias.num_meshes; i++) {
		mAliasMesh_t *mesh = &mod->alias.meshes[i];
		R_ModCalcUniqueNormalsAndTangents(mesh, mod->alias.num_frames, config.smoothness);
		/** @todo currently md2 models only have one mesh - for
		 * md3 files this would get overwritten for each mesh */
		WriteToFile(mod, mesh, mdxFileName);

		cntCalculated++;
	}

	return cntCalculated;
}

static void PrecalcNormalsAndTangentsBatch (const char *pattern)
{
	const char *filename;
	int cntCalculated, cntAll;

	FS_BuildFileList(pattern);

	cntAll = cntCalculated = 0;

	while ((filename = FS_NextFileFromFileList(pattern)) != NULL) {
		cntAll++;
		cntCalculated += PrecalcNormalsAndTangents(filename);
	}

	FS_NextFileFromFileList(NULL);

	Com_Printf("%i/%i\n", cntCalculated, cntAll);
}

static void Usage (void)
{
	Com_Printf("Usage:\n");
	Com_Printf(" -mdx                     generate mdx files\n");
	Com_Printf(" -skinfix                 fix skins for md2 models\n");
	Com_Printf(" -skincheck               check the skins for every md2 model\n");
	Com_Printf(" -skinedit <filename>     edit skin of a model\n");
	Com_Printf(" -overwrite               overwrite existing mdx files\n");
	Com_Printf(" -s <float>               sets the smoothness value for normal-smoothing (in the range -1.0 to 1.0)\n");
	Com_Printf(" -f <filename>            build tangentspace for the specified model file\n");
	Com_Printf(" -v --verbose             print debug messages\n");
	Com_Printf(" -h --help                show this help screen\n");
}

static void UM_DefaultParameter (void)
{
	config.smoothness = 0.5;
}

/**
 * @brief Parameter parsing
 */
static void UM_Parameter (int argc, const char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-overwrite")) {
			config.overwrite = qtrue;
		} else if (!strcmp(argv[i], "-f") && (i + 1 < argc)) {
			Q_strncpyz(config.inputName, argv[++i], sizeof(config.inputName));
		} else if (!strcmp(argv[i], "-s") && (i + 1 < argc)) {
			config.smoothness = strtod(argv[++i], NULL);
			if (config.smoothness < -1.0 || config.smoothness > 1.0){
				Usage();
				Exit(1);
			}
		} else if (!strcmp(argv[i], "-mdx")) {
			config.action = ACTION_MDX;
		} else if (!strcmp(argv[i], "-skinfix")) {
			config.action = ACTION_SKINFIX;
		} else if (!strcmp(argv[i], "-skincheck")) {
			config.action = ACTION_SKINCHECK;
		} else if (!strcmp(argv[i], "-skinedit")) {
			config.action = ACTION_SKINEDIT;
			if (i + 1 == argc) {
				Usage();
				Exit(1);
			}
			Q_strncpyz(config.fileName, argv[i + 1], sizeof(config.fileName));
			i++;
		} else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")) {
			config.verbose = qtrue;
		} else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
			Usage();
			Exit(0);
		} else {
			Com_Printf("Parameters unknown. Try --help.\n");
			Usage();
			Exit(1);
		}
	}
}

static void MD2Check (const dMD2Model_t *md2, const char *fileName, int bufSize)
{
	/* sanity checks */
	const uint32_t version = LittleLong(md2->version);
	uint32_t numSkins;

	if (version != MD2_ALIAS_VERSION)
		Com_Error(ERR_DROP, "%s has wrong version number (%i should be %i)", fileName, version, MD2_ALIAS_VERSION);

	if (bufSize != LittleLong(md2->ofs_end))
		Com_Error(ERR_DROP, "model %s broken offset values (%i, %i)", fileName, bufSize, LittleLong(md2->ofs_end));

	numSkins = LittleLong(md2->num_skins);
	if (numSkins < 0 || numSkins >= MD2_MAX_SKINS)
		Com_Error(ERR_DROP, "Could not load model '%s' - invalid num_skins value: %i\n", fileName, numSkins);
}

static void MD2SkinEdit (const byte *buf, const char *fileName, int bufSize, void *userData)
{
	const char *md2Path;
	uint32_t numSkins;
	int i;
	const dMD2Model_t *md2 = (const dMD2Model_t *)buf;

	MD2Check(md2, fileName, bufSize);

	md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
	numSkins = LittleLong(md2->num_skins);

	for (i = 0; i < numSkins; i++) {
		const char *name = md2Path + i * MD2_MAX_SKINNAME;
		Com_Printf("  \\ - skin %i: %s\n", i, name);
		/** @todo: Implement this */
	}
}

typedef void (*modelWorker_t) (const byte *buf, const char *fileName, int bufSize, void *userData);

/**
 * @brief The caller has to ensure that the model is from the expected format
 * @param worker The worker callback
 * @param fileName The file name of the model to load
 * @param userData User data that is passed to the worker function
 */
static void ModelWorker (modelWorker_t worker, const char *fileName, void *userData)
{
	byte *buf = NULL;
	int modfilelen;

	/* load the file */
	modfilelen = FS_LoadFile(fileName, &buf);
	if (!buf)
		Com_Error(ERR_FATAL, "%s not found", fileName);

	switch (LittleLong(*(unsigned *) buf)) {
	case IDALIASHEADER:
	case DPMHEADER:
	case IDMD3HEADER:
	case IDBSPHEADER:
		worker(buf, fileName, modfilelen, userData);
		break;

	default:
		if (!Q_strcasecmp(fileName + strlen(fileName) - 4, ".obj"))
			worker(buf, fileName, modfilelen, userData);
		else
			Com_Error(ERR_DROP, "ModelWorker: unknown fileid for %s", fileName);
	}

	FS_FreeFile(buf);
}

static void MD2SkinFix (const byte *buf, const char *fileName, int bufSize, void *userData)
{
	const char *md2Path;
	uint32_t numSkins;
	int i;
	const dMD2Model_t *md2 = (const dMD2Model_t *)buf;
	byte *model = NULL;

	MD2Check(md2, fileName, bufSize);

	md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
	numSkins = LittleLong(md2->num_skins);

	for (i = 0; i < numSkins; i++) {
		const char *extension;
		int errors = 0;
		const char *name = md2Path + i * MD2_MAX_SKINNAME;

		if (name[0] != '.')
			errors++;
		else
			/* skip the . to not confuse the extension extraction below */
			name++;

		extension = Com_GetExtension(name);
		if (extension != NULL)
			errors++;

		if (errors > 0) {
			dMD2Model_t *fixedMD2;
			char *skinPath;
			char path[MD2_MAX_SKINNAME];
			char pathBuf[MD2_MAX_SKINNAME];
			const char *fixedPath;
			if (model == NULL) {
				model = Mem_Dup(buf, bufSize);
				Com_Printf("model: %s\n", fileName);
			}
			fixedMD2 = (dMD2Model_t *)model;
			skinPath = (char *) fixedMD2 + LittleLong(fixedMD2->ofs_skins) + i * MD2_MAX_SKINNAME;

			memset(path, 0, sizeof(path));

			if (extension != NULL) {
				Com_StripExtension(name, pathBuf, sizeof(pathBuf));
				fixedPath = pathBuf;
			} else {
				fixedPath = name;
			}
			if (name[0] != '.')
				Com_sprintf(path, sizeof(path), ".%s", Com_SkipPath(fixedPath));
			Com_Printf("  \\ - skin %i: changed path to '%s'\n", i + 1, path);
			if (R_AliasModelGetSkin(fileName, path) == r_noTexture) {
				Com_Printf("    \\ - could not load the skin with the new path\n");
			} else {
				memcpy(skinPath, path, sizeof(path));
			}
		}
	}
	if (model != NULL) {
		FS_WriteFile(model, bufSize, fileName);
		Mem_Free(model);
	}
}

static void MD2SkinCheck (const byte *buf, const char *fileName, int bufSize, void *userData)
{
	const char *md2Path;
	uint32_t numSkins;
	int i;
	qboolean headline = qfalse;
	const dMD2Model_t *md2 = (const dMD2Model_t *)buf;

	MD2Check(md2, fileName, bufSize);

	md2Path = (const char *) md2 + LittleLong(md2->ofs_skins);
	numSkins = LittleLong(md2->num_skins);

	for (i = 0; i < numSkins; i++) {
		const char *extension;
		int errors = 0;
		const char *name = md2Path + i * MD2_MAX_SKINNAME;

		if (name[0] != '.')
			errors++;
		else
			/* skip the . to not confuse the extension extraction below */
			name++;

		extension = Com_GetExtension(name);
		if (extension != NULL)
			errors++;

		if (errors > 0) {
			if (!headline) {
				Com_Printf("model: %s\n", fileName);
				headline = qtrue;
			}
			Com_Printf("  \\ - skin %i: %s - %i errors/warnings\n", i + 1, name, errors);
			if (name[0] != '.')
				Com_Printf("    \\ - skin contains full path\n");
			if (extension != NULL)
				Com_Printf("    \\ - skin contains extension '%s'\n", extension);
			if (R_AliasModelGetSkin(fileName, md2Path + i * MD2_MAX_SKINNAME) == r_noTexture)
				Com_Printf("  \\ - could not load the skin\n");
		}
	}
}

static void MD2Visitor (modelWorker_t worker, void *userData)
{
	const char *fileName;
	const char *pattern = "**.md2";

	FS_BuildFileList(pattern);

	while ((fileName = FS_NextFileFromFileList(pattern)) != NULL)
		ModelWorker(worker, fileName, userData);

	FS_NextFileFromFileList(NULL);
}

static void SkinCheck (void)
{
	MD2Visitor(MD2SkinCheck, NULL);
}

static void SkinFix (void)
{
	MD2Visitor(MD2SkinFix, NULL);
}

int main (int argc, const char **argv)
{
	Com_Printf("---- ufomodel "VERSION" ----\n");
	Com_Printf(BUILDSTRING"\n");

	UM_DefaultParameter();
	UM_Parameter(argc, argv);

	if (config.action == ACTION_NONE) {
		Usage();
		Exit(1);
	}

	Swap_Init();
	Mem_Init();

	com_genericPool = Mem_CreatePool("ufomodel");
	com_fileSysPool = Mem_CreatePool("ufomodel filesys");
	vid_modelPool = Mem_CreatePool("ufomodel model");
	vid_imagePool = Mem_CreatePool("ufomodel image");

	FS_InitFilesystem(qfalse);

	r_noTexture = Mem_PoolAlloc(sizeof(*r_noTexture), vid_imagePool, 0);
	Q_strncpyz(r_noTexture->name, "noTexture", sizeof(r_noTexture->name));

	switch (config.action) {
	case ACTION_MDX:
		if (config.inputName[0] == '\0') {
			PrecalcNormalsAndTangentsBatch("**.md2");
			PrecalcNormalsAndTangentsBatch("**.md3");
			PrecalcNormalsAndTangentsBatch("**.dpm");
			/** @todo see https://sourceforge.net/tracker/?func=detail&aid=2993773&group_id=157793&atid=805242 */
			/*PrecalcNormalsAndTangentsBatch("**.obj");*/
		} else {
			PrecalcNormalsAndTangents(config.inputName);
		}
		break;

	case ACTION_SKINEDIT:
		ModelWorker(MD2SkinEdit, config.fileName, NULL);
		break;

	case ACTION_SKINCHECK:
		SkinCheck();
		break;

	case ACTION_SKINFIX:
		SkinFix();
		break;

	default:
		Exit(1);
	}

	Mem_Shutdown();

	return 0;
}
