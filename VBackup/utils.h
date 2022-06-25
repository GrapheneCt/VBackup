#ifndef _VB_UTILS_H_
#define _VB_UTILS_H_

#include <kernel.h>
#include <paf.h>

#include "common.h"
#include "dialog.h"

using namespace paf;

#define ROUND_UP(x, a)	(((x) + ((a) - 1)) & ~((a) - 1))
#define ROUND_DOWN(x, a) ((x) & ~((a) - 1))
#define IS_ALIGNED(x, a) (((x) & ((a) - 1)) == 0)

class VBUtils
{
public:

	class AsyncEnqueue : public job::JobItem
	{
	public:

		using job::JobItem::JobItem;

		typedef void(*FinishHandler)();

		~AsyncEnqueue() {}

		SceVoid Run()
		{
			Dialog::OpenPleaseWait(g_vbPlugin, SCE_NULL, VBUtils::GetString("msg_wait"));
			eventHandler(eventId, self, a3, pUserData);
			Dialog::Close();
		}

		SceVoid Finish()
		{
			if (finishHandler)
				finishHandler();
		}

		ui::EventCallback::EventHandler eventHandler;
		FinishHandler finishHandler;
		SceInt32 eventId;
		ui::Widget *self;
		SceInt32 a3;
		ScePVoid pUserData;
	};

	static SceVoid Init();

	static SceUInt32 GetHash(const char *name);

	static wchar_t *GetStringWithNum(const char *name, SceUInt32 num);

	static wchar_t *GetString(const char *name);

	static SceVoid SetPowerTickTask(SceBool enable);

	static SceVoid Exit();

	static SceVoid RunCallbackAsJob(ui::EventCallback::EventHandler eventHandler, VBUtils::AsyncEnqueue::FinishHandler finishHandler, SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData);

private:

	static SceVoid PowerTickTask(ScePVoid pUserData);

};


#endif
