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

#include "libcommon/utility/utility.h"
#include "libcommon/asserts.h"
#include "libcommon/utility/types.h"

#include <shlobj.h>
#include <winbase.h>
#include <windows.h>
#include <winerror.h>
#include <shlguid.h>
#include <string>

#include <QLibrary>
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QVariant>
#include <QCoreApplication>

static const char systemRunPathC[] = "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char runPathC[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run";
static const char themePathC[] = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
static const char lightThemeKeyC[] = "SystemUsesLightTheme";

namespace KDC {

static KDC::SyncPath getAppSupportDir_private() {
    return std::filesystem::temp_directory_path().parent_path().parent_path().native();
}

static KDC::SyncPath getAppDir_private() {
    return "";
}

static inline bool hasDarkSystray_private() {
    QString themePath = QLatin1String(themePathC);
    QSettings settings(themePath, QSettings::NativeFormat);
    return !settings.value(QLatin1String(lightThemeKeyC), true).toBool();
}

}  // namespace KDC
