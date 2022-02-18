#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <paf.h>

#include "../common.h"
#include "../utils.h"
#include "menu_main.h"
#include "menu_search.h"

using namespace paf;

static menu::main::Page *s_mainPageInstance = SCE_NULL;

SceVoid menu::main::ButtonCB::ButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	String *text8 = SCE_NULL;

	switch (self->hash) {
	case ButtonHash_Backup:
		g_currentPagemode = Pagemode_Backup;
		g_backupFromList = SCE_FALSE;

		new menu::list::Page("ux0:app", SCE_NULL);
		menu::search::Page::Create();
		break;
	case ButtonHash_Restore:
		g_currentPagemode = Pagemode_Restore;
		g_backupFromList = SCE_FALSE;

		text8 = String::WCharToNewString(VBUtils::GetStringWithNum("msg_option_backup_device_", menu::settings::Settings::GetInstance()->backup_device), text8);
		new menu::list::Page(text8->data, SCE_NULL);
		menu::search::Page::Create();
		break;
	case ButtonHash_BackupList:
		g_backupFromList = SCE_TRUE;
		g_currentPagemode = Pagemode_Backup;

		new menu::list::Page("ux0:app", SCE_NULL);
		menu::search::Page::Create();
		break;
	default:
		break;
	}

	menu::main::Page::Destroy();

	if (text8)
		delete text8;
}

menu::main::Page::Page()
{
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *commonWidget;

	searchParam.hash = VBUtils::GetHash("plane_main_action_bg");
	commonWidget = g_root->GetChildByHash(&searchParam, 0);

	if (!commonWidget) {

		searchParam.hash = VBUtils::GetHash("menu_template_main_action");
		g_vbPlugin->TemplateOpen(g_root, &searchParam, &tmpParam);

		auto buttonCB = new menu::main::ButtonCB();

		searchParam.hash = ButtonHash_Backup;
		commonWidget = g_root->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(ui::Widget::EventMain_Decide, buttonCB, 0);

		searchParam.hash = ButtonHash_Restore;
		commonWidget = g_root->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(ui::Widget::EventMain_Decide, buttonCB, 0);

		searchParam.hash = ButtonHash_BackupList;
		commonWidget = g_root->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(ui::Widget::EventMain_Decide, buttonCB, 0);

		searchParam.hash = VBUtils::GetHash("plane_main_action_bg");
		commonWidget = g_root->GetChildByHash(&searchParam, 0);
	}

	commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_SlideFromBottom1);
	if (commonWidget->animationStatus & 0x80)
		commonWidget->animationStatus &= ~0x80;

	searchParam.hash = VBUtils::GetHash("menu_button_action_backup_list");
	commonWidget = g_root->GetChildByHash(&searchParam, 0);

	if (io::Misc::Exists(BACKUP_LIST_PATH))
		commonWidget->Enable(0);
	else
		commonWidget->Disable(0);
}

menu::main::Page::~Page()
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;

	searchParam.hash = VBUtils::GetHash("plane_main_action_bg");
	commonWidget = g_root->GetChildByHash(&searchParam, 0);

	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_SlideFromBottom1);
}

SceVoid menu::main::Page::Create()
{
	if (!s_mainPageInstance)
		s_mainPageInstance = new menu::main::Page();

	g_currentPagemode = Pagemode_Main;
}

SceVoid menu::main::Page::Destroy()
{
	if (s_mainPageInstance) {
		delete s_mainPageInstance;
		s_mainPageInstance = SCE_NULL;
	}
}

