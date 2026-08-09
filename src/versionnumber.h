#pragma once

#include <alpm.h>

#include <QString>

/// Version-string comparator backed by libalpm's pacman version logic
/// (alpm_pkg_vercmp), preserving reference semantics for
/// installed-vs-repo comparison.
class VersionNumber {
public:
    VersionNumber() = default;
    explicit VersionNumber(const QString& value) : m_str(value) {}

    bool operator<(const VersionNumber& other) const { return alpm_pkg_vercmp(m_str.toUtf8().constData(), other.m_str.toUtf8().constData()) < 0; }
    bool operator>(const VersionNumber& other) const { return alpm_pkg_vercmp(m_str.toUtf8().constData(), other.m_str.toUtf8().constData()) > 0; }
    bool operator==(const VersionNumber& other) const { return alpm_pkg_vercmp(m_str.toUtf8().constData(), other.m_str.toUtf8().constData()) == 0; }
    bool operator<=(const VersionNumber& other) const { return !(*this > other); }
    bool operator>=(const VersionNumber& other) const { return !(*this < other); }

    const QString& toString() const { return m_str; }
    QString toQString() const { return m_str; }

private:
    QString m_str;
};
