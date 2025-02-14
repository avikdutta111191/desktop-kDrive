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

#include "movejob.h"

#include "libcommonserver/io/iohelper.h"
#include "libcommonserver/utility/utility.h"  // Path2WStr

namespace KDC {

MoveJob::MoveJob(int driveDbId, const SyncPath &destFilepath, const NodeId &fileId, const NodeId &destDirId,
                 const SyncName &name /*= ""*/)
    : AbstractTokenNetworkJob(ApiDrive, 0, 0, driveDbId, 0),
      _destFilepath(destFilepath),
      _fileId(fileId),
      _destDirId(destDirId),
      _name(name) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_POST;
}

MoveJob::~MoveJob() {
    if (_vfsForceStatus && _vfsStatus) {
        bool isPlaceholder = false;
        bool isHydrated = false;
        bool isSyncing = false;
        int progress = 0;
        if (!_vfsStatus(_destFilepath, isPlaceholder, isHydrated, isSyncing, progress)) {
            LOGW_WARN(_logger, L"Error in vfsStatus for path=" << Path2WStr(_destFilepath).c_str());
        }

        if (!_vfsForceStatus(_destFilepath, false, 100,
                             isHydrated)) {  // TODO : to be refactored, some parameters are used on macOS only
            LOGW_WARN(_logger, L"Error in vfsForceStatus for path=" << Path2WStr(_destFilepath).c_str());
        }
    }
}

bool MoveJob::canRun() {
    if (bypassCheck()) {
        return true;
    }

    // Check that we still have to move the folder
    bool exists;
    IoError ioError = IoErrorSuccess;
    if (!IoHelper::checkIfPathExists(_destFilepath, exists, ioError)) {
        LOGW_WARN(_logger, L"Error in IoHelper::checkIfPathExists: " << Utility::formatIoError(_destFilepath, ioError).c_str());
        _exitCode = ExitCodeSystemError;
        _exitCause = ExitCauseFileAccessError;
        return false;
    }

    if (!exists) {
        LOGW_DEBUG(_logger, L"File " << Path2WStr(_destFilepath).c_str()

                                     << L" is not in its destination folder. Aborting current sync and restart.");
        _exitCode = ExitCodeDataError;  // Data error so the snapshots will be re-created
        _exitCause = ExitCauseUnexpectedFileSystemEvent;
        return false;
    }

    return true;
}

std::string MoveJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/files/";
    str += _fileId;
    str += "/move/";
    str += _destDirId;
    return str;
}

void MoveJob::setData(bool &canceled) {
    Poco::JSON::Object json;
    if (!_name.empty()) {
        json.set("name", _name);

        std::stringstream ss;
        json.stringify(ss);
        _data = ss.str();
    }
    canceled = false;
}

}  // namespace KDC
