#ifndef _VB_MENU_MAIN_H_
#define _VB_MENU_MAIN_H_

#define BACKUP_LIST_PATH "ur0:vbackup_list.txt"

namespace menu {
	namespace main {

		enum
		{
			Pagemode_Restore,
			Pagemode_Backup,
			Pagemode_Main
		};

		enum
		{
			ButtonHash_Restore = 0x9d868d62,
			ButtonHash_Backup = 0xf6d6de63,
			ButtonHash_BackupList = 0xf21cc2bc
		};

		class ButtonCB : public ui::EventCallback
		{
		public:

			ButtonCB()
			{
				eventHandler = ButtonCBFun;
			};

			virtual ~ButtonCB()
			{

			};

			static SceVoid ButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);
		};

		class Page
		{
		public:

			Page();

			~Page();

			static SceVoid Create();

			static SceVoid Destroy();
		};

	}
}

#endif