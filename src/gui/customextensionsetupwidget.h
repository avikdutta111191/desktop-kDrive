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

#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSize>
#include <QTimer>
#include <QVBoxLayout>

namespace KDC {

class CustomExtensionSetupWidget : public QWidget {
        Q_OBJECT

        Q_PROPERTY(QColor logo_color READ logoColor WRITE setLogoColor)

    public:
        explicit CustomExtensionSetupWidget(QWidget *parent = nullptr, bool addDriveSetup = true);

        QString picturePath(bool fullDiskAccess);
        void addDriveMainLayoutInit();
        void dialogMainLayoutInit();

        QPushButton *_finishedButton;

    signals:
        void finishedButtonTriggered();

    private:
        QLabel *_logoTextIconLabel;
        QLabel *_descriptionLabel;
        QColor _logoColor;
        QLabel *_step1Label;
        QLabel *_step2Label;
        QTimer *_timer;
        QVBoxLayout *_mainLayout;

        bool _isAddDriveSetup;

        void initUI();
        inline QColor logoColor() const { return _logoColor; };
        void setLogoColor(const QColor &color);

    private slots:
        void onLinkActivated(const QString &link);
        void onUpdateProgress();
        void onFinishedButtonTriggered(bool checked = false);
};

}  // namespace KDC
