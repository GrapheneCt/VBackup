#include <kernel.h>
#include <appmgr.h>
#include <stdlib.h>
#include <string.h>
#include <paf.h>

#include "../dialog.h"
#include "../common.h"
#include "../utils.h"
#include "../sfo.h"
#include "menu_list.h"
#include "menu_search.h"
#include "menu_backup.h"

using namespace paf;

#define MAX_ENTRIES 1024

SceVoid menu::list::BackButtonCB::BackButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	Resource::Element searchParam;
	Page *tmpCurr = g_currentDispFilePage;
	g_currentDispFilePage = g_currentDispFilePage->prev;
	delete tmpCurr;

	searchParam.hash = VBUtils::GetHash("button_common_back");
	ui::Widget *backButton = g_rootPage->GetChildByHash(&searchParam, 0);

	// Disable back button if at root. In VBackup always disable
	/*
	if (!VBUtils::IsRootDevice(g_currentDispFilePage->cwd->data))
		backButton->PlayAnimation(600.0f, ui::Widget::Animation_Reset);
	else
		backButton->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);
	*/

	backButton->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);

	menu::search::Page::Destroy();
	menu::main::Page::Create();
}

SceVoid menu::list::ButtonCB::ButtonCBFun(SceInt32 eventId, paf::ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	Entry *entry = (Entry *)pUserData;

	g_backupEntryList = (Entry **)sce_paf_malloc(sizeof(Entry *));
	g_backupEntryList[0] = entry;
	g_backupEntryListSize = 1;

	if (g_currentPagemode == menu::main::Pagemode_Backup) {
		Dialog::OpenTwoButton(
			g_vbPlugin,
			SCE_NULL,
			VBUtils::GetString("msg_backup_dialog"),
			VBUtils::GetHash("msg_backup_dialog_full"),
			VBUtils::GetHash("msg_backup_dialog_savedata_only"),
			menu::backup::Page::BeginDialogCB);
	}
	else {
		menu::backup::Page::BeginDialogCB(Dialog::ButtonCode_Button1, SCE_NULL);
	}
}

SceVoid menu::list::EntryParserThread::EntryFunction()
{
	SceInt32 res = -1;
	Resource::Element searchParam;
	Resource::Element searchParamSubtext;
	Plugin::TemplateInitParam tmpParam;
	ui::Widget *subText = SCE_NULL;
	shared_ptr<LocalFile> fres;
	graphics::Surface *tex = SCE_NULL;
	wstring wtitleName;
	string appName;
	string sfoPath;
	string iconPath;
	string savedataOnlyPath;
	wstring *wappName = SCE_NULL;
	ScePVoid listBuffer = SCE_NULL;
	SFO *sfo = SCE_NULL;

	searchParam.hash = VBUtils::GetHash("menu_template_list_button_item");
	searchParamSubtext.hash = VBUtils::GetHash("text_menu_button_item_subtext");

	Entry *entry = page->entries;
	Entry *lastEntry = SCE_NULL;

	if (g_backupFromList) {
		io::File list;

		list.Open(BACKUP_LIST_PATH, SCE_O_RDONLY, 0);
		SceSize listSize = list.GetSize();
		listBuffer = sce_paf_malloc(listSize);
		list.Read(listBuffer, listSize);
		list.Close();

		g_backupEntryListSize = 0;
	}

	for (int i = 0; i < page->entryNum; i++) {

		if (IsCanceled())
			break;

		// If in list mode skip unlisted entries
		if (g_backupFromList) {
			if (!sce_paf_strstr((char *)listBuffer, entry->name->string.data)) {
				entry->type = Entry::Type_Unsupported;
			}
		}

		// Skip unsupported entries
		if (entry->type == Entry::Type_Unsupported) {
			entry = entry->next;
			continue;
		}
		
		// Set titleid name
		wtitleName.Clear();
		wtitleName += entry->name->wstring;
		
		// Check if savedata-only
		if (g_currentPagemode == menu::main::Pagemode_Restore) {
			savedataOnlyPath.Clear();
			savedataOnlyPath = *(page->cwd) + "/" + entry->name->string + "/savedata_only.flag";
			if (io::Misc::Exists(savedataOnlyPath.data)) {
				wtitleName += L", ";
				wtitleName += VBUtils::GetString("msg_backup_dialog_savedata_only");
			}
		}

		// Create full app name from SFO
		sfoPath.Clear();
		sfoPath = *(page->cwd) + "/" + entry->name->string + "/sce_sys/param.sfo";
		
		sfo = SCE_NULL;
		sfo = new SFO(sfoPath.data);
		res = sfo->GetString("TITLE", &appName);
		
		for (char *currentPos; (currentPos = sce_paf_strchr(appName.data, '\n')) != SCE_NULL; *currentPos = ' ');

		if (res == SCE_OK) {
			wappName = SCE_NULL;
			wappName = wstring::CharToNewWString(appName.data, wappName);
		}
		else {
			wappName = new wstring();
			*wappName = wtitleName;
			wtitleName = L"";
		}

		if (page->key) {
			if (!sce_paf_wcsstr(wappName->data, page->key->data) || !sce_paf_wcsstr(entry->name->wstring.data, page->key->data)) {
				entry = entry->next;
				continue;
			}
		}

		thread::s_mainThreadMutex.Lock();
		g_vbPlugin->TemplateOpen(page->box, &searchParam, &tmpParam);
		thread::s_mainThreadMutex.Unlock();

		entry->button = (ui::ImageButton *)page->box->GetChildByNum(page->box->childNum - 1);

		// Load icon0 texture
		iconPath.Clear();
		if (g_currentPagemode == menu::main::Pagemode_Backup) {
			iconPath = "ur0:appmeta/";
			iconPath += entry->name->string + "/icon0.png";
		}
		else if (g_currentPagemode == menu::main::Pagemode_Restore) {
			iconPath = *(page->cwd) + "/" + entry->name->string + "/sce_sys/icon0.png";
		}

		LocalFile::Open(&fres, iconPath.data, SCE_O_RDONLY, 0, &res);

		tex = SCE_NULL;
		if (res == SCE_OK) {
			graphics::Surface::CreateFromFile(&tex, g_vbPlugin->memoryPool, &fres);
			tex->base.refCount = -1;
		}

		entry->button->hash = (SceUInt32)entry->button;

		entry->buttonCB = new ButtonCB;
		entry->buttonCB->pUserData = entry;

		thread::s_mainThreadMutex.Lock();
		if (tex) {
			entry->button->SetTextureBase(&tex);
		}
		entry->button->SetLabel(wappName);
		subText = entry->button->GetChildByHash(&searchParamSubtext, 0);
		subText->SetLabel(&wtitleName);
		if (entry->type == Entry::Type_Unsupported)
			entry->button->Disable(0);
		entry->button->RegisterEventCallback(ui::Widget::EventMain_Decide, entry->buttonCB, 0);
		thread::s_mainThreadMutex.Unlock();
		if (i == page->entryNum - 1)
			lastEntry = entry;
		entry = entry->next;

		if (g_backupFromList) {
			g_backupEntryListSize++;
		}
		
		delete wappName;
		delete sfo;
	}

	if (listBuffer)
		sce_paf_free(listBuffer);
	
	if (g_currentDispFilePage != SCE_NULL) {
		if (g_currentDispFilePage->prev != SCE_NULL) {
			g_currentDispFilePage->prev->root->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);
			if (g_currentDispFilePage->prev->root->animationStatus & 0x80)
				g_currentDispFilePage->prev->root->animationStatus &= ~0x80;
		}
		g_currentDispFilePage->root->PlayAnimation(0.0f, ui::Widget::Animation_3D_SlideToBack1);
		if (g_currentDispFilePage->root->animationStatus & 0x80)
			g_currentDispFilePage->root->animationStatus &= ~0x80;
	}
	page->root->PlayAnimation(-5000.0f, ui::Widget::Animation_3D_SlideFromFront);
	if (page->root->animationStatus & 0x80)
		page->root->animationStatus &= ~0x80;

	Dialog::Close();

	if (g_backupFromList)
		common::Utils::AddMainThreadTask(menu::list::Page::ListBackupTask, page->entries);

	Cancel();
}

SceVoid menu::list::Page::ListBackupTask(ScePVoid pUserData)
{
	Entry *entry = (Entry *)pUserData;
	SceInt32 i = 0;

	g_backupEntryList = (Entry **)sce_paf_malloc(sizeof(Entry *) * g_backupEntryListSize);

	while (SCE_TRUE) {

		if (!entry)
			break;

		if (entry->type == Entry::Type_App) {
			g_backupEntryList[i] = entry;
			i++;
		}

		entry = entry->next;
	}

	menu::backup::Page::BeginDialogCB(Dialog::ButtonCode_Button2, SCE_NULL);
	common::Utils::RemoveMainThreadTask(ListBackupTask, pUserData);
}

SceVoid menu::list::Page::Init()
{
	g_currentDispFilePage = SCE_NULL;
}

SceVoid menu::list::Page::LoadCancelDialogCB(Dialog::ButtonCode button, ScePVoid pUserData)
{
	menu::list::BackButtonCB::BackButtonCBFun(0, SCE_NULL, 0, SCE_NULL);
}

menu::list::Page::Page(const char* path, const wchar_t *keyword)
{
	Resource::Element searchParam;
	Plugin::TemplateInitParam tmpParam;

	parserThread = SCE_NULL;
	entries = SCE_NULL;
	cwd = SCE_NULL;
	key = SCE_NULL;
	entryNum = 0;

	// Recursive creation of previous pages. We don't need that in VBackup
	/*
	if (!VBUtils::IsRootDevice(path)) {
		if (g_currentDispFilePage == SCE_NULL) {

			// Find last '/' in working directory
			SceInt32 i = sce_paf_strlen(path) - 2;
			for (; i >= 0; i--) {
				// Slash discovered
				if (path[i] == '/') {
					slashPos = i + 1; // Save pointer
					break; // Stop search
				}
			}

			sce_paf_memcpy(tmpPath, path, slashPos);
			tmpPath[slashPos] = 0;

			new Page(tmpPath);
		}
	}
	*/

	Dialog::OpenPleaseWait(g_vbPlugin, SCE_NULL, VBUtils::GetString("msg_wait"), SCE_TRUE, LoadCancelDialogCB);

	cwd = new string(path);
	if (keyword)
		key = new wstring(keyword);

	if (g_currentDispFilePage == SCE_NULL)
		prev = SCE_NULL;
	else {
		prev = g_currentDispFilePage;
		g_currentDispFilePage->next = this;
	}

	searchParam.hash = VBUtils::GetHash("menu_template_list");
	g_vbPlugin->TemplateOpen(g_root, &searchParam, &tmpParam);

	searchParam.hash = VBUtils::GetHash("plane_list_bg");
	root = (ui::Plane *)g_root->GetChildByHash(&searchParam, 0);
	root->hash = (SceUInt32)root;
	common::Utils::WidgetStateTransition(0.0f, root, ui::Widget::Animation_Reset, SCE_FALSE, SCE_TRUE);

	searchParam.hash = VBUtils::GetHash("button_common_back");
	ui::Widget *backButton = g_rootPage->GetChildByHash(&searchParam, 0);

	// For VBackup back button is always present
	/*
	if (!VBUtils::IsRootDevice(cwd->data))
		backButton->PlayAnimation(300.0f, ui::Widget::Animation_Reset);
	else
		backButton->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);
	*/

	backButton->PlayAnimation(300.0f, ui::Widget::Animation_Reset);

	searchParam.hash = VBUtils::GetHash("list_scroll_box");
	box = (ui::Box *)root->GetChildByHash(&searchParam, 0);
	box->hash = (SceUInt32)box;

	PopulateEntries(path);

	/*
	if (prev) {
		prev->parserThread->Join();
	}
	*/

	// This is moved to parser thread to avoid some nasty visual glitches
	/*
	if (g_currentDispFilePage != SCE_NULL) {
		if (g_currentDispFilePage->prev != SCE_NULL) {
			g_currentDispFilePage->prev->root->PlayAnimationReverse(0.0f, ui::Widget::Animation_Reset);
			if (g_currentDispFilePage->prev->root->animationStatus & 0x80)
				g_currentDispFilePage->prev->root->animationStatus &= ~0x80;
		}
		g_currentDispFilePage->root->PlayAnimation(0.0f, ui::Widget::Animation_3D_SlideToBack1);
		if (g_currentDispFilePage->root->animationStatus & 0x80)
			g_currentDispFilePage->root->animationStatus &= ~0x80;
	}
	root->PlayAnimation(-5000.0f, ui::Widget::Animation_3D_SlideFromFront);
	if (root->animationStatus & 0x80)
		root->animationStatus &= ~0x80;
	*/

	parserThread = new EntryParserThread(SCE_KERNEL_DEFAULT_PRIORITY_USER, SCE_KERNEL_256KiB, "VB::EntryParser");
	parserThread->page = this;
	parserThread->Start();

	g_currentDispFilePage = this;
}

menu::list::Page::~Page()
{
	if (parserThread) {
		parserThread->Cancel();
		parserThread->Join();
		delete parserThread;
	}

	common::Utils::WidgetStateTransition(-100.0f, root, ui::Widget::Animation_3D_SlideFromFront, SCE_TRUE, SCE_FALSE);

	if (prev != SCE_NULL) {
		prev->root->PlayAnimationReverse(0.0f, ui::Widget::Animation_3D_SlideToBack1);
		prev->root->PlayAnimation(0.0f, ui::Widget::Animation_Reset);
		if (prev->root->animationStatus & 0x80)
			prev->root->animationStatus &= ~0x80;
		if (prev->prev != SCE_NULL) {
			prev->prev->root->PlayAnimation(0.0f, ui::Widget::Animation_Reset);
			if (prev->prev->root->animationStatus & 0x80)
				prev->prev->root->animationStatus &= ~0x80;
		}
	}

	if (entries != SCE_NULL)
		ClearEntries(entries);

	delete cwd;
	if (key)
		delete key;
}

SceVoid menu::list::Page::ClearEntries(Entry *entry)
{
	if (entry->next != SCE_NULL)
		ClearEntries(entry->next);

	delete entry;
}

SceInt32 menu::list::Page::Cmpstringp(const ScePVoid p1, const ScePVoid p2)
{
	io::Dir::Dirent *entryA = (io::Dir::Dirent *)p1;
	io::Dir::Dirent *entryB = (io::Dir::Dirent *)p2;

	if ((entryA->type == io::Type_Dir) && (entryB->type != io::Type_Dir))
		return -1;
	else if ((entryA->type != io::Type_Dir) && (entryB->type == io::Type_Dir))
		return 1;
	else {
		switch (menu::settings::Settings::GetInstance()->sort) {
		case 0: // Sort alphabetically (ascending - A to Z)
			return sce_paf_strcasecmp(entryA->name.data, entryB->name.data);
			break;
		case 1: // Sort alphabetically (descending - Z to A)
			return sce_paf_strcasecmp(entryB->name.data, entryA->name.data);
			break;
		case 2: // Sort by file size (largest first)
			return entryA->size > entryB->size ? -1 : entryA->size < entryB->size ? 1 : 0;
			break;
		case 3: // Sort by file size (smallest first)
			return entryB->size > entryA->size ? -1 : entryB->size < entryA->size ? 1 : 0;
			break;
		}
	}

	return 0;
}

SceInt32 menu::list::Page::PopulateEntries(const char *rootPath)
{
	io::Dir dir;

	if (dir.Open(rootPath) >= 0) {

		int entryCount = 0;
		io::Dir::Dirent *fsEntries = new io::Dir::Dirent[MAX_ENTRIES];

		while (dir.Read(&fsEntries[entryCount]) >= 0)
			entryCount++;

		dir.Close();
		sce_paf_qsort(fsEntries, entryCount, sizeof(io::Dir::Dirent), (int(*)(const void *, const void *))Cmpstringp);

		for (int i = 0; i < entryCount; i++) {
			// Allocate Memory
			Entry *item = new Entry();

			// Set type and check if supported
			if (fsEntries[i].type != io::Type_Dir) {

				// Only dirs are supported in VBackup
				/*
				sce_paf_strncpy(item->ext, VBUtils::GetFileExt(fsEntries[i].name.data), 5);

				if (VBUtils::IsSupportedExtension(item->ext)) {
				}
				else {
					delete item;
					continue;
				}
				*/

				delete item;
				continue;
			}
			else if (fsEntries[i].type == io::Type_Dir) {

				string sfoPath = *cwd + "/" + fsEntries[i].name + "/sce_sys/param.sfo";

				if (sce_paf_strlen(fsEntries[i].name.data) != 9 || !io::Misc::Exists(sfoPath.data)) {
					delete item;
					continue;
				}

				item->type = Entry::Type_App;
			}
			else {
				delete item;
				continue;
			}

			item->name = new swstring(fsEntries[i].name.data);

			item->name->string.ToWString(&item->name->wstring);

			entryNum++;

			// New file
			if (entries == SCE_NULL)
				entries = item;

			// Existing file
			else {
				Entry *list = entries;

				while (list->next != SCE_NULL)
					list = list->next;

				list->next = item;
			}
		}

		delete[] fsEntries;
	}
	else
		return SCE_ERROR_ERRNO_ENOENT;

	return 0;
}
