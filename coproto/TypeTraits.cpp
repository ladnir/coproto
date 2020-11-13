
#include "coproto/TypeTraits.h"
#include <vector>
#include <array>
#include <string>
#include <type_traits>
#include "coproto/Defines.h"

namespace coproto
{

	namespace tests {

		static_assert(has_data_member_func_v<std::array<char, 5>>, "");
		static_assert(has_data_member_func_v<std::array<long long, 5>>, "");
		static_assert(has_data_member_func_v<std::vector<char>>, "");
		static_assert(has_data_member_func_v<std::vector<long long>>, "");
		static_assert(has_data_member_func_v<span<char>>, "");
		static_assert(has_data_member_func_v<span<long long>>, "");
		static_assert(has_data_member_func_v<std::string>, "");
		static_assert(has_data_member_func_v<int> == false, "");
		//static_assert(has_resize_member_func_v<int>::value == false, "");

		static_assert(has_size_member_func_v<std::array<char, 5>>, "");
		static_assert(has_size_member_func_v<std::array<long long, 5>>, "");
		static_assert(has_size_member_func_v<std::vector<char>>, "");
		static_assert(has_size_member_func_v<std::vector<long long>>, "");
		static_assert(has_size_member_func_v<span<char>>, "");
		static_assert(has_size_member_func_v<span<long long>>, "");
		static_assert(has_size_member_func_v<std::string>, "");
		static_assert(has_size_member_func_v<int> == false, "");

		static_assert(has_resize_member_func_v<std::array<char, 5>> == false, "");
		static_assert(has_resize_member_func_v<std::array<long long, 5>> == false, "");
		static_assert(has_resize_member_func_v<span<char>> == false, "");
		static_assert(has_resize_member_func_v<span<long long>> == false, "");
		static_assert(has_resize_member_func_v<std::vector<char>>, "");
		static_assert(has_resize_member_func_v<std::vector<long long>>, "");
		static_assert(has_resize_member_func_v<std::string>, "");
		static_assert(has_resize_member_func_v<int> == false, "");

		static_assert(is_trivial_container_v<std::string> == true, "");
		static_assert(is_trivial_container_v<std::array<char, 5>> == false, "");
		static_assert(is_trivial_container_v<std::array<long long, 5>> == false, "");
		static_assert(is_trivial_container_v<span<char>>, "");
		static_assert(is_trivial_container_v<span<long long>>, "");
		static_assert(is_trivial_container_v<std::vector<char>>, "");
		static_assert(is_trivial_container_v<std::vector<long long>>, "");
		static_assert(is_trivial_container_v<std::string>, "");

		static_assert(is_trivial_container_v<span<span<char>>> == false, "");
		static_assert(is_trivial_container_v<span<span<long long>>> == false, "");
		static_assert(is_trivial_container_v<std::vector<std::vector<char>>> == false, "");
		static_assert(is_trivial_container_v<std::vector<std::vector<long long>>> == false, "");
		static_assert(is_trivial_container_v<int> == false, "");

		static_assert(is_resizable_trivial_container_v<std::string> == true, "");
		static_assert(is_resizable_trivial_container_v<std::array<char, 5>> == false, "");
		static_assert(is_resizable_trivial_container_v<std::array<long long, 5>> == false, "");
		static_assert(is_resizable_trivial_container_v<span<char>> == false, "");
		static_assert(is_resizable_trivial_container_v<span<long long>> == false, "");
		static_assert(is_resizable_trivial_container_v<std::vector<char>>, "");
		static_assert(is_resizable_trivial_container_v<std::vector<long long>>, "");
		static_assert(is_resizable_trivial_container_v<std::string>, "");
		static_assert(is_resizable_trivial_container_v<int> == false, "");

	}

}
