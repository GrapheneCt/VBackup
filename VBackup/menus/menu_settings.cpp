#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <paf.h>
#include <audioout.h>
#include <shellaudio.h>
#include <libsysmodule.h>
#include <libdbg.h>
#include <bxce.h>
#include <app_settings.h>
#include <ini_file_processor.h>

#include "../common.h"
#include "../utils.h"
#include "menu_settings.h"

#define WIDE2(x) L##x
#define WIDE(x) WIDE2(x)

using namespace paf;
using namespace sce;

static SceBool s_needBackupCwdReload = SCE_FALSE;
static SceBool s_needPageReload = SCE_FALSE;
static SceUInt32 s_callerMode = 0;
static SceWChar16 *s_verinfo = SCE_NULL;

static menu::settings::Settings *s_settingsInstance = SCE_NULL;

menu::settings::Settings::Settings()
{
	SceInt32 ret;
	Framework::PluginInitParam pluginParam;
	AppSettings::InitParam sparam;

	settingsReset = SCE_FALSE;

	pluginParam.pluginName = "app_settings_plugin";
	pluginParam.resourcePath = "vs0:vsh/common/app_settings_plugin.rco";
	pluginParam.scopeName = "__main__";

	pluginParam.pluginCreateCB = AppSettings::PluginCreateCB;
	pluginParam.pluginInitCB = AppSettings::PluginInitCB;
	pluginParam.pluginStartCB = AppSettings::PluginStartCB;
	pluginParam.pluginStopCB = AppSettings::PluginStopCB;
	pluginParam.pluginExitCB = AppSettings::PluginExitCB;
	pluginParam.pluginPath = "vs0:vsh/common/app_settings.suprx";
	pluginParam.unk_58 = 0x96;

	Framework::s_frameworkInstance->LoadPlugin(&pluginParam);

	LocalFile::Open(&sparam.xmlFile, "app0:vbackup_settings.xml", SCE_O_RDONLY, 0, &ret);

	sparam.allocCB = sce_paf_malloc;
	sparam.freeCB = sce_paf_free;
	sparam.reallocCB = sce_paf_realloc;
	sparam.safeMemoryOffset = 0;
	sparam.safeMemorySize = k_safeMemIniLimit;

	ret = sce::AppSettings::GetInstance(&sparam, &appSet);

	ret = -1;
	appSet->GetInt("settings_version", &ret, 0);
	if (ret != k_settingsVersion) {
		appSet->Initialize();

		appSet->SetInt("sort", k_defSort);
		appSet->SetInt("backup_device", k_defBackupDevice);
		appSet->SetInt("appclose", k_defAppClose);
		appSet->SetInt("backup_overwrite", k_defBackupOverwrite);
		appSet->SetInt("compression", k_defCompression);

		appSet->SetInt("settings_version", k_settingsVersion);

		settingsReset = SCE_TRUE;
	}

	appSet->GetInt("sort", &sort, k_defSort);
	appSet->GetInt("backup_device", &backup_device, k_defBackupDevice);
	appSet->GetInt("appclose", &appclose, k_defAppClose);
	appSet->GetInt("backup_overwrite", &backup_overwrite, k_defBackupOverwrite);
	appSet->GetInt("compression", &compression, k_defCompression);

	WString *verinfo = new WString();

#ifdef _DEBUG
	*verinfo = L"DEBUG ";
#else
	*verinfo = L"RELEASE ";
#endif
	*verinfo += WIDE(__DATE__);
	*verinfo += L" v 0.99 core rev. 5";
	s_verinfo = (SceWChar16 *)verinfo->data;

	s_settingsInstance = this;
}

menu::settings::Settings::~Settings()
{
	SCE_DBG_LOG_ERROR("Invalid function call\n");
	sceKernelExitProcess(0);
}

SceVoid menu::settings::Settings::Open(SceUInt32 mode)
{
	s_callerMode = mode;

	AppSettings::InterfaceCallbacks ifCb;

	ifCb.listChangeCb = CBListChange;
	ifCb.listForwardChangeCb = CBListForwardChange;
	ifCb.listBackChangeCb = CBListBackChange;
	ifCb.isVisibleCb = CBIsVisible;
	ifCb.elemInitCb = CBElemInit;
	ifCb.elemAddCb = CBElemAdd;
	ifCb.valueChangeCb = CBValueChange;
	ifCb.valueChangeCb2 = CBValueChange2;
	ifCb.termCb = CBTerm;
	ifCb.getStringCb = CBGetString;
	ifCb.getTexCb = CBGetTex;

	Plugin *appSetPlug = paf::Plugin::Find("app_settings_plugin");
	AppSettings::Interface *appSetIf = (sce::AppSettings::Interface *)appSetPlug->GetInterface(1);
	appSetIf->Show(&ifCb);
}

SceVoid menu::settings::Settings::CBListChange(const char *elementId)
{

}

SceVoid menu::settings::Settings::CBListForwardChange(const char *elementId)
{

}

SceVoid menu::settings::Settings::CBListBackChange(const char *elementId)
{

}

SceInt32 menu::settings::Settings::CBIsVisible(const char *elementId, SceBool *pIsVisible)
{
	*pIsVisible = SCE_TRUE;

	return SCE_OK;
}

SceInt32 menu::settings::Settings::CBElemInit(const char *elementId)
{
	return SCE_OK;
}

SceInt32 menu::settings::Settings::CBElemAdd(const char *elementId, paf::ui::Widget *widget)
{
	return SCE_OK;
}

SceInt32 menu::settings::Settings::CBValueChange(const char *elementId, const char *newValue)
{
	char *end;
	SceInt32 ret = SCE_OK;
	SceUInt32 elemHash = VBUtils::GetHash(elementId);
	SceInt32 value = sce_paf_strtol(newValue, &end, 10);
	String *text8 = SCE_NULL;
	WString label16;
	String label8;

	switch (elemHash) {
	case Hash_Sort:
		if (g_currentPagemode != menu::main::Pagemode_Main) {
			s_needPageReload = SCE_TRUE;
		}
		GetInstance()->sort = value;
		break;
	case Hash_BackupDevice:
		text8 = String::WCharToNewString(VBUtils::GetStringWithNum("msg_option_backup_device_", value), text8);
		if (!io::Misc::Exists(text8->data))
			io::Misc::MkdirRWSYS(text8->data);
		if (io::Misc::Exists(text8->data)) {
			if (g_currentPagemode == menu::main::Pagemode_Restore) {
				s_needPageReload = SCE_TRUE;
				s_needBackupCwdReload = SCE_TRUE;
			}
			GetInstance()->backup_device = value;
		}
		else
			ret = SCE_ERROR_ERRNO_ENOENT;
		break;
	case Hash_AppClose:
		GetInstance()->appclose = value;
		break;
	case Hash_Compression:
		GetInstance()->compression = value;
		break;
	case Hash_BackupOverwrite:
		GetInstance()->backup_overwrite = value;
		break;
	default:
		ret = SCE_ERROR_ERRNO_ENOENT;
		break;
	}


	if (text8) {
		text8->Clear();
		delete text8;
	}

	return ret;
}

SceInt32 menu::settings::Settings::CBValueChange2(const char *elementId, const char *newValue)
{
	return SCE_OK;
}

SceVoid menu::settings::Settings::CBTerm()
{
	Resource::Element searchParam;
	String *text8 = SCE_NULL;
	SceInt32 value = 0;

	// Reset file browser pages if needed
	if (s_needPageReload) {

		menu::list::Page *tmpCurr;

		if (s_needBackupCwdReload) {

			// This condition is VBackup-specific
			if (!g_currentDispFilePage->prev) {
				delete g_currentDispFilePage;
				g_currentDispFilePage = SCE_NULL;
			}
			else {
				while (g_currentDispFilePage->prev != SCE_NULL) {
					tmpCurr = g_currentDispFilePage;
					g_currentDispFilePage = g_currentDispFilePage->prev;
					delete tmpCurr;
				}
			}

			GetAppSetInstance()->GetInt("backup_device", &value, 0);
			text8 = String::WCharToNewString(VBUtils::GetStringWithNum("msg_option_backup_device_", value), text8);

			menu::list::Page *newPage = new menu::list::Page(text8->data, SCE_NULL);

			text8->Clear();
			delete text8;

			s_needBackupCwdReload = SCE_FALSE;
		}
		else {
			text8 = new String(g_currentDispFilePage->cwd->data);

			// This condition is VBackup-specific
			if (!g_currentDispFilePage->prev) {
				delete g_currentDispFilePage;
				g_currentDispFilePage = SCE_NULL;
			}
			else {
				tmpCurr = g_currentDispFilePage;
				g_currentDispFilePage = g_currentDispFilePage->prev;
				delete tmpCurr;
			}

			menu::list::Page *newPage = new menu::list::Page(text8->data, SCE_NULL);

			text8->Clear();
			delete text8;
		}

		s_needPageReload = SCE_FALSE;
	}

	g_rootPage->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1);
}

SceWChar16 *menu::settings::Settings::CBGetString(const char *elementId)
{
	Resource::Element searchParam;
	SceWChar16 *res = SCE_NULL;

	searchParam.hash = VBUtils::GetHash(elementId);

	res = g_vbPlugin->GetString(&searchParam);

	if (res[0] == 0) {
		if (!sce_paf_strcmp(elementId, "msg_verinfo"))
			return s_verinfo;
	}

	return res;
}

SceInt32 menu::settings::Settings::CBGetTex(graphics::Surface **surf, const char *elementId)
{
	return SCE_OK;
}

menu::settings::Settings *menu::settings::Settings::GetInstance()
{
	return s_settingsInstance;
}

AppSettings *menu::settings::Settings::GetAppSetInstance()
{
	return s_settingsInstance->appSet;
}

SceVoid menu::settings::SettingsButtonCB::SettingsButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	Resource::Element searchParam;
	Plugin::SceneInitParam rwiParam;
	SceUInt32 callerMode = 0;

	if (pUserData)
		callerMode = *(SceUInt32 *)pUserData;

	menu::settings::Settings::GetInstance()->Open(callerMode);

	g_rootPage->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);
}