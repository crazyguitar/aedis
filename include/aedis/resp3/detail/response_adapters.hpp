/* Copyright (c) 2019 - 2021 Marcelo Zimbres Silva (mzimbres at gmail dot com)
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include <set>
#include <optional>
#include <system_error>
#include <map>
#include <list>
#include <deque>
#include <vector>
#include <charconv>

#include <aedis/command.hpp>
#include <aedis/resp3/type.hpp>
#include <aedis/resp3/node.hpp>
#include <aedis/resp3/serializer.hpp>
#include <aedis/resp3/adapter_error.hpp>

namespace aedis {
namespace resp3 {

namespace detail
{

struct adapter_ignore {
   void
   operator()(
      type, std::size_t, std::size_t, char const*, std::size_t,
      std::error_code&) { }
};

template <class T>
typename std::enable_if<std::is_integral<T>::value, void>::type
from_string(
   T& i,
   char const* data,
   std::size_t data_size,
   std::error_code& ec)
{
   auto const res = std::from_chars(data, data + data_size, i);
   if (res.ec != std::errc())
      ec = std::make_error_code(res.ec);
}

template <class CharT, class Traits, class Allocator>
void
from_string(
   std::basic_string<CharT, Traits, Allocator>& s,
   char const* data,
   std::size_t data_size,
   std::error_code&)
{
  s.assign(data, data_size);
}

void set_on_resp3_error(type t, std::error_code& ec)
{
   switch (t) {
      case type::simple_error:
	 ec = adapter_error::simple_error;
	 return;
      case type::blob_error:
	 ec = adapter_error::blob_error;
	 return;
      default: return;
   }
}

/** A general pupose redis response class
  
    A pre-order-view of the response tree.
 */
template <class Container>
class adapter_general {
private:
   Container* result_;

public:
   adapter_general(Container& c = nullptr): result_(&c) {}

   /** @brief Function called by the parser when new data has been processed.
    *  
    *  Users who what to customize their response types are required to derive
    *  from this class and override this function, see examples.
    *
    *  \param t The RESP3 type of the data.
    *
    *  \param n When t is an aggregate data type this will contain its size
    *     (see also element_multiplicity) for simple data types this is always 1.
    *
    *  \param depth The element depth in the tree.
    *
    *  \param data A pointer to the data.
    *
    *  \param size The size of data.
    */
   void
   operator()(
      type t,
      std::size_t n,
      std::size_t depth,
      char const* data,
      std::size_t size,
      std::error_code&)
      {
	 result_->emplace_back(t, n, depth, std::string{data, size});
      }
};

// Adapter for simple data types.
template <class Node>
class adapter_node {
private:
   Node* result_;

public:
   adapter_node(Node& t) : result_(&t) {}

   void
   operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t data_size,
      std::error_code&)
   {
     result_->data_type = t;
     result_->aggregate_size = aggregate_size;
     result_->depth = depth;
     result_->data.assign(data, data_size);
   }
};

// Adapter for simple data types.
template <class T>
class adapter_simple {
private:
   T* result_;

public:
   adapter_simple(T& t) : result_(&t) {}

   void
   operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t data_size,
      std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (is_aggregate(t)) {
	 ec == adapter_error::expects_simple_type;
	 return;
      }

      assert(aggregate_size == 1);

      if (depth != 0) {
	 ec == adapter_error::nested_unsupported;
	 return;
      }

      from_string(*result_, data, data_size, ec);
   }
};

template <class T>
class adapter_optional_simple {
private:
  std::optional<T>* result_;

public:
   adapter_optional_simple(std::optional<T>& o) : result_(&o) {}

   void
   operator()(
      type t,
      std::size_t aggregate_size,
      std::size_t depth,
      char const* data,
      std::size_t data_size,
      std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (is_aggregate(t)) {
	 ec == adapter_error::expects_simple_type;
	 return;
      }

      assert(aggregate_size == 1);

      if (depth != 0) {
	 ec == adapter_error::nested_unsupported;
	 return;
      }

      if (t == type::null)
         return;

      if (!result_->has_value())
        *result_ = T{};

      from_string(result_->value(), data, data_size, ec);
   }
};

/* A response type that parses the response directly in a vector.
 */
template <class Container>
class adapter_vector {
private:
   int i_ = -1;
   Container* result_;

public:
   adapter_vector(Container& v) : result_{&v} {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size,
       std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (is_aggregate(t)) {
	 if (depth != 0 || i_ != -1) {
	    ec == adapter_error::nested_unsupported;
	    return;
	 }

         auto const m = element_multiplicity(t);
         result_->resize(m * aggregate_size);
         ++i_;
      } else {
	 if (depth != 1) {
	    ec == adapter_error::nested_unsupported;
	    return;
	 }

	 assert(aggregate_size == 1);

         from_string(result_->at(i_), data, data_size, ec);
         ++i_;
      }
   }
};

template <class Container>
class adapter_list {
private:
   Container* result_;

public:
   adapter_list(Container& ref): result_(&ref) {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size,
       std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (is_aggregate(t)) {
	 if (depth != 0) {
	    ec == adapter_error::nested_unsupported;
	    return;
	 }
         return;
      }

      assert(aggregate_size == 1);

      if (depth != 1) {
	 ec == adapter_error::nested_unsupported;
	 return;
      }

      result_->push_back({});
      from_string(result_->back(), data, data_size, ec);
   }
};

template <class Container>
class adapter_set {
private:
   Container* result_;
   Container::iterator hint_;

public:
   adapter_set(Container& c)
   : result_(&c)
   , hint_(std::end(c))
   {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size,
       std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (t == type::set) {
        assert(depth == 0);
        return;
      }

      assert(!is_aggregate(t));

      assert(depth == 1);
      assert(aggregate_size == 1);

      typename Container::key_type obj;
      from_string(obj, data, data_size, ec);
      if (hint_ == std::end(*result_)) {
         auto const ret = result_->insert(std::move(obj));
         hint_ = ret.first;
      } else {
         hint_ = result_->insert(hint_, std::move(obj));
      }
   }
};

template <class Container>
class adapter_map {
private:
   Container* result_;
   Container::iterator current_;
   bool on_key_ = true;

public:
   adapter_map(Container& c)
   : result_(&c)
   , current_(std::end(c))
   {}

   void
   operator()(type t,
       std::size_t aggregate_size,
       std::size_t depth,
       char const* data,
       std::size_t data_size,
       std::error_code& ec)
   {
      set_on_resp3_error(t, ec);

      if (t == type::map) {
        assert(depth == 0);
        return;
      }

      assert(!is_aggregate(t));
      assert(depth == 1);
      assert(aggregate_size == 1);

      if (on_key_) {
         typename Container::key_type obj;
         from_string(obj, data, data_size, ec);
         current_ = result_->insert(current_, {std::move(obj), {}});
      } else {
         typename Container::mapped_type obj;
         from_string(obj, data, data_size, ec);
         current_->second = std::move(obj);
      }

      on_key_ = !on_key_;
   }
};

} // detail
} // resp3
} // aedis