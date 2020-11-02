#pragma once
#include "error_code.h"
#include <variant>
#include <coroutine>
#include "Resumable.h"
#include "InlinePoly.h"
#include <cassert>

namespace coproto
{

    template<typename T>
    struct OkTag {
        const T& mV;

        operator const T& ()
        {
            return mV;
        }
    };

    template<typename T>
    struct OkMvTag {
        T& mV;

        operator T&& ()
        {
            return mV;
        }
    };

    template<typename E>
    struct ErrorTag {
        const E& mE;

        operator const E& ()
        {
            return mE;
        }
    };

    template<typename E>
    struct ErrorMvTag {
        E& mE;

        operator E&& ()
        {
            return std::move(mE);
        }
    };

    template<typename T>
    ErrorTag<typename std::remove_cvref_t<T>> Err(const T& t)
    {
        return ErrorTag<typename std::remove_cvref_t<T>>(t);
    }

    template<typename T>
    ErrorMvTag<typename std::remove_cvref_t<T>> Err(T&& t)
    {
        return ErrorMvTag<typename std::remove_cvref_t<T>>(t);
    }


    template<typename T>
    OkTag<typename std::remove_cvref_t<T>> Ok(const T& t)
    {
        return OkTag<typename std::remove_cvref_t<T>>(t);
    }

    template<typename T>
    OkMvTag<typename std::remove_cvref_t<T>> Ok(T&& t)
    {
        return OkMvTag<typename std::remove_cvref_t<T>>(t);
    }


    template<typename T, typename Error = error_code>
    class Result
    {
    public:
        using value_type = std::remove_cvref_t<T>;
        using error_type = std::remove_cvref_t<Error>;

        Result(OkTag<value_type>&& v) :mVar(v.mV){}
        Result(OkMvTag<value_type>&&v) : mVar(std::move(v.mV)) {}
        Result(ErrorTag<error_type>&&e): mVar(e.mE) {}
        Result(ErrorMvTag<error_type>&&e) :mVar(std::move(e.mE)) {}


        std::variant<value_type, error_type> mVar;
        std::variant<value_type, error_type>& var() {
            return mVar;
            //return coroutine_handle.promise().mVar;
        };
        const std::variant<value_type, error_type>& var() const {
            return mVar;
            //return coroutine_handle.promise().mVar;
        };


        bool hasValue() const {
            return var().index() == 0;
        }

        bool hasError() const {
            return !hasValue();
        }

        explicit operator bool() {
            return hasError();
        }

        value_type& value()
        {
            if (hasError())
                throw std::runtime_error("value() was called on a Result<T,E> which stores an error_type");

            return std::get<0>(var());
        }

        const value_type& value() const
        {
            if (hasError())
                throw std::runtime_error("value() was called on a Result<T,E> which stores an error_type");

            return value();
        }

        error_type& error()
        {
            if (hasValue())
                throw std::runtime_error("error() was called on a Result<T,E> which stores an value_type");

            return std::get<1>(var());
        }

        const error_type& error() const
        {
            if (hasValue())
                throw std::runtime_error("error() was called on a Result<T,E> which stores an value_type");

            return std::get<1>(var());
        }





        value_type& valueOr(value_type& alt)
        {
            if (hasError())
                return alt;
            return value();
        }

        const value_type& valueOr(const value_type& alt) const
        {
            if (hasError())
                return alt;
            return value();
        }

        error_type& errorOr(error_type& alt)
        {
            if (hasError())
                return error();
            return alt;
        }

        const error_type& errorOr(const error_type& alt) const
        {
            if (hasError())
                return error();
            return alt;
        }



        void operator=(OkMvTag<value_type>&& v)
        {
            var() = { std::in_place_index<0>, std::move(v.mV) };
        }
        void operator=(OkTag<value_type>&& v)
        {
            var() = { std::in_place_index<0>, v.mV };
        } 

        void operator=(ErrorMvTag<error_type>&& v)
        {
            var() = { std::in_place_index<1>, std::move(v.mE) };
        }
        void operator=(ErrorTag<error_type>&& v)
        {
            var() = { std::in_place_index<1>, v.mE };
        }
    };



    namespace internal
    {

        template<typename T>
        struct ResultWrapperHelper;
        template<>
        struct ResultWrapperHelper<void>
        {
            using type = error_code;
        };
        template<typename T>
        struct ResultWrapperHelper
        {
            using type = Result<T, error_code>;
        };


        template<typename T>
        class ResultWrapper : public Resumable
        {
        public:

            using value_type = typename internal::ResultWrapperHelper<T>::type;
            using type = Proto<value_type>;

            internal::InlinePoly<Resumable, inlineSize> mBase;

            enum class Status
            {
                Init,
                InProgress,
                Done
            };
            Status mStatus = Status::Init;

            value_type mRes = Err(make_error_code(code::success));
            std::exception_ptr mExPtr = nullptr;

            ResultWrapper() = delete;
            ResultWrapper(const ResultWrapper&) = delete;

            ResultWrapper(internal::InlinePoly<Resumable, inlineSize>&& o)
                : mBase(std::move(o))
            {}

            ResultWrapper(ResultWrapper&& o)
                : mBase(std::move(o.mBase))
            {
            }

            error_code resume_(Scheduler& sched) override {

                error_code ec;
                if (mStatus == Status::Init)
                {
                    mStatus = Status::InProgress;
                    assert(mBase->done() == false);

                    sched.addDep(*this, *mBase.get());

                    ec = sched.resume(mBase.get());
                    if (ec == code::suspend)
                        return ec;
                }

                if (mStatus == Status::InProgress)
                {
                    mStatus = Status::Done;
                    assert(mBase->done());
                    sched.fulfillDep(*this, {}, nullptr);
                }
                else
                    assert(0 && COPROTO_LOCATION);

                return {};
            };

#ifdef COPROTO_LOGGING
            std::string getName() override
            {
                return mBase->getName() + "_wrap";
            }
#endif

            void* getValue() override
            {

                if constexpr (!std::is_same_v<error_code, value_type>) {
                    if (!mRes.error()) {
                        auto v = (T*)mBase.get()->getValue();
                        assert(v);

                        T& vv = *v;
                        mRes = Ok(std::move(vv));
                    }
                }

                return &mRes;
            }

            bool done() override {
                return mStatus == Status::Done;
            }

            void setError(error_code ec, std::exception_ptr p) override {
                assert(ec);
                mRes = Err(std::move(ec));
                mExPtr = std::move(p);
            }
            std::exception_ptr getExpPtr() override {
                return mExPtr;
            }

            error_code getErrorCode() override {
                if constexpr (!std::is_same_v<error_code, value_type>)
                {
                    if (mRes.hasError())
                        return mRes.error();
                    return {};
                }
                else
                {
                    return mRes;
                }
            }
        };



    }

}