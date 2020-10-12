#pragma once
#include <type_traits>
#include <concepts>

namespace coproto
{
	template<typename T>
	concept has_data_member_func_v = requires {
		typename T::pointer;
		{ std::declval<T>().data() } ->
			std::same_as<typename T::pointer>;
	};

	template<typename T>
	concept has_size_member_func_v = requires {
		typename T::size_type;
		{ std::declval<T>().size() } -> std::same_as<typename T::size_type>;
	};

	template<typename T>
	concept has_resize_member_func_v = requires {
		typename T::size_type;
		{ std::declval<T>().resize(std::declval<typename T::size_type>()) } -> std::same_as<void>;
	};

	template<typename T>
	concept has_trvial_value_type_v =
		requires { typename T::value_type; } && 
		std::is_trivial<typename T::value_type>::value;

	template<class Container>
	concept is_trivial_container_v =
		has_data_member_func_v<Container> &&
		has_size_member_func_v<Container> &&
		has_trvial_value_type_v<Container> &&
		std::is_trivial<Container>::value == false;

	template<class Container>
	concept is_resizable_trivial_container_v =
		is_trivial_container_v<Container> &&
		has_resize_member_func_v<Container>;


}