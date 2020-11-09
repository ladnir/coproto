
#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include "Proto.h"
#include <algorithm>
#include <numeric>
#include <string>

#include "Buffers.h"
#include "LocalExecutor.h"

namespace coproto
{












	namespace tests
	{

		auto types = { LocalExecutor::interlace, LocalExecutor::blocking, LocalExecutor::async, LocalExecutor::asyncThread };


		Proto<int> echoServer(u64 i, u64 length, u64 rep, std::string name, bool v)
		{
#ifdef COPROTO_LOGGING
			auto np = name + "_server_" + std::to_string(i) + "_" + std::to_string(length);
			co_await Name(np);
#endif

			auto exp = std::vector<char>(length);
			std::iota(exp.begin(), exp.end(), 0);

			if (v)
				std::cout << name << " s start " << i << " " << length << std::endl;

			if (v)
				std::cout << name << " s recv " << i << " " << length << " begin" << std::endl;
			auto msg = std::vector<char>();

			for (u64 i = 0; i < rep; ++i)
			{
				auto r = recv<std::vector<char>>();
#ifdef COPROTO_LOGGING
				r.setName(np + "_r" + std::to_string(i));
#endif
				msg = co_await std::move(r);
				//msg = co_await recv<std::vector<char>>();
				if (exp != msg)
				{
					std::cout << "bad msg " << COPROTO_LOCATION << std::endl;
					throw std::runtime_error("");
				}
			}

			if (v)
				std::cout << name << " s recv " << i << " " << length << " done" << std::endl;


			if (v)
				std::cout << name << " s send " << i << " " << length << std::endl;
			for (u64 i = 0; i < rep; ++i)
			{
				auto s = send(msg);
#ifdef COPROTO_LOGGING
				s.setName(np + "_s" + std::to_string(i));
#endif
				co_await std::move(s);
			}

			co_await EndOfRound();
			if (i)
			{
				co_return co_await echoServer(i - 1, length, rep, name, v);
			}
			else
			{
				if (v)
					std::cout << name << " ###################### s done " << i << " " << length << std::endl;
				co_return 0;
			}
		}
		Proto<int> echoClient(u64 i, u64 length, u64 rep, std::string name, bool v)
		{
#ifdef COPROTO_LOGGING
			auto np = name + "_client_" + std::to_string(i) + "_" + std::to_string(length);
			co_await Name(np);
#endif
			if (v)
				std::cout << name << " c start " << i << " " << length << std::endl;

			auto msg = std::vector<char>(length);
			std::iota(msg.begin(), msg.end(), 0);
			if (v)
				std::cout << name << " c send " << i << " " << length << std::endl;
			for (u64 i = 0; i < rep; ++i)
			{
				auto s = send(msg);
#ifdef COPROTO_LOGGING
				s.setName(np + "_s" + std::to_string(i));
#endif
				co_await std::move(s);
			}
			co_await EndOfRound();

			if (v)
				std::cout << name << " c recv " << i << " " << length << " begin" << std::endl;

			for (u64 i = 0; i < rep; ++i)
			{

				auto r = recv<std::vector<char>>();
#ifdef COPROTO_LOGGING
				r.setName(np + "_r" + std::to_string(i));
#endif
				auto msg2 = co_await std::move(r);
				//auto msg2 = co_await recv<std::vector<char>>();
				if (msg2 != msg)
				{
					std::cout << "bad msg " << COPROTO_LOCATION << std::endl;
					throw std::runtime_error("");
				}
			}
			if (v)
				std::cout << name << " c recv " << i << " " << length << " done" << std::endl;

			if (i)
			{
				co_return co_await echoClient(i - 1, length, rep, name, v);
			}
			else
			{

				if (v)
					std::cout << name << " ###################### c done " << i << " " << length << std::endl;
				co_return 0;
			}
		}




		void strSendRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {
				std::string str("hello from 0");

				for (u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						co_await send(str);
						//std::cout << " p1 sent" << std::endl;

						co_await EndOfRound();
						//std::cout << " p1 EOR" << std::endl;

						str = co_await recv<std::string>();
						//std::cout << " p1 recv" << std::endl;

						if (str != "hello from " + std::to_string(i * 2 + 1))
							throw std::runtime_error(COPROTO_LOCATION);
						str.back() += 1;
					}
					else
					{
						co_await recv(str);
						//std::cout << " p0 recv" << std::endl;

						if (str != "hello from " + std::to_string(i * 2 + 0))
							throw std::runtime_error(COPROTO_LOCATION);

						str.back() += 1;
						co_await send(str);
						//std::cout << " p0 send" << std::endl;


						co_await EndOfRound();
						//std::cout << " p0 EOR" << std::endl;

					}
				}


				//std::cout << " p0 done" << std::endl;

			};



			for (auto t : types)
			{

				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);
				//std::cout << sched.mScheds[0].getDot() << std::endl;
				//std::cout << sched.mScheds[1].getDot() << std::endl;

				if (ec)
					throw std::runtime_error(ec.message());

				if (LocalExecutor::interlace)
				{
					if (sched.mScheds[0].numRounds() != 6)
						throw std::runtime_error("num round");
					if (sched.mScheds[1].numRounds() != 6)
						throw std::runtime_error("num round");
				}
			}
		}


		void resultSendRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {
				std::string str("hello from 0");
				//co_await Name("main");

				for (u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						//std::cout << " p1 sent " << i << std::endl;
						auto ec = co_await send(str).wrap();
						//std::cout << " p1 sent " << i << " ok" << std::endl;

						//std::cout << " p1 recv " << i << std::endl;
						Result<std::string> r = co_await recv<std::string>().wrap();
						//std::cout << " p1 recv " << i << " ok " << std::endl;

						if (r.hasError())
							throw std::runtime_error(COPROTO_LOCATION);

						str = r.value();

						if (str != "hello from " + std::to_string(i * 2 + 1))
							throw std::runtime_error(COPROTO_LOCATION);
						str.back() += 1;
					}
					else
					{
						//std::cout << " p0 recv " << i << std::endl;
						co_await recv(str);
						//std::cout << " p0 recv " << i << " ok" << std::endl;

						if (str != "hello from " + std::to_string(i * 2 + 0))
							throw std::runtime_error(COPROTO_LOCATION);

						str.back() += 1;

						//std::cout << " p0 sent " << i << std::endl;
						co_await send(str);
						//std::cout << " p0 sent " << i << " ok" << std::endl;

					}
				}
			};


			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);

				if (ec)
					throw std::runtime_error(ec.message());
			}
		}


		void returnValueTest()
		{
			int val = 42;
			auto proto = [val](bool party) -> Proto<int> {
				std::string str("hello from 0");
				co_return val;
			};


			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);

				if (ec)
					throw std::runtime_error(ec.message());

				if (*(int*)p0.mBase->getValue() != val)
					throw std::runtime_error("");
				if (*(int*)p1.mBase->getValue() != val)
					throw std::runtime_error("");
			}
		}


		void typedRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff, rBuff;
				for (u64 i = 0; i < 5; ++i)
				{
					if (party)
					{
						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);
						co_await send(std::move(buff));
						rBuff = co_await recv<std::vector<u64>>();

						buff.resize(2 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);

						if (buff != rBuff)
							throw std::runtime_error(COPROTO_LOCATION);
					}
					else
					{
						rBuff = co_await recvVec<u64>();

						buff.resize(1 + i * 2);
						std::fill(buff.begin(), buff.end(), i * 2);

						if (buff != rBuff)
							throw std::runtime_error(COPROTO_LOCATION);

						buff.resize(buff.size() + 1);
						std::fill(buff.begin(), buff.end(), i * 2 + 1);
						co_await send(std::move(buff));
					}
				}
			};


			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);

				if (ec)
					throw std::runtime_error(ec.message());
			}
		}



		void zeroSendRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff;
				co_await send(buff);
			};


			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);
				if (!ec)
					throw std::runtime_error("");
			}
		}


		void badRecvSizeTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff(3);

				if (party)
				{
					co_await send(buff);
				}
				else
				{
					buff.resize(1);
					co_await recvFixedSize(buff);
				}
			};
			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);
				if (ec != code::badBufferSize)
					throw std::runtime_error(ec.message());
			}
		}


		void zeroSendErrorCodeTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff;
				auto ec = co_await send(buff).wrap();

				if (ec != code::sendLengthZeroMsg)
					throw std::runtime_error("");
			};
			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);
				if (ec)
					throw std::runtime_error("");
			}
		}


		void badRecvSizeErrorCodeTest()
		{
			auto proto = [](bool party) -> Proto<> {

				std::vector<u64> buff(3);

				if (party)
				{
					co_await send(buff).wrap();
				}
				else
				{
					buff.resize(1);
					auto ec = co_await recvFixedSize(buff).wrap();

					if (ec != code::badBufferSize)
						throw std::runtime_error("");
				}
			};

			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);
				if (ec)
					throw std::runtime_error(ec.message());
			}
		}

		void throwsTest()
		{
			auto proto = [](bool party) -> Proto<> {

				if (party)
					throw std::runtime_error("");
				else
				{
					throw std::runtime_error("");
					//co_await recvVec<char>();
				}
				co_return;
			};

			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);
				if (!ec)
					throw std::runtime_error("");
			}
		}

		void nestedSendRecvTest()
		{
			auto proto = [](bool party) -> Proto<> {
				std::string str("hello from 0");
				u64 n = 5;
				if (party)
				{
					//std::cout << "p1 send " << std::endl;
					auto ec = co_await send(std::move(str)).wrap();
					if (ec)
						throw std::runtime_error(COPROTO_LOCATION);

					co_await echoServer(n, 10, 1, "p1", false);
				}
				else
				{
					//std::cout << "p0 recv " << std::endl;
					co_await recv(str);
					//std::cout << " p0 recv" << std::endl;

					if (str != "hello from 0")
						throw std::runtime_error(COPROTO_LOCATION);

					co_await echoClient(n, 10, 1, "p0", false);
					//std::cout << " p0 sent" << std::endl;

				}
			};
			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);
				if (ec)
					throw std::runtime_error(ec.message());
			}
		}


		Proto<> throwServer(u64 i)
		{
			auto msg = co_await recv<std::string>();
			co_await send((msg));

			if (i)
				co_await throwServer(i - 1);
			else
				throw std::runtime_error("");
		}

		Proto<> throwClient(u64 i)
		{
			auto msg = std::string("hello world");
			co_await send(msg);
			if (msg != co_await recv<std::string>())
			{
				throw std::runtime_error("hello world");
			}

			if (i)
				co_await throwClient(i - 1);
		}

		void nestedProtocolThrowTest()
		{

			auto proto = [](bool party) -> Proto<> {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);
					co_await throwServer(4);
				}
				else
				{
					std::vector<u64> buff(10);
					co_await recv(buff);
					co_await throwClient(4);
				}
			};

			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);
				if (!ec)
					throw std::runtime_error("");
			}
		}


		void nestedProtocolErrorCodeTest()
		{
			bool hasEc = false;
			u64 n = 5;
			auto proto = [&hasEc, n](bool party) -> Proto<> {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);
					auto ec = co_await throwServer(n).wrap();

					if (ec == code::uncaughtException)
						hasEc = true;
				}
				else
				{
					std::vector<u64> buff(10);
					co_await recv(buff);
					co_await throwClient(n);
				}
			};


			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);
				if (ec || !hasEc)
					throw std::runtime_error("");
			}
		}


		void asyncProtocolTest()
		{

			int i = 0;


#define MULTI
			bool print = false;
			u64 n = 40;
			u64 rep = 8;
			auto proto = [n, print, rep](bool party) -> Proto<> {

				if (party)
				{
					auto name = std::string("p1");
					co_await Name(name);
					std::vector<u64> buff(10);



					co_await recv(buff);
					co_await EndOfRound();

					auto fu0 = co_await echoServer(n, 5, rep, name, print).async();

#ifdef MULTI
					auto fu1 = co_await echoServer(n + 2, 6, rep, name, print).async();
					auto fu2 = co_await echoServer(n, 7, rep, name, print).async();
					auto fu3 = co_await echoServer(n + 7, 8, rep, name, print).async();
					auto fu4 = co_await echoServer(n, 9, rep, name, print).async();
#endif

					co_await echoClient(n, 10, rep, name, print);
					//co_await send(buff);

					co_await fu0;
#ifdef MULTI
					co_await fu1;
					co_await fu2;
					co_await fu3;
					co_await fu4;
#endif
				}
				else
				{
					auto name = std::string("p0");
					co_await Name(name);
					std::vector<u64> buff(10);
					co_await send(buff);
					//co_await recv(buff);
					auto fu0 = co_await echoClient(n, 5, rep, name, print).async();
#ifdef MULTI
					auto fu1 = co_await echoClient(n + 2, 6, rep, name, print).async();
					auto fu2 = co_await echoClient(n, 7, rep, name, print).async();
					auto fu3 = co_await echoClient(n + 7, 8, rep, name, print).async();
					auto fu4 = co_await echoClient(n, 9, rep, name, print).async();
#endif
					co_await echoServer(n, 10, rep, name, print);
					//co_await recv(buff);

					co_await fu0;
#ifdef MULTI
					co_await fu1;
					co_await fu2;
					co_await fu3;
					co_await fu4;
#endif
				}
			};
#ifdef MULTI 
#undef MULTI
#endif

			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				//sched.mScheds[0].mPrint = true;
				//sched.mScheds[1].mPrint = true;

				auto ec = sched.execute(p0, p1, t);


				//std::cout << sched.mScheds[0].getDot() << std::endl;
				//std::cout << sched.mScheds[1].getDot() << std::endl;
				if (ec)
					throw std::runtime_error(ec.message());

				auto r0 = sched.mScheds[0].numRounds();
				auto r1 = sched.mScheds[1].numRounds();
			}
			//if (r0 != n + 1)
			//	throw std::runtime_error("num round");
			//if (r1 != n)
			//	throw std::runtime_error("num round");

		}

		void asyncThrowProtocolTest()
		{
			u64 n = 3;
			auto proto = [n](bool party) -> Proto<> {

				if (party)
				{
					std::vector<u64> buff(10);
					co_await send(buff);

					auto token = co_await throwServer(n).async();
					//co_await send(buff);

					co_await token;
				}
				else
				{
					std::vector<u64> buff(10);
					co_await recv(buff);
					auto fu = co_await throwClient(n).async();
					//co_await recv(buff);

					co_await fu;
				}

			};


			for (auto t : types)
			{
				auto p0 = proto(0);
				auto p1 = proto(1);
				LocalExecutor sched;
				//sched.mScheds[0].mPrint = true;
				//sched.mScheds[1].mPrint = true;
				auto ec = sched.execute(p0, p1, t);
				if (ec != code::uncaughtException)
					throw std::runtime_error("");
			}
		}


		void endOfRoundTest()
		{

			auto recvProto = [&]() -> Proto<> {
				std::vector<u8> msg;
				co_await recv(msg);
				co_await EndOfRound();
				co_await send(msg);
			};
			auto sendProto = [&]() -> Proto<> {
				std::vector<u8> msg(10);
				co_await send(msg);
				co_await EndOfRound();
				co_await recv(msg);
			};

			auto recvProto2 = [&]() -> Proto<> {
				std::vector<u8> msg(10);
				co_await recvProto();
				co_await send(msg);
			};
			auto sendProto2 = [&]() -> Proto<> {
				std::vector<u8> msg;
				co_await sendProto();
				co_await recv(msg);
			};


			for (auto t : types)
			{
				auto p0 = sendProto2();
				auto p1 = recvProto2();
				LocalExecutor sched;
				auto ec = sched.execute(p0, p1, t);


				if (t == LocalExecutor::interlace)
				{
					if (sched.mScheds[0].numRounds() != 2)
						throw std::runtime_error("num round");
					if (sched.mScheds[1].numRounds() != 1)
						throw std::runtime_error("num round");
				}
				if (ec)
					throw std::runtime_error(ec.message());
			}
		}


//		struct ErrorSocketEvaluator
//		{
//
//
//			struct Sock : public Socket
//			{
//				bool mCanceled = false;
//				std::list<std::vector<u8>> mInbound, mOutbound;
//				ErrorSocketEvaluator* mEval = nullptr;
//				error_code recv(span<u8> data)
//				{
//					assert(mEval);
//
//					if (mCanceled)
//						return code::ioError;
//
//					auto ec = mEval->getError();
//					if (ec)
//						return ec;
//
//
//					if (mInbound.size())
//					{
//						auto& front = mInbound.front();
//						assert(data.size() == front.size());
//
//						std::memcpy(data.data(), front.data(), data.size());
//						mInbound.pop_front();
//					}
//					else
//					{
//						ec = code::suspend;
//					}
//
//					return ec;
//				}
//				error_code send(span<u8> data)
//				{
//					assert(mEval);
//
//					if (mCanceled)
//						return code::ioError;
//
//					auto ec = mEval->getError();
//					if (ec)
//						return ec;
//
//					mOutbound.emplace_back(data.begin(), data.end());
//					return {};
//				}
//
//
//				void cancel() override
//				{
//					mCanceled = true;
//					mInbound.clear();
//					mOutbound.clear();
//				}
//			};
//
//			void sendMsgs(u64 sender)
//			{
//
//				if (mSocks[sender].mCanceled)
//					mSocks[sender ^ 1].cancel();
//				else
//				{
//					assert(mSocks[sender ^ 1].mInbound.size() == 0);
//					mSocks[sender ^ 1].mInbound = std::move(mSocks[sender].mOutbound);
//				}
//			}
//
//
//			u64 mCurIdx = 0, mErrorIdx = ~0ull;
//			error_code getError()
//			{
//				if (mCurIdx++ == mErrorIdx)
//					return code::ioError;
//				else
//					return {};
//			}
//
//			std::array<Sock, 2> mSocks;
//			std::array<Scheduler, 2> mScheds;
//			error_code execute(Resumable& p0, Resumable& p1, u64 i)
//			{
//#ifdef COPROTO_LOGGING
//				if (p0.mName.size() == 0)
//					p0.setName("main");
//				if (p1.mName.size() == 0)
//					p1.setName("main");
//#endif
//
//				mCurIdx = 0;
//				mErrorIdx = i;
//
//				p0.mSlotIdx = 0;
//				p1.mSlotIdx = 0;
//				mScheds[0].scheduleReady(p0);
//				mScheds[1].scheduleReady(p1);
//				mScheds[0].mRoundIdx = 0;
//				mScheds[1].mRoundIdx = 0;
//
//				mSocks[0].cancel();
//				mSocks[1].cancel();
//				mSocks[0].mCanceled = false;
//				mSocks[1].mCanceled = false;
//
//				mScheds[0].mSock = &mSocks[0];
//				mScheds[1].mSock = &mSocks[1];
//				mSocks[0].mEval = this;
//				mSocks[1].mEval = this;
//
//				while (
//					mScheds[0].done() == false ||
//					mScheds[1].done() == false)
//				{
//
//					if (mScheds[0].done() == false)
//					{
//						mScheds[0].run();
//						if (mScheds[0].done())
//						{
//							auto e0 = p0.getErrorCode();
//							if (e0)
//								return e0;
//						}
//
//						sendMsgs(0);
//					}
//
//					if (mScheds[1].done() == false)
//					{
//						mScheds[1].run();
//						if (mScheds[1].done())
//						{
//							auto e1 = p1.getErrorCode();
//							if (e1)
//								return e1;
//						}
//
//						sendMsgs(1);
//					}
//				}
//
//				if (p0.getErrorCode())
//					return p0.getErrorCode();
//				if (p1.getErrorCode())
//					return p1.getErrorCode();
//				return {};
//			}
//
//		};


		void errorSocketTest()
		{
#define MULTI
			bool print = false;
			u64 n = 3;
			u64 rep = 2;
			auto proto = [n, print, rep](bool party) -> Proto<> {

				if (party)
				{
					auto name = std::string("p1");
					co_await Name(name);
					std::vector<u64> buff(10);



					co_await recv(buff);
					co_await EndOfRound();

					auto fu0 = co_await echoServer(n, 5, rep, name, print).async();

#ifdef MULTI
					auto fu1 = co_await echoServer(n + 2, 6, rep, name, print).async();
					auto fu2 = co_await echoServer(n, 7, rep, name, print).async();
					auto fu3 = co_await echoServer(n + 7, 8, rep, name, print).async();
					auto fu4 = co_await echoServer(n, 9, rep, name, print).async();
#endif

					co_await echoClient(n, 10, rep, name, print);
					//co_await send(buff);

					co_await fu0;
#ifdef MULTI
					co_await fu1;
					co_await fu2;
					co_await fu3;
					co_await fu4;
#endif
				}
				else
				{
					auto name = std::string("p0");
					co_await Name(name);
					std::vector<u64> buff(10);
					co_await send(buff);
					//co_await recv(buff);
					auto fu0 = co_await echoClient(n, 5, rep, name, print).async();
#ifdef MULTI
					auto fu1 = co_await echoClient(n + 2, 6, rep, name, print).async();
					auto fu2 = co_await echoClient(n, 7, rep, name, print).async();
					auto fu3 = co_await echoClient(n + 7, 8, rep, name, print).async();
					auto fu4 = co_await echoClient(n, 9, rep, name, print).async();
#endif
					co_await echoServer(n, 10, rep, name, print);
					//co_await recv(buff);

					co_await fu0;
#ifdef MULTI
					co_await fu1;
					co_await fu2;
					co_await fu3;
					co_await fu4;
#endif
				}
			};
#ifdef MULTI 
#undef MULTI
#endif

			for (auto type : { LocalExecutor::interlace, LocalExecutor::async })
			{

				LocalExecutor eval;
				auto p0 = proto(0);
				auto p1 = proto(1);
				auto ec = eval.execute(p0, p1, type);
				auto numOps = eval.mOpIdx;

				if (ec)
					throw std::runtime_error(ec.message());

				for (u64 i = 0; i < numOps; ++i)
				{
					auto p0 = proto(0);
					auto p1 = proto(1);

					LocalExecutor eval;
					eval.mErrorIdx = i;
					auto ec = eval.execute(p0, p1, type);


					if (ec != code::ioError)
						throw std::runtime_error("error was expected");
				}

				
			}
		}
	}
}