
#ifndef _PAF_GZIP_H_
#define _PAF_GZIP_H_

#ifdef __cplusplus
extern "C" {
#endif

int sce_paf_gzip_init(void);

int sce_paf_gzip_compress(const char *src, const char *dst);

int sce_paf_gzip_decompress(const char *src, const char *dst);

void sce_paf_gzip_interrupt();

#ifdef __cplusplus
}
#endif

#endif /* _PAF_GZIP_H_ */