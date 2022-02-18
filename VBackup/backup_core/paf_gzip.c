
#include <kernel.h>
#include <vshbridge.h>
#include <paf/stdc.h>
#include "paf_gzip.h"

#define deflateInit2_ deflateInit2__dummy
#define inflateInit2_ inflateInit2__dummy
#define deflate deflate_dummy
#define deflateEnd deflateEnd_dummy
#define inflate inflate_dummy
#define inflateEnd inflateEnd_dummy
#include <zlib.h>
#undef deflateInit2_
#undef inflateInit2_
#undef deflate
#undef deflateEnd
#undef inflate
#undef inflateEnd

#define CHUNK_SIZE SCE_KERNEL_16KiB
#define LEVEL Z_DEFAULT_COMPRESSION

int sceKernelGetModuleInfoByAddr(SceUIntPtr addr, SceKernelModuleInfo *info);
extern int _ZN3paf6thread17s_mainThreadMutexE;

int(*deflateInit2_)(z_streamp strm, int  level, int  method,
	int windowBits, int memLevel,
	int strategy, const char *version,
	int stream_size);

int(*inflateInit2_)(z_streamp strm, int  windowBits,
	const char *version, int stream_size);

int(*deflate)(z_streamp strm, int flush);

int(*deflateEnd)(z_streamp strm);

int(*inflate)(z_streamp strm, int flush);

int(*inflateEnd)(z_streamp strm);

static voidpf sce_paf_gzip_zalloc(voidpf opaque, uInt items, uInt size)
{
	return sce_paf_calloc(items, size);
}

static void sce_paf_gzip_zfree(voidpf opaque, voidpf address)
{
	sce_paf_free(address);
}

int sce_paf_gzip_init(void) {

	int res;
	SceKernelModuleInfo module_info;

	res = sceKernelGetModuleInfoByAddr(&_ZN3paf6thread17s_mainThreadMutexE, &module_info);
	if (res < 0)
		return res;

	res = vshSblAimgrIsTool();
	if (res < 0)
		return res;

	if (res) {
		deflateInit2_ = (void *)(module_info.segments[0].vaddr + 0x17DC11);
		deflate = (void *)(module_info.segments[0].vaddr + 0x17E27F);
		deflateEnd = (void *)(module_info.segments[0].vaddr + 0x17DB8D);

		inflateInit2_ = (void *)(module_info.segments[0].vaddr + 0x17F48F);
		inflate = (void *)(module_info.segments[0].vaddr + 0x17F625);
		inflateEnd = (void *)(module_info.segments[0].vaddr + 0x180A2B);
	}
	else {
		deflateInit2_ = (void *)(module_info.segments[0].vaddr + 0x17D05D);
		deflate = (void *)(module_info.segments[0].vaddr + 0x17D6CB);
		deflateEnd = (void *)(module_info.segments[0].vaddr + 0x17CFD9);

		inflateInit2_ = (void *)(module_info.segments[0].vaddr + 0x17E8DB);
		inflate = (void *)(module_info.segments[0].vaddr + 0x17EA71);
		inflateEnd = (void *)(module_info.segments[0].vaddr + 0x17FE77);
	}

	return 0;
}

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
int sce_paf_gzip_compress(const char *src, const char *dst)
{
	int ret, flush;
	unsigned have;
	z_stream strm;
	unsigned char *in = NULL;
	unsigned char *out = NULL;
	FILE *source = NULL;
	FILE *dest = NULL;

	/* allocate deflate state */
	strm.zalloc = sce_paf_gzip_zalloc;
	strm.zfree = sce_paf_gzip_zfree;
	strm.opaque = Z_NULL;
	ret = deflateInit2_(&strm, LEVEL, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY, "1.2.5", sizeof(z_stream));
	if (ret != Z_OK)
		return ret;
	
	source = sce_paf_fopen(src, "rb");
	if (!source) {
		ret = Z_ERRNO;
		goto error_ret;
	}
	
	dest = sce_paf_fopen(dst, "wb");
	if (!dest) {
		ret = Z_ERRNO;
		goto error_ret;
	}

	in = sce_paf_malloc(CHUNK_SIZE);
	if (!in) {
		ret = Z_ERRNO;
		goto error_ret;
	}

	out = sce_paf_malloc(CHUNK_SIZE);
	if (!out) {
		ret = Z_ERRNO;
		goto error_ret;
	}

	/* compress until end of file */
	do {
		strm.avail_in = sce_paf_fread(in, 1, CHUNK_SIZE, source);
		if (sce_paf_ferror(source)) {
			ret = Z_ERRNO;
			goto error_ret;
		}
		flush = sce_paf_feof(source) ? Z_FINISH : Z_NO_FLUSH;
		strm.next_in = in;

		/* run deflate() on input until output buffer not full, finish
		   compression if all of source has been read in */
		do {
			strm.avail_out = CHUNK_SIZE;
			strm.next_out = out;
			ret = deflate(&strm, flush);    /* no bad return value */
			if (ret == Z_STREAM_ERROR) {
				goto error_ret;
			}
			have = CHUNK_SIZE - strm.avail_out;
			if (sce_paf_fwrite(out, 1, have, dest) != have || sce_paf_ferror(dest)) {
				ret = Z_ERRNO;
				goto error_ret;
			}
		} while (strm.avail_out == 0);
		if (strm.avail_in != 0) {
			ret = Z_ERRNO;
			goto error_ret;
		}

		/* done when last data in file processed */
	} while (flush != Z_FINISH);

	if (ret != Z_STREAM_END) {
		ret = Z_ERRNO;
		goto error_ret;
	}

	/* clean up and return */
	(void)deflateEnd(&strm);

	sce_paf_fclose(source);
	sce_paf_fclose(dest);

	sce_paf_free(in);
	sce_paf_free(out);

	return Z_OK;

error_ret:

	(void)deflateEnd(&strm);

	if (!source)
		sce_paf_fclose(source);

	if (!dest)
		sce_paf_fclose(dest);

	if (in)
		sce_paf_free(in);

	if (out)
		sce_paf_free(out);

	return ret;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
int sce_paf_gzip_decompress(const char *src, const char *dst)
{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char *in = NULL;
	unsigned char *out = NULL;
	FILE *source = NULL;
	FILE *dest = NULL;

	/* allocate inflate state */
	strm.zalloc = sce_paf_gzip_zalloc;
	strm.zfree = sce_paf_gzip_zfree;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit2_(&strm, 15 | 16, "1.2.5", sizeof(z_stream));
	if (ret != Z_OK)
		return ret;

	source = sce_paf_fopen(src, "rb");
	if (!source) {
		ret = Z_ERRNO;
		goto error_ret;
	}

	dest = sce_paf_fopen(dst, "wb");
	if (!dest) {
		ret = Z_ERRNO;
		goto error_ret;
	}

	in = sce_paf_malloc(CHUNK_SIZE);
	if (!in) {
		ret = Z_ERRNO;
		goto error_ret;
	}

	out = sce_paf_malloc(CHUNK_SIZE);
	if (!out) {
		ret = Z_ERRNO;
		goto error_ret;
	}

	/* decompress until deflate stream ends or end of file */
	do {
		strm.avail_in = sce_paf_fread(in, 1, CHUNK_SIZE, source);
		if (sce_paf_ferror(source)) {
			ret = Z_ERRNO;
			goto error_ret;
		}
		if (strm.avail_in == 0)
			break;
		strm.next_in = in;

		/* run inflate() on input until output buffer not full */
		do {
			strm.avail_out = CHUNK_SIZE;
			strm.next_out = out;
			ret = inflate(&strm, Z_NO_FLUSH);
			if (ret == Z_STREAM_ERROR) {
				goto error_ret;
			}
			switch (ret) {
			case Z_NEED_DICT:
				ret = Z_DATA_ERROR;     /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				goto error_ret;
			}
			have = CHUNK_SIZE - strm.avail_out;
			if (sce_paf_fwrite(out, 1, have, dest) != have || sce_paf_ferror(dest)) {
				ret = Z_ERRNO;
				goto error_ret;
			}
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);

	sce_paf_fclose(source);
	sce_paf_fclose(dest);

	sce_paf_free(in);
	sce_paf_free(out);

	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;

error_ret:

	(void)inflateEnd(&strm);

	if (!source)
		sce_paf_fclose(source);

	if (!dest)
		sce_paf_fclose(dest);

	if (in)
		sce_paf_free(in);

	if (out)
		sce_paf_free(out);

	return ret;
}