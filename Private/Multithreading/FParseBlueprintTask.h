#pragma once
#include "Async/AsyncWork.h"
#include <string>

class FParseBlueprintTask: public FNonAbandonableTask
{
	friend class FAsyncTask<FParseBlueprintTask>;
public:
	void SetPath(std::string BlueprintPath);
protected:
	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FParseBlueprintTask, STATGROUP_ThreadPoolAsyncTasks);
	}
};
