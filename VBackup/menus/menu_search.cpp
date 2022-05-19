#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <paf.h>

#include "../common.h"
#include "../utils.h"
#include "menu_search.h"

using namespace paf;

menu::search::Page *s_searchPageInstance = SCE_NULL;

SceVoid menu::search::ButtonCB::ButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	Resource::Element searchParam;
	ui::TextBox *searchBox;
	string *text8 = SCE_NULL;
	wstring search16;
	menu::list::Page *tmpCurr;

	searchParam.hash = VBUtils::GetHash("text_box_top_search");
	searchBox = (ui::TextBox *)g_rootPage->GetChildByHash(&searchParam, 0);

	searchBox->GetLabel(&search16);

	text8 = new string(g_currentDispFilePage->cwd->data);

	if (!g_currentDispFilePage->prev) {
		delete g_currentDispFilePage;
		g_currentDispFilePage = SCE_NULL;
	}
	else {
		tmpCurr = g_currentDispFilePage;
		g_currentDispFilePage = g_currentDispFilePage->prev;
		delete tmpCurr;
	}

	if (search16.length != 0) {
		menu::list::Page *newPage = new menu::list::Page(text8->data, search16.data);
	}
	else {
		menu::list::Page *newPage = new menu::list::Page(text8->data, SCE_NULL);
	}

	text8->Clear();
	delete text8;
}

menu::search::Page::Page()
{
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *commonWidget;

	searchParam.hash = VBUtils::GetHash("plane_top_search");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);

	if (!commonWidget) {

		searchParam.hash = VBUtils::GetHash("menu_template_search");
		g_vbPlugin->TemplateOpen(g_rootPage, &searchParam, &tmpParam);

		auto buttonCB = new menu::search::ButtonCB();

		searchParam.hash = VBUtils::GetHash("image_button_top_search");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(ui::Widget::EventMain_Decide, buttonCB, 0);

		searchParam.hash = VBUtils::GetHash("text_box_top_search");
		commonWidget = (ui::TextBox *)g_rootPage->GetChildByHash(&searchParam, 0);
		commonWidget->RegisterEventCallback(0x1000000B, buttonCB, 0);

		searchParam.hash = VBUtils::GetHash("plane_top_search");
		commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);
	}

	commonWidget->PlayAnimation(0.0f, ui::Widget::Animation_Fadein1);
	if (commonWidget->animationStatus & 0x80)
		commonWidget->animationStatus &= ~0x80;
}

menu::search::Page::~Page()
{
	Resource::Element searchParam;
	ui::Widget *commonWidget;

	searchParam.hash = VBUtils::GetHash("plane_top_search");
	commonWidget = g_rootPage->GetChildByHash(&searchParam, 0);

	commonWidget->PlayAnimationReverse(0.0f, ui::Widget::Animation_Fadein1);
}

SceVoid menu::search::Page::Create()
{
	if (!s_searchPageInstance)
		s_searchPageInstance = new menu::search::Page();
}

SceVoid menu::search::Page::Destroy()
{
	if (s_searchPageInstance) {
		delete s_searchPageInstance;
		s_searchPageInstance = SCE_NULL;
	}
}