#include "InlinePoly.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <array>

namespace coproto
{
	std::string hexPtr(void* p)
	{
		std::stringstream ss;
		ss << std::hex << u64(p);
		return ss.str();
	}


	namespace tests
	{

		struct Log
		{
			enum Op
			{
				ConstructBase,
				ConstructSmall,
				ConstructMoveSmall,
				ConstructLarge,
				ConstructMoveLarge,
				DestructBase,
				DestructSmall,
				DestructLarge
			};

			std::vector<Op> mOps;
		};

		namespace {
			struct Base
			{
				Log& mLog;
				Base(Log& l)
					:mLog(l)
				{
					mLog.mOps.push_back(Log::ConstructBase);
					//std::cout << "Base() " << hexPtr(this) << std::endl;
				}

				//Base(Base&& o)
				//{
				//	std::cout << "Base(Base&&) " << hexPtr(this) << " from " <<hexPtr(&o) << std::endl;

				//}

				virtual ~Base()
				{
					mLog.mOps.push_back(Log::DestructBase);
					//std::cout << "~Base() " << hexPtr(this) << std::endl;

				}
			};

			struct Small : public Base
			{
				Small(Log& l)
					: Base(l)
				{
					mLog.mOps.push_back(Log::ConstructSmall);
					//std::cout << "Small() " << hexPtr(this) << std::endl;
				}

				Small(Small&& o)
					:Base(o.mLog)
				{
					mLog.mOps.push_back(Log::ConstructMoveSmall);

					//std::cout << "Small(Small&&) " << hexPtr(this) << " from " << hexPtr(&o) << std::endl;

				}

				~Small()
				{
					mLog.mOps.push_back(Log::DestructSmall);

					//std::cout << "~Small() " << hexPtr(this) << std::endl;

				}
			};



			struct Large : public Base
			{
				Large(Log& l)
					: Base(l)
				{
					mLog.mOps.push_back(Log::ConstructLarge);
					//std::cout << "Large() " << hexPtr(this) << std::endl;
				}

				Large(Large&& o)
					:Base(o.mLog)
				{
					mLog.mOps.push_back(Log::ConstructMoveLarge);
					//std::cout << "Large(Large&&) " << hexPtr(this) << " from " << hexPtr(&o) << std::endl;

				}

				~Large()
				{
					mLog.mOps.push_back(Log::DestructLarge);
					//std::cout << "~Large() " << hexPtr(this) << std::endl;

				}

				std::array<u8, 512> _;
			};
		}


		void InlinePolyTest()
		{

			//Small small;
			//Large large;
			//Base& bb = small;
			//Base& cc = large;
			Log log;
			int i = 0;
			{

				internal::InlinePoly<Base, 256> a;
				a.emplace<Small>(log);

				if (a.isStoredInline() == false)
					throw std::runtime_error("");

				if (log.mOps.size() != 2)
					throw std::runtime_error("");
				if (log.mOps[i++] != Log::ConstructBase)
					throw std::runtime_error("");
				if (log.mOps[i++] != Log::ConstructSmall)
					throw std::runtime_error("");

				internal::InlinePoly<Base, 256> b;

				b = std::move(a);

				if (log.mOps.size() != 6)
					throw std::runtime_error("");
				if (log.mOps[i++] != Log::ConstructBase)
					throw std::runtime_error("");
				if (log.mOps[i++] != Log::ConstructMoveSmall)
					throw std::runtime_error("");
				if (log.mOps[i++] != Log::DestructSmall)
					throw std::runtime_error("");
				if (log.mOps[i++] != Log::DestructBase)
					throw std::runtime_error("");


				if (b.isStoredInline() == false)
					throw std::runtime_error("");

				internal::InlinePoly<Base, 256> c;
				c.emplace<Large>(log);

				if (log.mOps.size() != 8)
					throw std::runtime_error("");
				if (log.mOps[i++] != Log::ConstructBase)
					throw std::runtime_error("");
				if (log.mOps[i++] != Log::ConstructLarge)
					throw std::runtime_error("");

				if (c.isStoredInline())
					throw std::runtime_error("");

				a = std::move(c);
				if (log.mOps.size() != 8)
					throw std::runtime_error("");

				if (a.isStoredInline())
					throw std::runtime_error("");

				c.emplace<Small>(log);


				if (log.mOps.size() != 10)
					throw std::runtime_error("");

				if (log.mOps[i++] != Log::ConstructBase)
					throw std::runtime_error("");
				if (log.mOps[i++] != Log::ConstructSmall)
					throw std::runtime_error("");
			}

			if (log.mOps.size() != 16)
				throw std::runtime_error("");

			// c
			if (log.mOps[i++] != Log::DestructSmall)
				throw std::runtime_error("");
			if (log.mOps[i++] != Log::DestructBase)
				throw std::runtime_error("");

			// b
			if (log.mOps[i++] != Log::DestructSmall)
				throw std::runtime_error("");
			if (log.mOps[i++] != Log::DestructBase)
				throw std::runtime_error("");

			// a 
			if (log.mOps[i++] != Log::DestructLarge)
				throw std::runtime_error("");
			if (log.mOps[i++] != Log::DestructBase)
				throw std::runtime_error("");
			if (i != 16)
				throw std::runtime_error("");
		}
	}

}