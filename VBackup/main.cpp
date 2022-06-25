
#include <kernel.h>
#include <libsysmodule.h>
#include <appmgr.h>
#include <shellsvc.h>
#include <libdbg.h>
#include <paf.h>

#include <lsdb.h>

#include "backup_core/paf_gzip.h"
#include "utils.h"
#include "menus/menu_settings.h"
#include "menus/menu_list.h"
#include "menus/menu_main.h"
#include "backup_core/fat_format.h"

using namespace paf;

Plugin *g_vbPlugin = SCE_NULL;
graph::Surface *g_defaultTex = SCE_NULL;
ui::Widget *g_rootPage = SCE_NULL;
ui::Widget *g_root = SCE_NULL;
ui::Widget *g_commonBusyInidcator = SCE_NULL;
menu::list::Page *g_currentDispFilePage = SCE_NULL;
menu::list::Entry **g_backupEntryList = SCE_NULL;
SceUInt32 g_backupEntryListSize = 0;
SceInt32 g_currentPagemode = menu::main::Pagemode_Main;
SceBool g_backupFromList = SCE_FALSE;

static SceInt32 s_oldMemSize = 0;
static SceInt32 s_oldGraphMemSize = 0;
static SceUInt32 s_iter = 0;

#ifdef _DEBUG
SceVoid leakTestTask(ScePVoid pUserData)
{
	Allocator *glAlloc = Allocator::GetGlobalAllocator();
	SceInt32 sz = glAlloc->GetFreeSize();
	string *str = new string();
	SceInt32 delta;

	str->MemsizeFormat(sz);
	//sceClibPrintf("[VB_DEBUG] Free heap memory: %s\n", str->data);
	delta = s_oldMemSize - sz;
	delta = -delta;
	if (delta) {
		sceClibPrintf("[VB_DEBUG] Memory delta: %d bytes\n", delta);
		sceClibPrintf("[VB_DEBUG] Free heap memory: %s\n", str->data);
	}
	s_oldMemSize = sz;
	delete str;
	/*
	str = new String();

	sz = Framework::s_frameworkInstance->GetDefaultGraphicsMemoryPool()->GetFreeSize();
	str->MemsizeFormat(sz);

	//sceClibPrintf("[VB_DEBUG] Free graphics pool: %s\n", str->data);

	delta = s_oldGraphMemSize - sz;
	delta = -delta;
	if (delta) {
		sceClibPrintf("[VB_DEBUG] Free graphics pool: %s\n", str->data);
		sceClibPrintf("[VB_DEBUG] Graphics pool delta: %d bytes\n", delta);
	}
	s_oldGraphMemSize = sz;
	delete str;*/
}
#endif

SceVoid WarningDialogCB(Dialog::ButtonCode button, ScePVoid pUserData)
{
	string *text8 = SCE_NULL;

	if (button == Dialog::ButtonCode_No) {
		s_frameworkInstance->ExitRenderingLoop();
		return;
	}

#ifdef _DEBUG
	//task::Register(leakTestTask, SCE_NULL);
#endif

#ifndef _DEBUG
	sceAppMgrDestroyOtherApp();
#endif

	text8 = ccc::UTF16toUTF8WithAlloc(VBUtils::GetStringWithNum("msg_option_backup_device_", menu::settings::Settings::GetInstance()->backup_device));
	if (!LocalFile::Exists(text8->c_str())) {
		Dir::Create(text8->c_str());
		if (!LocalFile::Exists(text8->c_str())) {
			menu::settings::Settings::GetInstance()->GetAppSetInstance()->SetInt("backup_device", 0);
			menu::settings::Settings::GetInstance()->backup_device = 0;
			delete text8;
			text8 = SCE_NULL;
			text8 = ccc::UTF16toUTF8WithAlloc(VBUtils::GetStringWithNum("msg_option_backup_device_", menu::settings::Settings::GetInstance()->backup_device));
			Dir::Create(text8->c_str());
		}
	}
}

SceVoid pluginLoadCB(Plugin *plugin)
{
	rco::Element searchParam;
	Plugin::PageInitParam rwiParam;
	Plugin::TemplateInitParam tmpParam;

	if (plugin == SCE_NULL) {
		SCE_DBG_LOG_ERROR("Plugin load FAIL!\n");
		return;
	}

	g_vbPlugin = plugin;

	new menu::settings::Settings();

	searchParam.hash = VBUtils::GetHash("page_common");
	g_rootPage = g_vbPlugin->PageOpen(&searchParam, &rwiParam);

	searchParam.hash = VBUtils::GetHash("plane_common_bg");
	g_root = g_rootPage->GetChild(&searchParam, 0);

	searchParam.hash = VBUtils::GetHash("busyindicator_common");
	g_commonBusyInidcator = (ui::BusyIndicator *)g_rootPage->GetChild(&searchParam, 0);

	searchParam.hash = VBUtils::GetHash("_common_texture_transparent");
	Plugin::GetTexture(&g_defaultTex, g_vbPlugin, &searchParam);

	menu::main::Page::Create();

	menu::list::Page::Init();

	searchParam.hash = VBUtils::GetHash("button_common_settings");
	ui::Widget *settingsButton = g_rootPage->GetChild(&searchParam, 0);
	auto settingsButtonCB = new menu::settings::SettingsButtonCB();
	settingsButton->RegisterEventCallback(ui::EventMain_Decide, settingsButtonCB, 0);

	searchParam.hash = VBUtils::GetHash("button_common_back");
	ui::Widget *backButton = g_rootPage->GetChild(&searchParam, 0);
	auto backButtonCB = new menu::list::BackButtonCB();
	backButton->RegisterEventCallback(ui::EventMain_Decide, backButtonCB, 0);
	backButton->PlayEffectReverse(0.0f, effect::EffectType_Reset);

	sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);

#ifdef _DEBUG
	WarningDialogCB(Dialog::ButtonCode_Yes, SCE_NULL);
#else
	if (menu::settings::Settings::GetInstance()->appclose) {
		Dialog::OpenYesNo(
			g_vbPlugin,
			SCE_NULL,
			VBUtils::GetString("msg_appclose_warning_dialog"),
			WarningDialogCB);
	}
	else {
		WarningDialogCB(Dialog::ButtonCode_Yes, SCE_NULL);
	}
#endif

	SCE_DBG_LOG_TRACE("Plugin init OK\n");
}

int main(SceSize args, void *argp)
{
	int res;

	sceShellUtilInitEvents(0);
	sceShellUtilLock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);

	if (SCE_DBG_LOGGING_ENABLED)
#ifdef _DEBUG
	{
		sceDbgSetMinimumLogLevel(SCE_DBG_LOG_LEVEL_TRACE);
	}
#else
	{
		sceDbgSetMinimumLogLevel(SCE_DBG_LOG_LEVEL_ERROR);
	}
#endif

	Framework::InitParam fwParam;
	fwParam.LoadDefaultParams();
	fwParam.applicationMode = Framework::Mode_Game;
	fwParam.defaultSurfacePoolSize = SCE_KERNEL_16MiB;
	fwParam.textSurfaceCacheSize = SCE_KERNEL_2MiB;

	Framework *fw = new Framework(fwParam);

	fw->LoadCommonResourceAsync();

	Plugin::InitParam pluginParam;

	pluginParam.pluginName = "vbackup_plugin";
	pluginParam.resourcePath = "app0:vbackup_plugin.rco";
	pluginParam.scopeName = "__main__";

	pluginParam.pluginStartCB = pluginLoadCB;

	fw->LoadPluginAsync(&pluginParam);

	VBUtils::Init();

	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_BXCE);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceSysmoduleLoadModuleInternal(%s): 0x%X\n", "SCE_SYSMODULE_INTERNAL_BXCE", res);
	}

	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_INI_FILE_PROCESSOR);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceSysmoduleLoadModuleInternal(%s): 0x%X\n", "SCE_SYSMODULE_INTERNAL_INI_FILE_PROCESSOR", res);
	}

	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceSysmoduleLoadModuleInternal(%s): 0x%X\n", "SCE_SYSMODULE_INTERNAL_COMMON_GUI_DIALOG", res);
	}

	res = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL);
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sceSysmoduleLoadModuleInternal(%s): 0x%X\n", "SCE_SYSMODULE_INTERNAL_PROMOTER_UTIL", res);
	}

	res = fat_format_init();
	if (res < 0) {
		SCE_DBG_LOG_ERROR("fat_format_init(): 0x%X\n", res);
	}

	res = sce_paf_gzip_init();
	if (res < 0) {
		SCE_DBG_LOG_ERROR("sce_paf_gzip_init(): 0x%X\n", res);
	}

	fw->EnterRenderingLoop();

	sceShellUtilUnlock(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN);

	delete fw;

	VBUtils::Exit();

	return 0;
}
