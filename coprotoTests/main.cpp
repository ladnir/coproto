

#include "coproto/Tests.h"

#include <iostream>

#include "cpp20Tutorial.h"
#include "cpp11Tutorial.h"
#include "SocketTutorial.h"


int main(int argc, char** argv)
{


	if (argc < 2 || strcmp(argv[1], "-u"))
	{
		cpp20Tutorial();
		cpp11Tutorial();
		SocketTutorial();
	}

	coproto::testCollection.run(argc, argv);
	//_CrtDumpMemoryLeaks();
	//std::cout << coproto::regStr() << std::endl;
	return 0;
}