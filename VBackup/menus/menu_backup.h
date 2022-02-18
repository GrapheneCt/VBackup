#ifndef _VB_MENU_BACKUP_H_
#define _VB_MENU_BACKUP_H_

#include <paf.h>

using namespace paf;

namespace menu {
	namespace backup {

		class BackButtonCB : public ui::Widget::EventCallback
		{
		public:

			BackButtonCB()
			{
				eventHandler = BackButtonCBFun;
			};

			virtual ~BackButtonCB()
			{

			};

			static SceVoid BackButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);
		};

		class BackupThread : public thread::Thread
		{
		public:

			using thread::Thread::Thread;

			static SceVoid UIResetTask(ScePVoid pUserData);

			static SceInt32 BREventCallback(SceInt32 event, SceInt32 status, SceInt64 value, ScePVoid data, ScePVoid argp);

			static SceInt32 vshIoMount(SceInt32 id, const char *path, SceInt32 permission, SceInt32 a4, SceInt32 a5, SceInt32 a6);

			static SceVoid BackupOverwriteDialogCB(Dialog::ButtonCode button, ScePVoid pUserData);

			SceVoid EntryFunction();

			SceBool savedataOnly;
		};

		class Page
		{
		public:

			Page(SceBool savedataOnly);

			~Page();

			static SceVoid Create(SceBool savedataOnly);

			static SceVoid Destroy();

			static SceVoid BeginDialogCB(Dialog::ButtonCode button, ScePVoid pUserData);

			BackupThread *backupThread;
		};

	}
}

#endif