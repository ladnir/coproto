#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include "Tests.h"

#include "Buffers.h"
#include "Proto.h"
#include "InlinePoly.h"

#ifdef _MSC_VER
#include <windows.h>
#endif
#include <iomanip>
#include <chrono>

namespace coproto
{


    const Color ColorDefault([]() -> Color {
#ifdef _MSC_VER
        CONSOLE_SCREEN_BUFFER_INFO   csbi;
        HANDLE m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        GetConsoleScreenBufferInfo(m_hConsole, &csbi);

        return (Color)(csbi.wAttributes & 255);
#else
        return Color::White;
#endif

        }());

#ifdef _MSC_VER
    static const HANDLE __m_hConsole(GetStdHandle(STD_OUTPUT_HANDLE));
#endif
#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define BOLDBLACK   "\033[1m\033[30m"      /* Bold Black */
#define BOLDRED     "\033[1m\033[31m"      /* Bold Red */
#define BOLDGREEN   "\033[1m\033[32m"      /* Bold Green */
#define BOLDYELLOW  "\033[1m\033[33m"      /* Bold Yellow */
#define BOLDBLUE    "\033[1m\033[34m"      /* Bold Blue */
#define BOLDMAGENTA "\033[1m\033[35m"      /* Bold Magenta */
#define BOLDCYAN    "\033[1m\033[36m"      /* Bold Cyan */
#define BOLDWHITE   "\033[1m\033[37m"      /* Bold White */

    std::array<const char*, 16> colorMap
    {
        "",         //    -- = 0,
        "",         //    -- = 1,
        GREEN,      //    LightGreen = 2,
        BLACK,      //    LightGrey = 3,
        RED,        //    LightRed = 4,
        WHITE,      //    OffWhite1 = 5,
        WHITE,      //    OffWhite2 = 6,
        "",         //         = 7
        BLACK,      //    Grey = 8,
        "",         //    -- = 9,
        BOLDGREEN,  //    Green = 10,
        BOLDBLUE,   //    Blue = 11,
        BOLDRED,    //    Red = 12,
        BOLDCYAN,   //    Pink = 13,
        BOLDYELLOW, //    Yellow = 14,
        RESET       //    White = 15
    };

    std::ostream& operator<<(std::ostream& out, Color tag)
    {
        if (tag == Color::Default)
            tag = ColorDefault;
#ifdef _MSC_VER
        SetConsoleTextAttribute(__m_hConsole, (WORD)tag | (240 & (WORD)ColorDefault));
#else

        out << colorMap[15 & (char)tag];
#endif
        return out;
    }


    void TestCollection::add(std::string name, std::function<void()> fn)
    {
        mTests.push_back({ std::move(name), std::move(fn) });
    }

    TestCollection::Result TestCollection::runOne(uint64_t idx)
    {
        if (idx >= mTests.size())
        {
            std::cout << Color::Red << "No test " << idx << std::endl;
            return Result::failed;
        }

        Result res = Result::failed;
        int w = int(std::ceil(std::log10(mTests.size())));
        std::cout << std::setw(w) << idx << " - " << Color::Blue << mTests[idx].mName << ColorDefault << std::flush;

        auto start = std::chrono::high_resolution_clock::now();
        try
        {
            mTests[idx].mTest(); std::cout << Color::Green << "  Passed" << ColorDefault;
            res = Result::passed;
        }
        catch (const UnitTestSkipped& e)
        {
            std::cout << Color::Yellow << "  Skipped - " << e.what() << ColorDefault;
            res = Result::skipped;
        }
        catch (const std::exception& e)
        {
            std::cout << Color::Red << "Failed - " << e.what() << ColorDefault;
        }
        auto end = std::chrono::high_resolution_clock::now();



        uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "   " << time << "ms" << std::endl;

        return res;
    }

    TestCollection::Result TestCollection::run(std::vector<u64> testIdxs, u64 repeatCount)
    {
        u64 numPassed(0), total(0), numSkipped(0);

        for (u64 r = 0; r < repeatCount; ++r)
        {
            for (auto i : testIdxs)
            {
                if (repeatCount != 1) std::cout << r << " ";
                auto res = runOne(i);
                numPassed += (res == Result::passed);
                total += (res != Result::skipped);
                numSkipped += (res == Result::skipped);
            }
        }

        if (numPassed == total)
        {
            std::cout << Color::Green << std::endl
                << "=============================================\n"
                << "            All Passed (" << numPassed << ")\n";
            if (numSkipped)
                std::cout << Color::Yellow << "            skipped (" << numSkipped << ")\n";

            std::cout << Color::Green
                << "=============================================" << std::endl << ColorDefault;
            return Result::passed;
        }
        else
        {
            std::cout << Color::Red << std::endl
                << "#############################################\n"
                << "           Failed (" << total - numPassed << ")\n" << Color::Green
                << "           Passed (" << numPassed << ")\n";

            if (numSkipped)
                std::cout << Color::Yellow << "            skipped (" << numSkipped << ")\n";

            std::cout << Color::Red
                << "#############################################" << std::endl << ColorDefault;
            return Result::failed;
        }
    }


    TestCollection::Result TestCollection::runAll(uint64_t rp)
    {
        std::vector<u64> v;
        for (u64 i = 0; i < mTests.size(); ++i)
            v.push_back(i);

        return run(v, rp);
    }

    TestCollection::Result TestCollection::run(int argc, char** argv)
    {
        std::vector<u64> idxs;
        bool t = false;
        for (u64 i = 1; i < argc; ++i)
        {
            if (argv[i] == std::string("-u"))
            {
                t = true;



            }
            else if (t)
            {
                std::stringstream ss(argv[i]);
                u64 idx;
                ss >> idx;
                idxs.push_back(idx);
            }
        }

        if (t && idxs.size() == 0)
            return runAll();
        else if (t)
            return run(idxs);

        return Result();
    }

    void TestCollection::list()
    {
        int w = int(std::ceil(std::log10(mTests.size())));
        for (uint64_t i = 0; i < mTests.size(); ++i)
        {
            std::cout << std::setw(w) << i << " - " << Color::Blue << mTests[i].mName << std::endl << ColorDefault;
        }
    }


    void TestCollection::operator+=(const TestCollection& t)
    {
        mTests.insert(mTests.end(), t.mTests.begin(), t.mTests.end());
    }
    



	TestCollection testCollection([](TestCollection& t) {
		
		t.add("InlinePolyTest                  ", tests::InlinePolyTest);
		t.add("strSendRecvTest                 ", tests::strSendRecvTest); 
		t.add("resultSendRecvTest              ", tests::resultSendRecvTest);
		t.add("typedRecvTest                   ", tests::typedRecvTest);
		
		t.add("zeroSendRecvTest                ", tests::zeroSendRecvTest);
		t.add("badRecvSizeTest                 ", tests::badRecvSizeTest);
		t.add("zeroSendErrorCodeTest           ", tests::zeroSendErrorCodeTest);
		t.add("badRecvSizeErrorCodeTest        ", tests::badRecvSizeErrorCodeTest);
		t.add("throwsTest                      ", tests::throwsTest);

		t.add("nestedSendRecvTest              ", tests::nestedSendRecvTest);
		t.add("nestedProtocolThrowTest         ", tests::nestedProtocolThrowTest);
		t.add("nestedProtocolErrorCodeTest     ", tests::nestedProtocolErrorCodeTest);
		t.add("asyncProtocolTest               ", tests::asyncProtocolTest);
		t.add("asyncThrowProtocolTest          ", tests::asyncThrowProtocolTest);
        t.add("endOfRoundTest                  ", tests::endOfRoundTest);
        t.add("errorSocketTest                 ", tests::errorSocketTest);
		//t.add("v1::intSendRecvTest             ", v1::tests::intSendRecvTest);
		//t.add("v1::arraySendRecvTest           ", v1::tests::arraySendRecvTest);
		//t.add("v1::basicSendRecvTest           ", v1::tests::strSendRecvTest);
		//t.add("v1::resizeSendRecvTest          ", v1::tests::resizeSendRecvTest);
		//t.add("v1::moveSendRecvTest            ", v1::tests::moveSendRecvTest);
		//t.add("v1::typedRecvTest               ", v1::tests::typedRecvTest);
		//t.add("v1::nestedProtocolTest          ", v1::tests::nestedProtocolTest);
		//t.add("v1::zeroSendRecvTest            ", v1::tests::zeroSendRecvTest);
		//t.add("v1::zeroSendErrorCodeTest       ", v1::tests::zeroSendErrorCodeTest);
		//t.add("v1::badRecvSizeTest             ", v1::tests::badRecvSizeTest);
		//t.add("v1::badRecvSizeErrorCodeTest    ", v1::tests::badRecvSizeErrorCodeTest);
		//t.add("v1::throwsTest                  ", v1::tests::throwsTest);
		//t.add("v1::nestedProtocolThrowTest     ", v1::tests::nestedProtocolThrowTest);
		//t.add("v1::nestedProtocolErrorCodeTest ", v1::tests::nestedProtocolErrorCodeTest);

		}); 
}