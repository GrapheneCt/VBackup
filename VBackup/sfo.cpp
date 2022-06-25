#include <kernel.h>
#include <paf.h>

#include "sfo.h"

#define SFO_MAGIC 0x46535000

#define PSF_TYPE_BIN 0
#define PSF_TYPE_STR 2
#define PSF_TYPE_VAL 4

SFO::SFO(const char *path)
{
	SceInt32 res = -1;
	SceSize fsize = 0;
	LocalFile file;
	LocalFile::OpenArg oarg;

	buffer = SCE_NULL;

	oarg.filename = path;
	res = file.Open(&oarg);
	if (res < 0)
		return;

	fsize = (SceSize)file.GetFileSize();
	buffer = sce_paf_malloc(fsize);
	if (!buffer)
		return;

	file.Read(buffer, fsize);
	file.Close();
}

SFO::~SFO()
{
	if (buffer)
		sce_paf_free(buffer);
}

SceInt32 SFO::GetValue(const char *name, SceUInt32 *pVal)
{
	if (!pVal || !name)
		return SCE_ERROR_ERRNO_EINVAL;

	if (!buffer)
		return SCE_ERROR_ERRNO_ENOENT;

	SfoHeader *header = (SfoHeader *)buffer;
	SfoEntry *entries = (SfoEntry *)((SceUInt32)buffer + sizeof(SfoHeader));

	if (header->magic != SFO_MAGIC)
		return SCE_ERROR_ERRNO_EILSEQ;

	int i;
	for (i = 0; i < header->count; i++) {
		if (sce_paf_strcmp((char *)(buffer + header->keyofs + entries[i].nameofs), name) == 0) {
			*pVal = *(SceUInt32 *)(buffer + header->valofs + entries[i].dataofs);
			return SCE_OK;
		}
	}

	return SCE_ERROR_ERRNO_ENODATA;
}

SceInt32 SFO::GetString(const char *name, string *pStr)
{
	if (!pStr || !name)
		return SCE_ERROR_ERRNO_EINVAL;

	if (!buffer)
		return SCE_ERROR_ERRNO_ENOENT;

	SfoHeader *header = (SfoHeader *)buffer;
	SfoEntry *entries = (SfoEntry *)((uint32_t)buffer + sizeof(SfoHeader));

	if (header->magic != SFO_MAGIC)
		return SCE_ERROR_ERRNO_EILSEQ;

	int i;
	for (i = 0; i < header->count; i++) {
		if (sce_paf_strcmp((char *)(buffer + header->keyofs + entries[i].nameofs), name) == 0) {
			char *data = (char *)(buffer + header->valofs + entries[i].dataofs);
			pStr->clear();
			pStr->append(data, sce_paf_strlen(data));
			return SCE_OK;
		}
	}

	return SCE_ERROR_ERRNO_ENODATA;
}

string SFO::GetTitle()
{
	string res;
	GetString("TITLE", &res);
	return res;
}

string SFO::GetTitle(const char *path)
{
	string res;
	SFO sfo(path);
	sfo.GetString("TITLE", &res);
	return res;
}