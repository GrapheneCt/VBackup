#include <apputil.h>
#include <kernel.h>
#include <shellsvc.h>
#include <power.h> 
#include <appmgr.h> 
#include <message_dialog.h>
#include <libdbg.h>
#include <paf.h>
#include <stdlib.h>

#include "utils.h"
#include "common.h"

static job::JobQueue *s_cbJobQueue = SCE_NULL;
static SceBool s_powerTickTaskState = SCE_FALSE;

SceUInt32 VBUtils::GetHash(const char *name)
{
	string searchRequest;
	rco::Element searchResult;

	searchRequest = name;
	searchResult.hash = searchResult.GetHash(&searchRequest);

	return searchResult.hash;
}

wchar_t *VBUtils::GetStringWithNum(const char *name, SceUInt32 num)
{
	rco::Element searchRequest;
	char fullName[128];

	sce_paf_snprintf(fullName, sizeof(fullName), "%s%u", name, num);

	searchRequest.hash = VBUtils::GetHash(fullName);
	wchar_t *res = (wchar_t *)g_vbPlugin->GetWString(&searchRequest);

	return res;
}

wchar_t *VBUtils::GetString(const char *name)
{
	rco::Element searchRequest;

	searchRequest.hash = VBUtils::GetHash(name);
	wchar_t *res = (wchar_t *)g_vbPlugin->GetWString(&searchRequest);

	return res;
}

SceVoid VBUtils::PowerTickTask(ScePVoid pUserData)
{
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
}

SceVoid VBUtils::SetPowerTickTask(SceBool enable)
{
	if (enable == s_powerTickTaskState)
		return;

	if (enable) {
		task::Register(VBUtils::PowerTickTask, SCE_NULL);
		s_powerTickTaskState = SCE_TRUE;
	}
	else {
		task::Unregister(VBUtils::PowerTickTask, SCE_NULL);
		s_powerTickTaskState = SCE_FALSE;
	}

}

SceVoid VBUtils::Init()
{
	job::JobQueue::Option queueOpt;
	queueOpt.workerNum = 1;
	queueOpt.workerOpt = SCE_NULL;
	queueOpt.workerPriority = SCE_KERNEL_HIGHEST_PRIORITY_USER + 30;
	queueOpt.workerStackSize = SCE_KERNEL_256KiB;

	s_cbJobQueue = new job::JobQueue("VB::CallbackJobQueue", &queueOpt);
}

SceVoid VBUtils::Exit()
{
	sceKernelExitProcess(0);
}

SceVoid VBUtils::RunCallbackAsJob(ui::EventCallback::EventHandler eventHandler, VBUtils::AsyncEnqueue::FinishHandler finishHandler, SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	auto asJob = SharedPtr<AsyncEnqueue>(new AsyncEnqueue("VB::AsyncCallback"));
	asJob->eventHandler = eventHandler;
	asJob->finishHandler = finishHandler;
	asJob->eventId = eventId;
	asJob->self = self;
	asJob->a3 = a3;
	asJob->pUserData = pUserData;

	s_cbJobQueue->Enqueue((SharedPtr<job::JobItem> *)&asJob);
}