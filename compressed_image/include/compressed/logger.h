#pragma once

#include "macros.h"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>


namespace
NAMESPACE_COMPRESSED_IMAGE
{
    namespace detail
    {
        inline std::shared_ptr<spdlog::logger> s_logger = nullptr;
        inline std::mutex s_logger_mutex;

        /// \brief The default logger name used internally if the user does not provide one.
        static inline std::string s_default_logger_name = "compressed_image";
    }


    /// \brief Set the logger instance used by the compressed-image api.
    ///
    /// This function allows consumers of the library to provide their own `spdlog::logger` instance.
    /// This can be useful to integrate the library’s logging output into an existing logging system,
    /// route messages to a file, or change verbosity dynamically.
    ///
    /// If no logger is set, the library will lazily create a default one that logs to `stdout` at warning level.
    ///
    /// \param logger The `spdlog::logger` instance to use for all library logging.
    inline void set_logger(std::shared_ptr<spdlog::logger> logger)
    {
        std::lock_guard<std::mutex> lock(detail::s_logger_mutex);
        detail::s_logger = logger;
    }

    /// \brief Retrieve the current logger instance used by the cryptomatte-api.
    ///
    /// If no logger has been previously set via `set_logger`, this function will initialize
    /// a default logger named `"cryptomatte_api"` that logs to standard output with color support,
    /// and at `spdlog::level::warn` verbosity.
    ///
    /// \return A shared pointer to the currently active `spdlog::logger`.
    inline std::shared_ptr<spdlog::logger> get_logger()
    {
        std::lock_guard<std::mutex> lock(detail::s_logger_mutex);
        if (!detail::s_logger)
        {
            detail::s_logger = spdlog::get(detail::s_default_logger_name);

            if (!detail::s_logger)
            {
                // Lazy init with a sensible default
                detail::s_logger = spdlog::stdout_color_mt(detail::s_default_logger_name);
                detail::s_logger->set_level(spdlog::level::info);
            }
        }
        return detail::s_logger;
    }
} // NAMESPACE_COMPRESSED_IMAGE
