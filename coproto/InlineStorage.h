//#pragma once
//#include "Defines.h"
//
//namespace coproto
//{
//	struct  InlineStorageTag
//	{
//	};
//
//	class Allocator
//	{
//	public:
//		span<u8> mStorage;
//	};
//
//	// This controller interface will allow us to 
//	// dectroy and move the underlying object.
//	template<typename Interface, int StorageSize>
//	struct Concept
//	{
//		virtual ~Concept() {};
//
//		// assumes dest is uninitialized and calls the 
//		// placement new move constructor with this as 
//		// dest destination.
//		virtual void moveTo(Allocator& dest) = 0;
//
//		// return size actual size of the underlying object
//		virtual u64 sizeOf() const = 0;
//	};
//
//	// This type will contain a vtable which allows
//	// us to dynamic dispatch to control actual instances
//	template<typename U>
//	struct ModelController : public Concept
//	{
//		// construct the
//		template<typename... Args,
//			typename Enabled =
//			typename std::enable_if<
//			std::is_constructible<U, Args...>::value
//			>::type
//		>
//			ModelController(Args&& ... args)
//			:mU(std::forward<Args>(args)...)
//		{}
//
//		void moveTo(Inline<Interface, StorageSize>& dest) override
//		{
//			assert(dest.get() == nullptr);
//			dest.emplace<U>(std::move(mU));
//		}
//
//		u64 sizeOf() const override
//		{
//			return sizeof(U);
//		}
//
//		U mU;
//	};
//	
//	template<typename T, int Size>
//	class InlineStorage2
//	{
//	public:
//
//		template<typename U, typename... Args>
//		void moveIn(InlineStorage2<T, Size>& storage, Args... args)
//		{
//			auto size = storage.utilized();
//
//		}
//
//
//	};
//
//	const int Size = 32;
//	struct Base
//	{ };
//
//	struct DImpl : public Base
//	{
//		int i;
//		Base* mBase;
//
//		DImpl(DImpl&& d)
//		{
//			mBase = &d;
//		}
//	};
//
//	struct D : public DImpl
//	{
//		InlineStorage2<Base, Size> mStorage;
//	};
//
//	struct Test : public Base
//	{
//		InlineStorage2<Base, Size> mStorage;
//
//		Test(D&& d)
//		{
//			auto& dd = (DImpl&)d;
//
//			mStorage.moveIn<DImple>( d.mStorage, dd);
//			//mImpl.moveIn(d.mImpl.moveFrom());
//		}
//	};
//}