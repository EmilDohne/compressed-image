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
            template <typename... Iterators>
            struct iterator
            {
                using iterator_category = std::forward_iterator_tag;
                using difference_type = std::ptrdiff_t;
                using value_type = std::tuple<typename std::iterator_traits<Iterators>::reference...>;
                using pointer = std::tuple<typename std::iterator_traits<Iterators>::pointer...>;
                using reference = std::tuple<typename std::iterator_traits<Iterators>::reference...>;

                explicit iterator(Iterators... iters) : data_{ iters... } {}

                value_type operator*() 
                {
                    return dereference(std::index_sequence_for<Iterators...>{});
                }

                bool operator==(const iterator& other) const { return equal(std::index_sequence_for<Iterators...>{}, other); }

                bool operator!=(const iterator& other) const { return !equal(std::index_sequence_for<Iterators...>{}, other); }

                iterator& operator++()
                {
                    advance(std::index_sequence_for<Iterators...>{}, 1);
                    return *this;
                }

                iterator operator+(const std::size_t offset) const
                {
                    auto it = *this;
                    it.advance(std::index_sequence_for<Iterators...>{}, offset);
                    return it;
                }

                iterator operator+(const iterator& other) const
                {
                    auto it = *this;
                    const auto distance = std::distance(it, other);
                    it.advance(std::index_sequence_for<Iterators...>{}, distance);
                    return it;
                }

                iterator& operator--()
                {
                    advance(std::index_sequence_for<Iterators...>{}, -1);
                    return *this;
                }

                iterator operator-(const int offset) const
                {
                    auto it = *this;
                    it.advance(std::index_sequence_for<Iterators...>{}, -offset);
                    return it;
                }

                iterator operator-(const iterator& other) const
                {
                    auto iterator = *this;
                    const auto distance = std::distance(other, iterator);
                    iterator.advance(std::index_sequence_for<Iterators...>{}, -distance);
                    return iterator;
                }

            private:

                template <size_t... I>
                value_type dereference(std::index_sequence<I...>)
                {
                    return std::tie(*std::get<I>(data_)...);
                }

                template <size_t... I>
                bool equal(std::index_sequence<I...>, const iterator& other) const
                {
                    return ((std::get<I>(data_) == std::get<I>(other.data_)) || ...);
                }

                template <std::size_t... I>
                void advance(std::index_sequence<I...>, const int offset)
                {
                    ((std::advance(std::get<I>(data_), offset)), ...);
                }

                
                std::tuple<Iterators...> data_;
            };
        }
        

        /// Adapted from https://debashish-ghosh.medium.com/lets-iterate-together-fd7f5e49672b.
        /// and https://github.com/andreiavrammsd/cpp-zip/blob/master/include/msd/zip.hpp zip() implementation.
        template <typename... T>
        struct zip
        {
            
            using iterator = detail::iterator<typename std::conditional_t<std::is_const_v<T>, typename T::const_iterator, typename T::iterator>...>;
            using const_iterator = detail::iterator<typename T::const_iterator...>;
            using value_type = typename iterator::value_type;


            /// Zip constructor taking a tuple of arguments.
            explicit zip(T &...args) : data_{ args... } {}
            iterator begin() const { return begin_impl<iterator>(std::index_sequence_for<T...>{}); }
            iterator end() const { return end_impl<iterator>(std::index_sequence_for<T...>{}); }
            const_iterator cbegin() const { return begin_impl<const_iterator>(std::index_sequence_for<T...>{}); }
            const_iterator cend() const { return end_impl<const_iterator>(std::index_sequence_for<T...>{}); }
            [[nodiscard]] std::size_t size() const { return size_impl(std::index_sequence_for<T...>{}); }
            [[nodiscard]] bool empty() const { return begin() == end(); }
            explicit operator bool() const { return !empty(); }
            value_type front()
            {
                assert(!empty());
                return *begin();
            }
            value_type front() const
            {
                assert(!empty());
                return *begin();
            }
            value_type back()
            {
                assert(!empty());
                return *std::prev(begin() + size());
            }
            value_type back() const
            {
                assert(!empty());
                return *std::prev(begin() + size());
            }
            value_type operator[](const std::size_t offset) const
            {
                assert(offset < size());
                return *std::next(begin(), offset);
            }

        private:
            template <typename Iterator, std::size_t... I>
            Iterator begin_impl(std::index_sequence<I...>) const
            {
                return Iterator{ std::get<I>(data_).begin()... };
            }

            template <typename Iterator, std::size_t... I>
            Iterator end_impl(std::index_sequence<I...>) const
            {
                return std::next(Iterator{ std::get<I>(data_).begin()... }, size());
            }

            template <std::size_t... I>
            std::size_t size_impl(std::index_sequence<I...>) const
            {
                return std::min({ std::distance(std::get<I>(data_).begin(), std::get<I>(data_).end())... });
            }

            std::tuple<T&...> data_;
        };
	
    }
}