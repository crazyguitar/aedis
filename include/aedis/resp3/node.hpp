/* Copyright (c) 2018-2022 Marcelo Zimbres Silva (mzimbres@gmail.com)
 *
 * Distributed under the Boost Software License, Version 1.0. (See
 * accompanying file LICENSE.txt)
 */

#ifndef AEDIS_RESP3_NODE_HPP
#define AEDIS_RESP3_NODE_HPP

#include <aedis/resp3/type.hpp>

#include <string>
#include <vector>

namespace aedis::resp3 {

/** \brief A node in the response tree.
 *  \ingroup high-level-api
 *
 *  Redis responses are the pre-order view of the response tree (see
 *  https://en.wikipedia.org/wiki/Tree_traversal#Pre-order,_NLR).
 *
 *  \remark Any Redis response can be received in an array of nodes,
 *  for example \c std::vector<node<std::string>>.
 */
template <class String>
struct node {
   /// The RESP3 type of the data in this node.
   type data_type = type::invalid;

   /// The number of elements of an aggregate.
   std::size_t aggregate_size{};

   /// The depth of this node in the response tree.
   std::size_t depth{};

   /// The actual data. For aggregate types this is usually empty.
   String value{};
};

/** @brief Converts the node to a string.
 *  @relates node
 *
 *  @param in The node object.
 */
template <class String>
auto to_string(node<String> const& in)
{
   std::string out;
   out += std::to_string(in.depth);
   out += '\t';
   out += to_string(in.data_type);
   out += '\t';
   out += std::to_string(in.aggregate_size);
   out += '\t';
   if (!is_aggregate(in.data_type))
      out.append(in.value.data(), in.value.size());

   return out;
}

/** @brief Compares a node for equality.
 *  @relates node
 *
 *  @param a Left hand side node object.
 *  @param b Right hand side node object.
 */
template <class String>
auto operator==(node<String> const& a, node<String> const& b)
{
   return a.aggregate_size == b.aggregate_size
       && a.depth == b.depth
       && a.data_type == b.data_type
       && a.value == b.value;
};

/** @brief Writes the node string to the stream.
 *  @relates node
 *
 *  @param os Output stream.
 *  @param node Node object.
 *
 *  \remark Binary data is not converted to text.
 */
template <class String>
auto operator<<(std::ostream& os, node<String> const& node) -> std::ostream&
{
   os << to_string(node);
   return os;
}

} // aedis::resp3

#endif // AEDIS_RESP3_NODE_HPP
