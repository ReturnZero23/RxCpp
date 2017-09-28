
// Copyright (c) Microsoft Open Technologies, Inc. All rights reserved. See License.txt in the project root for license information.

#pragma once

#if !defined(RXCPP_RX_SOURCES_HPP)
#define RXCPP_RX_SOURCES_HPP

#include "rx-includes.hpp"

namespace rxcpp {

namespace sources {

struct tag_source {};
template<class T>
struct source_base
{
    typedef T value_type;
    typedef tag_source source_tag;
};

/// note by returnzer0:
/// is_XXX() 在RxCpp中大量使用来判断类型
/// 
/// template<class T>
/// class is_source
/// {
///     Part1: template<class C>
///            static typename C::source_tag* check(int);
///
///     Part2: template<class C>
///            static void check(...);
/// public:
///     Part3: static const bool value = std::is_convertible<decltype(check<rxu::decay_t<T>>(0)), tag_source*>::value;
/// };
/// Part1: 声明一个特化版本的check，当调用check<class T>时返回T::XXX_tag
/// Part2: 声明一个泛化版本的check，当调用check<class T>时 T 类型没有 T::XXX_tag 时返回 void 。 
/// Part3: rxu::decay_t<T> 先取出 T 的类型
///        然后调用 check<class T> 取出 T::XXX_tag ，然后判断 T::XXX_tag 和 类型 T::XXX_tag是否是可转换的。

template<class T>
class is_source
{
    template<class C>
    static typename C::source_tag* check(int);
    template<class C>
    static void check(...);
public:
    static const bool value = std::is_convertible<decltype(check<rxu::decay_t<T>>(0)), tag_source*>::value;
};

}
namespace rxs=sources;

}

#include "sources/rx-create.hpp"
#include "sources/rx-range.hpp"
#include "sources/rx-iterate.hpp"
#include "sources/rx-interval.hpp"
#include "sources/rx-empty.hpp"
#include "sources/rx-defer.hpp"
#include "sources/rx-never.hpp"
#include "sources/rx-error.hpp"
#include "sources/rx-scope.hpp"
#include "sources/rx-timer.hpp"

#endif
