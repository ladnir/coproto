
#include "coproto/TypeTraits.h"
#include <vector>
#include <array>
#include <string>
#include <type_traits>
#include "coproto/Defines.h"

namespace coproto
{

	template<typename, typename T>
	struct has_resize {
		static_assert(
			std::integral_constant<T, false>::value,
			"Second template parameter needs to be of function type.");
	};

	// specialization that does the checking
	template<typename C, typename Ret, typename... Args>
	struct has_resize<C, Ret(Args...)> {
	private:
		template<typename T>
		static constexpr auto check(T*)
			-> typename
			std::is_same<
			decltype(std::declval<T>().resize(std::declval<Args>()...)),
			Ret    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
			>::type;  // attempt to call it and see if the return type is correct

		template<typename>
		static constexpr std::false_type check(...);

		typedef decltype(check<C>(0)) type;

	public:
		static constexpr bool value = type::value;
	};


	template<typename Container, typename = void>
	struct has_resize2 : public std::false_type
	{
	};


	template<typename Container>
	struct has_resize2<Container, std::void_t<typename Container::size_type>> : public has_resize<Container, void(typename Container::size_type)>
	{
	};



	template<typename Container>
	using has_resize_member_func_v2 = has_resize2<Container>;



	namespace tests {



		static_assert(has_data_member_func<std::array<char, 5>>::value, "");
		static_assert(has_data_member_func<std::array<long long, 5>>::value, "");
		static_assert(has_data_member_func<std::vector<char>>::value, "");
		static_assert(has_data_member_func<std::vector<long long>>::value, "");
		static_assert(has_data_member_func<span<char>>::value, "");
		static_assert(has_data_member_func<span<long long>>::value, "");
		static_assert(has_data_member_func<std::string>::value, "");
		static_assert(has_data_member_func<int>::value == false, "");
		//static_assert(has_resize_member_func_v<int>::value == false, "");

		static_assert(has_size_member_func<std::array<char, 5>>::value, "");
		static_assert(has_size_member_func<std::array<long long, 5>>::value, "");
		static_assert(has_size_member_func<std::vector<char>>::value, "");
		static_assert(has_size_member_func<std::vector<long long>>::value, "");
		static_assert(has_size_member_func<span<char>>::value, "");
		static_assert(has_size_member_func<span<long long>>::value, "");
		static_assert(has_size_member_func<std::string>::value, "");
		static_assert(has_size_member_func<int>::value == false, "");


		static_assert(has_size_type<std::array<char, 5>>::value, "");
		static_assert(has_size_type<std::array<long long, 5>>::value, "");
		static_assert(has_size_type<std::vector<char>>::value, "");
		static_assert(has_size_type<std::vector<long long>>::value, "");
		static_assert(has_size_type<span<char>>::value, "");
		static_assert(has_size_type<span<long long>>::value, "");
		static_assert(has_size_type<std::string>::value, "");
		static_assert(has_size_type<int>::value == false, "");
		
		static_assert(has_size_member_func<std::array<char, 5>>::value, "");
		static_assert(has_size_member_func<std::array<long long, 5>>::value, "");
		static_assert(has_size_member_func<std::vector<char>>::value, "");
		static_assert(has_size_member_func<std::vector<long long>>::value, "");
		static_assert(has_size_member_func<span<char>>::value, "");
		static_assert(has_size_member_func<span<long long>>::value, "");
		static_assert(has_size_member_func<std::string>::value, "");
		static_assert(has_size_member_func<int>::value == false, "");

		static_assert(has_resize_member_func<std::array<char, 5>>::value == false, "");
		static_assert(has_resize_member_func<std::array<long long, 5>>::value == false, "");
		static_assert(has_resize_member_func<span<char>>::value == false, "");
		static_assert(has_resize_member_func<span<long long>>::value == false, "");
		static_assert(has_resize_member_func<std::vector<char>>::value, "");
		static_assert(has_resize_member_func<std::vector<long long>>::value, "");
		static_assert(has_resize_member_func<std::string>::value, "");
		static_assert(has_resize_member_func<int>::value == false, "");


		static_assert(has_resize_member_func_v2<std::array<char, 5>>::value == false, "");
		static_assert(has_resize_member_func_v2<std::array<long long, 5>>::value == false, "");
		static_assert(has_resize_member_func_v2<span<char>>::value == false, "");
		static_assert(has_resize_member_func_v2<span<long long>>::value == false, "");
		static_assert(has_resize_member_func_v2<std::vector<char>>::value, "");
		static_assert(has_resize_member_func_v2<std::vector<long long>>::value, "");
		static_assert(has_resize_member_func_v2<std::string>::value, "");
		static_assert(has_resize_member_func_v2<int>::value == false, "");

		static_assert(is_trivial_container<std::string>::value == true, "");
		static_assert(is_trivial_container<std::array<char, 5>>::value == false, "");
		static_assert(is_trivial_container<std::array<long long, 5>>::value == false, "");
		static_assert(is_trivial_container<span<char>>::value, "");
		static_assert(is_trivial_container<span<long long>>::value, "");
		static_assert(is_trivial_container<std::vector<char>>::value, "");
		static_assert(is_trivial_container<std::vector<long long>>::value, "");
		static_assert(is_trivial_container<std::string>::value, "");

		static_assert(is_trivial_container<span<span<char>>>::value == false, "");
		static_assert(is_trivial_container<span<span<long long>>>::value == false, "");
		static_assert(is_trivial_container<std::vector<std::vector<char>>>::value == false, "");
		static_assert(is_trivial_container<std::vector<std::vector<long long>>>::value == false, "");
		static_assert(is_trivial_container<int>::value == false, "");

		static_assert(is_resizable_trivial_container<std::string>::value == true, "");
		static_assert(is_resizable_trivial_container<std::array<char, 5>>::value == false, "");
		static_assert(is_resizable_trivial_container<std::array<long long, 5>>::value == false, "");
		static_assert(is_resizable_trivial_container<span<char>>::value == false, "");
		static_assert(is_resizable_trivial_container<span<long long>>::value == false, "");
		static_assert(is_resizable_trivial_container<std::vector<char>>::value, "");
		static_assert(is_resizable_trivial_container<std::vector<long long>>::value, "");
		static_assert(is_resizable_trivial_container<std::string>::value, "");
		static_assert(is_resizable_trivial_container<int>::value == false, "");



		struct Base {};


		struct Derived_int : public Base
		{
			Derived_int(int)
			{}
		};

		struct Derived_int_str : public Base
		{
			Derived_int_str(int, std::string)
			{}
		};

		static_assert(is_poly_emplaceable<Base, Base>::value == true, "");
		static_assert(is_poly_emplaceable<Base, Derived_int, int>::value == true, "");
		static_assert(is_poly_emplaceable<Base, Derived_int_str, int, std::string>::value == true, "");
		static_assert(is_poly_emplaceable<Base, std::string>::value == false, "");
		static_assert(is_poly_emplaceable<Base, std::string, int>::value == false, "");
		static_assert(is_poly_emplaceable<Base, std::string, int, std::string>::value == false, "");

	}

}
