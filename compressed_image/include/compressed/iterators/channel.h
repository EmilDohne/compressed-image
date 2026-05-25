#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <type_traits>
#include <variant>
#include <vector>

#include "compressed/blosc2/typedefs.h"
#include "compressed/blosc2/wrapper.h"
#include "compressed/containers/chunk_span.h"
#include "compressed/context.h"
#include "compressed/cuda/compression.h"
#include "compressed/detail/scoped_timer.h"
#include "compressed/enums.h"
#include "compressed/macros.h"
#include "compressed/util.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    template <typename T>
    struct fitted_buffer
    {
        fitted_buffer() = default;

        explicit fitted_buffer(size_t initial_size)
        {
            m_buffer.resize(initial_size);
            m_size = initial_size;
        }

        std::span<T> get()
        {
            return std::span<T>(m_buffer.begin(), m_buffer.begin() + m_size);
        }

        std::span<const T> get() const
        {
            return std::span<const T>(m_buffer.begin(), m_buffer.begin() + m_size);
        }

        void reset()
        {
            m_size = m_buffer.size();
            m_is_fitted = false;
        }

        void ensure_capacity(size_t capacity)
        {
            if (capacity > m_buffer.size())
            {
                m_buffer.resize(capacity);
            }

            if (!m_is_fitted)
            {
                m_size = m_buffer.size();
            }
        }

        void refit(size_t new_size)
        {
            if (new_size > m_buffer.size())
            {
                throw std::invalid_argument(
                    std::format("New size exceeds buffer capacity. Maximum size is {:L}", m_buffer.size())
                );
            }
            m_size = new_size;
            m_is_fitted = true;
        }

        size_t capacity() const noexcept
        {
            return m_buffer.size();
        }

    private:
        util::default_init_vector<T> m_buffer;
        bool m_is_fitted = false;
        size_t m_size = 0;
    };

    template <typename T>
    struct channel_iterator
    {
        using element_type = std::remove_const_t<T>;
        using schunk_pointer = schunk_var_ptr<element_type>;

        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = container::chunk_span<T>;
        using pointer = value_type*;
        using reference = value_type&;

        channel_iterator() = default;

        channel_iterator(
            schunk_pointer schunk,
            size_t chunk_index,
            size_t num_chunks,
            size_t width,
            size_t height,
            enums::codec codec,
            uint8_t compression_level,
            size_t num_threads,
            size_t block_size,
            size_t chunk_size
        )
            : m_state(
                std::make_shared<state>(
                    std::move(schunk),
                    chunk_index,
                    num_chunks,
                    width,
                    height,
                    codec,
                    compression_level,
                    num_threads,
                    block_size,
                    chunk_size
                )
            )
        {
        }

        ~channel_iterator()
        {
            if constexpr (!std::is_const_v<T>)
            {
                try
                {
                    flush();
                }
                catch (...)
                {
                    // Iterators must not throw from destructors.
                }
            }
        }

        reference operator*()
        {
            ensure_dereferenceable();
            load_current_chunk();

            if constexpr (!std::is_const_v<T>)
            {
                m_state->dirty = true;
            }

            return m_state->current_chunk;
        }

        pointer operator->()
        {
            return &operator*();
        }

        channel_iterator& operator++()
        {
            ensure_state();

            if constexpr (!std::is_const_v<T>)
            {
                flush();
            }

            if (m_state->chunk_index < m_state->num_chunks)
            {
                ++m_state->chunk_index;
            }

            m_state->loaded = false;
            return *this;
        }

        channel_iterator operator++(int)
        {
            channel_iterator copy = *this;
            ++(*this);
            return copy;
        }

        bool operator==(const channel_iterator& other) const noexcept
        {
            if (!m_state || !other.m_state)
            {
                return !m_state && !other.m_state;
            }

            return m_state->schunk == other.m_state->schunk && m_state->chunk_index == other.m_state->chunk_index;
        }

        bool operator!=(const channel_iterator& other) const noexcept
        {
            return !(*this == other);
        }

    private:
        struct state
        {
            state(
                schunk_pointer schunk_,
                size_t chunk_index_,
                size_t num_chunks_,
                size_t width_,
                size_t height_,
                enums::codec codec_,
                uint8_t compression_level_,
                size_t num_threads_,
                size_t block_size_,
                size_t chunk_size_
            )
                : schunk(std::move(schunk_)),
                  chunk_index(chunk_index_),
                  num_chunks(num_chunks_),
                  width(width_),
                  height(height_),
                  codec(codec_),
                  compression_level(compression_level_),
                  num_threads(num_threads_),
                  block_size(block_size_),
                  chunk_size(chunk_size_)
            {
            }

            schunk_pointer schunk = nullptr;
            size_t chunk_index = 0;
            size_t num_chunks = 0;
            size_t width = 0;
            size_t height = 0;
            enums::codec codec = enums::codec::lz4;
            uint8_t compression_level = 9;
            size_t num_threads = 1;
            size_t block_size = 0;
            size_t chunk_size = 0;

            fitted_buffer<element_type> decompressed_buffer{};
            fitted_buffer<std::byte> compressed_buffer{};
            value_type current_chunk{};

            std::optional<compression_context_var> context{};
            bool loaded = false;
            bool dirty = false;
        };

        std::shared_ptr<state> m_state{};

        void ensure_state() const
        {
            if (!m_state || !m_state->schunk)
            {
                throw std::runtime_error("Invalid channel iterator state.");
            }
        }

        void ensure_dereferenceable() const
        {
            ensure_state();

            if (m_state->chunk_index >= m_state->num_chunks)
            {
                throw std::out_of_range("Cannot dereference end channel iterator.");
            }
        }

        void ensure_context()
        {
            ensure_state();

            if (m_state->context.has_value())
            {
                return;
            }

            const int gpu_device = enums::is_gpu_codec(m_state->codec) ? cuda::current_device() : 0;
            m_state->context = create_context(
                m_state->codec,
                m_state->num_threads,
                m_state->compression_level,
                m_state->block_size,
                gpu_device
            );
        }

        void load_current_chunk()
        {
            if (m_state->loaded)
            {
                return;
            }

            ensure_context();

            const size_t chunk_elems = std::visit(
                [&](const auto& schunk)
                {
                    return schunk.chunk_elements(m_state->chunk_index);
                },
                *m_state->schunk
            );

            const size_t max_chunk_elems = m_state->chunk_size / sizeof(element_type);
            m_state->decompressed_buffer.ensure_capacity(max_chunk_elems);
            m_state->decompressed_buffer.refit(chunk_elems);

            auto writable_buffer = m_state->decompressed_buffer.get();

            std::visit(
                [&](const auto& schunk)
                {
                    if (enums::is_gpu_codec(m_state->codec))
                    {
                        schunk.chunk(writable_buffer, m_state->chunk_index);
                    }
                    else
                    {
                        auto& cpu_context = std::get<cpu_compression_context>(*m_state->context);
                        schunk.chunk(cpu_context.decompression_ctx.get(), writable_buffer, m_state->chunk_index);
                    }
                },
                *m_state->schunk
            );

            if constexpr (std::is_const_v<T>)
            {
                m_state->current_chunk = value_type(
                    std::span<const element_type>(writable_buffer.data(), writable_buffer.size()),
                    m_state->width,
                    m_state->height,
                    m_state->chunk_index,
                    m_state->chunk_size / sizeof(element_type)
                );
            }
            else
            {
                m_state->current_chunk = value_type(
                    writable_buffer,
                    m_state->width,
                    m_state->height,
                    m_state->chunk_index,
                    m_state->chunk_size / sizeof(element_type)
                );
            }

            m_state->loaded = true;
            m_state->dirty = false;
        }

        void flush()
        {
            if constexpr (std::is_const_v<T>)
            {
                return;
            }
            else
            {
                if (!m_state || !m_state->loaded || !m_state->dirty || m_state->chunk_index >= m_state->num_chunks)
                {
                    return;
                }

                ensure_context();

                auto buffer = m_state->decompressed_buffer.get();

                std::visit(
                    [&](auto& schunk)
                    {
                        if (buffer.size() != schunk.chunk_elements(m_state->chunk_index))
                        {
                            throw std::invalid_argument(
                                std::format(
                                    "Invalid iterator chunk buffer size. Expected {} elements, got {}.",
                                    schunk.chunk_elements(m_state->chunk_index),
                                    buffer.size()
                                )
                            );
                        }

                        if (enums::is_gpu_codec(m_state->codec))
                        {
                            auto& gpu_context = std::get<gpu_compression_context>(*m_state->context);
                            schunk.set_chunk(gpu_context.ctx, buffer, m_state->chunk_index);
                        }
                        else
                        {
                            auto& cpu_context = std::get<cpu_compression_context>(*m_state->context);
                            schunk.set_chunk(cpu_context.compression_ctx, buffer, m_state->chunk_index);
                        }
                    },
                    *m_state->schunk
                );

                m_state->dirty = false;
            }
        }

        static compression_context_var create_context(
            const enums::codec codec,
            const size_t num_threads,
            const size_t compression_level,
            const size_t block_size,
            const int gpu_device
        )
        {
            if (enums::is_gpu_codec(codec))
            {
                return gpu_compression_context{
                    .ctx = cuda::make_compression_context<element_type>(codec, gpu_device, block_size)
                };
            }

            return cpu_compression_context{
                .compression_ctx = blosc2::create_compression_context<element_type>(
                    num_threads,
                    codec,
                    compression_level,
                    block_size
                ),
                .decompression_ctx = blosc2::create_decompression_context(num_threads),
                .nthreads = num_threads
            };
        }
    };
} // NAMESPACE_COMPRESSED_IMAGE
