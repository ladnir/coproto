#pragma once
#include "Scheduler.h"
#include "Proto.h"
#include <cassert>

namespace coproto
{


	class LocalScheduler
	{
	public:

		struct Sock : public Socket
		{
			//u64 mIdx;
			//LocalScheduler* mSched;

			std::list<std::vector<u8>> mInbound, mOutbound;

			error_code recv(span<u8> data) override;
			error_code send(span<u8> data) override;
		};

		std::array<Sock, 2> mSocks;
		std::array<Scheduler, 2> mScheds;

		void sendMsgs(u64 sender)
		{
			assert(mSocks[sender ^ 1].mInbound.size() == 0);
			mSocks[sender ^ 1].mInbound = std::move(mSocks[sender].mOutbound);
		}

		template<typename T>
		error_code execute(Proto<T>& p0, Proto<T>& p1)
		{
#ifdef COPROTO_LOGGING
			if (p0.mBase->mName.size() == 0)
				p0.mBase->setName("main");
			if (p1.mBase->mName.size() == 0)
				p1.mBase->setName("main");
#endif

			//mScheds[0].mPrint = true;

			//bool v = false;
			//mSocks[0].mIdx = 0;
			//mSocks[0].mSched = this;
			//mSocks[1].mIdx = 1;
			//mSocks[1].mSched = this;

			mScheds[0].mSock = &mSocks[0];
			mScheds[1].mSock = &mSocks[1];


			p0.mBase->mSlotIdx = 0;
			p1.mBase->mSlotIdx = 0;
			mScheds[0].scheduleReady(*p0.mBase.get());
			mScheds[1].scheduleReady(*p1.mBase.get());
			mScheds[0].mRoundIdx = 0;
			mScheds[1].mRoundIdx = 0;

			while (
				mScheds[0].done() == false ||
				mScheds[1].done() == false)
			{

				if (mScheds[0].done() == false)
				{
					mScheds[0].runRound();

					if (mScheds[0].done())
					{
						auto e0 = p0.mBase->getErrorCode();
						if (e0)
							return e0;
					}

					sendMsgs(0);
					if (mScheds[0].mPrint)
						std::cout << "-------------- p0 suspend --------------" << std::endl;
				}

				if (mScheds[1].done() == false)
				{
					mScheds[1].runRound();


					if (mScheds[1].done())
					{
						auto e1 = p1.mBase->getErrorCode();
						if (e1)
							return e1;
					}

					sendMsgs(1);

					if (mScheds[0].mPrint)
						std::cout << "-------------- p1 suspend --------------" << std::endl;
				}
			}

			//std::cout << mScheds[0].getDot() << std::endl;
			//std::cout << mScheds[1].getDot() << std::endl;

			return {};
		}
	};

}