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

#include "syncpal/isyncworker.h"
#include "syncpal/syncpal.h"
#include "utility/utility.h"
#include "db/syncdb.h"
#include "update_detection/file_system_observer/fsoperationset.h"
#include "updatetree.h"

namespace KDC {

class UpdateTreeWorker;

typedef ExitCode (UpdateTreeWorker::*stepptr)();

class UpdateTreeWorker : public ISyncWorker {
    public:
        UpdateTreeWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name, const std::string &shortName,
                         ReplicaSide side);
        UpdateTreeWorker(std::shared_ptr<SyncDb> syncDb, std::shared_ptr<FSOperationSet> operationSet,
                         std::shared_ptr<UpdateTree> updateTree, const std::string &name, const std::string &shortName,
                         ReplicaSide side);
        ~UpdateTreeWorker();

        void execute() override;

    private:
        std::shared_ptr<SyncDb> _syncDb;
        std::shared_ptr<FSOperationSet> _operationSet;
        std::shared_ptr<UpdateTree> _updateTree;
        using FSOpPtrMap = std::unordered_map<SyncPath, FSOpPtr, hashPathFunction>;
        FSOpPtrMap _createFileOperationSet;
        ReplicaSide _side;

        /**
         * Create node where opType is Move
         * and nodeType is Directory.
         * return : ExitCodeOk if task is successful.
         */
        ExitCode step1MoveDirectory();

        /**
         * Create node where opType is Move
         * and nodeType is File.
         * return : ExitCodeOk if task is successful.
         */
        ExitCode step2MoveFile();

        /**
         * Create node where opType is Delete
         * and nodeType is Directory.
         * return : ExitCodeOk if task is successful.
         */
        ExitCode step3DeleteDirectory();

        /**
         * Create node where opType is Delete
         * and nodeType is File.
         * return : ExitCodeOk if task is successful.
         */
        ExitCode step4DeleteFile();

        /**
         * Create node where opType is Create
         * and nodeType is Directory.
         * return : ExitCodeOk if task is successful.
         */
        ExitCode step5CreateDirectory();

        /**
         * Create node where opType is Create
         * and nodeType is File.
         * return : ExitCodeOk if task is successful.
         */
        ExitCode step6CreateFile();

        /**
         * Create node where opType is Edit
         * and nodeType is File.
         * return : ExitCodeOk if task is successful.
         */
        ExitCode step7EditFile();

        /**
         * Update existing node with information from DB
         * and add missing nodes without change events.
         * return : ExitCodeOk if task is successful.
         */
        ExitCode step8CompleteUpdateTree();

        ExitCode createMoveNodes(const NodeType &nodeType);

        void updateNodeId(std::shared_ptr<Node> node, const NodeId &newId);

        ExitCode getNewPathAfterMove(const SyncPath &path, SyncPath &newPath);
        ExitCode updateNodeWithDb(const std::shared_ptr<Node> parentNode);
        ExitCode updateTmpNode(const std::shared_ptr<Node> tmpNode);
        ExitCode getOriginPath(const std::shared_ptr<Node> node, SyncPath &path);
        ExitCode updateNameFromDbForMoveOp(const std::shared_ptr<Node> node, FSOpPtr moveOp);

        // Log update information if extended logging is on.
        void logUpdate(const std::shared_ptr<Node> node, const OperationType opType,
                       const std::shared_ptr<Node> parentNode = nullptr);
        void updateTmpNode(const std::shared_ptr<Node> node, FSOpPtr op, FSOpPtr deleteOp);


        /**
         * Detect and handle create operations on files or directories
         * with identical standardized paths.
         * The existence of such duplicate standardized paths can be caused by:
         * - a file deletion operation was not reported by the user OS.
         * - the user has created several files whose names have different encodings but same normalization (an issue
         *reported on Windows 10 and 11). This function fills `_createFileOperation` with all create operations on files.
         *
         *\return : ExitCodeOk if no problematic create operations were detected.
         */
        ExitCode handleCreateOperationsWithSamePath();

        std::shared_ptr<Node> getOrCreateNodeFromPath(const SyncPath &path);
        void mergingTempNodeToRealNode(std::shared_ptr<Node> tmpNode, std::shared_ptr<Node> realNode);

        /**
         * Check that there is no temporary node remaining in the update tree
         * @return true if no temporary node is found
         */
        bool integrityCheck();

        /**
         * Draw the update tree in the log file for debugging purpose
         */
        void drawUpdateTree();
        void drawUpdateTreeRow(const std::shared_ptr<Node> node, SyncName &treeStr, uint64_t depth = 0);

        friend class TestUpdateTreeWorker;
};

}  // namespace KDC
