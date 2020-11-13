#pragma once

#include <vector>
#include <list>
#include <sstream>
#include <iostream>

//#include <optional>
#include "coproto/Optional.h"

#include "coproto/Defines.h"
#include "coproto/Scheduler.h"
#include "coproto/Result.h"
#include "coproto/InlineVector.h"
#include "coproto/InlinePoly.h"
#include "coproto/Async.h"
#include "coproto/EndOfRound.h"
#include "coproto/Name.h"
#include "coproto/Resumable.h"

#include <cassert>


namespace coproto
{
	//std::string hexPtr(void* p);

	namespace internal
	{
		struct ProtoImpl : public InlinePoly<Resumable, inlineSize>
		{
			std::unique_ptr<Scheduler> mSched;

			error_code evaluate(Socket& sock, bool print)
			{
				if (mSched == nullptr)
				{
					mSched = make_unique<Scheduler>();
					mSched->scheduleReady(*get());
					get()->mSlotIdx = 0;
				}

				mSched->mSock = &sock;
				mSched->mPrint = print;

				mSched->run();

				if (get()->done())
					return get()->getErrorCode();
				else
					return code::suspend;
			}

			void evaluate(AsyncSocket& sock, std::function<void(error_code)>&& cont, Executor& ex, bool print)
			{
				assert(mSched == nullptr);
				mSched = make_unique<Scheduler>();
				mSched->scheduleReady(*get());
				get()->mSlotIdx = 0;

				mSched->mASock = &sock;
				mSched->mExecutor = &ex;
				mSched->mCont = std::move(cont);
				mSched->mPrint = print;

				mSched->dispatch([this]() {
					mSched->run();
					});

			}
		};
	}

	template<typename T = void>
	class ProtoV
	{
	public:
		using promise_type = internal::ProtoPromise<T>;

		internal::ProtoImpl mBase;


		ProtoV() = default;
		ProtoV(const ProtoV&) = delete;
		ProtoV(ProtoV&&) = default;

		using return_type = T;
		using wrap_type = typename internal::WrapPromise<T>::type;
		wrap_type wrap()
		{
			wrap_type r;

			//using Interface = Resumable;
			//using U = Prom;
			//using Args = decltype(std::move(mBase));
			//static_assert(is_poly_emplaceable<Interface, U, Args>::value, "");

			using Prom = internal::WrapPromise<return_type>;
			internal::InlinePoly<Resumable, internal::inlineSize>& b = r.mBase;

			b.emplace<Prom>(std::move(mBase));

			return r;
		}

		ProtoV<Async<T>> async()
		{
			ProtoV<Async<T>> r;
			auto ptr = new internal::AsyncPromise<T>(std::move(mBase));
			COPROTO_REG_NEW(ptr, "async");
			r.mBase.setOwned(ptr);
			//++gNewDel;
			//std::cout << "new " << hexPtr(r.mBase.get()) << std::endl;
			return r;
		}

		void setName(std::string name)
		{
#ifdef COPROTO_LOGGING
			mBase->setName(name);
#endif
		}

		error_code evaluate(Socket& sock)
		{
			return mBase.evaluate(sock, false);
		}

		void evaluate(AsyncSocket& sock, std::function<void(error_code)>&& cont, Executor& ex)
		{
			mBase.evaluate(sock, std::move(cont), ex, false);

		}

		operator internal::ProtoImpl& ()
		{
			return mBase;
		}
	};
	using Proto = ProtoV<void>;


	namespace internal
	{

		template<typename T>
		struct ReturnStorage;

		template<>
		struct ReturnStorage<void>
		{
			void return_void()
			{}


			void* getValue()
			{
				return nullptr;
			}
		};

		template<typename T>
		struct ReturnStorage
		{
			static_assert(std::is_void<T>::value == false, "");
			optional<T> mVal;

			void return_value(T&& t)
			{
				mVal = std::forward<T>(t);
			}

			void return_value(const T& t)
			{
				mVal = t;
			}

			void* getValue()
			{
				return &mVal.value();
			}


		};
	}
	

}
