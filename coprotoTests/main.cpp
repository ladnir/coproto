

#include "coproto/Tests.h"

#include <iostream>



int main(int argc, char** argv)
{
	coproto::testCollection.run(argc, argv);
	//_CrtDumpMemoryLeaks();
	//std::cout << coproto::regStr() << std::endl;
	return 0;
}