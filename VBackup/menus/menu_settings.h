#ifndef _VB_MENU_SETTINGS_H_
#define _VB_MENU_SETTINGS_H_

#include <paf.h>
#include <app_settings.h>

using namespace paf;

namespace menu {
	namespace settings {

		class Settings
		{
		public:

			enum Hash
			{
				Hash_Sort = 0xbc47f234,
				Hash_BackupDevice = 0xafabfbd3,
				Hash_AppClose = 0x1beb82a5,
				Hash_BackupOverwrite = 0xc9e33378,
				Hash_Compression = 0xe129b5d8
			};

			Settings();

			~Settings();

			static Settings *GetInstance();

			static sce::AppSettings *GetAppSetInstance();

			SceVoid Open(SceUInt32 mode);

			SceInt32 sort;
			SceInt32 backup_device;
			SceInt32 backup_overwrite;
			SceInt32 compression;
			SceInt32 appclose;

			const SceUInt32 k_safeMemIniLimit = 0x400;
			const SceInt32 k_settingsVersion = 3;

		private:

			static SceVoid CBListChange(const char *elementId);

			static SceVoid CBListForwardChange(const char *elementId);

			static SceVoid CBListBackChange(const char *elementId);

			static SceInt32 CBIsVisible(const char *elementId, SceBool *pIsVisible);

			static SceInt32 CBElemInit(const char *elementId);

			static SceInt32 CBElemAdd(const char *elementId, paf::ui::Widget *widget);

			static SceInt32 CBValueChange(const char *elementId, const char *newValue);

			static SceInt32 CBValueChange2(const char *elementId, const char *newValue);

			static SceVoid CBTerm();

			static wchar_t *CBGetString(const char *elementId);

			static SceInt32 CBGetTex(graph::Surface **surf, const char *elementId);

			sce::AppSettings *appSet;
			SceBool settingsReset;

			const SceInt32 k_defSort = 0;
			const SceInt32 k_defBackupDevice = 0;
			const SceInt32 k_defBackupOverwrite = 1;
			const SceInt32 k_defAppClose = 1;
			const SceInt32 k_defCompression = 0;
		};

		class SettingsButtonCB : public ui::EventCallback
		{
		public:

			SettingsButtonCB()
			{
				eventHandler = SettingsButtonCBFun;
			};

			virtual ~SettingsButtonCB()
			{

			};

			static SceVoid SettingsButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);
		};
	}
}

#endif
