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

#include "libcommon/utility/types.h"
#include "libsyncengine/progress/syncfileitem.h"

#include <memory>

#include <QObject>
#include <QScopedPointer>
#include <QSharedPointer>

#include <log4cplus/logger.h>
#include <log4cplus/loggingmacros.h>

namespace KDC {

struct VfsSetupParams {
        int _syncDbId;
        int _driveId;
        int _userId;
        std::filesystem::path _localPath;
        std::filesystem::path _targetPath;
        std::string _namespaceCLSID;
        KDC::ExecuteCommand _executeCommand;
        log4cplus::Logger _logger;
};

/** Interface describing how to deal with virtual/placeholder files.
 *
 * There are different ways of representing files locally that will only
 * be filled with data (hydrated) on demand. One such way would be suffixed
 * files, others could be FUSE based or use Windows CfAPI.
 *
 * This interface intends to decouple the sync algorithm and Folder from
 * the details of how a particular VFS solution works.
 *
 * An instance is usually created through a plugin via the createVfsFromPlugin()
 * function.
 */
class Vfs : public QObject {
        Q_OBJECT

    public:
        static QString modeToString(KDC::VirtualFileMode virtualFileMode);
        static KDC::VirtualFileMode modeFromString(const QString &str);

        explicit Vfs(VfsSetupParams &vfsSetupParams, QObject *parent = nullptr);

        virtual ~Vfs();

        inline void setSyncFileStatusCallback(void (*syncFileStatus)(int, const KDC::SyncPath &, KDC::SyncFileStatus &)) {
            _syncFileStatus = syncFileStatus;
        }
        inline void setSyncFileSyncingCallback(void (*syncFileSyncing)(int, const KDC::SyncPath &, bool &)) {
            _syncFileSyncing = syncFileSyncing;
        }
        inline void setSetSyncFileSyncingCallback(void (*setSyncFileSyncing)(int, const KDC::SyncPath &, bool)) {
            _setSyncFileSyncing = setSyncFileSyncing;
        }
        inline void setExclusionAppListCallback(void (*exclusionAppList)(QString &)) { _exclusionAppList = exclusionAppList; }

        virtual KDC::VirtualFileMode mode() const = 0;

        /** Initializes interaction with the VFS provider.
         *
         * The plugin-specific work is done in startImpl().
         */
        bool start(bool &installationDone, bool &activationDone, bool &connectionDone);

        /// Stop interaction with VFS provider. Like when the client application quits.
        /// Also deregister the folder with the sync provider, like when a folder is removed.
        void stop(bool unregister);

        /** Whether the socket api should show pin state options
         *
         * Some plugins might provide alternate shell integration, making the normal
         * context menu actions redundant.
         */
        virtual bool socketApiPinStateActionsShown() const = 0;

        /** Return true when download of a file's data is currently ongoing.
         *
         * See also the beginHydrating() and doneHydrating() signals.
         */
        virtual bool isHydrating() const = 0;

        /** Update placeholder metadata during discovery.
         *
         * If the remote metadata changes, the local placeholder's metadata should possibly
         * change as well.
         *
         * Returning false and setting error indicates an error.
         */
        virtual bool updateMetadata(const QString &filePath, time_t creationTime, time_t modtime, qint64 size,
                                    const QByteArray &fileId, QString *error) = 0;

        /// Create a new dehydrated placeholder
        virtual bool createPlaceholder(const KDC::SyncPath &relativeLocalPath, const KDC::SyncFileItem &item) = 0;

        /** Convert a hydrated placeholder to a dehydrated one. Called from PropagateDownlaod.
         *
         * This is different from delete+create because preserving some file metadata
         * (like pin states) may be essential for some vfs plugins.
         */
        virtual bool dehydratePlaceholder(const QString &path) = 0;

        /** Discovery hook: even unchanged files may need UPDATE_METADATA.
         *
         * For instance cfapi vfs wants local hydrated non-placeholder files to
         * become hydrated placeholder files.
         */
        virtual bool needsMetadataUpdate(const KDC::SyncFileItem &item) = 0;

        /** Convert a new file to a hydrated placeholder.
         *
         * Some VFS integrations expect that every file, including those that have all
         * the remote data, are "placeholders".
         * to convert newly downloaded, fully hydrated files into placeholders.
         *
         * Implementations must make sure that calling this function on a file that already
         * is a placeholder is acceptable.
         *
         * replacesFile can optionally contain a filesystem path to a placeholder that this
         * new placeholder shall supersede, for rename-replace actions with new downloads,
         * for example.
         */
        virtual bool convertToPlaceholder(const QString &path, const KDC::SyncFileItem &item, bool &needRestart) = 0;

        virtual bool updateFetchStatus(const QString &tmpPath, const QString &path, qint64 received, bool &canceled,
                                       bool &finished) = 0;

        virtual bool forceStatus(const QString &path, bool isSyncing, int progress, bool isHydrated = false) = 0;
        virtual bool cleanUpStatuses() { return true; };

        /// Determine whether the file at the given absolute path is a dehydrated placeholder.
        virtual bool isDehydratedPlaceholder(const QString &filePath, bool isAbsolutePath = false) = 0;

        /** Similar to isDehydratedPlaceholder() but used from sync discovery.
         *
         * This function shall set stat->type if appropriate.
         * It may rely on stat->path and stat_data (platform specific data).
         *
         * Returning true means that type was fully determined.
         */
        // virtual bool statTypeVirtualFile(csync_file_stat_t *stat, void *stat_data, const QString &fileDirectory) = 0;

        /** Sets the pin state for the item at a path.
         *
         * The pin state is set on the item and for all items below it.
         *
         * Usually this would forward to setting the pin state flag in the db table,
         * but some vfs plugins will store the pin state in file attributes instead.
         *
         * fileRelativePath is relative to the sync folder. Can be "" for root folder.
         */
        virtual bool setPinState(const QString &fileRelativePath, KDC::PinState state) = 0;

        /** Returns the pin state of an item at a path.
         *
         * Usually backed by the db's effectivePinState() function but some vfs
         * plugins will override it to retrieve the state from elsewhere.
         *
         * fileRelativePath is relative to the sync folder. Can be "" for root folder.
         *
         * Returns none on retrieval error.
         */
        virtual KDC::PinState pinState(const QString &fileRelativePath) = 0;
        virtual bool status(const QString &filePath, bool &isPlaceholder, bool &isHydrated, bool &isSyncing, int &progress) = 0;

        virtual bool setThumbnail(const QString &filePath, const QPixmap &pixmap) = 0;

        virtual bool setAppExcludeList() = 0;

        virtual bool getFetchingAppList(QHash<QString, QString> &appTable) = 0;

        virtual void exclude(const QString &) = 0;
        virtual bool isExcluded(const QString &filePath) = 0;

        virtual void cancelHydrate(const QString &) {}

        virtual bool fileStatusChanged(const QString &systemFileName, KDC::SyncFileStatus fileStatus) = 0;

        virtual void convertDirContentToPlaceholder(const QString &, bool) {}

        virtual void clearFileAttributes(const QString &) = 0;

        inline void setExtendedLog(bool extendedLog) { _extendedLog = extendedLog; }

        inline const std::string &namespaceCLSID() { return _vfsSetupParams._namespaceCLSID; }
        inline void setNamespaceCLSID(const std::string &CLSID) { _vfsSetupParams._namespaceCLSID = CLSID; }

    signals:
        /// Emitted when a user-initiated hydration starts
        void beginHydrating();
        /// Emitted when the hydration ends
        void doneHydrating();

    protected:
        VfsSetupParams _vfsSetupParams;

        // Callbacks
        void (*_syncFileStatus)(int syncDbId, const KDC::SyncPath &itemPath, KDC::SyncFileStatus &status);
        void (*_syncFileSyncing)(int syncDbId, const KDC::SyncPath &itemPath, bool &syncing);
        void (*_setSyncFileSyncing)(int syncDbId, const KDC::SyncPath &itemPath, bool syncing);
        void (*_exclusionAppList)(QString &appList);

        inline bool extendedLog() { return _extendedLog; }

        /** Setup the plugin for the folder.
         *
         * For example, the VFS provider might monitor files to be able to start a file
         * hydration (download of a file's remote contents) when the user wants to open
         * it.
         *
         * Usually some registration needs to be done with the backend. This function
         * should take care of it if necessary.
         */
        virtual bool startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) = 0;

        virtual void stopImpl(bool unregister) = 0;

        inline log4cplus::Logger logger() { return _vfsSetupParams._logger; }

    private:
        void effectivePinState(const QString &relativePath, KDC::PinState &effPinState);

        bool _extendedLog;
        bool _started;
};

}  // namespace KDC

Q_DECLARE_INTERFACE(KDC::Vfs, "Vfs")

//
// VfsOff
//

namespace KDC {

/// Implementation of Vfs for Vfs::Off mode - does nothing
class VfsOff : public Vfs {
        Q_OBJECT
        Q_INTERFACES(KDC::Vfs)

    public:
        VfsOff(VfsSetupParams &vfsSetupParams, QObject *parent = nullptr);

        virtual ~VfsOff();

        KDC::VirtualFileMode mode() const override { return KDC::VirtualFileModeOff; }

        bool socketApiPinStateActionsShown() const override { return false; }
        bool isHydrating() const override { return false; }

        bool updateMetadata(const QString &, time_t, time_t, qint64, const QByteArray &, QString *) override { return true; }
        bool createPlaceholder(const KDC::SyncPath &, const KDC::SyncFileItem &) override { return true; }
        bool dehydratePlaceholder(const QString &) override { return true; }
        bool convertToPlaceholder(const QString &, const KDC::SyncFileItem &, bool &) override { return true; }
        bool updateFetchStatus(const QString &, const QString &, qint64, bool &, bool &) override { return true; }
        bool forceStatus(const QString &path, bool isSyncing, int progress, bool isHydrated = false) override;

        bool needsMetadataUpdate(const KDC::SyncFileItem &) override { return false; }
        bool isDehydratedPlaceholder(const QString &, bool) override { return false; }

        bool setPinState(const QString &, KDC::PinState) override { return true; }
        KDC::PinState pinState(const QString &) override { return KDC::PinStateAlwaysLocal; }
        bool status(const QString &, bool &, bool &, bool &, int &) override { return true; }
        virtual bool setThumbnail(const QString &, const QPixmap &) override { return true; }
        virtual bool setAppExcludeList() override { return true; }
        virtual bool getFetchingAppList(QHash<QString, QString> &) override { return true; }
        virtual void exclude(const QString &) override {}
        virtual bool isExcluded(const QString &) override { return false; }
        virtual bool fileStatusChanged(const QString &, KDC::SyncFileStatus) override { return true; }

        virtual void clearFileAttributes(const QString &) override {}

    protected:
        bool startImpl(bool &installationDone, bool &activationDone, bool &connectionDone) override;
        void stopImpl(bool /*unregister*/) override {}
};

/// Check whether the plugin for the mode is available.
bool isVfsPluginAvailable(KDC::VirtualFileMode virtualFileMode, QString &error);

/// Return the best available VFS mode.
KDC::VirtualFileMode bestAvailableVfsMode();

/// Create a VFS instance for the mode, returns nullptr on failure.
std::unique_ptr<Vfs> createVfsFromPlugin(KDC::VirtualFileMode virtualFileMode, VfsSetupParams &vfsSetupParams, QString &error);

}  // namespace KDC
