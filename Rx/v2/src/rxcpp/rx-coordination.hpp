// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_COORDINATION_HPP)
#define RXCPP_RX_COORDINATION_HPP

#include "rx-includes.hpp"

namespace rxcpp {

/// note by returnzer0:
/// 声明 tag_coordinator 和 tag_coordination 
/// 声明 tag_coordinator 和 tag_coordination 的类型判断模板函数

struct tag_coordinator {};
struct coordinator_base {typedef tag_coordinator coordinator_tag;};

template<class T, class C = rxu::types_checked>
struct is_coordinator : public std::false_type {};

template<class T>
struct is_coordinator<T, typename rxu::types_checked_from<typename T::coordinator_tag>::type>
    : public std::is_convertible<typename T::coordinator_tag*, tag_coordinator*> {};

struct tag_coordination {};
struct coordination_base {typedef tag_coordination coordination_tag;};

namespace detail {

template<class T, class C = rxu::types_checked>
struct is_coordination : public std::false_type {};

template<class T>
struct is_coordination<T, typename rxu::types_checked_from<typename T::coordination_tag>::type>
    : public std::is_convertible<typename T::coordination_tag*, tag_coordination*> {};

}

template<class T, class Decayed = rxu::decay_t<T>>
struct is_coordination : detail::is_coordination<Decayed>
{
};

template<class Coordination, class DecayedCoordination = rxu::decay_t<Coordination>>
using coordination_tag_t = typename DecayedCoordination::coordination_tag;

/// note by returnzer0:
///          coordinator_base               coordinator_base : coordinator_tag
///                 |
///             coordinator

template<class Input>
class coordinator : public coordinator_base
{
public:
    typedef Input input_type;

private:
    struct not_supported {typedef not_supported type;};

    template<class Observable>
    struct get_observable
    {
        typedef decltype((*(input_type*)nullptr).in((*(Observable*)nullptr))) type;
    };

    template<class Subscriber>
    struct get_subscriber
    {
        typedef decltype((*(input_type*)nullptr).out((*(Subscriber*)nullptr))) type;
    };

    template<class F>
    struct get_action_function
    {
        typedef decltype((*(input_type*)nullptr).act((*(F*)nullptr))) type;
    };

public:
    input_type input;

    /// note by returnzer0:
    /// Switch Class T 的类型，如果是 action_function 、 observable 、 subscriber 中的一个，就调用 private 域中
    /// 的 get 函数取出他的类型然后定义 Type 。

    template<class T>
    struct get
    {
        typedef typename std::conditional<
            rxsc::detail::is_action_function<T>::value, get_action_function<T>, typename std::conditional<
            is_observable<T>::value, get_observable<T>, typename std::conditional<
            is_subscriber<T>::value, get_subscriber<T>, not_supported>::type>::type>::type::type type;
    };

    /// note by returnzer0:
    /// 使用 Input 类型初始化 coordinator 
    /// (Input 类型的就是 identity_one_worker 和 serialize_one_worker 类型的input_type 他自己，这样下面定义的几个函数 in,out,act,都是递归调用
    /// 传入coordinator实例的成员函数 )

    coordinator(Input i) : input(i) {}

    rxsc::worker get_worker() const {
        return input.get_worker();
    }
    rxsc::scheduler get_scheduler() const {
        return input.get_scheduler();
    }

    template<class Observable>
    auto in(Observable o) const
        -> typename get_observable<Observable>::type {
        return input.in(std::move(o));
        static_assert(is_observable<Observable>::value, "can only synchronize observables");
    }

    template<class Subscriber>
    auto out(Subscriber s) const
        -> typename get_subscriber<Subscriber>::type {
        return input.out(std::move(s));
        static_assert(is_subscriber<Subscriber>::value, "can only synchronize subscribers");
    }

    template<class F>
    auto act(F f) const
        -> typename get_action_function<F>::type {
        return input.act(std::move(f));
        static_assert(rxsc::detail::is_action_function<F>::value, "can only synchronize action functions");
    }
};

class identity_one_worker : public coordination_base
{
    /// note by returnzer0:
    /// identity_one_worker 类中只存在一个 scheduler 类型的 factory
    /// 还有 input_type 去实例化 coordinator

    rxsc::scheduler factory;

    class input_type
    {
        rxsc::worker controller;
        rxsc::scheduler factory;
    public:
        explicit input_type(rxsc::worker w)
            : controller(w)
            , factory(rxsc::make_same_worker(w))
        {
        }
        inline rxsc::worker get_worker() const {
            return controller;
        }
        inline rxsc::scheduler get_scheduler() const {
            return factory;
        }
        inline rxsc::scheduler::clock_type::time_point now() const {
            return factory.now();
        }
        template<class Observable>
        auto in(Observable o) const
            -> Observable {
            return o;
        }
        template<class Subscriber>
        auto out(Subscriber s) const
            -> Subscriber {
            return s;
        }
        template<class F>
        auto act(F f) const
            -> F {
            return f;
        }
    };

public:

    explicit identity_one_worker(rxsc::scheduler sc) : factory(sc) {}

    typedef coordinator<input_type> coordinator_type;

    inline rxsc::scheduler::clock_type::time_point now() const {
        return factory.now();
    }

    /// note by returnzer0:
    /// 创建 coordinator 调用 scheduler 的 create_worker 传入 composite_subscription 生成 worker
    /// 这个创建的过程主要由具体的 scheduler 进行实现（scheduler_interface 中定义了create_worker 的接口 ）
    /// 用 worker 初始化 input_type ，然后使用 input_type 初始化 coordinator

    inline coordinator_type create_coordinator(composite_subscription cs = composite_subscription()) const {
        auto w = factory.create_worker(std::move(cs));
        return coordinator_type(input_type(std::move(w)));
    }
};

/// note by returnzer0:
/// 以下是三个函数传不同的 scheduler 实例化 identity_one_worker

inline identity_one_worker identity_immediate() {
    static identity_one_worker r(rxsc::make_immediate());
    return r;
}

inline identity_one_worker identity_current_thread() {
    static identity_one_worker r(rxsc::make_current_thread());
    return r;
}

inline identity_one_worker identity_same_worker(rxsc::worker w) {
    return identity_one_worker(rxsc::make_same_worker(w));
}

/// note by returnzer0:
/// 针对异步实现的 scheduler 使用 serialize_one_worker
/// 基本结构类似 identity_one_worker，在identity_one_worker的基础上增加了同步锁

class serialize_one_worker : public coordination_base
{
    rxsc::scheduler factory;

    template<class F>
    struct serialize_action
    {
        F dest;
        std::shared_ptr<std::mutex> lock;
        serialize_action(F d, std::shared_ptr<std::mutex> m)
            : dest(std::move(d))
            , lock(std::move(m))
        {
            if (!lock) {
                std::terminate();
            }
        }
        auto operator()(const rxsc::schedulable& scbl) const
            -> decltype(dest(scbl)) {
            std::unique_lock<std::mutex> guard(*lock);
            return dest(scbl);
        }
    };

    template<class Observer>
    struct serialize_observer
    {
        typedef serialize_observer<Observer> this_type;
        typedef rxu::decay_t<Observer> dest_type;
        typedef typename dest_type::value_type value_type;
        typedef observer<value_type, this_type> observer_type;
        dest_type dest;
        std::shared_ptr<std::mutex> lock;

        serialize_observer(dest_type d, std::shared_ptr<std::mutex> m)
            : dest(std::move(d))
            , lock(std::move(m))
        {
            if (!lock) {
                std::terminate();
            }
        }
        void on_next(value_type v) const {
            std::unique_lock<std::mutex> guard(*lock);
            dest.on_next(v);
        }
        void on_error(std::exception_ptr e) const {
            std::unique_lock<std::mutex> guard(*lock);
            dest.on_error(e);
        }
        void on_completed() const {
            std::unique_lock<std::mutex> guard(*lock);
            dest.on_completed();
        }

        template<class Subscriber>
        static subscriber<value_type, observer_type> make(const Subscriber& s, std::shared_ptr<std::mutex> m) {
            return make_subscriber<value_type>(s, observer_type(this_type(s.get_observer(), std::move(m))));
        }
    };

    class input_type
    {
        rxsc::worker controller;
        rxsc::scheduler factory;
        std::shared_ptr<std::mutex> lock;
    public:
        explicit input_type(rxsc::worker w, std::shared_ptr<std::mutex> m)
            : controller(w)
            , factory(rxsc::make_same_worker(w))
            , lock(std::move(m))
        {
        }
        inline rxsc::worker get_worker() const {
            return controller;
        }
        inline rxsc::scheduler get_scheduler() const {
            return factory;
        }
        inline rxsc::scheduler::clock_type::time_point now() const {
            return factory.now();
        }
        template<class Observable>
        auto in(Observable o) const
            -> Observable {
            return o;
        }
        template<class Subscriber>
        auto out(const Subscriber& s) const
            -> decltype(serialize_observer<decltype(s.get_observer())>::make(s, lock)) {
            return      serialize_observer<decltype(s.get_observer())>::make(s, lock);
        }
        template<class F>
        auto act(F f) const
            ->      serialize_action<F> {
            return  serialize_action<F>(std::move(f), lock);
        }
    };

public:

    explicit serialize_one_worker(rxsc::scheduler sc) : factory(sc) {}

    typedef coordinator<input_type> coordinator_type;

    inline rxsc::scheduler::clock_type::time_point now() const {
        return factory.now();
    }

    inline coordinator_type create_coordinator(composite_subscription cs = composite_subscription()) const {
        auto w = factory.create_worker(std::move(cs));
        std::shared_ptr<std::mutex> lock = std::make_shared<std::mutex>();
        return coordinator_type(input_type(std::move(w), std::move(lock)));
    }
};

inline serialize_one_worker serialize_event_loop() {
    static serialize_one_worker r(rxsc::make_event_loop());
    return r;
}

inline serialize_one_worker serialize_new_thread() {
    static serialize_one_worker r(rxsc::make_new_thread());
    return r;
}

inline serialize_one_worker serialize_same_worker(rxsc::worker w) {
    return serialize_one_worker(rxsc::make_same_worker(w));
}

}

#endif
