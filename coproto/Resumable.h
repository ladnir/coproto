#pragma once
#include "coproto/Defines.h"
#include "coproto/error_code.h"
#include "coproto/InlineVector.h"
#include "coproto/InlinePoly.h"
#include "coproto/TypeTraits.h"
#include <vector>

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
		//std::vector<Resumable*> mUpstream, mDwstream;

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


	namespace internal
	{

		template<typename Container>
		requires is_resizable_trivial_container_v<Container>
			void tryResize(u64 size, Container& container)
		{
			try {
				if ((size % sizeof(typename Container::value_type)) == 0)
					container.resize(size / sizeof(typename Container::value_type));
			}
			catch (...)
			{
			}
		}

		template<typename Container>
		requires (!is_resizable_trivial_container_v<Container>)
			void tryResize(u64 size, Container& container)
		{
		}

		template<typename Container>
		requires is_trivial_container_v<Container>
			span<u8> asSpan(Container& container)
		{
			return span<u8>((u8*)container.data(), container.size() * sizeof(typename Container::value_type));
		}

		template<typename ValueType>
		requires std::is_trivial_v<ValueType>
			span<u8> asSpan(ValueType& container)
		{
			return span<u8>((u8*)&container, sizeof(ValueType));
		}
	}

	struct SendOp
	{
		virtual ~SendOp() {}
		virtual span<u8> asSpan() = 0;

	};

	template<typename Container>
	struct RefSendBuffer : public SendOp
	{
		Container& mCont;

		RefSendBuffer(Container& c)
			: mCont(c)
		{}

		RefSendBuffer(RefSendBuffer&&) = default;

		span<u8> asSpan() override
		{
			return internal::asSpan(mCont);
		}

	};


	template<typename Container>
	struct MvSendBuffer : public SendOp
	{
		Container mCont;

		MvSendBuffer(Container&& c)
			:mCont(std::forward<Container>(c))
		{}

		span<u8> asSpan() override
		{
			return internal::asSpan(mCont);
		}

	};


	struct SendBuffer
	{
		internal::InlinePoly<SendOp, sizeof(u64) * 8> mStorage;

		span<u8> asSpan() {
			return mStorage->asSpan();
		}
	};


	struct RecvBuffer
	{
		virtual span<u8> asSpan(u64 resize) = 0;
	};


}