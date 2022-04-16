/* Copyright (c) 2019 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <vector>
#include <tuple>

#include <boost/mp11.hpp>
#include <boost/variant2.hpp>

#include <aedis/resp3/type.hpp>
#include <aedis/resp3/read.hpp>
#include <aedis/adapter/detail/adapters.hpp>
#include <aedis/adapter/error.hpp>

namespace aedis {
namespace adapter {

/** @brief Traits class for response objects.
 *  @ingroup any
 *
 *  Provides traits for all supported response types i.e. all STL containers
 *  and C++ buil-in types.
 */
template <class ResponseType>
struct response_traits
{
   /// The adapter type.
   using adapter_type = adapter::detail::wrapper<ResponseType>;

   /** @brief Returns an adapter for the reponse r
    *
    *  @param r The response object e.g a C++ container.
    *  @return An adapter suitable for use in resp3::read or resp3::async_read.
    *  @remark Users can also use the free adapt function for type deduction.
    */
   static auto adapt(ResponseType& r) noexcept { return adapter_type{&r}; }
};

/// Template typedef for response_traits.
template <class T>
using adapter_t = typename response_traits<T>::adapter_type;

template <class T>
struct response_traits<resp3::node<T>>
{
   using response_type = resp3::node<T>;
   using adapter_type = adapter::detail::general_simple<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <class String, class Allocator>
struct response_traits<std::vector<resp3::node<String>, Allocator>>
{
   using response_type = std::vector<resp3::node<String>, Allocator>;
   using adapter_type = adapter::detail::general_aggregate<response_type>;
   static auto adapt(response_type& v) noexcept { return adapter_type{&v}; }
};

template <>
struct response_traits<void>
{
   using response_type = void;
   using adapter_type = resp3::detail::ignore_response;
   static auto adapt() noexcept { return adapter_type{}; }
};

namespace detail {

// Duplicated here to avoid circular include dependency.
template<class T>
auto internal_adapt(T& t) noexcept
   { return response_traits<T>::adapt(t); }

template <std::size_t N>
struct assigner {
  template <class T1, class T2>
  static void assign(T1& dest, T2& from)
  {
     dest[N] = internal_adapt(std::get<N>(from));
     assigner<N - 1>::assign(dest, from);
  }
};

template <>
struct assigner<0> {
  template <class T1, class T2>
  static void assign(T1& dest, T2& from)
  {
     dest[0] = internal_adapt(std::get<0>(from));
  }
};

template <class Tuple>
class static_aggregate_adapter {
private:
   using adapters_array_type = 
      std::array<
         boost::mp11::mp_unique<
            boost::mp11::mp_rename<
               boost::mp11::mp_transform<
                  adapter_t, Tuple>,
                  boost::variant2::variant>>,
         std::tuple_size<Tuple>::value>;

   std::size_t i_ = 0;
   std::size_t aggregate_size_ = 0;
   adapters_array_type adapters_;

public:
   static_aggregate_adapter(Tuple* r = nullptr)
   {
      detail::assigner<std::tuple_size<Tuple>::value - 1>::assign(adapters_, *r);
   }

   void count(resp3::node<boost::string_view> const& nd)
   {
      if (nd.depth == 1) {
         if (is_aggregate(nd.data_type))
            aggregate_size_ = element_multiplicity(nd.data_type) * nd.aggregate_size;
         else
            ++i_;

         return;
      }

      if (--aggregate_size_ == 0)
         ++i_;
   }

   void
   operator()(
      resp3::node<boost::string_view> const& nd,
      boost::system::error_code& ec)
   {
      using boost::variant2::visit;

      if (nd.depth == 0) {
         auto const real_aggr_size = nd.aggregate_size * element_multiplicity(nd.data_type);
         if (real_aggr_size != std::tuple_size<Tuple>::value)
	    ec = error::incompatible_size;

         return;
      }

      visit([&](auto& arg){arg(nd, ec);}, adapters_[i_]);
      count(nd);
   }
};

} // detail

template <class... Ts>
struct response_traits<std::tuple<Ts...>>
{
   using response_type = std::tuple<Ts...>;
   using adapter_type = detail::static_aggregate_adapter<response_type>;
   static auto adapt(response_type& r) noexcept { return adapter_type{&r}; }
};

} // adapter
} // aedis