#pragma once
#include "Defines.h"
#include "TypeTraits.h"
#include <cassert>
#include "error_code.h"
#include <iostream>
#include <memory>
#include "InlinePoly.h"

namespace coproto
{
	std::string hexPtr(void* p);
	namespace internal
	{

		template<typename Container>
		typename std::enable_if<is_resizable_trivial_container_v<Container>, error_code>::type
			tryResize(u64 size, Container& container)
		{
			if (size % sizeof(typename Container::value_type))
				return code::bufferResizeNotMultipleOfValueType;
			try {
				container.resize(size / sizeof(typename Container::value_type));
				return {};
			}
			catch (...)
			{
				return code::bufferResizedFailed;
			}
		}

		template<typename Container>
		typename std::enable_if<!is_resizable_trivial_container_v<Container>, error_code>::type
			tryResize(u64 size, Container& container)
		{
			return code::noResizeSupport;
		}

		template<typename Container>
		typename std::enable_if<is_trivial_container_v<Container>, span<u8>>::type
			asSpan(Container& container)
		{
			return span<u8>((u8*)container.data(), container.size() * sizeof(typename Container::value_type));
		}

		template<typename ValueType>
		typename std::enable_if<std::is_trivial_v<ValueType>, span<u8>>::type
			asSpan(ValueType& container)
		{
			return span<u8>((u8*)&container, sizeof(ValueType));
		}
	}

	struct BufferInterface
	{
		virtual span<u8> asSpan() = 0;
		virtual error_code tryResize(u64 size) = 0;
	};


	struct Buffer
	{
		internal::InlinePoly<BufferInterface, sizeof(u64) * 4> mStorage;

		span<u8> asSpan() {
			return mStorage->asSpan();
		}
		error_code tryResize(u64 size) {
			return mStorage->tryResize(size);
		}
	};




}
