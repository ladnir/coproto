#include "NativeProto.h"
#include "coproto/LocalEvaluator.h"

namespace coproto
{

#define CP_SEND(X)					\
	do{								\
		{auto ec = send(X);			\
		if (ec)						\
		{							\
			mState = __LINE__;		\
			return ec;				\
		}}							\
	 [[fallthrough]];case __LINE__:do{}while(0);	}while(0)		

#define CP_RECV(X)					\
	do{								\
		{auto ec = recv(X);			\
		if (ec)						\
		{							\
			mState = __LINE__;		\
			return ec;				\
		}}							\
	 [[fallthrough]];case __LINE__:do{}while(0);	}while(0)		


#define CP_RECV_EC(res, X)			\
		{auto ec = recv(X);			\
		if (ec)						\
		{							\
			mState = __LINE__;		\
			return ec;				\
		}}							\
	 [[fallthrough]];case __LINE__:	\
	 res = getAwaitResult()	

#define CP_AWAIT(X)					\
	do{								\
		{auto ec = await(X);		\
		if (ec)						\
		{							\
			mState = __LINE__;		\
			return ec;				\
		}}							\
	 [[fallthrough]];case __LINE__:do{}while(0);	}while(0)				

#define CP_END_OF_ROUND()			\
	do{								\
		{auto ec = endOfRound();	\
		if (ec)						\
		{							\
			mState = __LINE__;		\
			return ec;				\
		}}							\
	 [[fallthrough]];case __LINE__:do{}while(0);	}while(0)				

#define CP_BEGIN()					\
	switch(mState)					\
	{								\
	case 0:							\
		do {}while(0)

#define CP_END()					\
		break;						\
	default:						\
		break;						\
	}								\
	do {}while(0)

	namespace tests
	{

		namespace {
			auto types = { LocalEvaluator::interlace, LocalEvaluator::blocking, LocalEvaluator::async, LocalEvaluator::asyncThread };
		}

		void Native_StrSendRecv_Test()
		{
			struct StrProto : public NativeProto
			{
				bool party;
				u64 i;
				std::string str;

				StrProto(bool p)
					:party(p)
				{}

				error_code resume() override
				{
					CP_BEGIN();

					str = ("hello from 0");

					for (i = 0; i < 5; ++i)
					{
						if (party)
						{
							CP_SEND(std::move(str));



							CP_END_OF_ROUND();

							CP_RECV_EC(auto ec, str);
							if(ec)
								throw std::runtime_error(COPROTO_LOCATION);

							if (str != "hello from " + std::to_string(i * 2 + 1))
								throw std::runtime_error(COPROTO_LOCATION);
							str.back() += 1;

						}
						else
						{
							str = {};
							CP_RECV(str);


							if (str != "hello from " + std::to_string(i * 2 + 0))
								throw std::runtime_error(COPROTO_LOCATION);

							str.back() += 1;
							CP_SEND(str);

							CP_END_OF_ROUND();
						}
					}

					CP_END();

					return {};
				}

			};



			for (auto t : types)
			{

				auto p0 = makeProto<StrProto>(0);
				auto p1 = makeProto<StrProto>(1);
				LocalEvaluator sched;
				auto ec = sched.execute(p0, p1, t);
				//std::cout << sched.mScheds[0].getDot() << std::endl;
				//std::cout << sched.mScheds[1].getDot() << std::endl;

				if (ec)
					throw std::runtime_error(ec.message());

				if (LocalEvaluator::interlace)
				{
					if (p0.mBase.mSched->numRounds() != 6)
						throw std::runtime_error("num round");
					if (p1.mBase.mSched->numRounds() != 6)
						throw std::runtime_error("num round");
				}
			}
		}

		Proto nEchoServer(u64 n, u64 len, u64 rep)
		{
			struct Server : coproto::NativeProto
			{
				u64 n;
				u64 len;
				u64 rep;
				std::vector<u8> buff;
				u64 i;

				Server(u64 nn, u64 ll, u64 rr)
					: n(nn)
					, len(ll)
					, rep(rr)
				{}

				error_code resume()
				{
					CP_BEGIN();

					for (i = 0; i < rep; ++i)
					{
						CP_RECV(buff);
					}

					for (i = 0; i < rep; ++i)
						CP_SEND(buff);

					if (n)
					{
						CP_AWAIT(nEchoServer(n - 1, len, rep));
					}


					CP_END();
					return {};
				}
			};

			return makeProto<Server>(n, len, rep);
		}

		Proto nEchoClient(u64 n, u64 len, u64 rep)
		{
			struct Client : coproto::NativeProto
			{
				u64 n;
				u64 len;
				u64 rep;
				std::vector<u8> buff;
				u64 i;

				Client(u64 nn, u64 ll, u64 rr)
					: n(nn)
					, len(ll)
					, rep(rr)
				{}

				error_code resume()
				{
					CP_BEGIN();

					buff.resize(len);

					for (i = 0; i < rep; ++i)
						CP_SEND(buff);


					for (i = 0; i < rep; ++i)
						CP_RECV(buff);

					if (n)
					{
						CP_AWAIT(nEchoClient(n - 1, len, rep));
					}

					CP_END();

					return {};
				}
			};

			return makeProto<Client>(n, len, rep);
		}

		void Native_NestedSendRecv_Test()
		{
			u64 n = 4, len = 10, rep = 4;

			for (auto t : types)
			{
				auto p0 = nEchoClient(n, len, rep);
				auto p1 = nEchoServer(n, len, rep);
				LocalEvaluator sched;
				auto ec = sched.execute(p0, p1, t);
				if (ec)
					throw std::runtime_error(ec.message());
			}
		}


		void Native_ZeroSendRecv_Test()
		{
			struct PP : public NativeProto
			{
				u64 mState = 0;
				std::vector<u64> buff;

				error_code resume() override
				{
					CP_BEGIN();
					CP_SEND(buff);

					CP_END();
					return {};
				}
			};


			for (auto t : types)
			{
				auto p0 = makeProto<PP>();
				auto p1 = makeProto<PP>();
				LocalEvaluator sched;
				auto ec = sched.execute(p0, p1, t);
				if (!ec)
					throw std::runtime_error("");
			}
		}

		void Native_ZeroSend_ErrorCode_Test()
		{
			struct PP : public NativeProto
			{
				u64 mState = 0;
				std::vector<u64> buff;
				bool failed = false;
				error_code resume() override
				{
					CP_BEGIN();
					CP_SEND_EC(auto ec, buff);

					if (!ec)
						failed = true;

					CP_END();
					return {};
				}
			};


			for (auto t : types)
			{
				auto p0 = makeProto<PP>();
				auto p1 = makeProto<PP>();
				LocalEvaluator sched;
				auto ec = sched.execute(p0, p1, t);
				if (!ec)
					throw std::runtime_error("");
			}
		}


	}



}