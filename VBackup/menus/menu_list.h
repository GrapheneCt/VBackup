#ifndef _VB_MENU_DISPLAYFILES_H_
#define _VB_MENU_DISPLAYFILES_H_

#include <paf.h>

#include "../dialog.h"

using namespace paf;

namespace menu {
	namespace list {

		class Page;
		class Entry;

		class BackButtonCB : public ui::Widget::EventCallback
		{
		public:

			BackButtonCB()
			{
				eventHandler = BackButtonCBFun;
			};

			virtual ~BackButtonCB()
			{

			};

			static SceVoid BackButtonCBFun(SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

		};

		class ButtonCB : public ui::Widget::EventCallback
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

		class Entry
		{
		public:

			enum Type 
			{
				Type_Unsupported,
				Type_App
			};

			Entry() :
				prev(SCE_NULL),
				next(SCE_NULL),
				name(SCE_NULL),
				type(Type_Unsupported),
				button(SCE_NULL)
			{

			}

			~Entry()
			{
				if (name)
					delete name;
			}

			Entry *prev;
			Entry *next;
			swstring *name;
			Type type;
			ui::ImageButton *button;
			ButtonCB *buttonCB;
		};

		class EntryParserThread : public thread::Thread
		{
		public:

			using thread::Thread::Thread;

			SceVoid EntryFunction();

			Page *page;
		};

		class Page
		{
		public:

			Page(const char *path, const wchar_t *keyword);

			~Page();

			static SceVoid Init();

			static SceVoid LoadCancelDialogCB(Dialog::ButtonCode button, ScePVoid pUserData);

			static SceVoid ListBackupTask(ScePVoid pUserData);

			ui::Plane *root;
			ui::Box *box;
			string *cwd;
			wstring *key;
			Page *prev;
			Page *next;
			SceUInt32 entryNum;
			Entry *entries;
			EntryParserThread *parserThread;

		private:

			static SceInt32 Cmpstringp(const ScePVoid p1, const ScePVoid p2);

			SceInt32 PopulateEntries(const char *rootPath);
			SceVoid ClearEntries(Entry *entries);
		};
	}
}

#endif
