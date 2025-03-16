#pragma once

#include <tuple>
#include <iterator>
#include <ranges>

#include "macros.h"


namespace NAMESPACE_COMPRESSED_IMAGE
{

	namespace ranges
	{

        namespace detail
        {
            template <typename... _Tp>
            bool variadic_or(_Tp &&...args)
            {
                return (... || args);
            }

            template <typename Tuple, std::size_t... I>
            bool any_equals(Tuple&& t1, Tuple&& t2, std::index_sequence<I...>)
            {
                return variadic_or(std::get<I>(std::forward<Tuple>(t1)) == std::get<I>(std::forward<Tuple>(t2))...);
            }
        }

        /// Copied from https://debashish-ghosh.medium.com/lets-iterate-together-fd7f5e49672b.
        /// zip() implementation that is near-identical to std::ranges::zip() which is C++23 only
        template <typename... T>
        struct zip
        {
            struct iterator
            {
                using iterator_category = std::forward_iterator_tag;
                using value_type = std::tuple<std::iter_value_t<std::ranges::iterator_t<T>>...>;
                using reference = std::tuple<std::iter_reference_t<std::ranges::iterator_t<T>>...>;
                using difference_type = std::ptrdiff_t;
                using pointer = std::tuple<typename std::iterator_traits<std::ranges::iterator_t<T>>::pointer...>;

                reference operator*()
                {
                    return std::apply([](auto&... e) { return reference(*e...); }, data_);
                }

                iterator& operator++()
                {
                    std::apply([](auto&... e) { ((++e), ...); }, data_);
                    return *this;
                }

                iterator operator++(int)
                {
                    iterator temp = *this;
                    std::apply([](auto&... e) { ((e++), ...); }, data_);  // Post-increment
                    return temp;
                }

                auto operator!=(const iterator& iter) const
                {
                    return !detail::any_equals(data_, iter.data_, std::index_sequence_for<T...>{});
                }

                bool operator==(const iterator& iter) const
                {
                    return detail::any_equals(data_, iter.data_, std::index_sequence_for<T...>{});
                }

                difference_type operator-(const iterator& other) const
                {
                    return std::get<0>(data_) - std::get<0>(other.data_);
                }

                std::tuple<std::ranges::iterator_t<T>...> data_;
            };

            zip(T &...args) : data(std::forward_as_tuple(std::forward<T>(args)...)) {}

            auto begin()
            {
                return iterator{ std::apply(
                    []<typename... _Tp>(_Tp && ...e) { return std::make_tuple(std::begin(std::forward<_Tp>(e))...); }, data) };
            }

            auto end()
            {
                return iterator{ std::apply(
                    []<typename... _Tp>(_Tp && ...e) { return std::make_tuple(std::end(std::forward<_Tp>(e))...); }, data) };
            }

            auto size() const
            {
                return std::apply([](auto&&... e) { return std::min({ std::ranges::size(e)... }); }, data);
            }

            std::tuple<T &...> data;
        };
	
    }
}