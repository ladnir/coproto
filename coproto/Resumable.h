#pragma once
#include "Defines.h"
#include "error_code.h"
#include "InlineVector.h"

namespace coproto
{
	class Scheduler;

	class Resumable
	{
	public:
		Resumable() = default;
		Resumable(const Resumable&) = default;
		Resumable(Resumable&&) = default;

		internal::InlineVector<Resumable*, 4> mUpstream, mDwstream;

		u32 mSlotIdx = ~0;
		u32& getSlot()
		{
			return mSlotIdx;
		}

#ifdef COPROTO_LOGGING
		std::string mName;
		void setName(std::string name)
		{
			mName = name;
		}

		virtual std::string getName()
		{
			if (mName.size() == 0)
			{
				mName = "unknown_" + hexPtr(this);
			}
			return mName;
		}
#endif

		virtual ~Resumable() {}
		virtual error_code resume_(Scheduler& sched) = 0;
		virtual bool done() = 0;
		virtual void* getValue() { return nullptr; };
		virtual void setError(error_code e, std::exception_ptr p) = 0;
		virtual std::exception_ptr getExpPtr() = 0;
		virtual error_code getErrorCode() = 0;
	};

}