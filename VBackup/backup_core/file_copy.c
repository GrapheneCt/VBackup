#include <libdbg.h>
#include <kernel.h>
#include <paf/stdc.h>

#define FILE_COPY_WORKING_BUFFER_SIZE SCE_KERNEL_256KiB

int file_copy_from_fd(const char *dst, SceUID src_fd, SceUInt64 size, void *workingBuffer, SceSize bufferSize){

	int res;
	SceUID fd;

	fd = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
	if(fd < 0){
		SCE_DBG_LOG_ERROR("sceIoOpen(): 0x%08X\n", fd);
		return fd;
	}

	SceIoStat stat;
	sce_paf_memset(&stat, 0, sizeof(stat));
	stat.st_size = size;

	res = sceIoChstatByFd(fd, &stat, SCE_CST_SIZE);
	if(res < 0){
		SCE_DBG_LOG_ERROR("sceIoChstatByFd(): 0x%08X\n", res);
		sceIoClose(fd);
		return res;
	}

	sceIoLseek(src_fd, 0LL, SCE_SEEK_SET);
	sceIoLseek(fd, 0LL, SCE_SEEK_SET);

	while(size >= bufferSize){

		res = sceIoRead(src_fd, workingBuffer, bufferSize);
		if(res < 0){
			goto error;
		}

		if(res != bufferSize){
			res = -1;
			goto error;
		}

		res = sceIoWrite(fd, workingBuffer, bufferSize);
		if(res < 0){
			goto error;
		}

		if(res != bufferSize){
			res = -1;
			goto error;
		}

		size -= bufferSize;
	}

	if(size > 0){
		res = sceIoRead(src_fd, workingBuffer, size);
		if(res < 0){
			goto error;
		}

		if(res != size){
			res = -1;
			goto error;
		}

		res = sceIoWrite(fd, workingBuffer, size);
		if(res < 0){
			goto error;
		}

		if(res != size){
			res = -1;
			goto error;
		}
	}

	res = 0;

error:
	if(fd >= 0)
		sceIoClose(fd);

	return 0;
}

int file_copy(const char *dst, const char *src){

	int res;
	SceUID fd;
	void *workingBuffer;

	workingBuffer = sce_paf_memalign(0x40, FILE_COPY_WORKING_BUFFER_SIZE);

	fd = sceIoOpen(src, SCE_O_RDONLY, 0);
	if(fd < 0){
		res = fd;
	}else{
		SceOff size = sceIoLseek(fd, 0LL, SCE_SEEK_END);
		if(size >= 0){
			res = file_copy_from_fd(dst, fd, size, workingBuffer, FILE_COPY_WORKING_BUFFER_SIZE);
		}else{
			res = (int)size;
		}
		sceIoClose(fd);
	}

	sce_paf_free(workingBuffer);
	workingBuffer = NULL;

	return res;
}
