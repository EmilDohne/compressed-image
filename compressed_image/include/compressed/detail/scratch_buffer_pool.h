#pragma once

#include <algorithm>
#include <cstddef>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "compressed/macros.h"
#include "compressed/util.h"

namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace detail
    {
        class scratch_buffer_pool;

        /// \brief RAII handle representing a temporary scratch buffer checked out from a scratch buffer pool.
        ///
        /// The lease owns the temporary byte buffer while compression is using it. When the lease is destroyed,
        /// the buffer is returned to the originating pool if the pool is still alive and the buffer satisfies the
        /// pool's caching limits.
        ///
        /// This type is move-only. Moving transfers both the buffer and the responsibility to return it.
        class scratch_buffer_lease
        {
        public:
            scratch_buffer_lease() = default;

            /// Construct a lease from a pool and an already allocated buffer.
            ///
            /// \param pool The pool the buffer should be returned to when the lease is destroyed.
            /// \param buffer The byte buffer owned by this lease.
            /// \param size The logical size of the scratch buffer to expose via span().
            scratch_buffer_lease(
                std::shared_ptr<scratch_buffer_pool> pool,
                util::default_init_vector<std::byte> buffer,
                const size_t size
            )
                : m_pool(std::move(pool)),
                  m_buffer(std::move(buffer)),
                  m_size(size)
            {
            }

            scratch_buffer_lease(scratch_buffer_lease&& other) noexcept
                : m_pool(std::move(other.m_pool)),
                  m_buffer(std::move(other.m_buffer)),
                  m_size(other.m_size)
            {
                other.m_size = 0;
            }

            scratch_buffer_lease& operator=(scratch_buffer_lease&& other) noexcept
            {
                if (this != &other)
                {
                    release();

                    m_pool = std::move(other.m_pool);
                    m_buffer = std::move(other.m_buffer);
                    m_size = other.m_size;

                    other.m_size = 0;
                }

                return *this;
            }

            scratch_buffer_lease(const scratch_buffer_lease&) = delete;
            scratch_buffer_lease& operator=(const scratch_buffer_lease&) = delete;

            ~scratch_buffer_lease()
            {
                release();
            }

            /// Retrieve a mutable span over the leased scratch memory.
            ///
            /// The returned span is only valid for as long as this lease remains alive and unmoved.
            ///
            /// \return A mutable span covering the requested logical scratch buffer size.
            std::span<std::byte> span() noexcept
            {
                return std::span<std::byte>(m_buffer.data(), m_size);
            }

            /// Retrieve a const span over the leased scratch memory.
            ///
            /// The returned span is only valid for as long as this lease remains alive and unmoved.
            ///
            /// \return A const span covering the requested logical scratch buffer size.
            std::span<const std::byte> span() const noexcept
            {
                return std::span<const std::byte>(m_buffer.data(), m_size);
            }

            /// Retrieve the logical size of the leased scratch span in bytes.
            ///
            /// \return The size requested when this lease was created.
            size_t size() const noexcept
            {
                return m_size;
            }

            /// Check whether the lease currently owns a buffer large enough for its logical size.
            ///
            /// \return True if the lease owns a large enough buffer, false otherwise.
            bool valid() const noexcept
            {
                return m_buffer.size() >= m_size;
            }

        private:
            /// Return the currently held buffer to the pool, if any.
            void release();

            std::shared_ptr<scratch_buffer_pool> m_pool{};
            util::default_init_vector<std::byte> m_buffer{};
            size_t m_size = 0;
        };


        /// \brief Configuration options for scratch buffer pooling.
        ///
        /// These options control how many returned buffers are cached and how much memory the pool may retain.
        /// Buffers that exceed the configured limits are simply released instead of cached.
        struct scratch_buffer_pool_options
        {
            size_t max_cached_buffers = 0;
            size_t max_cached_bytes = 1024 * 1024 * 1024; // 1GB
        };


        /// \brief Thread-safe pool for temporary compression scratch buffers.
        ///
        /// The pool is used by low-level CPU compression paths to avoid repeatedly allocating temporary output
        /// buffers for Blosc2 compression. Buffers are handed out as move-only scratch_buffer_lease objects and are
        /// automatically returned to the pool when the lease goes out of scope.
        ///
        /// The pool itself does not have global ownership. Channels keep a shared reference to the active pool,
        /// while the global registry only stores a weak reference. This allows the pool to be globally discoverable
        /// while still being destroyed when the last channel / iterator reference disappears.
        class scratch_buffer_pool : public std::enable_shared_from_this<scratch_buffer_pool>
        {
        public:
            explicit scratch_buffer_pool(const scratch_buffer_pool_options options = {})
                : m_options(options)
            {
                if (m_options.max_cached_buffers == 0)
                {
                    m_options.max_cached_buffers = std::max<size_t>(1, std::thread::hardware_concurrency());
                }
            }

            /// Acquire a scratch buffer of at least \p size bytes.
            ///
            /// This function first attempts to reuse the smallest cached buffer that is large enough. If none is available,
            /// a new buffer is allocated. The returned lease keeps the pool alive for the duration of the lease.
            ///
            /// \param size The minimum scratch buffer size in bytes.
            /// \return A move-only lease containing a scratch buffer with logical size \p size.
            scratch_buffer_lease acquire(const size_t size)
            {
                util::default_init_vector<std::byte> buffer{};

                {
                    std::scoped_lock lock(m_mutex);

                    auto best = m_available.end();
                    for (auto it = m_available.begin(); it != m_available.end(); ++it)
                    {
                        if (it->size() >= size && (best == m_available.end() || it->size() < best->size()))
                        {
                            best = it;
                        }
                    }

                    if (best != m_available.end())
                    {
                        m_cached_bytes -= best->size();
                        buffer = std::move(*best);
                        m_available.erase(best);
                    }
                }

                // Fallback if no available buffer is found. Resize it and have it be returned to the pool after.
                if (buffer.size() < size)
                {
                    buffer.resize(size);
                }

                return scratch_buffer_lease(shared_from_this(), std::move(buffer), size);
            }

            /// Clear all currently cached scratch buffers.
            ///
            /// Active leases are not affected. Buffers currently checked out will either be returned later or released,
            /// depending on the pool limits at the time they are returned.
            void clear()
            {
                std::scoped_lock lock(m_mutex);
                m_available.clear();
                m_cached_bytes = 0;
            }

            /// Retrieve the total number of bytes currently cached by the pool.
            ///
            /// This only includes buffers currently stored in the pool, not buffers checked out by active leases.
            ///
            /// \return The number of cached bytes.
            size_t cached_bytes() const
            {
                std::scoped_lock lock(m_mutex);
                return m_cached_bytes;
            }

            /// Retrieve the number of buffers currently cached by the pool.
            ///
            /// This only includes buffers currently stored in the pool, not buffers checked out by active leases.
            ///
            /// \return The number of cached buffers.
            size_t cached_buffers() const
            {
                std::scoped_lock lock(m_mutex);
                return m_available.size();
            }

        private:
            friend class scratch_buffer_lease;

            /// Return a buffer to the pool if doing so does not exceed the configured cache limits.
            ///
            /// \param buffer The buffer to return.
            void release(util::default_init_vector<std::byte> buffer)
            {
                if (buffer.empty())
                {
                    return;
                }

                std::scoped_lock lock(m_mutex);

                if (m_available.size() >= m_options.max_cached_buffers)
                {
                    return;
                }

                if (m_cached_bytes + buffer.size() > m_options.max_cached_bytes)
                {
                    return;
                }

                m_cached_bytes += buffer.size();
                m_available.push_back(std::move(buffer));
            }

            scratch_buffer_pool_options m_options{};
            mutable std::mutex m_mutex{};
            std::vector<util::default_init_vector<std::byte>> m_available{};
            size_t m_cached_bytes = 0;
        };

        inline void scratch_buffer_lease::release()
        {
            if (!m_pool || m_buffer.empty())
            {
                return;
            }

            auto pool = std::move(m_pool);
            auto buffer = std::move(m_buffer);
            m_size = 0;

            pool->release(std::move(buffer));
        }

        /// \brief Weak global registry for the currently active scratch buffer pool.
        ///
        /// The registry allows low-level compression code to discover the active scratch pool without threading pool
        /// references through every compression API. It intentionally stores only a weak reference so that the pool is
        /// destroyed once all owning channel / iterator references are gone.
        class scratch_pool_registry
        {
        public:
            /// Retrieve the current pool or create a new one for channel-owned use.
            ///
            /// Channels call this to obtain a shared reference to the globally discoverable pool. The registry keeps only
            /// a weak reference; the returned shared pointer is what keeps the pool alive.
            ///
            /// \return A shared pointer to the active scratch buffer pool.
            static std::shared_ptr<scratch_buffer_pool> get_or_create_for_channel()
            {
                std::scoped_lock lock(mutex());

                if (auto pool = pool_ref().lock())
                {
                    return pool;
                }

                auto pool = std::make_shared<scratch_buffer_pool>();
                pool_ref() = pool;
                return pool;
            }

            /// Retrieve the currently active scratch pool, if one is still alive.
            ///
            /// Low-level compression wrappers use this to acquire pooled scratch buffers when a channel-owned pool exists.
            /// If no pool is alive, callers should fall back to local temporary allocation.
            ///
            /// \return The active pool, or nullptr if no channel-owned pool exists.
            static std::shared_ptr<scratch_buffer_pool> current()
            {
                std::scoped_lock lock(mutex());
                return pool_ref().lock();
            }

            /// Clear cached buffers from the active pool, if one exists.
            ///
            /// This does not destroy the pool while channels still hold shared references to it.
            static void clear()
            {
                if (auto pool = current())
                {
                    pool->clear();
                }
            }

            /// Retrieve the number of bytes cached in the active pool.
            ///
            /// \return The active pool's cached byte count, or 0 if no pool exists.
            static size_t cached_bytes()
            {
                if (auto pool = current())
                {
                    return pool->cached_bytes();
                }

                return 0;
            }

            /// Retrieve the number of buffers cached in the active pool.
            ///
            /// \return The active pool's cached buffer count, or 0 if no pool exists.
            static size_t cached_buffers()
            {
                if (auto pool = current())
                {
                    return pool->cached_buffers();
                }

                return 0;
            }

        private:
            /// Retrieve the registry mutex.
            ///
            /// \return A process-local mutex guarding the weak pool reference.
            static std::mutex& mutex()
            {
                static std::mutex value{};
                return value;
            }

            /// Retrieve the weak reference to the currently active pool.
            ///
            /// \return A process-local weak pointer to the active pool.
            static std::weak_ptr<scratch_buffer_pool>& pool_ref()
            {
                static std::weak_ptr<scratch_buffer_pool> value{};
                return value;
            }
        };
    } // namespace detail

    /// \brief Clear cached scratch buffers from the active global scratch pool.
    ///
    /// This releases memory currently cached by the pool without invalidating any live channels or active compression
    /// operations. Buffers checked out by active leases are not affected and may be returned to the pool later.
    inline void clear_scratch_pool()
    {
        detail::scratch_pool_registry::clear();
    }

    /// \brief Retrieve the number of bytes currently cached by the active global scratch pool.
    ///
    /// This value does not include buffers that are currently checked out by active compression operations.
    ///
    /// \return The number of bytes cached by the active pool, or 0 if no pool is alive.
    inline size_t scratch_pool_cached_bytes()
    {
        return detail::scratch_pool_registry::cached_bytes();
    }

    /// \brief Retrieve the number of buffers currently cached by the active global scratch pool.
    ///
    /// This value does not include buffers that are currently checked out by active compression operations.
    ///
    /// \return The number of buffers cached by the active pool, or 0 if no pool is alive.
    inline size_t scratch_pool_cached_buffers()
    {
        return detail::scratch_pool_registry::cached_buffers();
    }
} // NAMESPACE_COMPRESSED_IMAGE
