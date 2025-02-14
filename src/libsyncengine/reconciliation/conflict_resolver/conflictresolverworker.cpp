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

#include "conflictresolverworker.h"
#include "reconciliation/platform_inconsistency_checker/platforminconsistencycheckerutility.h"
#include "libcommonserver/utility/utility.h"

namespace KDC {

ConflictResolverWorker::ConflictResolverWorker(std::shared_ptr<SyncPal> syncPal, const std::string &name,
                                               const std::string &shortName)
    : OperationProcessor(syncPal, name, shortName) {}

void ConflictResolverWorker::execute() {
    ExitCode exitCode(ExitCodeUnknown);

    LOG_SYNCPAL_DEBUG(_logger, "Worker started: name=" << name().c_str());

    _syncPal->_syncOps->startUpdate();

    while (!_syncPal->_conflictQueue->empty()) {
        bool continueSolving = false;
        exitCode = generateOperations(_syncPal->_conflictQueue->top(), continueSolving);
        if (exitCode != ExitCodeOk) {
            break;
        }

        if (continueSolving) {
            _syncPal->_conflictQueue->pop();
        } else {
            _syncPal->_conflictQueue->clear();
            break;
        }
    }

    // The sync must be restarted after the execution of the operations that resolve the conflict
    _syncPal->_restart = true;

    setDone(exitCode);
    LOG_SYNCPAL_DEBUG(_logger, "Worker stopped: name=" << name().c_str());
}

ExitCode ConflictResolverWorker::generateOperations(const Conflict &conflict, bool &continueSolving) {
    LOGW_SYNCPAL_INFO(_logger, L"Solving " << Utility::s2ws(Utility::conflictType2Str(conflict.type())).c_str()
                                           << L" conflict for items " << SyncName2WStr(conflict.node()->name()).c_str() << L" ("
                                           << Utility::s2ws(*conflict.node()->id()).c_str() << L") and "
                                           << SyncName2WStr(conflict.correspondingNode()->name()).c_str() << L" ("
                                           << Utility::s2ws(*conflict.correspondingNode()->id()).c_str() << ")");

    continueSolving = false;
    switch (conflict.type()) {
        case ConflictTypeCreateCreate:
        case ConflictTypeEditEdit:
        case ConflictTypeMoveCreate:
        case ConflictTypeMoveMoveDest: {
            // Rename the file on the local replica and remove it from DB
            SyncOpPtr op = std::make_shared<SyncOperation>();
            op->setType(OperationTypeMove);
            op->setAffectedNode(conflict.remoteNode());
            op->setCorrespondingNode(conflict.localNode());
            op->setTargetSide(ReplicaSideLocal);

            SyncName newName;
            if (!generateConflictedName(conflict.localNode(), newName)) {
                op->setNewParentNode(_syncPal->updateTree(ReplicaSideLocal)->rootNode());
            }
            op->setNewName(newName);
            op->setConflict(conflict);

            LOGW_SYNCPAL_INFO(_logger, L"Operation "
                                           << Utility::s2ws(Utility::opType2Str(op->type())).c_str() << L" to be propagated on "
                                           << Utility::s2ws(Utility::side2Str(op->targetSide())).c_str() << L" replica for item "
                                           << SyncName2WStr(op->correspondingNode()->name()).c_str() << L" ("
                                           << Utility::s2ws(*op->correspondingNode()->id()).c_str() << L")");

            _syncPal->_syncOps->pushOp(op);

            continueSolving = isConflictsWithLocalRename(conflict.type());  // solve them all in the same sync
            break;
        }
        case ConflictTypeEditDelete: {
            // Edit operation win
            auto deleteNode =
                conflict.node()->hasChangeEvent(OperationTypeDelete) ? conflict.node() : conflict.correspondingNode();
            auto editNode = conflict.node()->hasChangeEvent(OperationTypeEdit) ? conflict.node() : conflict.correspondingNode();
            if (deleteNode->parentNode()->hasChangeEvent(OperationTypeDelete)) {
                // Move the deleted node to root with a new name
                SyncOpPtr moveOp = std::make_shared<SyncOperation>();
                moveOp->setType(OperationTypeMove);
                moveOp->setAffectedNode(deleteNode);
                moveOp->setCorrespondingNode(editNode);
                moveOp->setTargetSide(editNode->side());
                SyncName newName;
                generateConflictedName(conflict.localNode(), newName);
                moveOp->setNewName(newName);
                moveOp->setNewParentNode(_syncPal->updateTree(deleteNode->side())->rootNode());
                moveOp->setConflict(conflict);

                LOGW_SYNCPAL_INFO(_logger, L"Operation " << Utility::s2ws(Utility::opType2Str(moveOp->type())).c_str()
                                                         << L" to be propagated on "
                                                         << Utility::s2ws(Utility::side2Str(moveOp->targetSide())).c_str()
                                                         << L" replica for item "
                                                         << SyncName2WStr(moveOp->correspondingNode()->name()).c_str() << L" ("
                                                         << Utility::s2ws(*moveOp->correspondingNode()->id()).c_str() << L")");

                _syncPal->_syncOps->pushOp(moveOp);

                // Generate a delete operation to remove entry from the DB only (not from the FS!)
                // The deleted file will be restored on next sync iteration
                std::unordered_set<std::shared_ptr<Node>> allDeletedNodes;
                findAllChildNodes(deleteNode, allDeletedNodes);

                SyncOpPtr deleteOp = std::make_shared<SyncOperation>();
                deleteOp->setType(OperationTypeDelete);
                deleteOp->setAffectedNode(deleteNode);
                deleteOp->setCorrespondingNode(editNode);
                deleteOp->setTargetSide(editNode->side());
                deleteOp->setOmit(true);  // Target side does not matter when we remove only in DB
                deleteOp->setConflict(conflict);

                LOGW_SYNCPAL_INFO(_logger, L"Operation " << Utility::s2ws(Utility::opType2Str(deleteOp->type())).c_str()
                                                         << L" to be propagated in DB only on "
                                                         << Utility::s2ws(Utility::side2Str(deleteOp->targetSide())).c_str()
                                                         << L" replica for item "
                                                         << SyncName2WStr(deleteOp->correspondingNode()->name()).c_str() << L" ("
                                                         << Utility::s2ws(*deleteOp->correspondingNode()->id()).c_str() << L")");

                _syncPal->_syncOps->pushOp(deleteOp);
            } else {
                // Delete the edit node from DB
                // This will cause the file to be detected as new in the next sync iteration, thus it will be restored
                std::unordered_set<std::shared_ptr<Node>> allDeletedNodes;
                findAllChildNodes(editNode, allDeletedNodes);

                SyncOpPtr deleteOp = std::make_shared<SyncOperation>();
                deleteOp->setType(OperationTypeDelete);
                deleteOp->setAffectedNode(editNode);
                deleteOp->setCorrespondingNode(deleteNode);
                deleteOp->setTargetSide(deleteNode->side());
                deleteOp->setOmit(true);  // Target side does not matter when we remove only in DB
                deleteOp->setConflict(conflict);

                LOGW_SYNCPAL_INFO(_logger, L"Operation " << Utility::s2ws(Utility::opType2Str(deleteOp->type())).c_str()
                                                         << L" to be propagated in DB only for item "
                                                         << deleteOp->correspondingNode()->name().c_str() << L" ("
                                                         << Utility::s2ws(*deleteOp->correspondingNode()->id()).c_str() << L")");

                _syncPal->_syncOps->pushOp(deleteOp);
            }

            break;
        }
        case ConflictTypeMoveDelete: {
            // Move operation win
            auto deleteNode =
                conflict.node()->hasChangeEvent(OperationTypeDelete) ? conflict.node() : conflict.correspondingNode();
            auto moveNode = conflict.node()->hasChangeEvent(OperationTypeMove) ? conflict.node() : conflict.correspondingNode();
            auto correspondingMoveNodeParent = correspondingNodeDirect(moveNode->parentNode());
            if (correspondingMoveNodeParent && correspondingMoveNodeParent->hasChangeEvent(OperationTypeDelete) &&
                _syncPal->_conflictQueue->hasConflict(ConflictTypeMoveParentDelete)) {
                // If the move operation happen within a directory that was deleted on the other replica,
                // therefor, we ignore the Move-Delete conflict
                // This conflict will be handled as Move-ParentDelete conflict
                LOG_SYNCPAL_INFO(_logger,
                                 "Move-Delete conflict ignored because it will be solved by solving Move-ParentDelete conflict");
                continueSolving = true;
                return ExitCodeOk;
            }

            // Get all children of the deleted node
            std::unordered_set<std::shared_ptr<Node>> allDeletedChildNodes;
            findAllChildNodes(deleteNode, allDeletedChildNodes);

            std::unordered_set<DbNodeId> deletedChildNodeDbIds;
            for (auto &childNode : allDeletedChildNodes) {
                deletedChildNodeDbIds.insert(*childNode->idb());
            }

            if (deleteNode->type() == NodeTypeDirectory) {
                // Get all DB IDs of the child nodes
                std::unordered_set<DbNodeId> allChildNodeDbIds;
                ExitCode res = findAllChildNodeIdsFromDb(deleteNode, allChildNodeDbIds);
                if (res != ExitCodeOk) {
                    return res;
                }

                for (const auto &dbId : allChildNodeDbIds) {
                    if (deletedChildNodeDbIds.find(dbId) == deletedChildNodeDbIds.end()) {
                        // This is an orphan node
                        bool found = false;
                        NodeId orphanNodeId;
                        if (!_syncPal->_syncDb->id(deleteNode->side(), dbId, orphanNodeId, found)) {
                            return ExitCodeDbError;
                        }
                        if (!found) {
                            LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node ID for dbId=" << dbId);
                            return ExitCodeDataError;
                        }

                        auto updateTree = _syncPal->updateTree(deleteNode->side());
                        auto orphanNode = updateTree->getNodeById(orphanNodeId);
                        auto correspondingOrphanNode = correspondingNodeInOtherTree(orphanNode);
                        if (!correspondingOrphanNode) {
                            LOGW_SYNCPAL_DEBUG(
                                _logger, L"Failed to get corresponding node: " << SyncName2WStr(orphanNode->name()).c_str());
                            return ExitCodeDataError;
                        }

                        // Move operation in db (temporarily, orphan nodes will be then handled in "Move-Move (Source)" conflict
                        // in next sync iterations)
                        SyncOpPtr op = std::make_shared<SyncOperation>();
                        op->setType(OperationTypeMove);
                        op->setAffectedNode(orphanNode);
                        orphanNode->setMoveOrigin(orphanNode->getPath());
                        op->setCorrespondingNode(correspondingOrphanNode);
                        op->setTargetSide(correspondingOrphanNode->side());
                        op->setOmit(true);
                        SyncName newName;
                        generateConflictedName(orphanNode, newName, true);
                        op->setNewName(newName);
                        op->setNewParentNode(_syncPal->updateTree(orphanNode->side())->rootNode());
                        op->setConflict(conflict);

                        LOGW_SYNCPAL_INFO(_logger, L"Operation " << Utility::s2ws(Utility::opType2Str(op->type())).c_str()
                                                                 << L" to be propagated in DB only for orphan node "
                                                                 << SyncName2WStr(op->correspondingNode()->name()).c_str()
                                                                 << L" (" << Utility::s2ws(*op->correspondingNode()->id()).c_str()
                                                                 << L")");

                        _syncPal->_syncOps->pushOp(op);

                        _registeredOrphans.insert({dbId, deleteNode->side()});
                    }
                }
            }

            // Generate a delete operation to remove entry from the DB only (not from the FS!)
            // The deleted file will be restored on next sync iteration
            SyncOpPtr op = std::make_shared<SyncOperation>();
            op->setType(OperationTypeDelete);
            op->setAffectedNode(deleteNode);
            op->setCorrespondingNode(moveNode);
            op->setTargetSide(moveNode->side());
            op->setOmit(true);  // Target side does not matter when we remove only in DB
            op->setConflict(conflict);

            LOGW_SYNCPAL_INFO(_logger, L"Operation " << Utility::s2ws(Utility::opType2Str(op->type())).c_str()
                                                     << L" to be propagated in DB only for item "
                                                     << SyncName2WStr(op->correspondingNode()->name()).c_str() << L" ("
                                                     << Utility::s2ws(*op->correspondingNode()->id()).c_str() << L")");

            _syncPal->_syncOps->pushOp(op);
            break;
        }
        case ConflictTypeMoveParentDelete: {
            // Undo move, the delete operation will be executed on a next sync iteration
            auto moveNode = conflict.node()->hasChangeEvent(OperationTypeMove) ? conflict.node() : conflict.correspondingNode();
            SyncOpPtr moveOp = std::make_shared<SyncOperation>();
            ExitCode res = undoMove(moveNode, moveOp);
            if (res != ExitCodeOk) {
                return res;
            }
            moveOp->setConflict(conflict);

            LOGW_SYNCPAL_INFO(
                _logger, L"Operation " << Utility::s2ws(Utility::opType2Str(moveOp->type())).c_str() << L" to be propagated on "
                                       << Utility::s2ws(Utility::side2Str(moveOp->targetSide())).c_str() << L" replica for item "
                                       << SyncName2WStr(moveOp->correspondingNode()->name()).c_str() << L" ("
                                       << Utility::s2ws(*moveOp->correspondingNode()->id()).c_str() << L")");

            _syncPal->_syncOps->pushOp(moveOp);
            break;
        }
        case ConflictTypeCreateParentDelete: {
            // Delete operation always win
            auto deleteNode =
                conflict.node()->hasChangeEvent(OperationTypeDelete) ? conflict.node() : conflict.correspondingNode();
            SyncOpPtr op = std::make_shared<SyncOperation>();
            op->setType(OperationTypeDelete);
            op->setAffectedNode(deleteNode);
            auto correspondingNode = correspondingNodeInOtherTree(deleteNode);
            op->setCorrespondingNode(correspondingNode);  // create node is both affected and corresponding node since it does not
                                                          // exit yet on other replice but we move it anyway
            op->setTargetSide(correspondingNode->side());
            op->setConflict(conflict);
            LOGW_SYNCPAL_INFO(_logger, L"Operation "
                                           << Utility::s2ws(Utility::opType2Str(op->type())).c_str() << L" to be propagated on "
                                           << Utility::s2ws(Utility::side2Str(op->targetSide())).c_str() << L" replica for item "
                                           << SyncName2WStr(deleteNode->name()).c_str() << L" ("
                                           << Utility::s2ws(*deleteNode->id()).c_str() << L")");

            _syncPal->_syncOps->pushOp(op);
            break;
        }
        case ConflictTypeMoveMoveSource: {
            auto loserNode = conflict.localNode();

            // Check if this node is a registered orphan
            if (_registeredOrphans.find(*conflict.node()->idb()) != _registeredOrphans.end()) {
                loserNode = _registeredOrphans.find(*conflict.node()->idb())->second == ReplicaSideLocal ? conflict.remoteNode()
                                                                                                         : conflict.localNode();
            }

            // Undo move on the loser replica
            SyncOpPtr moveOp = std::make_shared<SyncOperation>();
            ExitCode res = undoMove(loserNode, moveOp);
            if (res != ExitCodeOk) {
                return res;
            }
            moveOp->setConflict(conflict);

            LOGW_SYNCPAL_INFO(
                _logger, L"Operation " << Utility::s2ws(Utility::opType2Str(moveOp->type())).c_str() << L" to be propagated on "
                                       << Utility::s2ws(Utility::side2Str(moveOp->targetSide())).c_str() << L" replica for item "
                                       << SyncName2WStr(moveOp->correspondingNode()->name()).c_str() << L" ("
                                       << Utility::s2ws(*moveOp->correspondingNode()->id()).c_str() << L")");

            _syncPal->_syncOps->pushOp(moveOp);
            break;
        }
        case ConflictTypeMoveMoveCycle: {
            // Undo move on the local replica
            SyncOpPtr moveOp = std::make_shared<SyncOperation>();
            ExitCode res = undoMove(conflict.localNode(), moveOp);
            if (res != ExitCodeOk) {
                return res;
            }
            moveOp->setConflict(conflict);
            LOGW_SYNCPAL_INFO(
                _logger, L"Operation " << Utility::s2ws(Utility::opType2Str(moveOp->type())).c_str() << L" to be propagated on "
                                       << Utility::s2ws(Utility::side2Str(moveOp->targetSide())).c_str() << L" replica for item "
                                       << SyncName2WStr(moveOp->correspondingNode()->name()).c_str() << L" ("
                                       << Utility::s2ws(*moveOp->correspondingNode()->id()).c_str() << L")");
            _syncPal->_syncOps->pushOp(moveOp);
            break;
        }
        default: {
            LOG_SYNCPAL_WARN(_logger, "Unknown conflict type: " << conflict.type());
            return ExitCodeDataError;
        }
    }

    return ExitCodeOk;
}

bool ConflictResolverWorker::generateConflictedName(const std::shared_ptr<Node> node, SyncName &newName,
                                                    bool isOrphanNode /*= false*/) {
    SyncPath absoluteLocalFilePath = _syncPal->_localPath / node->getPath();
    newName = PlatformInconsistencyCheckerUtility::instance()->generateNewValidName(
        absoluteLocalFilePath, isOrphanNode ? PlatformInconsistencyCheckerUtility::SuffixTypeOrphan
                                            : PlatformInconsistencyCheckerUtility::SuffixTypeConflict);

    // Check path size
    size_t pathSize = absoluteLocalFilePath.parent_path().native().size() + 1 + newName.size();
    if (PlatformInconsistencyCheckerUtility::instance()->checkPathLength(pathSize, node->type())) {
        // Path is now too long, file needs to be moved to root directory
        return false;
    }

    return true;
}

void ConflictResolverWorker::findAllChildNodes(const std::shared_ptr<Node> parentNode,
                                               std::unordered_set<std::shared_ptr<Node>> &children) {
    for (auto &child : parentNode->children()) {
        auto childNode = child.second;
        if (childNode->type() == NodeTypeDirectory) {
            findAllChildNodes(childNode, children);
        }
        children.insert(childNode);
    }
}

ExitCode ConflictResolverWorker::findAllChildNodeIdsFromDb(const std::shared_ptr<Node> parentNode,
                                                           std::unordered_set<DbNodeId> &childrenDbIds) {
    std::vector<NodeId> nodeIds;
    bool found = false;
    if (!_syncPal->_syncDb->ids(parentNode->side(), nodeIds, found)) {
        return ExitCodeDbError;
    }
    if (!found) {
        LOG_SYNCPAL_WARN(_logger, "Failed to retrieve node IDs in DB");
        return ExitCodeDataError;
    }

    for (const auto &nodeId : nodeIds) {
        if (nodeId == *parentNode->id()) {
            continue;
        }

        bool isAncestor = false;
        if (!_syncPal->_syncDb->ancestor(parentNode->side(), *parentNode->id(), nodeId, isAncestor, found)) {
            return ExitCodeDbError;
        }
        if (!found) {
            LOG_SYNCPAL_WARN(_logger, "Failed to retrieve ancestor for node ID: " << nodeId.c_str() << " in DB");
            return ExitCodeDataError;
        }

        if (isAncestor) {
            DbNodeId dbNodeId;
            if (!_syncPal->_syncDb->dbId(parentNode->side(), nodeId, dbNodeId, found)) {
                return ExitCodeDbError;
            }
            if (!found) {
                LOG_SYNCPAL_WARN(_logger, "Failed to retrieve DB node ID for node ID=" << nodeId.c_str());
                return ExitCodeDataError;
            }

            childrenDbIds.insert(dbNodeId);
        }
    }
    return ExitCodeOk;
}

ExitCode ConflictResolverWorker::undoMove(const std::shared_ptr<Node> moveNode, SyncOpPtr moveOp) {
    if (!moveNode->moveOrigin().has_value()) {
        LOG_SYNCPAL_WARN(_logger, "Failed to retrieve origin parent path");
        return ExitCodeDataError;
    }

    auto updateTree = _syncPal->updateTree(moveNode->side());
    auto originParentNode = updateTree->getNodeByPath(moveNode->moveOrigin()->parent_path());
    auto originPath = moveNode->moveOrigin();
    bool undoPossible = true;

    if (!originParentNode) {
        LOG_SYNCPAL_WARN(_logger, "Failed to retrieve origin parent node");
        return ExitCodeDataError;
    }

    if (isABelowB(originParentNode, moveNode) || originParentNode->hasChangeEvent(OperationTypeDelete)) {
        undoPossible = false;
    } else {
        auto potentialOriginNode = originParentNode->getChildExcept(originPath->filename().native(), OperationTypeDelete);
        if (potentialOriginNode && (potentialOriginNode->hasChangeEvent(OperationTypeCreate) ||
                                    potentialOriginNode->hasChangeEvent(OperationTypeMove))) {
            undoPossible = false;
        }
    }

    if (undoPossible) {
        moveOp->setNewParentNode(originParentNode);
        moveOp->setNewName(originPath->filename().native());
    } else {
        moveOp->setNewParentNode(_syncPal->updateTree(moveNode->side())->rootNode());
        SyncName newName;
        generateConflictedName(moveNode, newName);
        moveOp->setNewName(newName);
    }

    moveOp->setType(OperationTypeMove);
    auto correspondingNode = correspondingNodeInOtherTree(moveNode);
    moveOp->setAffectedNode(correspondingNode);
    moveOp->setCorrespondingNode(moveNode);
    moveOp->setTargetSide(moveNode->side());

    return ExitCodeOk;
}

}  // namespace KDC
