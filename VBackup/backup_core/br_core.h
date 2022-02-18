
#ifndef _BR_CORE_H_
#define _BR_CORE_H_

#include <kernel.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BR_STATUS_NONE  (0)
#define BR_STATUS_BEGIN (1)
#define BR_STATUS_END   (2)
#define BR_STATUS_ERROR (3)
#define BR_EVENT_NOTICE_CONT_MOUNT      (0)
#define BR_EVENT_NOTICE_CONT_NUMBER     (1)
#define BR_EVENT_NOTICE_CONT_COPY       (2)
#define BR_EVENT_NOTICE_CONT_PROMOTE    (3)
#define BR_EVENT_NOTICE_CONT_COMPRESS   (4)
#define BR_EVENT_NOTICE_CONT_DECOMPRESS (5)

typedef SceInt32(*BREventCallback)(SceInt32 event, SceInt32 status, SceInt64 value, ScePVoid data, ScePVoid argp);

void set_core_callback(BREventCallback cb);

void set_interrupt();

int do_restore(const char *path);

int do_backup(const char *path, const char *titleid, int savedata_only, int use_compression);

int do_delete_backup(const char *path, const char *titleid);

#ifdef __cplusplus
}
#endif

#endif /* _BR_CORE_H_ */
