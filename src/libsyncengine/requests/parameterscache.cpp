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

#include "parameterscache.h"
#include "libparms/db/parmsdb.h"
#include "libcommonserver/log/log.h"

#include <log4cplus/loggingmacros.h>

namespace KDC {

std::shared_ptr<ParametersCache> ParametersCache::_instance = nullptr;

std::shared_ptr<ParametersCache> ParametersCache::instance(bool isTest /*= false*/) {
    if (_instance == nullptr) {
        try {
            _instance = std::shared_ptr<ParametersCache>(new ParametersCache(isTest));
        } catch (std::exception const &) {
            return nullptr;
        }
    }

    return _instance;
}

ParametersCache::ParametersCache(bool isTest /*= false*/) {
    if (isTest) {
        // In test, use extended log by default
        _parameters.setExtendedLog(true);
        return;
    }

    // Load parameters
    bool found = false;
    if (!ParmsDb::instance()->selectParameters(_parameters, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectParameters");
        throw std::runtime_error("Failed to create ParametersCache instance!");
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Parameters not found");
        throw std::runtime_error("Failed to create ParametersCache instance!");
    }
}

ExitCode ParametersCache::save() {
    // Get old parameters
    Parameters oldParameters;
    bool found = false;
    if (!ParmsDb::instance()->selectParameters(oldParameters, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::selectParameters");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Parameters not found");
        return ExitCodeDataError;
    }

    // Update parameters
    if (!ParmsDb::instance()->updateParameters(_parameters, found)) {
        LOG_WARN(Log::instance()->getLogger(), "Error in ParmsDb::updateParameters");
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_WARN(Log::instance()->getLogger(), "Parameters not found");
        return ExitCodeDataError;
    }

    // Check if Log parameters have been updated
    if (oldParameters.useLog() != _parameters.useLog() || oldParameters.logLevel() != _parameters.logLevel() ||
        oldParameters.purgeOldLogs() != _parameters.purgeOldLogs()) {
        if (!Log::instance()->configure(_parameters.useLog(), _parameters.logLevel(), _parameters.purgeOldLogs())) {
            LOG_WARN(Log::instance()->getLogger(), "Error in Log::configure");
            return ExitCodeSystemError;
        }
    }

    return ExitCodeOk;
}

}  // namespace KDC
