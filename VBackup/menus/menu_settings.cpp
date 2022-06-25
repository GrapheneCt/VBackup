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
static const wchar_t *s_verinfo = SCE_NULL;

static menu::settings::Settings *s_settingsInstance = SCE_NULL;

menu::settings::Settings::Settings()
{
	SceInt32 ret;
	SceSize fsize = 0;
	const char *fmime = SCE_NULL;
	Plugin::InitParam pluginParam;
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

	s_frameworkInstance->LoadPlugin(&pluginParam);

	sparam.xmlFile = g_vbPlugin->resource->GetFile(VBUtils::GetHash("file_vbackup_settings"), &fsize, &fmime);

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

	wstring *verinfo = new wstring();

#ifdef _DEBUG
	*verinfo = L"DEBUG ";
#else
	*verinfo = L"RELEASE ";
#endif
	*verinfo += WIDE(__DATE__);
	*verinfo += L" v 1.03 core rev. 8";
	s_verinfo = verinfo->c_str();

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
	string *text8 = SCE_NULL;
	wstring label16;
	string label8;

	switch (elemHash) {
	case Hash_Sort:
		if (g_currentPagemode != menu::main::Pagemode_Main) {
			s_needPageReload = SCE_TRUE;
		}
		GetInstance()->sort = value;
		break;
	case Hash_BackupDevice:
		text8 = ccc::UTF16toUTF8WithAlloc(VBUtils::GetStringWithNum("msg_option_backup_device_", value));
		if (!LocalFile::Exists(text8->c_str()))
			Dir::Create(text8->c_str());
		if (LocalFile::Exists(text8->c_str())) {
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
		text8->clear();
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
	rco::Element searchParam;
	string *text8 = SCE_NULL;
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
			text8 = ccc::UTF16toUTF8WithAlloc(VBUtils::GetStringWithNum("msg_option_backup_device_", value));

			menu::list::Page *newPage = new menu::list::Page(text8->c_str(), SCE_NULL);

			text8->clear();
			delete text8;

			s_needBackupCwdReload = SCE_FALSE;
		}
		else {
			text8 = new string(g_currentDispFilePage->cwd->c_str());

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

			menu::list::Page *newPage = new menu::list::Page(text8->c_str(), SCE_NULL);

			text8->clear();
			delete text8;
		}

		s_needPageReload = SCE_FALSE;
	}

	g_rootPage->PlayEffect(0.0f, effect::EffectType_Fadein1);
}

wchar_t *menu::settings::Settings::CBGetString(const char *elementId)
{
	rco::Element searchParam;
	wchar_t *res = SCE_NULL;

	searchParam.hash = VBUtils::GetHash(elementId);

	res = g_vbPlugin->GetWString(&searchParam);

	if (res[0] == 0) {
		if (!sce_paf_strcmp(elementId, "msg_verinfo"))
			return (wchar_t *)s_verinfo;
	}

	return res;
}

SceInt32 menu::settings::Settings::CBGetTex(graph::Surface **surf, const char *elementId)
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
	rco::Element searchParam;
	SceUInt32 callerMode = 0;

	if (pUserData)
		callerMode = *(SceUInt32 *)pUserData;

	menu::settings::Settings::GetInstance()->Open(callerMode);

	g_rootPage->PlayEffectReverse(0.0f, effect::EffectType_Fadein1);
}