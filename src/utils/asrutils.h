#ifndef ASRUTIL_H
#define ASRUTIL_H

#include <string>
#include <QString>
#include <cctype>

namespace AsrUtil {
inline std::string trim(std::string s) {
    auto is_space = [](unsigned char c) { return std::isspace(c); };
    size_t b = 0, e = s.size();
    while (b < e && is_space(s[b])) b++;
    while (e > b && is_space(s[e-1])) e--;
    return s.substr(b, e-b);
}

inline std::string normalizeText(const std::string& str) {
    QString qs = QString::fromUtf8(str.c_str()).trimmed();
    if (qs.isEmpty()) return {};
    QString out;
    QChar prev;
    int cnt = 1;
    bool space = false;
    for (auto c : qs) {
        if (c.isSpace()) {
            if (!space && !out.isEmpty()) { out += ' '; space = true; }
            continue;
        }
        space = false;
        if (c == prev) { if (cnt < 2) { out += c; cnt++; } }
        else { prev = c; cnt = 1; out += c; }
    }
    return out.toStdString();
}

inline std::string mergeOverlap(const std::string& a, const std::string& b) {
    std::string A = trim(a), B = trim(b);
    if (A.empty()) return B; if (B.empty()) return A;
    size_t max = std::min(A.size(), B.size());
    for (size_t l = max; l > 0; l--) {
        if (A.substr(A.size()-l) == B.substr(0,l)) return A + B.substr(l);
    }
    return B;
}

inline double whisperTsToSec(int64_t ts) { return ts * 0.01; }
}

#endif
