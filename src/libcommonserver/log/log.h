/*
 * Infomaniak kDrive - Desktop
 * Copyright (C) 2023-2024 Infomaniak Network SA
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "libcommonserver/commonserverlib.h"
#include "libcommon/utility/types.h"

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

#include <sentry.h>

namespace KDC {

#ifdef NDEBUG
#define LOG_DEBUG(logger, logEvent)                                                        \
    {                                                                                      \
        std::ostringstream stream;                                                         \
        stream << logEvent;                                                                \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, stream.str().c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("debug"));         \
        sentry_add_breadcrumb(crumb);                                                      \
    }                                                                                      \
    LOG4CPLUS_DEBUG(logger, logEvent)

#define LOGW_DEBUG(logger, logEvent)                                                                      \
    {                                                                                                     \
        std::wstringstream stream;                                                                        \
        stream << logEvent;                                                                               \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, Utility::ws2s(stream.str()).c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("debug"));                        \
        sentry_add_breadcrumb(crumb);                                                                     \
    }                                                                                                     \
    LOG4CPLUS_DEBUG(logger, logEvent)

#define LOG_INFO(logger, logEvent)                                                         \
    {                                                                                      \
        std::ostringstream stream;                                                         \
        stream << logEvent;                                                                \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, stream.str().c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("info"));          \
        sentry_add_breadcrumb(crumb);                                                      \
    }                                                                                      \
    LOG4CPLUS_INFO(logger, logEvent)

#define LOGW_INFO(logger, logEvent)                                                                       \
    {                                                                                                     \
        std::wstringstream stream;                                                                        \
        stream << logEvent;                                                                               \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, Utility::ws2s(stream.str()).c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("info"));                         \
        sentry_add_breadcrumb(crumb);                                                                     \
    }                                                                                                     \
    LOG4CPLUS_INFO(logger, logEvent)

#define LOG_WARN(logger, logEvent)                                                         \
    {                                                                                      \
        std::ostringstream stream;                                                         \
        stream << logEvent;                                                                \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, stream.str().c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("warning"));       \
        sentry_add_breadcrumb(crumb);                                                      \
    }                                                                                      \
    LOG4CPLUS_WARN(logger, logEvent)

#define LOGW_WARN(logger, logEvent)                                                                       \
    {                                                                                                     \
        std::wstringstream stream;                                                                        \
        stream << logEvent;                                                                               \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, Utility::ws2s(stream.str()).c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("warning"));                      \
        sentry_add_breadcrumb(crumb);                                                                     \
    }                                                                                                     \
    LOG4CPLUS_WARN(logger, logEvent)

#define LOG_ERROR(logger, logEvent)                                                        \
    {                                                                                      \
        std::ostringstream stream;                                                         \
        stream << logEvent;                                                                \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, stream.str().c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("error"));         \
        sentry_add_breadcrumb(crumb);                                                      \
    }                                                                                      \
    LOG4CPLUS_ERROR(logger, logEvent)

#define LOGW_ERROR(logger, logEvent)                                                                      \
    {                                                                                                     \
        std::wstringstream stream;                                                                        \
        stream << logEvent;                                                                               \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, Utility::ws2s(stream.str()).c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("error"));                        \
        sentry_add_breadcrumb(crumb);                                                                     \
    }                                                                                                     \
    LOG4CPLUS_ERROR(logger, logEvent)

#define LOG_FATAL(logger, logEvent)                                                        \
    {                                                                                      \
        std::ostringstream stream;                                                         \
        stream << logEvent;                                                                \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, stream.str().c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("fatal"));         \
        sentry_add_breadcrumb(crumb);                                                      \
    }                                                                                      \
    LOG4CPLUS_FATAL(logger, logEvent)

#define LOGW_FATAL(logger, logEvent)                                                                      \
    {                                                                                                     \
        std::wstringstream stream;                                                                        \
        stream << logEvent;                                                                               \
        sentry_value_t crumb = sentry_value_new_breadcrumb(nullptr, Utility::ws2s(stream.str()).c_str()); \
        sentry_value_set_by_key(crumb, "level", sentry_value_new_string("fatal"));                        \
        sentry_add_breadcrumb(crumb);                                                                     \
    }                                                                                                     \
    LOG4CPLUS_FATAL(logger, logEvent)
#else
#define LOG_DEBUG(logger, logEvent) LOG4CPLUS_DEBUG(logger, logEvent)

#define LOGW_DEBUG(logger, logEvent) LOG4CPLUS_DEBUG(logger, logEvent)

#define LOG_INFO(logger, logEvent) LOG4CPLUS_INFO(logger, logEvent)

#define LOGW_INFO(logger, logEvent) LOG4CPLUS_INFO(logger, logEvent)

#define LOG_WARN(logger, logEvent) LOG4CPLUS_WARN(logger, logEvent)

#define LOGW_WARN(logger, logEvent) LOG4CPLUS_WARN(logger, logEvent)

#define LOG_ERROR(logger, logEvent) LOG4CPLUS_ERROR(logger, logEvent)

#define LOGW_ERROR(logger, logEvent) LOG4CPLUS_ERROR(logger, logEvent)

#define LOG_FATAL(logger, logEvent) LOG4CPLUS_FATAL(logger, logEvent)

#define LOGW_FATAL(logger, logEvent) LOG4CPLUS_FATAL(logger, logEvent)
#endif

class COMMONSERVER_EXPORT Log {
    public:
        ~Log();
        static std::shared_ptr<Log> instance(const log4cplus::tstring &filePath = log4cplus::tstring());

        Log(Log const &) = delete;
        void operator=(Log const &) = delete;

        static bool isSet() { return _instance != nullptr; }

        inline log4cplus::Logger getLogger() { return _logger; }
        bool configure(bool useLog, LogLevel logLevel, bool purgeOldLogs);

		/*! Returns the path of the log file.
         * \return The path of the log file.
         */
        SyncPath getLogFilePath() const;

        static const std::wstring instanceName;
        static const std::wstring rfName;
        static const std::wstring rfPattern;
        static const int rfMaxBackupIdx;

    private:
        friend class TestLog;
        Log(const log4cplus::tstring &filePath);

        static std::shared_ptr<Log> _instance;
        log4cplus::Logger _logger;
        SyncPath _filePath;
};

}  // namespace KDC
