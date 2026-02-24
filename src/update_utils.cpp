// SPDX-License-Identifier: MIT

#include "remindme/update_utils.hpp"

#include <QJsonObject>
#include <QJsonValue>
#include <QStringList>

namespace remindme
{
namespace UpdateUtils
{
VersionTriplet parseVersionTriplet(QString text)
{
    text = text.trimmed();
    if (text.startsWith('v', Qt::CaseInsensitive))
        text.remove(0, 1);

    const QStringList parts = text.split('.');
    if (parts.size() != 3)
        return {};

    bool majorOk = false;
    bool minorOk = false;
    bool patchOk = false;
    VersionTriplet parsed;
    parsed.major = parts[0].toInt(&majorOk);
    parsed.minor = parts[1].toInt(&minorOk);
    parsed.patch = parts[2].toInt(&patchOk);
    parsed.valid = majorOk && minorOk && patchOk;
    return parsed;
}

bool isRemoteVersionNewer(const QString &remoteTag, const QString &currentVersion)
{
    const VersionTriplet remote = parseVersionTriplet(remoteTag);
    const VersionTriplet current = parseVersionTriplet(currentVersion);
    if (!remote.valid || !current.valid)
        return false;

    if (remote.major != current.major)
        return remote.major > current.major;
    if (remote.minor != current.minor)
        return remote.minor > current.minor;
    return remote.patch > current.patch;
}

int updateAssetPriority(const QString &assetName)
{
    const QString lowerName = assetName.toLower();
    if (lowerName.endsWith(".exe") && (lowerName.contains("setup") || lowerName.contains("installer")))
        return 300;
    if (lowerName.endsWith(".msi"))
        return 250;
    if (lowerName.endsWith(".exe"))
        return 200;
    if (lowerName.endsWith(".zip"))
        return 100;
    return 0;
}

bool isInstallerAssetName(const QString &assetName)
{
    const QString lowerName = assetName.toLower();
    if (lowerName.endsWith(".msi"))
        return true;
    return lowerName.endsWith(".exe") && (lowerName.contains("setup") || lowerName.contains("installer"));
}

UpdateAssetInfo pickBestReleaseAsset(const QJsonArray &assets)
{
    UpdateAssetInfo best;
    int bestPriority = 0;

    for (const QJsonValue &value : assets)
    {
        if (!value.isObject())
            continue;

        const QJsonObject asset = value.toObject();
        const QString name = asset.value("name").toString().trimmed();
        const QString urlText = asset.value("browser_download_url").toString().trimmed();
        if (name.isEmpty() || urlText.isEmpty())
            continue;

        const int priority = updateAssetPriority(name);
        if (priority <= 0 || priority < bestPriority)
            continue;

        const QUrl url(urlText);
        if (!url.isValid())
            continue;

        best.name = name;
        best.downloadUrl = url;
        best.sha256Hex.clear();
        best.isInstaller = isInstallerAssetName(name);

        const QString digest = asset.value("digest").toString().trimmed();
        if (digest.startsWith("sha256:", Qt::CaseInsensitive))
            best.sha256Hex = digest.mid(7).trimmed().toLower();

        bestPriority = priority;
    }

    return best;
}
}
}
