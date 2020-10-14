#pragma once
#include "error_code.h"
#include <variant>
#include <coroutine>

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

        //static_assert(!std::is_same_v< value_type, error_type>, "");

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


        bool hasValue()
        {
            //return std::holds_alternative<value_type>(var());
            return var().index() == 0;
        }

        bool hasError()
        {
            return !hasValue();
        }

        operator bool()
        {
            return hasError();
        }

        value_type& unwrap()
        {
            if (hasError())
                throw std::runtime_error("unwrap() was called on a Result<T,E> which stores an error_type");

            return std::get<0>(var());
        }

        const value_type& unwrap() const
        {
            if (hasError())
                throw std::runtime_error("unwrap() was called on a Result<T,E> which stores an error_type");

            return std::get<0>(var());
        }

        value_type& unwrapOr(value_type& alt)
        {
            if (hasError())
                return alt;
            return std::get<0>(var());
        }

        const value_type& unwrapOr(const value_type& alt) const
        {
            if (hasError())
                return alt;
            return std::get<0>(var());
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

        //struct promise_type
        //{
        //    std::variant<value_type, error_type> mVar;

        //    Result get_return_object() {
        //        return std::coroutine_handle<promise_type>::from_promise(*this);
        //    }
        //    std::suspend_never initial_suspend() { return {}; }
        //    std::suspend_never final_suspend() noexcept { return {}; }
        //    void unhandled_exception() {
        //        auto exceptionPtr = std::current_exception();
        //        if (exceptionPtr)
        //            std::rethrow_exception(exceptionPtr);
        //    }
        //    void return_value(Result value) { mVar = value; };

        //    struct ResultAwaiter {
        //        Result mRes;

        //        ResultAwaiter(Result&& res) : mRes(std::move(res)) {}

        //        bool await_ready() { return true; }
        //        void await_suspend(std::coroutine_handle<promise_type> coro_handle) {}
        //        Result await_resume() {
        //            return mRes;
        //        }
        //    };

        //    ResultAwaiter await_transform(Result&& s) {
        //        return ResultAwaiter(std::move(s));
        //    }


        //    struct ValueAwaiter {
        //        value_type mRes;

        //        ValueAwaiter(value_type&& res) : mRes(std::move(res)) {}

        //        bool await_ready() { return true; }
        //        void await_suspend(std::coroutine_handle<promise_type> coro_handle) {}
        //        value_type await_resume() {
        //            return mRes;
        //        }
        //    };

        //    ValueAwaiter await_transform(value_type&& s) {
        //        return ValueAwaiter(std::move(s));
        //    }

        //    struct ErrorAwaiter {
        //        error_type mRes;

        //        ErrorAwaiter(error_type&& res) : mRes(std::move(res)) {}

        //        bool await_ready() { return true; }
        //        void await_suspend(std::coroutine_handle<promise_type> coro_handle) {}
        //        error_type await_resume() {
        //            return mRes;
        //        }
        //    };

        //    ErrorAwaiter await_transform(error_type&& s) {
        //        return ErrorAwaiter(std::move(s));
        //    }


        //};

        //using coro_handle = std::coroutine_handle<promise_type>;

        ////private:
        //coro_handle coroutine_handle;

        //Result(coro_handle handle)
        //    : coroutine_handle(handle)
        //{}
    };
}