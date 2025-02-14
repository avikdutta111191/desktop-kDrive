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

#include "testio.h"

#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

using namespace CppUnit;

namespace KDC {

void TestIo::testGetXAttrValue() {
#if defined(__APPLE__)
    // A regular file without any extended attributes
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        IoError ioError = IoErrorSuccess;
        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular directory without any extended attributes
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        IoError ioError = IoErrorSuccess;
        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular symbolic link on a file without any extended attributes
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorSuccess;
        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg";  // This file does not exist.
        IoError ioError = IoErrorSuccess;
        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
        CPPUNIT_ASSERT(value.empty());
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a');  // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName;  // This file doesn't exist.
        std::string value;
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(!_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorFileNameTooLong);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular file missing owner read permission and without extended attribute: access denied expected
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "permission_less_file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }
        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);

        std::string value;
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);
        CPPUNIT_ASSERT(ioError == IoErrorAccessDenied);
        CPPUNIT_ASSERT(value.empty());
    }

    // An existing file with an extended attribute
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "sugar-free", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "sugar-free");
    }

    // An existing directory with an extended attribute
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path;

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "super-dry", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "super-dry");
    }

    // A regular symbolic link on a file, with an extended attribute set for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "regular-file-symlink", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "regular-file-symlink");

        CPPUNIT_ASSERT(_testObj->getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular symbolic link on a folder, with an extended attribute for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "regular-dir-symlink", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "regular-dir-symlink");

        CPPUNIT_ASSERT(_testObj->getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A dangling symbolic link on a file, with an extended attribute set for the link
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "non_existing_test_file.txt";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "dangling-symbolic-link", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "dangling-symbolic-link");
    }

    // A MacOSX Finder alias on a regular file, with an extended attribute set for the alias.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "sane-alias", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "sane-alias");

        CPPUNIT_ASSERT(_testObj->getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // An existing file with an extended attribute
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "file.txt";
        {
            std::ofstream ofs(path);
            ofs << "Some content.\n";
        }

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "sugar-free", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "sugar-free");
    }

    // An existing directory with an extended attribute
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path;

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "super-dry", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "super-dry");
    }

    // A regular symbolic link on a file, with an extended attribute set for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "regular-file-symlink", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "regular-file-symlink");

        CPPUNIT_ASSERT(_testObj->getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A regular symbolic link on a folder, with an extended attribute for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "regular-dir-symlink", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "regular-dir-symlink");

        CPPUNIT_ASSERT(_testObj->getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }

    // A dangling symbolic link on a file, with an extended attribute set for the link
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "non_existing_test_file.txt";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "dangling-symbolic-link", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "dangling-symbolic-link");
    }

    // A MacOSX Finder alias on a regular file, with an extended attribute set for the alias.
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const SyncPath path = temporaryDirectory.path / "regular_file_alias";

        IoError aliasError;
        CPPUNIT_ASSERT(IoHelper::createAliasFromPath(targetPath, path, aliasError));

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, "status", "sane-alias", ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        std::string value;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value == "sane-alias");

        CPPUNIT_ASSERT(_testObj->getXAttrValue(targetPath, "status", value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorAttrNotFound);
        CPPUNIT_ASSERT(value.empty());
    }
#elif defined(_WIN32)
    // A regular file
    {
        const SyncPath path = _localTestDirPath / "test_pictures/picture-1.jpg";
        IoError ioError = IoErrorUnknown;
        bool value = true;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(!value);

        ioError = IoErrorUnknown;
        value = true;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_DIRECTORY, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(!value);

        ioError = IoErrorUnknown;
        value = true;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_NORMAL, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(!value);
    }

    // A regular directory
    {
        const SyncPath path = _localTestDirPath / "test_pictures";
        IoError ioError = IoErrorUnknown;
        bool value = true;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(!value);

        ioError = IoErrorUnknown;
        value = false;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_DIRECTORY, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value);
    }

    // A regular symbolic link on a file without any extended attributes
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorSuccess;
        bool value = true;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(!value);
    }

    // A non-existing file
    {
        const SyncPath path = _localTestDirPath / "non-existing.jpg";  // This file does not exist.
        IoError ioError = IoErrorSuccess;
        bool value = true;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
        CPPUNIT_ASSERT(!value);
    }

    // A non-existing file with a very long name
    {
        const std::string veryLongfileName(1000, 'a');  // Exceeds the max allowed name length on every file system of interest.
        const SyncPath path = _localTestDirPath / veryLongfileName;  // This file doesn't exist.
        bool value = true;
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorNoSuchFileOrDirectory);
        CPPUNIT_ASSERT(!value);
    }

    // A regular file missing owner read permission and without extended attribute: no error expected
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "permission_less_file.txt";
        { std::ofstream ofs(path); }
        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::remove);

        bool value = true;
        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));

        std::filesystem::permissions(path, std::filesystem::perms::owner_read, std::filesystem::perm_options::add);
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(!value);
    }

    // An existing file with an extended attribute
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "file.txt";
        { std::ofstream ofs(path); }

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool value = false;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value);
    }

    // An existing directory with an extended attribute
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path;

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool value = false;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value);
    }

    // A regular symbolic link on a file, with an extended attribute set for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures/picture-1.jpg";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_file_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorSuccess;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_READONLY, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool value = false;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_READONLY, value, ioError));

        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value);

        CPPUNIT_ASSERT(_testObj->getXAttrValue(targetPath, FILE_ATTRIBUTE_READONLY, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(!value);

        // Restore permission to allow automatic removal of the temporary directory
        _testObj->setXAttrValue(path, FILE_ATTRIBUTE_NORMAL, ioError);
    }

    // A regular symbolic link on a folder, with an extended attribute for the link
    {
        const SyncPath targetPath = _localTestDirPath / "test_pictures";
        const TemporaryDirectory temporaryDirectory;
        const SyncPath path = temporaryDirectory.path / "regular_dir_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool value = false;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_HIDDEN, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value);

        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(targetPath, FILE_ATTRIBUTE_HIDDEN, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(!value);

        // Restore permission to allow automatic removal of the temporary directory
        _testObj->setXAttrValue(path, FILE_ATTRIBUTE_NORMAL, ioError);
    }

    // A dangling symbolic link on a file, with an extended attribute set for the link
    {
        const TemporaryDirectory temporaryDirectory;
        const SyncPath targetPath = temporaryDirectory.path / "non_existing_test_file.txt";  // This file does not exist.
        const SyncPath path = temporaryDirectory.path / "dangling_symbolic_link";
        std::filesystem::create_symlink(targetPath, path);

        IoError ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->setXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);

        bool value = false;
        ioError = IoErrorUnknown;
        CPPUNIT_ASSERT(_testObj->getXAttrValue(path, FILE_ATTRIBUTE_OFFLINE, value, ioError));
        CPPUNIT_ASSERT(ioError == IoErrorSuccess);
        CPPUNIT_ASSERT(value);
    }
#endif
}
}  // namespace KDC
