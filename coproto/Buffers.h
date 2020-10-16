#pragma once
#include "TypeTraits.h"
#include <cassert>
#include "Defines.h"
#include "error_code.h"

namespace coproto
{

    namespace internal
    {

        template<typename Interface, int StorageSize = 248 /* makes the whole thing 256 bytes */>
        class Inline
        {


            // This controller interface will allow us to 
            // dectroy and move the underlying object.
            struct Controller
            {
                virtual ~Controller() {};

                // assumes dest is uninitialized and calls the 
                // placement new move constructor with this as 
                // dest destination.
                virtual void moveTo(Inline<Interface, StorageSize>& dest) = 0;

                // return size actual size of the underlying object
                virtual u64 sizeOf() const = 0;
            };

            // This type will contain a vtable which allows
            // us to dynamic dispatch to control actual instances
            template<typename U>
            struct ModelController : Controller
            {
                // construct the
                template<typename... Args,
                    typename Enabled =
                    typename std::enable_if<
                    std::is_constructible<U, Args...>::value
                    >::type
                >
                    ModelController(Args&& ... args)
                    :mU(std::forward<Args>(args)...)
                {}

                void moveTo(Inline<Interface, StorageSize>& dest) override
                {
                    assert(dest.get() == nullptr);
                    dest.emplace<U>(std::move(mU));
                }

                u64 sizeOf() const override
                {
                    return sizeof(U);
                }

                U mU;
            };

        public:

            using Storage = typename std::aligned_storage<StorageSize>::type;

            Interface* mData = nullptr;
            Storage mStorage;

            Inline() = default;
            Inline(const Inline<Interface>&) = delete;

            Inline(Inline<Interface>&& m)
            {
                *this = std::forward<Inline<Interface>>(m);
            }

            ~Inline()
            {
                destruct();
            }

            Inline<Interface, StorageSize>& operator=(Inline<Interface, StorageSize>&& m)
            {
                destruct();

                if (m.isStoredInline())
                {
                    m.getController().moveTo(*this);
                    m.destruct();
                    m.mData = nullptr;
                }
                else
                {
                    isOwning() = m.isOwning();
                    std::swap(mData, m.mData);
                }
                return *this;
            }

            bool& isOwning()
            {
                return *(bool*)&getController();
            }

            template<typename U, typename... Args >
            typename std::enable_if<
                (sizeof(ModelController<U>) <= sizeof(Storage)) &&
                std::is_base_of<Interface, U>::value&&
                std::is_constructible<U, Args...>::value
            >::type
                emplace(Args&& ... args)
            {
                destruct();

                // Do a placement new to the local storage and then take the
                // address of the U type and store that in our data pointer.
                ModelController<U>* ptr = (ModelController<U>*) & getController();
                new (ptr) ModelController<U>(std::forward<Args>(args)...);
                mData = &(ptr->mU);
            }

            template<typename U, typename... Args >
            typename std::enable_if<
                (sizeof(ModelController<U>) > sizeof(Storage)) &&
                std::is_base_of<Interface, U>::value&&
                std::is_constructible<U, Args...>::value
                    >::type
                emplace(Args&& ... args)
            {
                destruct();

                // this object is too big, use the allocator. Local storage
                // will be unused as denoted by (isSBO() == false).
                mData = new U(std::forward<Args>(args)...);
                isOwning() = true;
            }


            void setBorrowed(Interface* ptr)
            {
                mData = ptr;
                isOwning() = false;
            }

            bool isStoredInline() const
            {
                auto begin = (u8*)this;
                auto end = begin + sizeof(Inline<Interface, StorageSize>);
                return
                    ((u8*)get() >= begin) &&
                    ((u8*)get() < end);
            }

            Interface* operator->() { return get(); }
            Interface* get() { return mData; }

            const Interface* operator->() const { return get(); }
            const Interface* get() const { return mData; }



            void destruct()
            {
                if (isStoredInline())
                    // manually call the virtual destructor.
                    getController().~Controller();
                else if (get() && isOwning())
                    // let the compiler call the destructor
                    delete get();

                mData = nullptr;
            }

            Controller& getController()
            {
                return *(Controller*)&mStorage;
            }

            const Controller& getController() const
            {
                return *(Controller*)&mStorage;
            }
        };


        template<typename Interface, typename U, typename... Args>
        typename  std::enable_if<
            std::is_constructible<U, Args...>::value&&
            std::is_base_of<Interface, U>::value, Inline<Interface>>::type
            make_SBO_ptr(Args && ... args)
        {
            Inline<Interface> t;
            t.template emplace<U>(std::forward<Args>(args)...);
            return (t);
        }

    }

    namespace internal
    {
        //template<typename Container>
        //error_code tryResize(u64 size, Container& container);

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

	struct Buffer
	{
        virtual span<u8> asSpan() = 0;
        virtual error_code tryResize(u64 size) = 0;
	};

    template<typename Container, bool supportResize = true>
    struct RefBuffer : public Buffer
    {
        static_assert(
            is_trivial_container_v<Container> ||
            std::is_trivial_v<Container>, "we expect a trivial containter or trivial type.");

        Container& mContainer;
        RefBuffer(Container& container)
            :mContainer(container)
        {}

        span<u8> asSpan() override
        {
            return internal::asSpan(mContainer);
        }

        error_code tryResize(u64 size) override
        {
            if (supportResize)
                return internal::tryResize(size, mContainer);
            else
                return code::noResizeSupport;
        }
    };


    template<typename Container>
    struct MoveBuffer : public Buffer
    {
        static_assert(
            is_trivial_container_v<Container> ||
            std::is_trivial_v<Container>, "we expect a trivial containter or trivial type.");

        Container mContainer;
        MoveBuffer(Container&& container)
            :mContainer(std::forward<Container>(container))
        {}

        span<u8> asSpan() override
        {
            return internal::asSpan(mContainer);
        }

        error_code tryResize(u64 size) override
        {
            return code::noResizeSupport;
        }
    };




    namespace tests
    {
        void SmallBufferTest();
    }

}
