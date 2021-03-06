
#include <kernel.h>
#include <paf/stdc.h>
#include <npdrm.h>
#include <promoterutil.h>
#include <vshbridge.h>
#include <appmgr.h>
#include <sce_atomic.h>
#include <libdbg.h>

#include "fat_format.h"
#include "fs_list.h"
#include "file_copy.h"
#include "paf_gzip.h"
#include "br_core.h"

#define SCE_PKG_TYPE_VITA 1

typedef struct SceNpDrmRifInfoParam { // size is 0x28-bytes
	char *content_id;
	SceUInt64 *account_id;
	int *license_version;
	int *license_flags;
	int *flags;
	int *sku_flags;
	SceInt64 *lic_start_time;
	SceInt64 *lic_exp_time;
	SceUInt64 *flags2;
	int padding;
} SceNpDrmRifInfoParam;

typedef struct SceNpDrmLicense { // size is 0x200
	SceInt16 version;       // -1, 1
	SceInt16 version_flags; // 0, 1
	SceInt16 license_type;  // 1
	SceInt16 license_flags; // 0x400:non-check ecdsa
	SceUInt64 account_id;
	char content_id[0x30];
	char key_table[0x10];
	char key1[0x10];
	SceInt64 start_time;
	SceInt64 expiration_time;
	char ecdsa_signature[0x28];
	SceInt64 flags;
	char key2[0x10];
	char unk_0xB0[0x10];
	char open_psid[0x10];
	char unk_0xD0[0x10];
	char cmd56_handshake_part[0x14];
	int debug_upgradable;
	int unk_0xF8;
	int sku_flag;
	char rsa_signature[0x100];
} SceNpDrmLicense;

int _sceNpDrmGetRifInfo(const SceNpDrmLicense *license, SceSize license_size, int check_sign, SceNpDrmRifInfoParam *pParam);

const SceUInt64 restore_aid_list[3] = {
	0LL,
	1LL,
	0x0123456789ABCDEFLL
};

const char restore_temp_path[] = "ux0:temp_restore";

static int16_t s_executionInterrupt = 0;

SceInt32(*BR_event_callback)(SceInt32 event, SceInt32 status, SceInt64 value, ScePVoid data, ScePVoid argp);

/* WRAPPER HELPERS */

int vshIoMount(int id, const char *path, int permission, int a4, int a5, int a6) {

	SceInt32 syscall[6];

	sce_paf_memset(syscall, 0, sizeof(syscall));

	syscall[0] = a4;
	syscall[1] = a5;
	syscall[2] = a6;

	return _vshIoMount(id, path, permission, syscall);
}

int sceNpDrmGetRifInfo(const SceNpDrmLicense *license, SceSize license_size, int check_sign, char *content_id, SceUInt64 *account_id, int *license_version, int *license_flags, int *flags, int *sku_flags, SceInt64 *lic_start_time, SceInt64 *lic_exp_time, SceUInt64 *flags2) {

	SceNpDrmRifInfoParam param;

	param.content_id = content_id;
	param.account_id = account_id;
	param.license_version = license_version;
	param.license_flags = license_flags;
	param.flags = flags;
	param.sku_flags = sku_flags;
	param.lic_start_time = lic_start_time;
	param.lic_exp_time = lic_exp_time;
	param.flags2 = flags2;
	param.padding = 0;

	return _sceNpDrmGetRifInfo(license, license_size, check_sign, &param);
}

/* FS HELPERS */

int write_file(const char *path, const void *data, SceSize size) {

	if (data == NULL || size == 0)
		return -1;

	SceUID fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
	if (fd < 0)
		return fd;

	sceIoWrite(fd, data, size);
	sceIoClose(fd);

	return 0;
}

int read_file(const char *path, void *data, SceSize size) {

	if (data == NULL || size == 0)
		return -1;

	SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;

	int res = sceIoRead(fd, data, size);
	sceIoClose(fd);

	return res;
}

int fs_remove(const char *path) {

	int res;
	FSListEntry *pEnt = NULL;

	res = fs_list_init(&pEnt, path, NULL, NULL);
	if (res < 0) {
		return res;
	}

	res = fs_list_remove(pEnt);

	fs_list_fini(pEnt);
	pEnt = NULL;

	return res;
}

int fs_list_copy_callback(const FSListEntry *ent, void *argp) {

	int res;
	SceUInt32 fpos = *(SceUInt32 *)(argp + 0);
	const char *dst = *(const char **)(argp + 4);
	BREventCallback callback = *(BREventCallback *)(argp + 8);
	ScePVoid cb_argp = *(ScePVoid *)(argp + 0xC);

	int16_t interrupted = sceAtomicLoad16AcqRel(&s_executionInterrupt);

	if (interrupted) {
		sceAtomicStore16AcqRel(&s_executionInterrupt, 0);
		return SCE_ERROR_ERRNO_EINTR;
	}

	const char *path = &(ent->path_full[fpos]);

	SceSize path_len = sce_paf_strlen(dst) + sce_paf_strlen(path);

	char *dst_path = sce_paf_malloc(path_len + 1);
	if (NULL == dst_path) {
		return SCE_ERROR_ERRNO_ENOMEM;
	}

	sce_paf_snprintf(dst_path, path_len + 1, "%s%s", dst, path);

	dst_path[path_len] = 0;

	if (callback != NULL) {
		callback(BR_EVENT_NOTICE_CONT_COPY, BR_STATUS_BEGIN, 0, ent->path_full, cb_argp);
	}

	if (SCE_STM_ISDIR(ent->ent.d_stat.st_mode)) {
		res = sceIoMkdir(dst_path, 0666);
	}
	else {
		res = file_copy(dst_path, ent->path_full);
	}

	if (callback != NULL) {
		if (res < 0) {
			callback(BR_EVENT_NOTICE_CONT_COPY, BR_STATUS_ERROR, (SceUInt64)res, NULL, cb_argp);
		}

		callback(BR_EVENT_NOTICE_CONT_COPY, BR_STATUS_END, 0, ent->path_full, cb_argp);
	}

	sce_paf_free(dst_path);
	dst_path = NULL;

	return res;
}

int fs_list_copy(const FSListEntry *pEnt, const char *dst, BREventCallback callback, ScePVoid *cb_argp) {

	SceUInt32 args[4];

	args[0] = sce_paf_strlen(pEnt->path_full);
	args[1] = (SceUInt32)dst;
	args[2] = (SceUInt32)callback;
	args[3] = (SceUInt32)cb_argp;

	return fs_list_execute(pEnt->child, fs_list_copy_callback, args);
}

/* BR HELPERS */

int restore_license_helper(char *rif_path, int rif_path_size, const char *rif_name, SceNpDrmLicense *license) {

	int res;

	sce_paf_snprintf(rif_path, rif_path_size, "grw0:license/%s", rif_name);

	res = read_file(rif_path, license, sizeof(*license));
	if (res == sizeof(*license)) {
		res = sceNpDrmGetRifInfo(license, sizeof(*license), 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("sceNpDrmGetRifInfo(): 0x%08X\n", res);
			SCE_DBG_LOG_ERROR("rif_name: %s\n", rif_name);
		}
	}
	else {
		SCE_DBG_LOG_ERROR("read_file(%s): 0x%08X\n", rif_path, res);
		res = -1;
	}

	return res;
}

int restore_license(const char *restore_temp_path) {

	int res;
	SceUInt64 account_id = 0LL;
	char path[0x80], rif_name[0x30], rif_path[0x80];
	SceNpDrmLicense license;

	// If invalid rif, remove and make by system it.
	sce_paf_snprintf(path, sizeof(path), "%s/app/sce_sys/package/work.bin", restore_temp_path);

	res = read_file(path, &license, sizeof(license));
	if (res == sizeof(license)) {
		res = sceNpDrmGetRifInfo(&license, sizeof(license), 1, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		if (res == 0) {
			return 0;
		}
	}

	if (res >= 0) {
		res = sceIoRemove(path);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("sceIoRemove(): 0x%08X\n", res);
			return res;
		}
	}

	// Find to account license file.
	res = sceRegMgrGetKeyBin("/CONFIG/NP/", "account_id", &account_id, sizeof(account_id));
	if (res >= 0) {
		SCE_DBG_LOG_DEBUG("Current account ID: 0x%016llX\n", account_id);

		res = _sceNpDrmGetRifName(rif_name, account_id);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("_sceNpDrmGetRifName(): 0x%08X\n", res);
			return res;
		}

		res = restore_license_helper(rif_path, sizeof(rif_path), rif_name, &license);
		if (res >= 0) {
			return file_copy(path, rif_path);
		}

		res = _sceNpDrmGetFixedRifName(rif_name, account_id);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("_sceNpDrmGetFixedRifName(): 0x%08X\n", res);
			return res;
		}

		res = restore_license_helper(rif_path, sizeof(rif_path), rif_name, &license);
		if (res >= 0) {
			return file_copy(path, rif_path);
		}
	}

	SCE_DBG_LOG_DEBUG("Doesn't found account license. Retry as prefixed AID\n");

	for (int i = 0; i < 3; i++) {
		res = _sceNpDrmGetRifName(rif_name, restore_aid_list[i]);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("_sceNpDrmGetRifName(): 0x%08X\n", res);
			return res;
		}

		res = restore_license_helper(rif_path, sizeof(rif_path), rif_name, &license);
		if (res >= 0) {
			return file_copy(path, rif_path);
		}
	}

	for (int i = 0; i < 3; i++) {
		res = _sceNpDrmGetFixedRifName(rif_name, restore_aid_list[i]);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("_sceNpDrmGetFixedRifName(): 0x%08X\n", res);
			return res;
		}

		res = restore_license_helper(rif_path, sizeof(rif_path), rif_name, &license);
		if (res >= 0) {
			return file_copy(path, rif_path);
		}
	}

	SCE_DBG_LOG_ERROR("Doesn't found license (final)\n");

	return -1;
}

int do_restore_core(const char *restore_temp_path) {

	int res;
	SceUInt32 nFile = 0;
	FSListEntry *pEnt = NULL;
	char path[0x80];

	sce_paf_snprintf(path, sizeof(path), "%s/app", restore_temp_path);

	res = sceIoMkdir(path, 0777);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceIoMkdir(%s): 0x%08X\n", path, res);
		return res;
	}

	res = fs_list_init(&pEnt, "grw0:/app", NULL, &nFile);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("fs_list_init(): 0x%08X\n", res);
		return res;
	}

	if (BR_event_callback != NULL)
		BR_event_callback(BR_EVENT_NOTICE_CONT_NUMBER, BR_STATUS_NONE, (SceInt64)nFile, NULL, NULL);

	res = fs_list_copy(pEnt, path, BR_event_callback, NULL);

	fs_list_fini(pEnt);
	pEnt = NULL;

	if (res < 0) {
		SCE_DBG_LOG_ERROR("fs_list_copy(): 0x%08X\n", res);
		return res;
	}

	res = restore_license(restore_temp_path);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("restore_license(): 0x%08X\n", res);
		return res;
	}

	if (BR_event_callback != NULL)
		BR_event_callback(BR_EVENT_NOTICE_CONT_PROMOTE, BR_STATUS_BEGIN, 0, NULL, NULL);

	scePromoterUtilityInit();

	res = scePromoterUtilityPromotePkgWithRif(path, 1);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("scePromoterUtilityPromotePkgWithRif(): 0x%08X\n", res);
	}

	scePromoterUtilityExit();

	if (BR_event_callback != NULL)
		BR_event_callback(BR_EVENT_NOTICE_CONT_PROMOTE, BR_STATUS_END, 0, NULL, NULL);

	return res;
}

int do_restore_patch_core(const char *restore_temp_path) {

	int res;
	SceIoStat stat;
	SceUInt32 nFile = 0;
	FSListEntry *pEnt = NULL;
	char path[0x80];

	res = sceIoGetstat("grw0:/patch", &stat);
	if (res < 0) {
		SCE_DBG_LOG_DEBUG("sceIoGetstat(): 0x%08X\n", res);
		return 0; // Not have update patch.
	}

	sce_paf_snprintf(path, sizeof(path), "%s/patch", restore_temp_path);

	res = sceIoMkdir(path, 0777);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceIoMkdir(%s): 0x%08X\n", path, res);
		return res;
	}

	res = fs_list_init(&pEnt, "grw0:/patch", NULL, &nFile);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("fs_list_init(): 0x%08X\n", res);
		return res;
	}

	if (BR_event_callback != NULL)
		BR_event_callback(BR_EVENT_NOTICE_CONT_NUMBER, BR_STATUS_NONE, (SceInt64)nFile, NULL, NULL);

	res = fs_list_copy(pEnt, path, BR_event_callback, NULL);

	fs_list_fini(pEnt);
	pEnt = NULL;

	if (res < 0) {
		SCE_DBG_LOG_ERROR("fs_list_copy(): 0x%08X\n", res);
		return res;
	}

	if (BR_event_callback != NULL)
		BR_event_callback(BR_EVENT_NOTICE_CONT_PROMOTE, BR_STATUS_BEGIN, 0, NULL, NULL);

	scePromoterUtilityInit();

	do {
		res = scePromoterUtilityPromotePkg(path, 0);
		if(res < 0){
			SCE_DBG_LOG_ERROR("scePromoterUtilityPromotePkg(%s):0x%X\n", path, res);
			break;
		}

		int state = 0;
		do {
			res = scePromoterUtilityGetState(&state);
			if (res < 0){
				state = 0;
			}

			sceKernelDelayThread(1000 * 1000);
		} while (state);

		if(res < 0){
			SCE_DBG_LOG_ERROR("scePromoterUtilityGetState():0x%X\n", res);
		}

		int result = 0;
		res = scePromoterUtilityGetResult(&result);
		if(res < 0){
			SCE_DBG_LOG_ERROR("scePromoterUtilityGetResult():0x%X\n", res);
		}

		res = result;
	} while(0);

	scePromoterUtilityExit();

	if (BR_event_callback != NULL)
		BR_event_callback(BR_EVENT_NOTICE_CONT_PROMOTE, BR_STATUS_END, 0, NULL, NULL);

	return res;
}

int do_restore_appmeta_core(const char *restore_temp_path, const char *titleid) {

	int res;
	FSListEntry *pEnt = NULL;
	ScePromoterUtilityImportParams param;

	sce_paf_memset(&param, 0, sizeof(param));

	sce_paf_strncpy(param.titleid, titleid, sizeof(param.titleid) - 1);
	param.titleid[sizeof(param.titleid) - 1] = 0;

	sce_paf_snprintf(param.path, sizeof(param.path) - 1, "%s/appmeta", restore_temp_path);

	res = sceIoMkdir(param.path, 0777);
	if (res < 0) {
		return res;
	}


	res = fs_list_init(&pEnt, "grw0:/appmeta", NULL, NULL);
	if (res < 0) {
		return res;
	}

	res = fs_list_copy(pEnt, param.path, BR_event_callback, NULL);

	fs_list_fini(pEnt);
	pEnt = NULL;

	if (res >= 0) {
		param.type = SCE_PKG_TYPE_VITA;
		param.attribute = 0;

		res = scePromoterUtilityPromoteImport(&param);
		SCE_DBG_LOG_TRACE("scePromoterUtilityPromoteImport(): 0x%08X\n", res);

		res = fs_list_init(&pEnt, param.path, NULL, NULL);
		if (res < 0) {
			return res;
		}

		res = fs_list_remove(pEnt);

		fs_list_fini(pEnt);
		pEnt = NULL;
	}

	return 0;
}

int do_restore_savedata_core(const char *restore_temp_path, const char *titleid) {

	int res, current_account2;
	FSListEntry *pEnt = NULL;
	char path[0x80];
	SceIoStat stat;

	res = sceIoGetstat("grw0:/savedata", &stat);
	if (res < 0) {
		SCE_DBG_LOG_INFO("No has savedata backup. Skip.\n");
		return 0; // Not has savedata
	}

	sce_paf_snprintf(path, sizeof(path), "ux0:/app/%s", titleid);

	res = sceIoGetstat(path, &stat);
	if (res < 0) {
		SCE_DBG_LOG_INFO("No has base app. Skip.\n");
		return 0; // Not has base app
	}

	res = sceRegMgrGetKeyInt("/CONFIG/NP2/", "current_account2", &current_account2);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceRegMgrGetKeyInt(): 0x%08X\n", res);
		return res;
	}

	sce_paf_snprintf(path, sizeof(path), "ux0:/user/%02d/savedata/%s", current_account2, titleid);

	res = sceIoGetstat(path, &stat);
	if (res >= 0) {
		SCE_DBG_LOG_WARNING("Savedata is has already.\n");
		SCE_DBG_LOG_WARNING("Should be remove the old savedata.\n");
		fs_remove(path);
	}

	res = sceIoMkdir(path, 0777);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceIoMkdir(%s): 0x%08X\n", path, res);
		return res;
	}

	res = fs_list_init(&pEnt, "grw0:/savedata", NULL, NULL);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("fs_list_init(): 0x%08X\n", res);
		return res;
	}

	res = fs_list_copy(pEnt, path, BR_event_callback, NULL);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("fs_list_copy(): 0x%08X\n", res);
	}

	fs_list_fini(pEnt);
	pEnt = NULL;

	return 0;
}

int do_backup_core_helper(const char *dst, FSListEntry *pEnt){

	int res;

	if (NULL == pEnt) {
		return 0;
	}

	res = sceIoMkdir(dst, 0666);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceIoMkdir(%s): 0x%08X\n", dst, res);
		return res;
	}

	res = fs_list_copy(pEnt, dst, BR_event_callback, NULL);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("fs_list_copy(%s): 0x%08X\n", dst, res);
		return res;
	}

	return 0;
}

typedef struct BackupContext {
	FSListEntry *pAppEnt;
	FSListEntry *pAppMetaEnt;
	FSListEntry *pLicenseEnt;
	FSListEntry *pSavedataEnt;
	FSListEntry *pPatchEnt;
	int savedata_only;
	int use_compression;
} BackupContext;

/*
 * path - devX:/vbackup
 */
int do_backup_core(const char *path, const char *titleid, const BackupContext *pContext) {

	int res;
	char temp_path[0x100], temp_path2[0x100], backup_path[0x80];

	sce_paf_snprintf(backup_path, sizeof(backup_path) - 1, "%s/%s", path, titleid);

	res = sceIoMkdir(backup_path, 0666);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceIoMkdir(%s): 0x%08X\n", backup_path, res);
		return res;
	}

	SceUInt32 nFile = 0, nDir = 0;
	SceUInt64 size = 0LL;
	SceUInt64 devMaxSize = 0LL;
	SceUInt64 devFreeSize = 0LL;

	if (pContext->pAppEnt)
		fs_list_get_full_size(pContext->pAppEnt, &size, &nFile, &nDir);
	if (pContext->pAppMetaEnt)
		fs_list_get_full_size(pContext->pAppMetaEnt, &size, &nFile, &nDir);
	if (pContext->pLicenseEnt)
		fs_list_get_full_size(pContext->pLicenseEnt, &size, &nFile, &nDir);
	if (pContext->pPatchEnt)
		fs_list_get_full_size(pContext->pPatchEnt, &size, &nFile, &nDir);
	if (pContext->pSavedataEnt)
		fs_list_get_full_size(pContext->pSavedataEnt, &size, &nFile, &nDir);

	char *devEnd = sce_paf_strchr(path, ':');
	int len = devEnd - path + 1;
	char device[0x10];

	sce_paf_memset(device, 0, sizeof(device));
	if (len < sizeof(device)) {
		sce_paf_strncpy(device, path, len);
	}

	res = sceAppMgrGetDevInfo(device, &devMaxSize, &devFreeSize);
	if (res == 0) {
		if (pContext->use_compression) {
			if ((size * 2) > devFreeSize)
				return SCE_ERROR_ERRNO_ENOMEM;
		}
		else {
			if (size > devFreeSize)
				return SCE_ERROR_ERRNO_ENOMEM;
		}
	}
	else {
		SCE_DBG_LOG_ERROR("sceAppMgrGetDevInfo(%s): 0x%08X\n", device, res);
	}

	SCE_DBG_LOG_TRACE("Size: %lld bytes\n", size);

	if (BR_event_callback != NULL)
		BR_event_callback(BR_EVENT_NOTICE_CONT_NUMBER, BR_STATUS_NONE, (SceUInt64)nFile, NULL, NULL);

	sce_paf_snprintf(temp_path, sizeof(temp_path), "%s/appcont.bin", backup_path);
	res = create_backup_image(temp_path, (SceUInt64)(nFile + nDir), size);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("create_backup_image(): 0x%08X\n", res);
		return res;
	}

	res = vshIoMount(0xA00, temp_path, 2, 0, 0, 0);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("vshIoMount(): 0x%08X\n", res);
		return res;
	}

	res = do_backup_core_helper("grw0:app", pContext->pAppEnt);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("do_backup_core_helper(): 0x%08X\n", res);
		return res;
	}

	res = do_backup_core_helper("grw0:appmeta", pContext->pAppMetaEnt);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("do_backup_core_helper(): 0x%08X\n", res);
		return res;
	}

	res = do_backup_core_helper("grw0:license", pContext->pLicenseEnt);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("do_backup_core_helper(): 0x%08X\n", res);
		return res;
	}

	res = do_backup_core_helper("grw0:patch", pContext->pPatchEnt);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("do_backup_core_helper(): 0x%08X\n", res);
		return res;
	}

	res = do_backup_core_helper("grw0:savedata", pContext->pSavedataEnt);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("do_backup_core_helper(): 0x%08X\n", res);
		return res;
	}

	sce_paf_snprintf(temp_path, sizeof(temp_path), "%s/sce_sys", backup_path);
	res = sceIoMkdir(temp_path, 0666);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceIoMkdir(%s): 0x%08X\n", temp_path, res);
		return res;
	}

	sce_paf_snprintf(temp_path, sizeof(temp_path), "%s/sce_sys/param.sfo", backup_path);
	sce_paf_snprintf(temp_path2, sizeof(temp_path2), "ux0:app/%s/sce_sys/param.sfo", titleid);
	res = file_copy(temp_path, temp_path2);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("file_copy(): 0x%08X\n", res);
		return res;
	}

	sce_paf_snprintf(temp_path, sizeof(temp_path), "%s/sce_sys/icon0.png", backup_path);
	sce_paf_snprintf(temp_path2, sizeof(temp_path2), "ur0:appmeta/%s/icon0.png", titleid);
	file_copy(temp_path, temp_path2);

	sce_paf_strncpy(temp_path, titleid, 0x20);
	res = write_file("grw0:titleid.bin", temp_path, 0x20);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("write_file(): 0x%08X\n", res);
		return res;
	}

	if (pContext->savedata_only) {
		sce_paf_snprintf(temp_path, sizeof(temp_path), "%s/savedata_only.flag", backup_path);
		res = write_file(temp_path, temp_path, 1);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("write_file(): 0x%08X\n", res);
			return res;
		}
	}

	res = vshIoUmount(0xA00, 0, 0, 0);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("vshIoUmount(): 0x%08X\n", res);
		vshIoUmount(0xA00, 1, 0, 0); // Forced unmount
		return res;
	}

	return 0;
}

/* MAIN FUNCS */

int do_backup(const char *path, const char *titleid, int savedata_only, int use_compression) {

	int res, current_account2;
	char temp_path[0x80];
	BackupContext context;
	// FSListEntry *pAppEnt = NULL, *pAppMetaEnt = NULL, *pLicenseEnt = NULL, *pSavedataEnt = NULL, *pPatchEnt = NULL;

	sce_paf_memset(&context, 0, sizeof(context));

	context.savedata_only   = savedata_only;
	context.use_compression = use_compression;

	do {
		if (!savedata_only) {
			sce_paf_snprintf(temp_path, sizeof(temp_path), "ux0:/app/%s", titleid);
			res = fs_list_init(&(context.pAppEnt), temp_path, NULL, NULL);
			if (res < 0) { // Application is not installed.
				SCE_DBG_LOG_ERROR("Application %s is not installed\n", titleid);
				break;
			}

			sce_paf_snprintf(temp_path, sizeof(temp_path), "ux0:/appmeta/%s", titleid);
			res = fs_list_init(&(context.pAppMetaEnt), temp_path, NULL, NULL);
			if (res < 0) {
				SCE_DBG_LOG_ERROR("appmeta file list: 0x%X\n", res);
				break;
			}

			sce_paf_snprintf(temp_path, sizeof(temp_path), "ux0:/license/app/%s", titleid);
			res = fs_list_init(&(context.pLicenseEnt), temp_path, NULL, NULL);
			if (res < 0) {
				SCE_DBG_LOG_ERROR("license file list: 0x%X\n", res);
				break;
			}

			sce_paf_snprintf(temp_path, sizeof(temp_path), "ux0:/patch/%s", titleid);
			fs_list_init(&(context.pPatchEnt), temp_path, NULL, NULL);
		}

		res = sceRegMgrGetKeyInt("/CONFIG/NP2/", "current_account2", &current_account2);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("sceRegMgrGetKeyInt(): 0x%08X\n", res);
			break;
		}

		sce_paf_snprintf(temp_path, sizeof(temp_path), "ux0:/user/%02d/savedata/%s", current_account2, titleid);
		fs_list_init(&(context.pSavedataEnt), temp_path, NULL, NULL);

		res = do_backup_core(path, titleid, &context);
	} while (0);

	if (!savedata_only) {
		fs_list_fini(context.pPatchEnt);
		fs_list_fini(context.pLicenseEnt);
		fs_list_fini(context.pAppMetaEnt);
		fs_list_fini(context.pAppEnt);
	}
	fs_list_fini(context.pSavedataEnt);

	if (res != 0)
		return res;

	if (use_compression) {
		SceIoStat stat;
		char compressed_path[0x80];

		sce_paf_snprintf(temp_path, sizeof(temp_path), "%s/%s/appcont.bin", path, titleid);

		sceIoGetstat(temp_path, &stat);

		if (stat.st_size < 4294967296) {
			if (BR_event_callback != NULL)
				BR_event_callback(BR_EVENT_NOTICE_CONT_COMPRESS, BR_STATUS_BEGIN, 0, NULL, NULL);
			sce_paf_snprintf(compressed_path, sizeof(compressed_path), "%s/%s/appcont.bin.gz", path, titleid);
			res = sce_paf_gzip_compress(temp_path, compressed_path);
			sceIoRemove(temp_path);
			if (BR_event_callback != NULL)
				BR_event_callback(BR_EVENT_NOTICE_CONT_COMPRESS, BR_STATUS_END, 0, NULL, NULL);
		}
	}

	return res;
}

int do_restore(const char *path) {

	int res, savedata_only, is_compressed;
	char titleid[0x20], temp_path[0x80];
	SceIoStat stat;
	is_compressed = 0;

	sce_paf_snprintf(temp_path, sizeof(temp_path) - 1, "%s/savedata_only.flag", path);
	res = sceIoGetstat(temp_path, &stat);
	if (res < 0)
		savedata_only = 0;
	else
		savedata_only = 1;

	sce_paf_snprintf(temp_path, sizeof(temp_path) - 1, "%s/appcont.bin", path);

	res = sceIoGetstat(temp_path, &stat);
	if (res < 0) {
		char compressed_path[0x80];

		sce_paf_snprintf(compressed_path, sizeof(compressed_path), "%s/appcont.bin.gz", path);

		if (BR_event_callback != NULL)
			BR_event_callback(BR_EVENT_NOTICE_CONT_DECOMPRESS, BR_STATUS_BEGIN, 0, NULL, NULL);
		res = sce_paf_gzip_decompress(compressed_path, temp_path);
		if (res != 0) {
			sceIoRemove(temp_path);
			return res;
		}

		is_compressed = 1;

		if (BR_event_callback != NULL)
			BR_event_callback(BR_EVENT_NOTICE_CONT_DECOMPRESS, BR_STATUS_END, 0, NULL, NULL);

		res = sceIoGetstat(temp_path, &stat);
		if (res < 0) {
			return res;
		}
	}

	SceUInt64 devMaxSize = 0LL;
	SceUInt64 devFreeSize = 0LL;

	res = sceAppMgrGetDevInfo("ux0:", &devMaxSize, &devFreeSize);
	if (res == 0) {
		if ((stat.st_size + SCE_KERNEL_1MiB) > devFreeSize) {
			if (is_compressed)
				sceIoRemove(temp_path);
			return SCE_ERROR_ERRNO_ENOMEM;
		}
	}

	res = vshIoMount(0xA00, temp_path, 1, 0, 0, 0);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("vshIoMount(): 0x%08X\n", res);
		vshIoUmount(0xA00, 1, 0, 0);
		if (is_compressed)
			sceIoRemove(temp_path);
		return res;
	}

	do {
		res = read_file("grw0:titleid.bin", titleid, sizeof(titleid));
		if (res < 0) {
			SCE_DBG_LOG_ERROR("read_file(): 0x%08X\n", res);
			break;
		}

		if (res != sizeof(titleid) || 9 != sce_paf_strlen(titleid)) {
			SCE_DBG_LOG_ERROR("Invalid titleid file\n");
			res = -1;
			break;
		}

		res = sceIoMkdir(restore_temp_path, 0777);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("sceIoMkdir(): 0x%08X\n", res);
			break;
		}

		SCE_DBG_LOG_TRACE("Restore titleid: %s, savedata only: %d\n", titleid, savedata_only);

		if (!savedata_only) {
			res = do_restore_core(restore_temp_path);
			if (res < 0) {
				SCE_DBG_LOG_ERROR("do_restore_core(): 0x%08X\n", res);
				break;
			}

			res = do_restore_patch_core(restore_temp_path);
			if (res < 0) {
				SCE_DBG_LOG_ERROR("do_restore_patch_core(): 0x%08X\n", res);
				break;
			}

			/*
			res = do_restore_appmeta_core(restore_temp_path, titleid);
			if (res < 0) {
				SCE_DBG_LOG_ERROR("do_restore_appmeta_core(): 0x%08X\n", res);
				break;
			}
			*/
		}

		res = do_restore_savedata_core(restore_temp_path, titleid);
		if (res < 0) {
			SCE_DBG_LOG_ERROR("do_restore_savedata_core(): 0x%08X\n", res);
			break;
		}

		res = 0;
	} while(0);

	vshIoUmount(0xA00, 0, 0, 0);

	fs_remove(restore_temp_path);

	if (is_compressed)
		sceIoRemove(temp_path);

	return res;
}

int do_delete_backup(const char *path, const char *titleid)
{
	char temp_path[0x100];

	sce_paf_snprintf(temp_path, sizeof(temp_path) - 1, "%s/%s", path, titleid);

	return fs_remove(temp_path);
}

void set_core_callback(BREventCallback cb)
{
	BR_event_callback = cb;
}

void set_interrupt()
{
	sce_paf_gzip_interrupt();

	int16_t interrupted = 1;

	sceAtomicStore16AcqRel(&s_executionInterrupt, 1);

	/*while (1) {
		interrupted = sceAtomicLoad16AcqRel(&s_executionInterrupt);
		sceKernelDelayThread(1000);
		if (!interrupted)
			break;
	}*/

}