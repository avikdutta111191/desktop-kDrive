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

#include "uploadsessioncanceljob.h"

namespace KDC {

UploadSessionCancelJob::UploadSessionCancelJob(int driveDbId, const SyncPath &filepath, const std::string &sessionToken)
    : AbstractUploadSessionJob(driveDbId, filepath, sessionToken) {
    _httpMethod = Poco::Net::HTTPRequest::HTTP_DELETE;
}

std::string UploadSessionCancelJob::getSpecificUrl() {
    std::string str = AbstractTokenNetworkJob::getSpecificUrl();
    str += "/upload/session/";
    str += _sessionToken;
    return str;
}

bool UploadSessionCancelJob::handleError(std::istream &is, const Poco::URI &uri) {
    if (_resHttp.getStatus() == Poco::Net::HTTPResponse::HTTP_BAD_REQUEST) {
        _exitCode = ExitCodeBackError;
        return false;
    }

    return AbstractTokenNetworkJob::handleError(is, uri);
}


}  // namespace KDC
