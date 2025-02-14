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

#include "filesystemobserverworker.h"

#include "jobs/network/networkjobsparams.h"

#include <Poco/JSON/Object.h>

namespace KDC {

class CsvFullFileListWithCursorJob;

class RemoteFileSystemObserverWorker : public FileSystemObserverWorker {
    public:
        RemoteFileSystemObserverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName);
        ~RemoteFileSystemObserverWorker();

    private:
        virtual void execute() override;
        virtual ExitCode generateInitialSnapshot() override;
        virtual ExitCode processEvents() override;
        virtual ReplicaSide getSnapshotType() const override { return ReplicaSide::ReplicaSideRemote; }

        ExitCode initWithCursor();
        ExitCode exploreDirectory(const NodeId &nodeId);
        ExitCode getItemsInDir(const NodeId &dirId, const bool saveCursor);

        ExitCode sendLongPoll(bool &changes);

        struct ActionInfo {
            ActionCode actionCode {ActionCode::actionCodeUnknown};
            NodeId nodeId;
            NodeId parentNodeId;
            SyncName name;
            SyncName path;
            SyncName destName;
            SyncTime createdAt {0};
            SyncTime modtime {0};
            NodeType type {NodeTypeUnknown};
            int64_t size {0};
            bool canWrite {true};
        };
        ExitCode processActions(Poco::JSON::Array::Ptr filesArray);
        ExitCode extractActionInfo(const Poco::JSON::Object::Ptr actionObj, ActionInfo &actionInfo);
        ExitCode processAction(const SyncName &usedName, const ActionInfo &actionInfo, std::set<NodeId, std::equal_to<>> &movedItems);

        ExitCode checkRightsAndUpdateItem(const NodeId &nodeId, bool &hasRights, SnapshotItem &snapshotItem);

        bool hasUnsupportedCharacters(const SyncName &name, const NodeId &nodeId, NodeType type);

        int _driveDbId = -1;
        std::string _cursor;

        int _listingFullCounter = 0;
        std::chrono::steady_clock::time_point _listingFullTimer = std::chrono::steady_clock::now();

        friend class TestRemoteFileSystemObserverWorker;
};

}  // namespace KDC
