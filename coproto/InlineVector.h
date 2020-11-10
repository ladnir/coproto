#pragma once
#include "Defines.h"

namespace coproto
{
	namespace internal
	{

		template<typename T, int inlineSize>
		class InlineVector
		{
		public:
			static const int bufferSize = std::max<int>(sizeof(u64), inlineSize * sizeof(T));

			using InlineStorage = typename std::aligned_storage<bufferSize, alignof(T)>::type;

			InlineStorage mStorage;
			span<T> mSpan;

			using iterator = typename span<T>::iterator;


			InlineVector()
			{
				mSpan = span<T>((T*)&mStorage, 0);
			}
			InlineVector(const InlineVector& c)
			{
				grow(c.size());
				mSpan = span<T>(mSpan.data(), c.size());
				for (u64 i = 0; i < c.size(); ++i)
				{
					new (&mSpan[i]) T(c.mSpan[i]);
				}
			}

			InlineVector(InlineVector&& c)
			{
				if (c.isInline())
				{
					grow(c.size());
					mSpan = span<T>(mSpan.data(), c.size());
					for (u64 i = 0; i < c.size(); ++i)
					{
						new (&mSpan[i]) T(std::move(c.mSpan[i]));
						if constexpr (std::is_trivially_destructible<T>::value == false)
							c.mSpan[i].~T();
					}
				}
				else
				{
					mSpan = c.mSpan;
				}

				c.mSpan = span<T>((T*)&mStorage, 0);
			}

			iterator begin() { return mSpan.begin(); }
			iterator end() { return mSpan.end(); }


			T* data() { return mSpan.data(); }
			const T* data() const { return mSpan.data(); }
			u64 size() const { return mSpan.size(); }


			void resize(u64 newSize)
			{
				grow(newSize);
				auto oldSize = size();
				mSpan = span<T>(data(), newSize);

				for (u64 i = oldSize; i < newSize; ++i)
					new (&mSpan[i]) T{};


				if constexpr (std::is_trivially_destructible<T>::value == false)
					for (u64 i = newSize; i < oldSize; ++i)
						data()[i].~T();
			}

			// prevent the constructor being called
			// while also handling alignment.
			struct alignas(T) TT { char _[sizeof(T)]; };


			void grow(u64 newCap)
			{
				if (newCap > capacity())
				{

					auto newData = (T*) new TT[newCap];

					for (u64 i = 0; i < size(); ++i)
					{
						new (&newData[i]) T(std::move(mSpan[i]));

						if constexpr (std::is_trivially_destructible<T>::value == false)
							mSpan[i].~T();
					}

					if (isInline() == false && size() > 0)
						delete data();

					mSpan = span<T>(newData, size());

					*(u64*)&mStorage = newCap;
				}
			}

			bool isInline()
			{
				return
					(void*)data() == (void*)&mStorage;
			}

			constexpr u64 inlineCapacity()
			{
				// todo fix alignemnt.
				return inlineSize / sizeof(T);
			}

			u64 capacity()
			{
				if (isInline())
					return inlineCapacity();
				else
				{
					return *(u64*)&mStorage;
				}
			}


			T& front() { return mSpan.front(); }
			const T& front() const { return mSpan.front(); }
			T& back() { return mSpan.back(); }
			const T& back() const { return mSpan.back(); }

			void pop_back()
			{

				if constexpr (std::is_trivially_destructible<T>::value == false)
					mSpan.back().~T();

				mSpan = span<T>(data(), size() - 1);
			}

			void push_back(const T& t)
			{
				grow(size() + 1);
				mSpan = span<T>(data(), size() + 1);
				new(&mSpan.back()) T(t);
			}
			void push_back(T&& t)
			{
				grow(size() + 1);
				mSpan = span<T>(data(), size() + 1);
				new(&mSpan.back()) T(std::move(t));
			}

			template<typename... Args>
			requires
				std::is_constructible<T, Args...>::value
			void emplace_back(Args&&... args)
			{
				grow(size() + 1);
				mSpan = span<T>(data(), size() + 1);
				new(&mSpan.back()) T(std::forward<Args>(args)...);
			}


			T& operator[](u64 i)
			{
				return mSpan[i];
			}

			const T& operator[](u64 i) const
			{
				return mSpan[i];
			}
		};


	}

}
