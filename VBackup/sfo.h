#ifndef _VB_SFO_H_
#define _VB_SFO_H_

#include <kernel.h>
#include <paf.h>

using namespace paf;

class SFO
{
public:

	SFO(const char *path);

	~SFO();

	SceInt32 GetValue(const char *name, SceUInt32 *pVal);
	SceInt32 GetString(const char *name, string *pStr);

	string GetTitle();

	static string GetTitle(const char *path);

private:

	class SfoHeader
	{
	public:

		SceUInt32 magic;
		SceUInt32 version;
		SceUInt32 keyofs;
		SceUInt32 valofs;
		SceUInt32 count;
	};

	class SfoEntry
	{
	public:

		SceUInt16 nameofs;
		SceUInt8  alignment;
		SceUInt8  type;
		SceUInt32 valsize;
		SceUInt32 totalsize;
		SceUInt32 dataofs;
	};

	ScePVoid buffer;
};

#endif
