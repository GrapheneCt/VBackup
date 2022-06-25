#ifndef _VB_COMMON_H_
#define _VB_COMMON_H_

#include <ctrl.h>
#include <appmgr.h>
#include <paf.h>

#include "menus/menu_list.h"
#include "menus/menu_main.h"
#include "menus/menu_settings.h"

extern paf::Plugin *g_vbPlugin;
extern graph::Surface *g_defaultTex;
extern paf::ui::Widget *g_rootPage;
extern paf::ui::Widget *g_root;
extern paf::ui::Widget *g_commonBusyInidcator;
extern menu::list::Page *g_currentDispFilePage;
extern menu::list::Entry **g_backupEntryList;
extern SceUInt32 g_backupEntryListSize;
extern SceInt32 g_currentPagemode;
extern SceBool g_backupFromList;

#endif
