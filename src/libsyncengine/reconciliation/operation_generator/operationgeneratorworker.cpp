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

#include "operationgeneratorworker.h"
#include "update_detection/update_detector/updatetree.h"
#include "libcommonserver/utility/utility.h"
#include "requests/parameterscache.h"

namespace KDC {

OperationGeneratorWorker::OperationGeneratorWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                                   const std::string &shortName)
    : OperationProcessor(syncPal, name, shortName) {}

void OperationGeneratorWorker::execute() {
    ExitCode exitCode(ExitCodeUnknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    _syncPal->_syncOps->startUpdate();
    _syncPal->_syncOps->clear();
    _bytesToDownload = 0;

    // Mark all nodes "Unprocessed"
    _syncPal->updateTree(ReplicaSideLocal)->markAllNodesUnprocessed();
    _syncPal->updateTree(ReplicaSideRemote)->markAllNodesUnprocessed();

    _deletedNodes.clear();

    // Initiate breadth-first search with root nodes from both update trees
    _queuedToExplore.push(_syncPal->updateTree(ReplicaSideLocal)->rootNode());
    _queuedToExplore.push(_syncPal->updateTree(ReplicaSideRemote)->rootNode());

    // Explore both update trees
    while (!_queuedToExplore.empty()) {
        if (stopAsked()) {
            exitCode = ExitCodeOk;
            break;
        }

        while (pauseAsked() || isPaused()) {
            if (!isPaused()) {
                setPauseDone();
            }

            Utility::msleep(LOOP_PAUSE_SLEEP_PERIOD);

            if (unpauseAsked()) {
                setUnpauseDone();
            }
        }

        std::shared_ptr<Node> currentNode = _queuedToExplore.front();
        _queuedToExplore.pop();

        if (!currentNode) {
            continue;
        }

        // Explore children even if node is processed
        for (const auto &child : currentNode->children()) {
            _queuedToExplore.push(child.second);
        }

        if (currentNode->status() == NodeStatusProcessed) {
            continue;
        }

        std::shared_ptr<Node> correspondingNode = correspondingNodeInOtherTree(currentNode);
        if (!correspondingNode && !currentNode->hasChangeEvent(OperationTypeCreate) &&
            (currentNode->hasChangeEvent(OperationTypeDelete) || currentNode->hasChangeEvent(OperationTypeEdit) ||
             currentNode->hasChangeEvent(OperationTypeMove))) {
            LOGW_SYNCPAL_WARN(_logger, L"Failed to get corresponding node: " << SyncName2WStr(currentNode->name()).c_str());
            exitCode = ExitCodeDataError;
            break;
        }

        if (currentNode->hasChangeEvent(OperationTypeCreate)) {
            if (!(currentNode->side() == ReplicaSideLocal && currentNode->isSharedFolder())) {
                generateCreateOperation(currentNode, correspondingNode);
            }
        }

        if (currentNode->hasChangeEvent(OperationTypeDelete)) {
            generateDeleteOperation(currentNode, correspondingNode);
        }

        if (currentNode->hasChangeEvent(OperationTypeEdit)) {
            generateEditOperation(currentNode, correspondingNode);
        }

        if (currentNode->hasChangeEvent(OperationTypeMove)) {
            generateMoveOperation(currentNode, correspondingNode);
        }
    }

    if (exitCode == ExitCodeUnknown && _queuedToExplore.empty()) {
        exitCode = ExitCodeOk;
    }

    if (_bytesToDownload > 0) {
        const int64_t freeBytes = Utility::freeDiskSpace(_syncPal->_localPath);
        if (freeBytes >= 0) {
            if (freeBytes < _bytesToDownload + Utility::freeDiskSpaceLimit()) {
                LOGW_SYNCPAL_WARN(_logger, L"Disk almost full, only "
                                               << freeBytes << L" B available at path " << Path2WStr(_syncPal->_localPath).c_str()
                                               << L", " << _bytesToDownload << L" B to download. Synchronization canceled.");
                exitCode = ExitCodeSystemError;
                setExitCause(ExitCauseNotEnoughDiskSpace);
            }
        } else {
            LOGW_SYNCPAL_WARN(_logger, L"Could not determine free space available at" << Path2WStr(_syncPal->_localPath).c_str());
        }
    }

    setDone(exitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

void OperationGeneratorWorker::generateCreateOperation(std::shared_ptr<Node> currentNode,
                                                       std::shared_ptr<Node> correspondingNode) {
    SyncOpPtr op = std::make_shared<SyncOperation>();

    // Check for Create-Create pseudo conflict
    if (correspondingNode && isPseudoConflict(currentNode, correspondingNode)) {
        op->setOmit(true);
        op->setCorrespondingNode(correspondingNode);
        correspondingNode->setStatus(NodeStatusProcessed);
    }

    op->setType(OperationTypeCreate);
    op->setAffectedNode(currentNode);
    ReplicaSide targetSide = otherSide(currentNode->side());
    op->setTargetSide(targetSide);
    // We do not set parent node here since it might has been just created as well. In that case, parent node does not exist yet
    // in update tree.
    op->setNewName(targetSide == ReplicaSideLocal ? currentNode->finalLocalName()
                                                  : currentNode->name());  // Use validName only on local replica
    currentNode->setStatus(NodeStatusProcessed);
    _syncPal->_syncOps->pushOp(op);

    if (op->omit()) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               L"Create-Create pseudo conflict detected. Operation Create to be propagated in DB only for item "
                                   << SyncName2WStr(currentNode->name()).c_str());
        }
    } else {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(
                _logger, L"Create operation "
                             << op->id() << L" to be propagated on " << Utility::s2ws(Utility::side2Str(op->targetSide())).c_str()
                             << L" replica for item " << SyncName2WStr(op->newName()).c_str() << L" ("
                             << Utility::s2ws(currentNode->id() ? currentNode->id().value() : "-1").c_str() << L")");
        }

        if (_syncPal->_vfsMode == VirtualFileModeOff && op->targetSide() == ReplicaSideLocal &&
            currentNode->type() == NodeTypeFile) {
            _bytesToDownload += currentNode->size();
        }
    }
}

void OperationGeneratorWorker::generateEditOperation(std::shared_ptr<Node> currentNode, std::shared_ptr<Node> correspondingNode) {
    SyncOpPtr op = std::make_shared<SyncOperation>();

    assert(correspondingNode);  // Node must exists on both replica (except for create operations)

    // Check for Edit-Edit pseudo conflict
    if (isPseudoConflict(currentNode, correspondingNode)) {
        op->setOmit(true);
        correspondingNode->setStatus(NodeStatusProcessed);
    }

    op->setType(OperationTypeEdit);
    op->setAffectedNode(currentNode);
    op->setCorrespondingNode(correspondingNode);
    op->setTargetSide(correspondingNode->side());
    if (currentNode->hasChangeEvent(OperationTypeMove) && currentNode->status() == NodeStatusUnprocessed) {
        currentNode->setStatus(NodeStatusPartiallyProcessed);
    } else {
        currentNode->setStatus(NodeStatusProcessed);
    }
    _syncPal->_syncOps->pushOp(op);

    if (op->omit()) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               L"Edit-Edit pseudo conflict detected. Operation Edit to be propagated in DB only for item "
                                   << SyncName2WStr(currentNode->name()).c_str());
        }
    } else {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Edit operation "
                                            << op->id() << L" to be propagated on "
                                            << Utility::s2ws(Utility::side2Str(op->targetSide())).c_str() << L" replica for item "
                                            << SyncName2WStr(currentNode->name()).c_str() << L"(ID: "
                                            << Utility::s2ws(currentNode->id() ? currentNode->id().value() : "-1").c_str()
                                            << L")");
        }

        if (_syncPal->_vfsMode == VirtualFileModeOff && op->targetSide() == ReplicaSideLocal &&
            currentNode->type() == NodeTypeFile) {
            // Keep only the difference between remote size and local size
            int64_t diffSize = currentNode->size() - correspondingNode->size();

            _bytesToDownload += diffSize;
        }
    }
}

void OperationGeneratorWorker::generateMoveOperation(std::shared_ptr<Node> currentNode, std::shared_ptr<Node> correspondingNode) {
    SyncOpPtr op = std::make_shared<SyncOperation>();

    assert(correspondingNode);  // Node must exists on both replica (except for create operations)

    // Check for Move-Move (Source) pseudo conflict
    if (isPseudoConflict(currentNode, correspondingNode)) {
        op->setOmit(true);
        correspondingNode->setStatus(NodeStatusProcessed);
    }

    /*
     * Special case:
     * 1 - The file name contained à special character, for exemple: "test:1.png" and was rename locally "test%3a1.png".
     * 2 - The file is rename "test%3a2.png" on local side. Since the "%3a" was not removed, the name is uploaded as it is on
     * local replica and appears now "test%3a2.png" on remote. 3 - The file is renamed "test:2.png" on remote replica. We then try
     * to rename the local file "test%3a2.png" but fail since it already exist
     */
    if (currentNode->side() == ReplicaSideRemote && correspondingNode->validLocalName().empty() &&
        currentNode->validLocalName() == correspondingNode->name()) {
        // Only update DB and tree
        op->setOmit(true);
    }

    op->setType(OperationTypeMove);
    op->setAffectedNode(currentNode);
    op->setCorrespondingNode(correspondingNode);
    op->setTargetSide(correspondingNode->side());
    op->setNewName(op->targetSide() == ReplicaSideLocal ? currentNode->finalLocalName()
                                                        : currentNode->name());  // Use validName only on local replica
    if (currentNode->hasChangeEvent(OperationTypeEdit) && currentNode->status() == NodeStatusUnprocessed) {
        currentNode->setStatus(NodeStatusPartiallyProcessed);
    } else {
        currentNode->setStatus(NodeStatusProcessed);
    }
    _syncPal->_syncOps->pushOp(op);

    if (op->omit()) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(
                _logger, L"Move-Move (Source) pseudo conflict detected. Operation Move to be propagated in DB only for item "
                             << SyncName2WStr(currentNode->name()).c_str());
        }
    } else {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger,
                               L"Move operation "
                                   << op->id() << L" to be propagated on "
                                   << Utility::s2ws(Utility::side2Str(op->targetSide())).c_str() << L" replica from \""
                                   << (currentNode->moveOrigin() ? Path2WStr(currentNode->moveOrigin().value()).c_str() : L"")
                                   << L"\" to \"" << Path2WStr(currentNode->getPath()).c_str() << L"\" (ID: "
                                   << Utility::s2ws(currentNode->id() ? currentNode->id().value() : "-1").c_str() << L")");
        }
    }
}

void OperationGeneratorWorker::generateDeleteOperation(std::shared_ptr<Node> currentNode,
                                                       std::shared_ptr<Node> correspondingNode) {
    SyncOpPtr op = std::make_shared<SyncOperation>();

    assert(correspondingNode);  // Node must exists on both replica (except for create operations)

    // Do not generate delete operation if parent already deleted
    if (_deletedNodes.find(*currentNode->parentNode()->id()) != _deletedNodes.end()) {
        return;
    }

    // Check if corresponding node has been also deleted
    if (correspondingNode->hasChangeEvent(OperationTypeDelete)) {
        op->setOmit(true);
    }

    op->setType(OperationTypeDelete);
    findAndMarkAllChildNodes(currentNode);
    currentNode->setStatus(NodeStatusProcessed);
    op->setAffectedNode(currentNode);
    op->setCorrespondingNode(correspondingNode);
    op->setTargetSide(correspondingNode->side());

    // Also mark all corresponding nodes as Processed
    findAndMarkAllChildNodes(correspondingNode);
    correspondingNode->setStatus(NodeStatusProcessed);

    _syncPal->_syncOps->pushOp(op);

    if (op->omit()) {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(_logger, L"Corresponding file already deleted on "
                                            << Utility::s2ws(Utility::side2Str(op->targetSide())).c_str()
                                            << L" replica. Operation Delete to be propagated in DB only for item "
                                            << SyncName2WStr(currentNode->name()).c_str());
        }
        _syncPal->_restart =
            true;  // In certains cases (e.g.: directory deleted and re-created with the same name), we need to trigger the start
                   // of next sync because nothing has changed but create events are not propagated
    } else {
        if (ParametersCache::isExtendedLogEnabled()) {
            LOGW_SYNCPAL_DEBUG(
                _logger, L"Delete operation "
                             << op->id() << L" to be propagated on " << Utility::s2ws(Utility::side2Str(op->targetSide())).c_str()
                             << L" replica for item " << SyncName2WStr(currentNode->name()).c_str() << L" ("
                             << Utility::s2ws(currentNode->id() ? currentNode->id().value() : "-1").c_str() << L")");
        }
    }

    _deletedNodes.insert(*currentNode->id());
}

void OperationGeneratorWorker::findAndMarkAllChildNodes(std::shared_ptr<Node> parentNode) {
    for (auto &childNode : parentNode->children()) {
        if (childNode.second->type() == NodeTypeDirectory) {
            findAndMarkAllChildNodes(childNode.second);
        }
        childNode.second->setStatus(NodeStatusProcessed);
    }
}

}  // namespace KDC
