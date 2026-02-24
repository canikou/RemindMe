// SPDX-License-Identifier: MIT

#pragma once

#include <QJsonArray>
#include <QString>
#include <QUrl>

namespace remindme
{
namespace UpdateUtils
{
struct VersionTriplet
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    bool valid = false;
};

VersionTriplet parseVersionTriplet(QString text);
bool isRemoteVersionNewer(const QString &remoteTag, const QString &currentVersion);

struct UpdateAssetInfo
{
    QString name;
    QUrl downloadUrl;
    QString sha256Hex;
    bool isInstaller = false;
};

int updateAssetPriority(const QString &assetName);
bool isInstallerAssetName(const QString &assetName);
UpdateAssetInfo pickBestReleaseAsset(const QJsonArray &assets);
}
}
