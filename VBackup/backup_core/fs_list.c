
#include <kernel.h>
#include <paf/stdc.h>
#include "fs_list.h"
#include "file_copy.h"

static int s_executionInterrupt = 0;

FSListEntry *fs_list_make_entry(FSListEntry *pCurrent, SceIoDirent *pDirent){

	FSListEntry *pEnt;

	pEnt = sce_paf_malloc(sizeof(*pEnt));
	if(pEnt == NULL){
		return NULL;
	}

	pEnt->parent = NULL;
	pEnt->child  = NULL;
	pEnt->next   = NULL;

	if(pDirent != NULL){

		SceSize path_len = sce_paf_strlen(pCurrent->path_full) + sce_paf_strlen("/") + sce_paf_strlen(pDirent->d_name);
		char *path = sce_paf_malloc(path_len + 1);

		sce_paf_snprintf(path, path_len + 1, "%s/%s", pCurrent->path_full, pDirent->d_name);
		path[path_len] = 0;

		// sceClibPrintf("%s\n", path);

		pEnt->path_full = path;
	}else{

		SceSize path_len = sce_paf_strlen(pCurrent->path_full);
		char *path = sce_paf_malloc(path_len + 1);
		path[path_len] = 0;

		sce_paf_strncpy(path, pCurrent->path_full, path_len);

		pEnt->path_full = path;
	}

	pEnt->nChild = 0;
	pEnt->fd = -1;

	if(pDirent != NULL){
		sce_paf_memcpy(&(pEnt->ent), pDirent, sizeof(pEnt->ent));
	}else{
		sce_paf_memset(&(pEnt->ent), 0, sizeof(pEnt->ent));
	}

	return pEnt;
}

int fs_list_free_entry(FSListEntry *pEnt){

	sce_paf_free(pEnt->path_full);
	sce_paf_free(pEnt);

	return 0;
}

int fs_list_init(FSListEntry **ppEnt, const char *path, SceUInt32 *pnDir, SceUInt32 *pnFile){

	int res;
	SceUID fd;
	SceUInt32 nDir = 0, nFile = 0;
	FSListEntry *pEnt, *current, *child;
	SceIoDirent dirent;

	pEnt = sce_paf_malloc(sizeof(*pEnt));
	if(pEnt == NULL){
		return -1;
	}

	sce_paf_memset(pEnt, 0, sizeof(*pEnt));
	pEnt->fd = -1;

	SceSize path_len = sce_paf_strlen(path);

	char *path_full = sce_paf_malloc(path_len + 1);

	path_full[path_len] = 0;
	sce_paf_strncpy(path_full, path, path_len);

	pEnt->path_full = path_full;

	res = sceIoGetstat(path, &(pEnt->ent.d_stat));
	if(res < 0){
		fs_list_free_entry(pEnt);
		return res;
	}

	current = pEnt;

	fd = sceIoDopen(current->path_full);
	if(fd < 0){
		fs_list_free_entry(pEnt);
		return fd;
	}

	current->fd = fd;

	while(current != NULL){

		res = sceIoDread(current->fd, &dirent);
		if(res > 0){
			child = fs_list_make_entry(current, &dirent);
			if(child != NULL){
				child->parent = current;
				child->next   = current->child;
				current->child = child;
				current->nChild += 1;

				if(SCE_STM_ISDIR(child->ent.d_stat.st_mode)){

					fd = sceIoDopen(child->path_full);
					if(fd >= 0){
						child->fd = fd;
						current = child;
					}
					nDir++;
				}
				else {
					nFile++;
				}
			}
		}else{ // No has more entries
			sceIoDclose(current->fd);
			current->fd = 0xDEADBEEF;
			current = current->parent;
		}
	}

	*ppEnt = pEnt;

	if (pnDir != NULL) {
		*pnDir += nDir;
	}

	if (pnFile != NULL) {
		*pnFile += nFile;
	}

	return 0;
}

int fs_list_fini(FSListEntry *pEnt){

	FSListEntry *curr = pEnt, *tmp;

	while(curr != NULL){
		if(curr->nChild != 0){
			curr = curr->child;
		}else if(curr->next != NULL){
			tmp = curr->next;
			fs_list_free_entry(curr);
			curr = tmp;
		}else{
			while(curr != NULL && curr->next == NULL){
				tmp = curr->parent;
				fs_list_free_entry(curr);
				curr = tmp;
			}
			if(curr != NULL){
				tmp = curr->next;
				fs_list_free_entry(curr);
				curr = tmp;
			}
		}
	}

	return 0;
}

int fs_list_execute(const FSListEntry *pEnt, int (* callback)(const FSListEntry *ent, void *argp), void *argp){

	int res;
	const FSListEntry *curr = pEnt;

	if(curr == NULL){
		return -1;
	}

	// curr = curr->child;

	while(curr != NULL && !s_executionInterrupt){

		res = callback(curr, argp);
		if(res < 0){
			return res;
		}

		if(curr->nChild != 0){
			curr = curr->child;
		}else if(curr->next != NULL){
			curr = curr->next;
		}else{
			while(curr != NULL && curr->next == NULL){
				curr = curr->parent;
			}
			if(curr != NULL){
				curr = curr->next;
			}
		}
	}

	return 0;
}

int fs_list_execute2(FSListEntry *pEnt, int (* callback)(FSListEntry *ent, void *argp), void *argp){

	int res;
	FSListEntry *curr = pEnt, *tmp;

	while(curr != NULL){
		if(curr->nChild != 0){
			curr = curr->child;
		}else if(curr->next != NULL){
			tmp = curr->next;

			res = callback(curr, argp);
			if(res < 0){
				return res;
			}

			curr = tmp;
		}else{
			while(curr != NULL && curr->next == NULL){
				tmp = curr->parent;

				res = callback(curr, argp);
				if(res < 0){
					return res;
				}

				curr = tmp;
			}
			if(curr != NULL){
				tmp = curr->next;

				res = callback(curr, argp);
				if(res < 0){
					return res;
				}

				curr = tmp;
			}
		}
	}

	return 0;
}


int fs_list_print_callback(const FSListEntry *ent, void *argp){

	sceClibPrintf("%s\n", ent->path_full);

	return 0;
}

int fs_list_print(const FSListEntry *pEnt){
	return fs_list_execute(pEnt, fs_list_print_callback, NULL);
}

int fs_list_get_full_size_callback(const FSListEntry *ent, void *argp){

	SceUInt64 align = *(SceUInt64 *)(argp + 8) - 1;

	*(SceUInt64 *)(argp) += ((ent->ent.d_stat.st_size + align) & ~align);

	if (!SCE_STM_ISDIR(ent->ent.d_stat.st_mode)) {
		*(SceUInt64 *)(argp + 0x10) += 1;
	}

	return 0;
}

int fs_list_get_full_size(const FSListEntry *pEnt, SceUInt64 *pSize, SceUInt32 *pnFile) {

	int res;
	SceUInt64 size[3] = { 0LL, 0x1000LL, 0LL };

	res = fs_list_execute(pEnt, fs_list_get_full_size_callback, size);
	if (res >= 0) {
		*pSize += size[0];
		*pnFile += (SceUInt32)size[2];
	}

	return res;
}

int fs_list_remove_callback(FSListEntry *ent, void *argp){

	// sceClibPrintf("Remove: %s\n", ent->path_full);

	if(SCE_STM_ISDIR(ent->ent.d_stat.st_mode)){
		return sceIoRmdir(ent->path_full);
	}

	return sceIoRemove(ent->path_full);
}

int fs_list_remove(FSListEntry *pEnt){
	return fs_list_execute2(pEnt, fs_list_remove_callback, NULL);
}
