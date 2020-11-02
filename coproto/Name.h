#pragma once
#include <string>

namespace coproto
{

	struct Name
	{
		Name(std::string n)
			:mName(n)
		{}

		std::string mName;
	};
}
