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

#include "utility/types.h"

#include <log4cplus/logger.h>

namespace KDC {

class SyncDb;

class VirtualFilesCleaner {
    public:
        VirtualFilesCleaner(const SyncPath &path, int syncDbId, std::shared_ptr<SyncDb> syncDb,
                            bool (*vfsStatus)(int, const SyncPath &, bool &, bool &, bool &, int &),
                            bool (*vfsClearFileAttributes)(int, const SyncPath &));

        VirtualFilesCleaner(const SyncPath &path);

        bool run();
        bool removeDehydratedPlaceholders(std::vector<SyncPath> &failedToRemovePlaceholders);

        inline ExitCode exitCode() const { return _exitCode; }
        inline ExitCause exitCause() const { return _exitCause; }

    private:
        bool removePlaceholdersRecursivly(const SyncPath &parentPath);

        log4cplus::Logger _logger;

        SyncPath _rootPath;
        int _syncDbId;
        std::shared_ptr<SyncDb> _syncDb = nullptr;
        bool (*_vfsStatus)(int syncDbId, const SyncPath &itemPath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing,
                           int &progress);
        bool (*_vfsClearFileAttributes)(int syncDbId, const SyncPath &itemPath);

        ExitCode _exitCode = ExitCodeUnknown;
        ExitCause _exitCause = ExitCauseUnknown;
};

}  // namespace KDC
