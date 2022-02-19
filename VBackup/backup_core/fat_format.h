
#ifndef _FAT_FORMAT_H_
#define _FAT_FORMAT_H_

#ifdef __cplusplus
extern "C" {
#endif

int fat_format_init(void);

int create_backup_image(const char *path, SceUInt64 nEntry, SceUInt64 size);

#ifdef __cplusplus
}
#endif

#endif /* _FAT_FORMAT_H_ */
