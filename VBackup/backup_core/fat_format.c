#include <kernel.h>
#include <vshbridge.h>
#include "fat_format.h"

#define FATFS_FORMAT_WORKING_BUFFER_SIZE SCE_KERNEL_256KiB

#define SCE_FAT_FORMAT_TYPE_FAT12 (1)
#define SCE_FAT_FORMAT_TYPE_FAT16 (2)
#define SCE_FAT_FORMAT_TYPE_FAT32 (3)
#define SCE_FAT_FORMAT_TYPE_EXFAT (4)

typedef struct SceFatFormatParam { // size is 0x30-bytes
	SceUInt64 data_0x00;
	const char *path;
	void *pWorkingBuffer;
	SceSize workingBufferSize;
	SceSize bytePerCluster;
	SceSize bytePerSector;
	SceUInt32 data_0x1C;       // Unknown. Cleared by internal.
	SceUInt32 fat_time;
	SceUInt32 data_0x24;       // Unknown. Must be zero.
	SceUInt32 processing_state;
	SceUInt32 sce_fs_type;
} SceFatFormatParam;

int (* sceBackupRestoreExecFsFatFormat)(SceFatFormatParam *pParam);

/*
 * Execute fat format
 *
 * param[in] s                 - The format target image path/block name
 * param[in] pWorkingBuffer    - The working area buffer. should be 0x1000 aligned.
 * param[in] workingBufferSize - The working area buffer size.
 * param[in] bytePerCluster    - The bytes per cluster. should be (bytePerCluster / 0x200) == 0.
 * param[in] sce_fs_type       - The SceFatFormatType.
 *
 * return zero or error code.
 */
int sceFatfsExecFormatInternal(const char *s, void *pWorkingBuffer, SceSize workingBufferSize, SceSize bytePerCluster, SceUInt32 sce_fs_type){

	int res;
	SceFatFormatParam ff_param;

	sce_paf_memset(pWorkingBuffer, 0, workingBufferSize);
	sce_paf_memset(&ff_param, 0, sizeof(ff_param));
	ff_param.data_0x00         = 0LL;
	ff_param.path              = s;
	ff_param.pWorkingBuffer    = pWorkingBuffer;
	ff_param.workingBufferSize = workingBufferSize;
	ff_param.bytePerCluster    = bytePerCluster;
	ff_param.bytePerSector     = 0;
	ff_param.data_0x1C         = 0;
	ff_param.fat_time          = 0x14789632;
	ff_param.data_0x24         = 0;
	ff_param.processing_state  = 0;
	ff_param.sce_fs_type       = sce_fs_type;

	if(bytePerCluster == 0){
		if(sce_fs_type == SCE_FAT_FORMAT_TYPE_EXFAT){
			ff_param.bytePerCluster = 0x20000;
		}else if(sce_fs_type == SCE_FAT_FORMAT_TYPE_FAT32){
			ff_param.bytePerCluster = 0x8000;
		}else if(sce_fs_type == SCE_FAT_FORMAT_TYPE_FAT16 || sce_fs_type == SCE_FAT_FORMAT_TYPE_FAT12){
			ff_param.bytePerCluster = 0x1000;
		}
	}

	res = sceBackupRestoreExecFsFatFormat(&ff_param);
	if(res >= 0)
		res = sceIoSync(s, 0);

	return res;
}

int sceFatfsExecFormat(const char *s, SceSize bytePerCluster, SceUInt32 sce_fs_type){

	int res;
	SceUID memid;
	void *pWorkingBase;

	res = sceKernelAllocMemBlock("SceFatfsFormatWorkingMemBlock", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW, FATFS_FORMAT_WORKING_BUFFER_SIZE, NULL);
	if(res < 0){
		return res;
	}

	memid = res;

	res = sceKernelGetMemBlockBase(memid, &pWorkingBase);
	if(res >= 0)
		res = sceFatfsExecFormatInternal(s, pWorkingBase, FATFS_FORMAT_WORKING_BUFFER_SIZE, bytePerCluster, sce_fs_type);

	sceKernelFreeMemBlock(memid);

	return res;
}

int create_backup_image(const char *path, SceUInt64 size){

	int res;
	SceUID fd;
	SceUInt32 ftype;
	SceIoStat stat;
	stat.st_size = size + 0x80000;

	fd = sceIoOpen(path, SCE_O_RDONLY | SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
	if(fd < 0){
		return fd;
	}

	res = sceIoChstatByFd(fd, &stat, SCE_CST_SIZE);
	sceIoClose(fd);

	if(stat.st_size >= 0x20000000LL){ // 512MiB
		ftype = SCE_FAT_FORMAT_TYPE_EXFAT;
	}else if(stat.st_size >= 0x1000000LL){ // 16MiB
		ftype = SCE_FAT_FORMAT_TYPE_FAT16;
	}else{
		ftype = SCE_FAT_FORMAT_TYPE_FAT12;
	}

	if(res >= 0){
		res = sceFatfsExecFormat(path, 0, ftype);
	}

	return res;
}



int fat_format_init(void){

	int res;
	SceUID modid = SCE_UID_INVALID_UID;
	SceUInt32 offset = 0xDEADBEEF;
	SceKernelModuleInfo module_info;

	modid = sceKernelLoadStartModule("vs0:/vsh/common/backup_restore.suprx", 0, NULL, 0, NULL, NULL);
	if(modid < 0)
		return modid;

	sce_paf_memset(&module_info, 0, sizeof(module_info));
	module_info.size = sizeof(module_info);

	res = sceKernelGetModuleInfo(modid, &module_info);
	if(res < 0)
		return res;

	if (vshSblAimgrIsCEX())
		offset = 0x19279;
	else
		offset = 0x1A60D;

	sceBackupRestoreExecFsFatFormat = (void *)(module_info.segments[0].vaddr + offset);

	return 0;
}
