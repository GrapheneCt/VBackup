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

static thread::JobQueue *s_cbJobQueue = SCE_NULL;
static SceBool s_powerTickTaskState = SCE_FALSE;

SceUInt32 VBUtils::GetHash(const char *name)
{
	Resource::Element searchRequest;
	Resource::Element searchResult;

	searchRequest.id = name;
	searchResult.hash = searchResult.GetHashById(&searchRequest);

	return searchResult.hash;
}

wchar_t *VBUtils::GetStringWithNum(const char *name, SceUInt32 num)
{
	Resource::Element searchRequest;
	char fullName[128];

	sce_paf_snprintf(fullName, sizeof(fullName), "%s%u", name, num);

	searchRequest.hash = VBUtils::GetHash(fullName);
	wchar_t *res = (wchar_t *)g_vbPlugin->GetString(&searchRequest);

	return res;
}

wchar_t *VBUtils::GetString(const char *name)
{
	Resource::Element searchRequest;

	searchRequest.hash = VBUtils::GetHash(name);
	wchar_t *res = (wchar_t *)g_vbPlugin->GetString(&searchRequest);

	return res;
}

SceInt32 VBUtils::Alphasort(const void *p1, const void *p2) 
{
	io::Dir::Dirent *entryA = (io::Dir::Dirent *)p1;
	io::Dir::Dirent *entryB = (io::Dir::Dirent *)p2;

	if ((entryA->type == io::Type_Dir) && (entryB->type != io::Type_Dir))
		return -1;
	else if ((entryA->type != io::Type_Dir) && (entryB->type == io::Type_Dir))
		return 1;

	return sce_paf_strcasecmp(entryA->name.data, entryB->name.data);
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
		common::Utils::AddMainThreadTask(VBUtils::PowerTickTask, SCE_NULL);
		s_powerTickTaskState = SCE_TRUE;
	}
	else {
		common::Utils::RemoveMainThreadTask(VBUtils::PowerTickTask, SCE_NULL);
		s_powerTickTaskState = SCE_FALSE;
	}

}

SceVoid VBUtils::Init()
{
	thread::JobQueue::Opt queueOpt;
	queueOpt.workerNum = 1;
	queueOpt.workerOpt = SCE_NULL;
	queueOpt.workerPriority = SCE_KERNEL_HIGHEST_PRIORITY_USER + 30;
	queueOpt.workerStackSize = SCE_KERNEL_256KiB;

	s_cbJobQueue = new thread::JobQueue("VB::CallbackJobQueue", &queueOpt);
}

SceVoid VBUtils::Exit()
{
	sceKernelExitProcess(0);
}

SceVoid VBUtils::RunCallbackAsJob(ui::Widget::EventCallback::EventHandler eventHandler, VBUtils::AsyncEnqueue::FinishHandler finishHandler, SceInt32 eventId, ui::Widget *self, SceInt32 a3, ScePVoid pUserData)
{
	AsyncEnqueue *asJob = new AsyncEnqueue("VB::AsyncCallback");
	asJob->eventHandler = eventHandler;
	asJob->finishHandler = finishHandler;
	asJob->eventId = eventId;
	asJob->self = self;
	asJob->a3 = a3;
	asJob->pUserData = pUserData;

	CleanupHandler *req = new CleanupHandler();
	req->refCount = 0;
	req->unk_08 = 1;
	req->cb = (CleanupHandler::CleanupCallback)AsyncEnqueue::JobKiller;

	ObjectWithCleanup itemParam;
	itemParam.object = asJob;
	itemParam.cleanup = req;

	s_cbJobQueue->Enqueue(&itemParam);
}