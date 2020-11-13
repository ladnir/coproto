#pragma once
#include "coproto/Defines.h"
#include <cassert>
#include "coproto/TypeTraits.h"
#include "coproto/span.h"

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
				: mSpan((T*)&mStorage, 0ull)
			{
				assert(!isHeap());
			}

			InlineVector(const InlineVector& c)
				: mSpan((T*)&mStorage, 0ull)
			{
				grow(c.size());
				mSpan = span<T>(mSpan.data(), c.size());
				for (u64 i = 0; i < c.size(); ++i)
				{
					new (&mSpan[i]) T(c.mSpan[i]);
				}
			}

			InlineVector(InlineVector&& c)
				: mSpan((T*)&mStorage, 0ull)
			{
				if (c.isInline())
				{
					grow(c.size());
					mSpan = span<T>(mSpan.data(), c.size());
					for (u64 i = 0; i < c.size(); ++i)
					{
						new (&mSpan[i]) T(std::move(c.mSpan[i]));
						CallDestructor<T>(c.mSpan[i]);
						//if constexpr (std::is_trivially_destructible<T>::value == false)
						//	c.mSpan[i].~T();
					}
				}
				else
				{
					mSpan = c.mSpan;
					setCapacity(c.capacity());
				}

				c.mSpan = span<T>((T*)&c.mStorage, 0ull);
			}

			bool isHeap()
			{
				return mSpan.data() && isInline() == false;
			}

			~InlineVector()
			{
				if (isHeap())
				{
					COPROTO_REG_DEL(data());
					//std::cout << "del " << hexPtr(data()) << std::endl;
					delete[] data();
				}
				else
				{
					assert(data() == (void*)&mStorage);
				}
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


					for (u64 i = newSize; i < oldSize; ++i)
						CallDestructor<T>(data()[i]);
			}

			// prevent the constructor being called
			// while also handling alignment.
			struct alignas(T) TT { char _[sizeof(T)]; };


			void grow(u64 newCap)
			{
				if (newCap > capacity())
				{

					auto newData = (T*) new TT[newCap];
					COPROTO_REG_NEW(newData, "grow");
					//std::cout << "new " << hexPtr(newData) << std::endl;

					for (u64 i = 0; i < size(); ++i)
					{
						new (&newData[i]) T(std::move(mSpan[i]));

						//if constexpr (std::is_trivially_destructible<T>::value == false)
						CallDestructor<T> d(data()[i]);
					}

					if (isHeap())
					{
						//--gNewDel;
						//std::cout << "del " << hexPtr(data()) << std::endl;
						COPROTO_REG_DEL(data());
						delete[] data();

					}

					mSpan = span<T>(newData, size());
					assert(isHeap());
					setCapacity(newCap);
				}

				assert(mSpan.data());
			}

			bool isInline()
			{
				return
					(void*)data() == (void*)&mStorage;
			}

			constexpr u64 inlineCapacity()
			{
				// todo fix alignemnt.
				return inlineSize;
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

			void setCapacity(u64 cap)
			{
				assert(isInline() == false);
				*(u64*)&mStorage = cap;
			}


			T& front() { return mSpan.front(); }
			const T& front() const { return mSpan.front(); }
			T& back() { return mSpan.back(); }
			const T& back() const { return mSpan.back(); }

			void pop_back()
			{

				//if constexpr (std::is_trivially_destructible<T>::value == false)
				CallDestructor<T>(mSpan.back());

				mSpan = span<T>(data(), size() - 1);
			}

			void push_back(const T& t)
			{
				if(size() == capacity())
					grow(size() * 2);
				mSpan = span<T>(data(), size() + 1);
				new(&mSpan.back()) T(t);
			}
			void push_back(T&& t)
			{
				if (size() == capacity())
					grow(size() * 2);
				mSpan = span<T>(data(), size() + 1);
				new(&mSpan.back()) T(std::move(t));
			}

			template<typename... Args>
			enable_if_t<std::is_constructible<T, Args...>::value>
				emplace_back(Args&&... args)
			{
				if (size() == capacity())
					grow(size() * 2);
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
