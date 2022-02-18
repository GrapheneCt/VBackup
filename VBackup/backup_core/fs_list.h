
#ifndef _FS_LIST_H_
#define _FS_LIST_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel.h>

typedef struct FSListEntry {
	struct FSListEntry *parent;
	struct FSListEntry *child;
	struct FSListEntry *next;
	char *path_full;
	SceSize nChild;
	SceUID fd;
	SceIoDirent ent;
} FSListEntry;

int fs_list_init(FSListEntry **ppEnt, const char *path, SceUInt32 *pnDir, SceUInt32 *pnFile);
int fs_list_fini(FSListEntry *pEnt);

int fs_list_get_full_size(const FSListEntry *pEnt, SceUInt64 *pSize, SceUInt32 *pnFile);
int fs_list_remove(FSListEntry *pEnt);

int fs_list_execute(const FSListEntry *pEnt, int(*callback)(const FSListEntry *ent, void *argp), void *argp);

#ifdef __cplusplus
}
#endif

#endif /* _FS_LIST_H_ */
