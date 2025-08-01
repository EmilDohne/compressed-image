//
// Basic instrumentation profiler by Cherno to be used for chrome://tracing

// Usage: include this header file somewhere in your code (eg. precompiled header), and then use like:
//
// Instrumentor::Get().BeginSession("Session Name");        // Begin session 
// {
//     PROFILE_FUNCTION();   // Place code like this in scopes you'd like to include in profiling
//     // Code
// }
// Instrumentor::Get().EndSession();                        // End Session
#pragma once

#include "compressed/macros.h"

#include <string>
#include <chrono>
#include <algorithm>
#include <fstream>

#include <mutex>
#include <thread>

#ifdef _COMPRESSED_PROFILE
#define _COMPRESSED_PROFILE_SCOPE(name) NAMESPACE_COMPRESSED_IMAGE::detail::InstrumentationTimer timer##__LINE__(name)
#define _COMPRESSED_PROFILE_FUNCTION()  NAMESPACE_COMPRESSED_IMAGE::detail::InstrumentationTimer timer##__FUNCTION__##__LINE__(__FUNCTION__)
#else
#define _COMPRESSED_PROFILE_SCOPE(name)
#define _COMPRESSED_PROFILE_FUNCTION()
#endif

namespace NAMESPACE_COMPRESSED_IMAGE
{

    namespace detail
    {

        struct ProfileResult
        {
            std::string Name;
            long long Start, End;
            uint32_t ThreadID;
        };

        struct InstrumentationSession
        {
            std::string Name;
        };

        class Instrumentor
        {
        private:
            InstrumentationSession* m_CurrentSession;
            std::ofstream m_OutputStream;
            int m_ProfileCount;
            std::mutex m_lock;
        public:
            Instrumentor()
                : m_CurrentSession(nullptr), m_ProfileCount(0)
            {
            }

            void BeginSession(const std::string& name, const std::string& filepath = "results.json")
            {
                m_OutputStream.open(filepath);
                WriteHeader();
                m_CurrentSession = new InstrumentationSession{ name };
            }

            void EndSession()
            {
                WriteFooter();
                m_OutputStream.close();
                delete m_CurrentSession;
                m_CurrentSession = nullptr;
                m_ProfileCount = 0;
            }

            void WriteProfile(const ProfileResult& result)
            {
                std::lock_guard<std::mutex> lock(m_lock);

                if (m_ProfileCount++ > 0)
                    m_OutputStream << ",";

                std::string name = result.Name;
                std::replace(name.begin(), name.end(), '"', '\'');

                m_OutputStream << "{";
                m_OutputStream << "\"cat\":\"function\",";
                m_OutputStream << "\"dur\":" << (result.End - result.Start) << ',';
                m_OutputStream << "\"name\":\"" << name << "\",";
                m_OutputStream << "\"ph\":\"X\",";
                m_OutputStream << "\"pid\":0,";
                m_OutputStream << "\"tid\":" << result.ThreadID << ",";
                m_OutputStream << "\"ts\":" << result.Start;
                m_OutputStream << "}";

                m_OutputStream.flush();
            }

            void WriteHeader()
            {
                m_OutputStream << "{\"otherData\": {},\"traceEvents\":[";
                m_OutputStream.flush();
            }

            void WriteFooter()
            {
                m_OutputStream << "]}";
                m_OutputStream.flush();
            }

            static Instrumentor& Get()
            {
                static Instrumentor instance;
                return instance;
            }
        };

        class InstrumentationTimer
        {
        public:
            InstrumentationTimer(const char* name)
                : m_Name(name), m_Stopped(false)
            {
                m_StartTimepoint = std::chrono::high_resolution_clock::now();
            }

            ~InstrumentationTimer()
            {
                if (!m_Stopped)
                    Stop();
            }

            void Stop()
            {
                auto endTimepoint = std::chrono::high_resolution_clock::now();

                long long start = std::chrono::time_point_cast<std::chrono::microseconds>(m_StartTimepoint).time_since_epoch().count();
                long long end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimepoint).time_since_epoch().count();

                uint32_t threadID = static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
                Instrumentor::Get().WriteProfile({ m_Name, start, end, threadID });

                m_Stopped = true;
            }
        private:
            const std::string m_Name{};
            std::chrono::time_point<std::chrono::high_resolution_clock> m_StartTimepoint;
            bool m_Stopped = false;
        };

    } // detail
} // NAMESPACE_COMPRESSED_IMAGE
