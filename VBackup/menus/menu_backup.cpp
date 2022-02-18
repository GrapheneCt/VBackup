#include <kernel.h>
#include <appmgr.h>
#include <shellsvc.h>
#include <stdlib.h>
#include <string.h>
#include <vshbridge.h>
#include <libdbg.h>
#include <paf.h>

#include "../common.h"
#include "../utils.h"
#include "../backup_core/br_core.h";
#include "menu_list.h"
#include "menu_search.h"
#include "menu_backup.h"

using namespace paf;

static menu::backup::Page *s_backupPageInstance = SCE_NULL;
static graphics::Surface *s_lastIconTex = SCE_NULL;

static SceInt32 s_backupOverwriteAnswer = -1;
static SceInt64 s_totalFileNum = 0;

SceVoid menu::backup::BackButtonCB::BackButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	Resource::Element searchParam;
	ui::Widget *button = SCE_NULL;
	ui::Widget *topText = SCE_NULL;
	WString text16;

	if (s_backupPageInstance->backupThread && !s_backupPageInstance->backupThread->IsCanceled()) {
		searchParam.hash = VBUtils::GetHash("button_main_backup_cancel");
		button = g_root->GetChildByHash(&searchParam, 0);

		searchParam.hash = VBUtils::GetHash("text_main_backup_top");
		topText = g_root->GetChildByHash(&searchParam, 0);

		text16 = VBUtils::GetString("msg_wait");
		topText->SetLabel(&text16);

		button->Disable(SCE_TRUE);

		set_interrupt();
	}
	else {
		menu::backup::Page::Destroy();
	}
}

SceVoid menu::backup::BackupThread::BackupOverwriteDialogCB(Dialog::ButtonCode button, ScePVoid pUserData)
{
	if (button == Dialog::ButtonCode_Yes)
		s_backupOverwriteAnswer = 1;
	else
		s_backupOverwriteAnswer = 0;
}


SceInt32 menu::backup::BackupThread::vshIoMount(SceInt32 id, const char *path, SceInt32 permission, SceInt32 a4, SceInt32 a5, SceInt32 a6)
{
	SceInt32 syscall[6];

	sce_paf_memset(syscall, 0, sizeof(syscall));

	syscall[0] = a4;
	syscall[1] = a5;
	syscall[2] = a6;

	return _vshIoMount(id, path, permission, syscall);
}

SceInt32 menu::backup::BackupThread::BREventCallback(SceInt32 event, SceInt32 status, SceInt64 value, ScePVoid data, ScePVoid argp)
{
	Resource::Element searchParam;
	WString text16;
	WString *ptext16 = SCE_NULL;
	ui::Widget *bottomText = SCE_NULL;
	ui::Widget *button = SCE_NULL;

	searchParam.hash = VBUtils::GetHash("text_main_backup_bottom");
	bottomText = g_root->GetChildByHash(&searchParam, 0);

	searchParam.hash = VBUtils::GetHash("button_main_backup_cancel");
	button = g_root->GetChildByHash(&searchParam, 0);

	text16 = L"";

	switch (event) {
	case BR_EVENT_NOTICE_CONT_NUMBER:
		s_totalFileNum = value;
		break;
	case BR_EVENT_NOTICE_CONT_DECOMPRESS:
		if (status == BR_STATUS_BEGIN)
			text16 = VBUtils::GetString("msg_decompressing");
		break;
	case BR_EVENT_NOTICE_CONT_COMPRESS:
		if (status == BR_STATUS_BEGIN) {
			button->Disable(SCE_TRUE);
			text16 = VBUtils::GetString("msg_compressing");
		}
		break;
	case BR_EVENT_NOTICE_CONT_COPY:
		if (status == BR_STATUS_BEGIN) {
			text16 = VBUtils::GetString("msg_processing");
			ptext16 = SCE_NULL;
			ptext16 = WString::CharToNewWString((char *)data, ptext16);
			text16 += *ptext16;
			delete ptext16;
		}
		break;
	case BR_EVENT_NOTICE_CONT_PROMOTE:
		if (status == BR_STATUS_BEGIN)
			text16 = VBUtils::GetString("msg_installing");
		break;
	default:
		break;
	}

	thread::s_mainThreadMutex.Lock();
	bottomText->SetLabel(&text16);
	thread::s_mainThreadMutex.Unlock();
}

SceVoid menu::backup::BackupThread::UIResetTask(ScePVoid pUserData)
{
	WString text16;
	Resource::Element searchParam;
	ui::Widget *topText = SCE_NULL;
	ui::BusyIndicator *indicator = SCE_NULL;
	ui::Widget *button = SCE_NULL;

	searchParam.hash = VBUtils::GetHash("text_main_backup_top");
	topText = g_root->GetChildByHash(&searchParam, 0);

	searchParam.hash = VBUtils::GetHash("busyindicator_main_backup_progress");
	indicator = (ui::BusyIndicator *)g_root->GetChildByHash(&searchParam, 0);

	searchParam.hash = VBUtils::GetHash("button_main_backup_cancel");
	button = g_root->GetChildByHash(&searchParam, 0);

	indicator->Stop();
	text16 = VBUtils::GetString("msg_ok");
	button->SetLabel(&text16);
	text16.Clear();
	text16 = VBUtils::GetString("msg_completed");
	topText->SetLabel(&text16);
	button->Enable(SCE_TRUE);

	common::Utils::RemoveMainThreadTask(UIResetTask, SCE_NULL);
}

SceVoid menu::backup::BackupThread::EntryFunction()
{
	SceInt32 res = -1;
	SceBool grwMounted = SCE_FALSE;
	Resource::Element searchParam;
	String text8;
	String *ptext8 = SCE_NULL;
	WString text16;
	ui::Widget *topText = SCE_NULL;
	ui::Widget *bottomText = SCE_NULL;
	ui::BusyIndicator *indicator = SCE_NULL;
	ui::Widget *icon = SCE_NULL;
	ui::Widget *button = SCE_NULL;
	ObjectWithCleanup fres;
	graphics::Surface *tex = SCE_NULL;
	graphics::Surface *oldTex = SCE_NULL;

	searchParam.hash = VBUtils::GetHash("text_main_backup_top");
	topText = g_root->GetChildByHash(&searchParam, 0);

	searchParam.hash = VBUtils::GetHash("text_main_backup_bottom");
	bottomText = g_root->GetChildByHash(&searchParam, 0);

	searchParam.hash = VBUtils::GetHash("busyindicator_main_backup_progress");
	indicator = (ui::BusyIndicator *)g_root->GetChildByHash(&searchParam, 0);

	searchParam.hash = VBUtils::GetHash("app_icon_simple_main_backup_icon");
	icon = g_root->GetChildByHash(&searchParam, 0);

	searchParam.hash = VBUtils::GetHash("button_main_backup_cancel");
	button = g_root->GetChildByHash(&searchParam, 0);

	text16.Clear();

	thread::s_mainThreadMutex.Lock();
	indicator->Start();
	text16 = VBUtils::GetString("msg_cancel_vb");
	button->SetLabel(&text16);
	button->Disable(SCE_TRUE);
	thread::s_mainThreadMutex.Unlock();

	sceShellUtilLock((SceShellUtilLockType)(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN | SCE_SHELL_UTIL_LOCK_TYPE_QUICK_MENU | SCE_SHELL_UTIL_LOCK_TYPE_POWEROFF_MENU));

	VBUtils::SetPowerTickTask(SCE_TRUE);

	res = vshIoUmount(0xA00, 0, 0, 0);
	if (res == SCE_OK) {
		grwMounted = SCE_TRUE;
	}

	set_core_callback(BREventCallback);

	for (int i = 0; i < g_backupEntryListSize; i++) {

		if (IsCanceled())
			break;

		text8.Clear();
		text16.Clear();
		
		if (g_currentPagemode == menu::main::Pagemode_Backup) {
			text16 = VBUtils::GetString("msg_backup_progress");
		}
		else if (g_currentPagemode == menu::main::Pagemode_Restore) {
			text16 = VBUtils::GetString("msg_restore_progress");
		}
		text16 += g_backupEntryList[i]->name->wstring;

		thread::s_mainThreadMutex.Lock();
		topText->SetLabel(&text16);
		thread::s_mainThreadMutex.Unlock();

		// Load icon0 texture
		text8.Clear();
		if (g_currentPagemode == menu::main::Pagemode_Backup) {
			text8 = "ur0:appmeta/";
			text8 += g_backupEntryList[i]->name->string + "/icon0.png";
		}
		else if (g_currentPagemode == menu::main::Pagemode_Restore) {
			text8 = *(g_currentDispFilePage->cwd) + "/" + g_backupEntryList[i]->name->string + "/sce_sys/icon0.png";
		}
		
		thread::s_mainThreadMutex.Lock();
		icon->SetTextureBase(&g_defaultTex);
		thread::s_mainThreadMutex.Unlock();
		if (tex)
			delete tex;

		tex = SCE_NULL;

		LocalFile::Open(&fres, text8.data, SCE_O_RDONLY, 0, &res);

		if (res == SCE_OK) {

			graphics::Surface::CreateFromFile(&tex, g_vbPlugin->memoryPool, &fres);

			if (tex) {
				tex->IncrementRefCount();
				thread::s_mainThreadMutex.Lock();
				icon->SetTextureBase(&tex);
				thread::s_mainThreadMutex.Unlock();
				s_lastIconTex = tex;
			}

			fres.cleanup->cb(fres.object);
			delete fres.cleanup;
		}
		
		ptext8 = SCE_NULL;
		ptext8 = String::WCharToNewString(VBUtils::GetStringWithNum("msg_option_backup_device_", menu::settings::Settings::GetInstance()->backup_device), ptext8);
			
		if (g_currentPagemode == menu::main::Pagemode_Backup) {

			s_totalFileNum = 0;
			button->Enable(SCE_TRUE);

			SCE_DBG_LOG_TRACE("Backup begin:\nBackup path: %s\nTitleID: %s\nSavedata only: %d\n\n", ptext8->data, g_backupEntryList[i]->name->string.data, savedataOnly);
			res = do_backup(ptext8->data, g_backupEntryList[i]->name->string.data, savedataOnly, menu::settings::Settings::GetInstance()->compression);
			SCE_DBG_LOG_TRACE("Backup end: 0x%X\n", res);

			if (res == SCE_ERROR_ERRNO_EEXIST) {
				s_backupOverwriteAnswer = -1;
				if (menu::settings::Settings::GetInstance()->backup_overwrite && !g_backupFromList) {
					Dialog::OpenYesNo(
						g_vbPlugin,
						SCE_NULL,
						VBUtils::GetString("msg_backup_overwrite_dialog"),
						BackupOverwriteDialogCB);
					Dialog::WaitEnd();
				}
				else {
					s_backupOverwriteAnswer = 1;
				}

				if (s_backupOverwriteAnswer == 0) {
					s_backupOverwriteAnswer = -1;
					delete ptext8;
					continue;
				}
				else if (s_backupOverwriteAnswer == 1) {
					s_backupOverwriteAnswer = -1;
					do_delete_backup(ptext8->data, g_backupEntryList[i]->name->string.data);
					res = do_backup(ptext8->data, g_backupEntryList[i]->name->string.data, savedataOnly, menu::settings::Settings::GetInstance()->compression);
					SCE_DBG_LOG_TRACE("Backup end: 0x%X\n", res);
				}
			}
			else if (res == SCE_ERROR_ERRNO_EINTR) {
				do_delete_backup(ptext8->data, g_backupEntryList[i]->name->string.data);
				delete ptext8;
				break;
			}

			if (res < 0) {
				if (res == SCE_ERROR_ERRNO_ENOMEM) {
					Dialog::OpenError(g_vbPlugin, res, VBUtils::GetString("msg_enomem"));
					Dialog::WaitEnd();
				}
				else if (res == SCE_ERROR_ERRNO_EINTR) {
					do_delete_backup(ptext8->data, g_backupEntryList[i]->name->string.data);
					delete ptext8;
					break;
				}
				else {
					Dialog::OpenError(g_vbPlugin, res);
					Dialog::WaitEnd();
				}
			}
			
			delete ptext8;
		}
		else if (g_currentPagemode == menu::main::Pagemode_Restore) {

			*ptext8 += "/";
			*ptext8 += g_backupEntryList[i]->name->string.data;

			text8.Clear();
			text8 = "ux0:app/";
			text8 += g_backupEntryList[i]->name->string.data;

			if (io::Misc::Exists(text8.data)) {
				s_backupOverwriteAnswer = -1;
				Dialog::OpenYesNo(
					g_vbPlugin,
					SCE_NULL,
					VBUtils::GetString("msg_restore_overwrite_dialog"),
					BackupOverwriteDialogCB);
				Dialog::WaitEnd();

				if (s_backupOverwriteAnswer == 0) {
					s_backupOverwriteAnswer = -1;
					delete ptext8;
					continue;
				}
			}

			SCE_DBG_LOG_TRACE("Restore begin:\nRestore path: %s\n\n", ptext8->data);
			res = do_restore(ptext8->data);
			SCE_DBG_LOG_TRACE("Restore end: 0x%X\n", res);

			if (res < 0) {
				if (res == SCE_ERROR_ERRNO_ENOMEM)
					Dialog::OpenError(g_vbPlugin, res, VBUtils::GetString("msg_enomem"));
				else
					Dialog::OpenError(g_vbPlugin, res);
				Dialog::WaitEnd();
			}

			delete ptext8;
		}
	}

	if (grwMounted)
		vshIoMount(0xA00, SCE_NULL, 0, 0, 0, 0);

	g_backupEntryListSize = 0;
	if (g_backupEntryList)
		sce_paf_free(g_backupEntryList);

	common::Utils::AddMainThreadTask(UIResetTask, SCE_NULL);

	VBUtils::SetPowerTickTask(SCE_FALSE);

	set_core_callback(SCE_NULL);

	sceShellUtilUnlock((SceShellUtilLockType)(SCE_SHELL_UTIL_LOCK_TYPE_PS_BTN | SCE_SHELL_UTIL_LOCK_TYPE_QUICK_MENU | SCE_SHELL_UTIL_LOCK_TYPE_POWEROFF_MENU));

	Cancel();
}

menu::backup::Page::Page(SceBool savedataOnly)
{
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *commonWidget;
	backupThread = SCE_NULL;

	searchParam.hash = VBUtils::GetHash("plane_main_backup_bg");
	commonWidget = g_root->GetChildByHash(&searchParam, 0);

	if (!commonWidget) {

		searchParam.hash = VBUtils::GetHash("menu_template_main_backup");
		g_vbPlugin->TemplateOpen(g_root, &searchParam, &tmpParam);

		auto buttonCB = new menu::backup::BackButtonCB();

		searchParam.hash = VBUtils::GetHash("button_main_backup_cancel");
		commonWidget = g_root->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(ui::Widget::EventMain_Decide, buttonCB, 0);

		searchParam.hash = VBUtils::GetHash("plane_main_backup_bg");
		commonWidget = g_root->GetChildByHash(&searchParam, 0);
	}

	commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_SlideFromBottom1);
	if (commonWidget->animationStatus & 0x80)
		commonWidget->animationStatus &= ~0x80;

	backupThread = new BackupThread(SCE_KERNEL_INDIVIDUAL_QUEUE_HIGHEST_PRIORITY + 10, SCE_KERNEL_256KiB, "VB::BackupWorker");
	backupThread->savedataOnly = savedataOnly;
	backupThread->Start();
}

menu::backup::Page::~Page()
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;

	if (backupThread) {
		backupThread->Cancel();
		backupThread->Join();
		delete backupThread;
	}

	searchParam.hash = VBUtils::GetHash("app_icon_simple_main_backup_icon");
	commonWidget = g_root->GetChildByHash(&searchParam, 0);

	commonWidget->SetTextureBase(&g_defaultTex);
	delete s_lastIconTex;

	searchParam.hash = VBUtils::GetHash("plane_main_backup_bg");
	commonWidget = g_root->GetChildByHash(&searchParam, 0);

	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_SlideFromBottom1);
}

SceVoid menu::backup::Page::Create(SceBool savedataOnly)
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;

	if (!s_backupPageInstance) {

		if (g_currentDispFilePage)
			g_currentDispFilePage->root->PlayAnimationReverse(0.0f, ui::Widget::Animation_SlideFromBottom1);

		searchParam.hash = VBUtils::GetHash("button_common_settings");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);

		searchParam.hash = VBUtils::GetHash("button_common_back");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);

		menu::search::Page::Destroy();

		s_backupPageInstance = new menu::backup::Page(savedataOnly);
	}
}

SceVoid menu::backup::Page::Destroy()
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;

	if (s_backupPageInstance) {

		if (g_currentDispFilePage) {
			g_currentDispFilePage->root->PlayAnimation(0.0f, ui::Widget::Animation_SlideFromBottom1);
			if (g_currentDispFilePage->root->animationStatus & 0x80)
				g_currentDispFilePage->root->animationStatus &= ~0x80;
		}

		searchParam.hash = VBUtils::GetHash("button_common_settings");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Reset);

		searchParam.hash = VBUtils::GetHash("button_common_back");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Reset);

		if (g_currentDispFilePage) {
			menu::search::Page::Create();
		}
		
		delete s_backupPageInstance;
		s_backupPageInstance = SCE_NULL;
	}

	if (g_backupFromList)
		menu::list::BackButtonCB::BackButtonCBFun(0, SCE_NULL, 0, SCE_NULL);
}


SceVoid menu::backup::Page::BeginDialogCB(Dialog::ButtonCode button, ScePVoid pUserData)
{
	if (button == Dialog::ButtonCode_Button1)
		menu::backup::Page::Create(SCE_FALSE);
	else
		menu::backup::Page::Create(SCE_TRUE);
}