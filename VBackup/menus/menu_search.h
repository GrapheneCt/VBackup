#ifndef _VB_MENU_SEARCH_H_
#define _VB_MENU_SEARCH_H_

namespace menu {
	namespace search {

		class ButtonCB : public ui::EventCallback
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

		class Page
		{
		public:

			Page();

			~Page();

			static SceVoid Create();

			static SceVoid Destroy();
		};

	}
}

#endif
