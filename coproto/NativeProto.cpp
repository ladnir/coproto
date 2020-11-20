#include "NativeProto.h"
#include "coproto/LocalEvaluator.h"
#include "coproto/Macros.h"
#include "coproto/Socket.h"
namespace coproto
{

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
				error_code ec;
				Socket& s;

				StrProto(bool p, Socket& ss)
					:party(p)
					,s(ss)
				{}

				error_code resume() override
				{
					CP_BEGIN();

					str = ("hello from 0");

					for (i = 0; i < 5; ++i)
					{
						if (party)
						{
							CP_AWAIT(s.send(str));

							CP_END_OF_ROUND();

							CP_RECV_EC(ec, s, str);

							if (ec)
								throw std::runtime_error(COPROTO_LOCATION);

							if (str != "hello from " + std::to_string(i * 2 + 1))
								throw std::runtime_error(COPROTO_LOCATION);
							str.back() += 1;

						}
						else
						{
							//str = {};
							CP_RECV(s, str);
							if (str != "hello from " + std::to_string(i * 2 + 0))
								throw std::runtime_error(COPROTO_LOCATION);

							str.back() += 1;
							CP_SEND(s, str);

							CP_END_OF_ROUND();
						}
					}
					CP_END();
					return {};
				}
			};

			for (auto t : types)
			{

				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);
				auto p0 = makeProto<StrProto>(0, s[0]);
				auto p1 = makeProto<StrProto>(1, s[1]);
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
		void Native_ZeroSendRecv_Test()
		{
			struct PP : public NativeProto
			{
				std::vector<u64> buff;
				Socket& s;

				PP(Socket& ss) : s(ss) {}

				error_code resume() override
				{
					CP_BEGIN();
					CP_SEND(s, buff);

					CP_END();
					return {};
				}
			};


			for (auto t : types)
			{
				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);
				auto p0 = makeProto<PP>(s[0]);
				auto p1 = makeProto<PP>(s[1]);
				auto ec = sched.execute(p0, p1, t);
				if (!ec)
					throw std::runtime_error("");
			}
		}

		void Native_ZeroSend_ErrorCode_Test()
		{
			struct PP : public NativeProto
			{
				std::vector<u64> buff;
				bool& failed;
				Socket& s;
				PP(bool& f, Socket& ss)
					:failed(f)
					, s(ss)
				{
					failed = true;
				}

				error_code resume() override
				{
					error_code ec;
					CP_BEGIN();


					CP_SEND_EC(ec,s, buff);

					if (ec == code::sendLengthZeroMsg)
						failed = false;
					else
						failed = true;


					CP_END();
					return ec;
				}
			};


			bool f0, f1;

			for (auto t : types)
			{
				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);
				auto p0 = makeProto<PP>(f0, s[0]);
				auto p1 = makeProto<PP>(f1, s[1]);
				auto ec = sched.execute(p0, p1, t);
				if (!ec || f0 || f1)
					throw std::runtime_error("");
			}
		}

		void Native_returnValue_Test()
		{
			struct Ret : public NativeProtoV<int>
			{
				int ret;

				Ret(int r)
					:ret(r)
				{}

				error_code resume() override
				{

					CP_RETURN(ret);
				}
			};


			struct PP : public NativeProto
			{
				int i;
				error_code resume() override
				{
					CP_BEGIN();
					{

						CP_AWAIT_VAL(i, makeProto<Ret>(42));

						if (i != 42)
							throw std::runtime_error("");
					}
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
				if (ec)
					throw std::runtime_error(ec.message());
			}
		}

		void Native_BadRecvSize_Test()
		{
			struct PP : public NativeProto
			{

				std::vector<u8> buff;
				span<u8> sp;
				bool party;
				Socket& s;

				PP(bool p, Socket& ss)
					:party(p)
					,s(ss)
				{}

				error_code resume() override
				{
					CP_BEGIN();
					if (party)
					{
						buff.resize(3);
						CP_SEND(s, buff);
					}
					else
					{
						buff.resize(1);
						sp = span<u8>(buff);
						CP_RECV(s, sp);
					}
					CP_END();
					return {};
				}
			};

			for (auto t : types)
			{
				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);
				auto p0 = makeProto<PP>(0, s[0]);
				auto p1 = makeProto<PP>(1, s[1]);
				auto ec = sched.execute(p0, p1, t);
				if (ec != code::badBufferSize)
					throw std::runtime_error(ec.message());
			}
		}

		void Native_BadRecvSize_ErrorCode_Test()
		{
			struct PP : public NativeProto
			{

				std::vector<u8> buff;
				span<u8> sp;
				error_code ec;
				bool party;
				bool& failed;
				Socket& s;
				PP(bool p, bool& f,  Socket& ss)
					:party(p)
					, failed(f)
					, s(ss)
				{
					failed = true;
				}

				error_code resume() override
				{
					CP_BEGIN();
					if (party)
					{
						buff.resize(3);
						CP_SEND(s, buff);
					}
					else
					{
						buff.resize(1);
						sp = span<u8>(buff);
						CP_RECV_EC(ec,s, sp);

						if (ec == code::badBufferSize)
							failed = false;
					}
					CP_END();
					return ec;
				}
			};


			bool f0, f1;
			for (auto t : types)
			{
				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);
				auto p0 = makeProto<PP>(0, f0, s[0]);
				auto p1 = makeProto<PP>(1, f1, s[1]);
				auto ec = sched.execute(p0, p1, t);
				if (ec != code::badBufferSize || f0)
					throw std::runtime_error(ec.message());
			}
		}




		Proto nEchoServer(u64 n, u64 len, u64 rep, bool t, bool w, Socket& s)
		{
			struct Server : coproto::NativeProto
			{
				u64 n;
				u64 len;
				u64 rep;
				std::vector<u8> buff;
				u64 i;
				bool throws;
				bool wrap;
				error_code ec;
				Socket& s;
				Server(u64 nn, u64 ll, u64 rr, bool t, bool w, Socket& ss)
					: n(nn)
					, len(ll)
					, rep(rr)
					, throws(t)
					, wrap(w)
					, s(ss)
				{}

				error_code resume()override
				{
					CP_BEGIN();
					buff.resize(len);

					for (i = 0; i < rep; ++i)
					{
						//std::cout << "s recv " << i << std::endl;;
						CP_RECV(s, buff);
					}

					for (i = 0; i < rep; ++i)
					{
						//std::cout << "s send " << i << std::endl;;
						CP_SEND(s, buff);
					}

					if (n)
					{
						if (wrap)
						{
							CP_AWAIT_VAL(ec, nEchoServer(n - 1, len, rep, throws, false, s).wrap());
							auto exp = throws ? code::uncaughtException : error_code{};
							if (ec.value() != exp.value())
								throw std::runtime_error("");

						}
						else
							CP_AWAIT(nEchoServer(n - 1, len, rep, throws, false, s));

					}
					else if (throws)
					{
						throw std::runtime_error("");
					}

					CP_END();
					return {};
				}
			};

			return makeProto<Server>(n, len, rep, t, w, s);
		}

		Proto nEchoClient(u64 n, u64 len, u64 rep, bool t, bool w, Socket& s)
		{
			struct Client : coproto::NativeProto
			{
				u64 n;
				u64 len;
				u64 rep;
				std::vector<u8> buff;
				u64 i;
				bool throws;
				bool wrap;
				error_code ec;
				Socket& s;

				Client(u64 nn, u64 ll, u64 rr, bool t, bool w, Socket& ss)
					: n(nn)
					, len(ll)
					, rep(rr)
					, throws(t)
					, wrap(w)
					, s(ss)
				{}

				error_code resume()override
				{
					CP_BEGIN();

					buff.resize(len);

					for (i = 0; i < rep; ++i)
						CP_SEND(s,buff);

					for (i = 0; i < rep; ++i)
						CP_RECV(s,buff);

					if (n)
					{
						if (wrap)
						{
							CP_AWAIT_VAL(ec, nEchoClient(n - 1, len, rep, throws, false, s).wrap());

							auto exp = throws ? code::uncaughtException : error_code{};
							if (ec.value() != exp.value())
								throw std::runtime_error("");
						}
						else
							CP_AWAIT(nEchoClient(n - 1, len, rep, throws, false, s));

					}
					else if (throws)
					{
						throw std::runtime_error("");
					}

					CP_END();
					return {};
				}
			};

			//is_poly_emplaceable<Resumable, P, Args...>::value

			return makeProto<Client>(n, len, rep, t, w, s);
		}

		void Native_nestedProtocol_Test()
		{
			u64 n = 0, len = 10, rep = 2;

			for (auto t : types /*{ LocalEvaluator::async }*/)
			{
				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);
				auto p0 = nEchoClient(n, len, rep, false, false, s[0]);
				auto p1 = nEchoServer(n, len, rep, false, false, s[1]);
				auto ec = sched.execute(p0, p1, t);
				if (ec)
					throw std::runtime_error(ec.message());
			}
		}


		void Native_nestedProtocol_Throw_Test()
		{
			u64 n = 4, len = 10, rep = 4;

			for (auto t : types)
			{
				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);
				auto p0 = nEchoClient(n, len, rep, true, false, s[0]);
				auto p1 = nEchoServer(n, len, rep, true, false, s[1]);
				auto ec = sched.execute(p0, p1, t);
				if (ec != code::uncaughtException)
					throw std::runtime_error(ec.message());
			}
		}

		void Native_nestedProtocol_ErrorCode_Test()
		{
			u64 n = 4, len = 10, rep = 4;

			for (auto t : types)
			{
				{

					LocalEvaluator sched;
					auto s = sched.getSocketPair(t);
					auto p0 = nEchoClient(n, len, rep, false, true, s[0]);
					auto p1 = nEchoServer(n, len, rep, false, true, s[1]);
					auto ec = sched.execute(p0, p1, t);
					if (ec)
						throw std::runtime_error(ec.message());

				}
				{

					LocalEvaluator sched;
					auto s = sched.getSocketPair(t);
					auto p0 = nEchoClient(n, len, rep, true, true, s[0]);
					auto p1 = nEchoServer(n, len, rep, true, true, s[1]);
					auto ec = sched.execute(p0, p1, t);
					if (ec)
						throw std::runtime_error(ec.message());
				}
			}
		}


		struct AwaitPP : public NativeProto
		{
			bool party;

			Async<void> f0, f1, f2;
			Socket& s;
			AwaitPP(bool p, Socket& ss)
				:party(p)
				,s(ss)
			{}
			error_code resume()override
			{
				u64 n = 4;
				u64 len = 10;
				u64 rep = 4;
				CP_BEGIN();


				if (party)
				{
					CP_AWAIT_VAL(f0, nEchoClient(n, len + 0, rep, false, false, s).async());
					CP_AWAIT_VAL(f1, nEchoClient(n + 1, len + 1, rep, false, false, s).async());
					CP_AWAIT_VAL(f2, nEchoClient(n + 2, len + 2, rep, false, false, s).async());

					CP_AWAIT(nEchoServer(n + 1, len + 4, rep, false, false, s));

					CP_AWAIT(f0);
					CP_AWAIT(f1);
					CP_AWAIT(f2);
				}
				else
				{

					CP_AWAIT_VAL(f0, nEchoServer(n, len + 0, rep, false, false, s).async());
					CP_AWAIT_VAL(f1, nEchoServer(n + 1, len + 1, rep, false, false, s).async());
					CP_AWAIT_VAL(f2, nEchoServer(n + 2, len + 2, rep, false, false, s).async());

					CP_AWAIT(nEchoClient(n + 1, len + 4, rep, false, false, s));

					CP_AWAIT(f0);
					CP_AWAIT(f1);
					CP_AWAIT(f2);
				}

				CP_END();
				return {};
			}
		};


		void Native_asyncProtocol_Test()
		{
			for (auto t : types /*{LocalEvaluator::async}*/)
			{
				//std::cout << "~~~~~~~~~~~~~~~~ "<< t<<"~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);
				auto p0 = makeProto<AwaitPP>(0, s[0]);
				auto p1 = makeProto<AwaitPP>(1, s[1]);
				auto ec = sched.execute(p0, p1, t);
				if (ec)
					throw std::runtime_error(ec.message());
			}
		}

		void Native_asyncProtocol_Throw_Test()
		{
			struct PP : public NativeProto
			{
				bool party;

				Async<void> f0, f1, f2;
				Socket& s;
				PP(bool p, Socket& ss)
					:party(p)
					,s(ss)
				{}
				error_code resume()override
				{
					u64 n = 3;
					u64 len = 10;
					u64 rep = 4;
					CP_BEGIN();


					if (party)
					{
						CP_AWAIT_VAL(f1, nEchoClient(n + 1, len + 1, rep, true, false,s).async());

						CP_AWAIT(nEchoServer(n + 1, len + 4, rep, false, false,s));

						CP_AWAIT(f1);
					}
					else
					{

						CP_AWAIT_VAL(f1, nEchoServer(n + 1, len + 1, rep, false, false,s).async());
						CP_AWAIT(nEchoClient(n + 1, len + 4, rep, false, false,s));

						CP_AWAIT(f1);
					}

					CP_END();
					return {};
				}
			};

			for (auto t : types)//{LocalEvaluator::async})//
			{
				//std::cout << "~~~~~~~~~~~~~~~~ "<< t<<"~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);
				auto p0 = makeProto<PP>(0, s[0]);
				auto p1 = makeProto<PP>(1, s[1]);
				auto ec = sched.execute(p0, p1, t);
				if (ec!= code::uncaughtException)
					throw std::runtime_error(ec.message());
			}
		}



		void Native_endOfRound_Test()
		{

			struct PP : public NativeProto
			{
				bool party, idx;
				std::vector<u8> msg;
				Socket& s;
				PP(bool p, bool i, Socket& ss)
					:party(p)
					,idx(i)
					,s(ss)
				{}

				error_code resume() override
				{
					CP_BEGIN();
					msg.resize(10);
					if (idx)
					{
						if (party) {

							CP_AWAIT(makeProto<PP>(party, false, s));
							CP_AWAIT(s.send(msg));
						}
						else
						{
							CP_AWAIT(makeProto<PP>(party, false,s));

							CP_AWAIT(s.recv(msg));
						}
				
					}
					else
					{
						if (party)
						{
							CP_AWAIT(s.recv(msg));
							CP_AWAIT(EndOfRound());
							CP_AWAIT(s.send(msg));
						}
						else
						{
							msg.resize(10);
							CP_AWAIT(s.send(msg));
							CP_AWAIT(EndOfRound());
							CP_AWAIT(s.recv(msg));
						}
					}

					CP_END();
					return {};
				}
			};
			for (auto t : types)
			{
				LocalEvaluator sched;
				auto s = sched.getSocketPair(t);

				auto p0 = makeProto<PP>(0, true, s[0]);
				auto p1 = makeProto<PP>(1, true, s[1]);
				auto ec = sched.execute(p0, p1, t);


				if (t == LocalEvaluator::interlace)
				{
					if (p0.mBase.mSched->numRounds() != 2)
						throw std::runtime_error("num round");
					if (p1.mBase.mSched->numRounds() != 1)
						throw std::runtime_error("num round");
				}
				if (ec)
					throw std::runtime_error(ec.message());
			}
		}

		void Native_errorSocket_Test()
		{




			for (auto type : { LocalEvaluator::interlace, LocalEvaluator::async })
			{

				LocalEvaluator eval;
				auto s = eval.getSocketPair(type);

				auto p0 = makeProto<AwaitPP>(0, s[0]);
				auto p1 = makeProto<AwaitPP>(1, s[1]);
				auto ec = eval.execute(p0, p1, type);
				auto numOps = eval.mOpIdx;

				if (ec)
					throw std::runtime_error(ec.message());

				for (u64 i = 0; i < numOps; ++i)
				{
					LocalEvaluator eval;
					auto s = eval.getSocketPair(type);
					auto p0 = makeProto<AwaitPP>(0, s[0]);
					auto p1 = makeProto<AwaitPP>(1, s[1]);

					eval.mErrorIdx = i;
					auto ec = eval.execute(p0, p1, type);


					if (ec != code::ioError)
						throw std::runtime_error("error was expected");
				}


			}

		}
		


	}



}