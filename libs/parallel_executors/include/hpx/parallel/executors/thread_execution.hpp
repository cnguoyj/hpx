//  Copyright (c) 2017 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

/// \file parallel/executors/thread_execution.hpp

#if !defined(HPX_PARALLEL_EXECUTORS_THREAD_EXECUTION_JAN_03_2017_1145AM)
#define HPX_PARALLEL_EXECUTORS_THREAD_EXECUTION_JAN_03_2017_1145AM

#include <hpx/config.hpp>
#if !defined(HPX_COMPUTE_DEVICE_CODE)
#include <hpx/lcos/dataflow.hpp>
#endif
#include <hpx/datastructures/detail/pack.hpp>
#include <hpx/datastructures/tuple.hpp>
#include <hpx/iterator_support/range.hpp>
#include <hpx/lcos/future.hpp>
#include <hpx/lcos/local/futures_factory.hpp>
#include <hpx/parallel/executors/execution.hpp>
#include <hpx/parallel/executors/fused_bulk_execute.hpp>
#include <hpx/runtime/threads/thread_executor.hpp>
#include <hpx/traits/future_access.hpp>
#include <hpx/traits/is_launch_policy.hpp>
#include <hpx/util/bind.hpp>
#include <hpx/util/bind_back.hpp>
#include <hpx/util/deferred_call.hpp>
#include <hpx/util/unwrap.hpp>

#include <algorithm>
#include <type_traits>
#include <utility>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
// define customization point specializations for thread executors
namespace hpx { namespace threads {
    ///////////////////////////////////////////////////////////////////////////
    // async_execute()
    template <typename Executor, typename F, typename... Ts>
    HPX_FORCEINLINE typename std::enable_if<
        hpx::traits::is_threads_executor<Executor>::value,
        hpx::lcos::future<typename hpx::util::detail::invoke_deferred_result<F,
            Ts...>::type>>::type
    async_execute(Executor&& exec, F&& f, Ts&&... ts)
    {
        typedef typename util::detail::invoke_deferred_result<F, Ts...>::type
            result_type;

        lcos::local::futures_factory<result_type()> p(
            std::forward<Executor>(exec),
            util::deferred_call(std::forward<F>(f), std::forward<Ts>(ts)...));
        p.apply();
        return p.get_future();
    }

    ///////////////////////////////////////////////////////////////////////////
    // sync_execute()
    template <typename Executor, typename F, typename... Ts>
    HPX_FORCEINLINE typename std::enable_if<
        hpx::traits::is_threads_executor<Executor>::value,
        typename hpx::util::detail::invoke_deferred_result<F,
            Ts...>::type>::type
    sync_execute(Executor&& exec, F&& f, Ts&&... ts)
    {
        return async_execute(std::forward<Executor>(exec), std::forward<F>(f),
            std::forward<Ts>(ts)...)
            .get();
    }

    ///////////////////////////////////////////////////////////////////////////
    // then_execute()
    template <typename Executor, typename F, typename Future, typename... Ts>
    HPX_FORCEINLINE typename std::enable_if<
        hpx::traits::is_threads_executor<Executor>::value,
        hpx::lcos::future<typename hpx::util::detail::invoke_deferred_result<F,
            Future, Ts...>::type>>::type
    then_execute(Executor&& exec, F&& f, Future&& predecessor, Ts&&... ts)
    {
        typedef typename hpx::util::detail::invoke_deferred_result<F, Future,
            Ts...>::type result_type;

        auto func = hpx::util::one_shot(
            hpx::util::bind_back(std::forward<F>(f), std::forward<Ts>(ts)...));

        typename hpx::traits::detail::shared_state_ptr<result_type>::type p =
            hpx::lcos::detail::make_continuation_exec<result_type>(
                std::forward<Future>(predecessor), std::forward<Executor>(exec),
                std::move(func));

        return hpx::traits::future_access<
            hpx::lcos::future<result_type>>::create(std::move(p));
    }

    ///////////////////////////////////////////////////////////////////////////
    // post()
    template <typename Executor, typename F, typename... Ts>
    HPX_FORCEINLINE typename std::enable_if<
        hpx::traits::is_threads_executor<Executor>::value>::type
    post(Executor&& exec, F&& f, Ts&&... ts)
    {
        exec.add(
            util::deferred_call(std::forward<F>(f), std::forward<Ts>(ts)...),
            "hpx::parallel::execution::post", threads::pending, true,
            exec.get_stacksize(), threads::thread_schedule_hint(), throws);
    }
    ///////////////////////////////////////////////////////////////////////////
    // post()
    template <typename Executor, typename F, typename Hint, typename... Ts>
    HPX_FORCEINLINE typename std::enable_if<
        hpx::traits::is_threads_executor<Executor>::value &&
        std::is_same<typename hpx::util::decay<Hint>::type,
            hpx::threads::thread_schedule_hint>::value>::type
    post(Executor&& exec, F&& f, Hint&& hint, Ts&&... ts)
    {
        exec.add(
            util::deferred_call(std::forward<F>(f), std::forward<Ts>(ts)...),
            "hpx::parallel::execution::post", threads::pending, true,
            exec.get_stacksize(), std::forward<Hint>(hint), throws);
    }

    ///////////////////////////////////////////////////////////////////////////
    // bulk_async_execute()
    template <typename Executor, typename F, typename Shape, typename... Ts>
    typename std::enable_if<hpx::traits::is_threads_executor<Executor>::value,
        std::vector<hpx::lcos::future<typename parallel::execution::detail::
                bulk_function_result<F, Shape, Ts...>::type>>>::type
    bulk_async_execute(Executor&& exec, F&& f, Shape const& shape, Ts&&... ts)
    {
        std::vector<hpx::future<typename parallel::execution::detail::
                bulk_function_result<F, Shape, Ts...>::type>>
            results;
        results.reserve(util::size(shape));

        for (auto const& elem : shape)
        {
            results.push_back(
                async_execute(exec, std::forward<F>(f), elem, ts...));
        }

        return results;
    }

    ///////////////////////////////////////////////////////////////////////////
    // bulk_sync_execute()
    template <typename Executor, typename F, typename Shape, typename... Ts>
    typename std::enable_if<hpx::traits::is_threads_executor<Executor>::value,
        typename parallel::execution::detail::bulk_execute_result<F, Shape,
            Ts...>::type>::type
    bulk_sync_execute(Executor&& exec, F&& f, Shape const& shape, Ts&&... ts)
    {
        std::vector<hpx::future<typename parallel::execution::detail::
                bulk_function_result<F, Shape, Ts...>::type>>
            results;
        results.reserve(util::size(shape));

        for (auto const& elem : shape)
        {
            results.push_back(
                async_execute(exec, std::forward<F>(f), elem, ts...));
        }

        return hpx::util::unwrap(results);
    }

    ///////////////////////////////////////////////////////////////////////////
    // bulk_then_execute()
    template <typename Executor, typename F, typename Shape, typename Future,
        typename... Ts>
    HPX_FORCEINLINE typename std::enable_if<
        hpx::traits::is_threads_executor<Executor>::value,
        hpx::future<typename parallel::execution::detail::
                bulk_then_execute_result<F, Shape, Future, Ts...>::type>>::type
    bulk_then_execute(Executor&& exec, F&& f, Shape const& shape,
        Future&& predecessor, Ts&&... ts)
    {
        using func_result_type =
            typename parallel::execution::detail::then_bulk_function_result<F,
                Shape, Future, Ts...>::type;

        // std::vector<future<func_result_type>>
        using result_type = std::vector<hpx::future<func_result_type>>;

        auto&& func =
            parallel::execution::detail::make_fused_bulk_async_execute_helper<
                result_type>(exec, std::forward<F>(f), shape,
                hpx::util::make_tuple(std::forward<Ts>(ts)...));

        // void or std::vector<func_result_type>
        using vector_result_type =
            typename parallel::execution::detail::bulk_then_execute_result<F,
                Shape, Future, Ts...>::type;

        // future<vector_result_type>
        using result_future_type = hpx::future<vector_result_type>;

        using shared_state_type =
            typename hpx::traits::detail::shared_state_ptr<
                vector_result_type>::type;

        using future_type = typename std::decay<Future>::type;

        // vector<future<func_result_type>> -> vector<func_result_type>
        shared_state_type p =
            lcos::detail::make_continuation_exec<vector_result_type>(
                std::forward<Future>(predecessor), std::forward<Executor>(exec),
                [HPX_CAPTURE_MOVE(func)](
                    future_type&& predecessor) mutable -> vector_result_type {
                    // use unwrap directly (instead of lazily) to avoid
                    // having to pull in dataflow
                    return hpx::util::unwrap(func(std::move(predecessor)));
                });

        return hpx::traits::future_access<result_future_type>::create(
            std::move(p));
    }
}}    // namespace hpx::threads

#endif
