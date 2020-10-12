#define _SILENCE_CXX20_IS_POD_DEPRECATION_WARNING

#include "TypeTraits.h"
#include <vector>
#include <string>

#include "cryptoTools/Common/Defines.h"

namespace coproto
{

	namespace tests {
		//#define COPROTO_INTERNAL_HAS_MEMBER_FUNC
		//
		//		template<typename, typename Container>
		//		struct has_data_member_func2 {
		//			static_assert(
		//				std::integral_constant<Container, false>::value,
		//				"Second template parameter needs to be of function type.");
		//		};
		//
		//		// specialization that does the checking
		//		template<typename C, typename Ret, typename... Args>
		//		struct has_data_member_func2<C, Ret(Args...)> {
		//		private:
		//			template<typename Container>
		//			static constexpr auto check(Container*)
		//				-> typename
		//				std::is_same<
		//				decltype(std::declval<Container>().data(std::declval<Args>()...)),
		//				Ret    // ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
		//				>::type;  // attempt to call it and see if the return type is correct
		//
		//			template<typename>
		//			static constexpr std::false_type check(...);
		//
		//			typedef decltype(check<C>(0)) type;
		//
		//		public:
		//			static constexpr bool value = type::value;
		//		};
//#define HAS_MEM_FUNC(func, name)                                        \
//    template<typename T, typename Sign>                                 \
//    struct name {                                                       \
//        typedef char yes[1];                                            \
//        typedef char no [2];                                            \
//        template <typename U, U> struct type_check;                     \
//        template <typename _1> static yes &chk(type_check<Sign, &_1::func > *); \
//        template <typename   > static no  &chk(...);                    \
//        static bool const value = sizeof(chk<T>(0)) == sizeof(yes);     \
//    }
//
//		HAS_MEM_FUNC(data, has_data_member_func2);

		//template<typename T>
		//constexpr bool has_data_mem() {
		//	requires(T it) {
		//		typename T::pointer;
		//		{ *it++ } -> std::same_as<typename T::pointer>;
		//	};
		//}
		//;


		//template <typename Iter>
		//constexpr bool is_iterator()
		//{
		//	return requires(Iter it) { *it++; };
		//}

		//// usage:
		//static_assert(is_iterator<int*>());

		

		//template<typename Container>
		//using has_data_member_func2 = std::is_convertible<
		//	typename Container::pointer,
		//	decltype(std::declval<Container>().data())
		//>;

		//template<class Container>
		//inline constexpr bool has_data_member_func_v2 = has_data_member_func2<Container>::value;


		static_assert(has_data_member_func_v<std::array<char, 5>>, "");
		static_assert(has_data_member_func_v<std::array<long long, 5>>, "");
		static_assert(has_data_member_func_v<std::vector<char>>, "");
		static_assert(has_data_member_func_v<std::vector<long long>>, "");
		static_assert(has_data_member_func_v<oc::span<char>>, "");
		static_assert(has_data_member_func_v<oc::span<long long>>, "");
		static_assert(has_data_member_func_v<std::string>, "");
		static_assert(has_data_member_func_v<int> == false, "");
		//static_assert(has_resize_member_func_v<int>::value == false, "");

		static_assert(has_size_member_func_v<std::array<char, 5>>, "");
		static_assert(has_size_member_func_v<std::array<long long, 5>>, "");
		static_assert(has_size_member_func_v<std::vector<char>>, "");
		static_assert(has_size_member_func_v<std::vector<long long>>, "");
		static_assert(has_size_member_func_v<oc::span<char>>, "");
		static_assert(has_size_member_func_v<oc::span<long long>>, "");
		static_assert(has_size_member_func_v<std::string>, "");
		static_assert(has_size_member_func_v<int> == false, "");

		static_assert(has_resize_member_func_v<std::array<char, 5>> == false, "");
		static_assert(has_resize_member_func_v<std::array<long long, 5>> == false, "");
		static_assert(has_resize_member_func_v<oc::span<char>> == false, "");
		static_assert(has_resize_member_func_v<oc::span<long long>> == false, "");
		static_assert(has_resize_member_func_v<std::vector<char>>, "");
		static_assert(has_resize_member_func_v<std::vector<long long>>, "");
		static_assert(has_resize_member_func_v<std::string>, "");
		static_assert(has_resize_member_func_v<int> == false, "");

		static_assert(is_trivial_container_v<std::string> == true, "");
		static_assert(is_trivial_container_v<std::array<char, 5>> == false, "");
		static_assert(is_trivial_container_v<std::array<long long, 5>> == false, "");
		static_assert(is_trivial_container_v<oc::span<char>>, "");
		static_assert(is_trivial_container_v<oc::span<long long>>, "");
		static_assert(is_trivial_container_v<std::vector<char>>, "");
		static_assert(is_trivial_container_v<std::vector<long long>>, "");
		static_assert(is_trivial_container_v<std::string>, "");

		static_assert(is_trivial_container_v<oc::span<oc::span<char>>> == false, "");
		static_assert(is_trivial_container_v<oc::span<oc::span<long long>>> == false, "");
		static_assert(is_trivial_container_v<std::vector<std::vector<char>>> == false, "");
		static_assert(is_trivial_container_v<std::vector<std::vector<long long>>> == false, "");
		static_assert(is_trivial_container_v<int> == false, "");

		static_assert(is_resizable_trivial_container_v<std::string> == true, "");
		static_assert(is_resizable_trivial_container_v<std::array<char, 5>> == false, "");
		static_assert(is_resizable_trivial_container_v<std::array<long long, 5>> == false, "");
		static_assert(is_resizable_trivial_container_v<oc::span<char>> == false, "");
		static_assert(is_resizable_trivial_container_v<oc::span<long long>> == false, "");
		static_assert(is_resizable_trivial_container_v<std::vector<char>>, "");
		static_assert(is_resizable_trivial_container_v<std::vector<long long>>, "");
		static_assert(is_resizable_trivial_container_v<std::string>, "");
		static_assert(is_resizable_trivial_container_v<int> == false, "");

	}

}
