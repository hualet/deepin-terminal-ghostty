#include "ProgramStatus.h"

#include <algorithm>

bool ProgramStatus::operator==(const ProgramStatus &other) const {
    // sequence is bookkeeping, not content.
    return id == other.id && state == other.state && kind == other.kind && progress == other.progress
           && app == other.app && title == other.title && message == other.message;
}

template <typename Predicate> bool ProgramStatusStore::removeIf(Predicate predicate) {
    bool removed = false;
    for (auto it = m_records.begin(); it != m_records.end();) {
        if (predicate(it.value())) {
            it = m_records.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    return removed;
}

bool ProgramStatusStore::apply(const ProgramStatus &report) {
    auto existing = m_records.find(report.id);
    if (existing != m_records.end()) {
        const bool changed = existing.value() != report;
        existing.value() = report;
        existing.value().sequence = m_nextSequence++;
        return changed;
    }

    if (m_records.size() >= kMaxRecords) {
        auto oldest =
            std::min_element(m_records.begin(), m_records.end(),
                             [](const ProgramStatus &a, const ProgramStatus &b) { return a.sequence < b.sequence; });
        m_records.erase(oldest);
    }

    ProgramStatus record = report;
    record.sequence = m_nextSequence++;
    m_records.insert(record.id, record);
    return true;
}

bool ProgramStatusStore::clear(const QString &id) {
    if (id.isEmpty()) {
        if (m_records.isEmpty())
            return false;
        m_records.clear();
        return true;
    }

    const QString childPrefix = id + QLatin1Char('/');
    return removeIf([&](const ProgramStatus &record) { return record.id == id || record.id.startsWith(childPrefix); });
}

bool ProgramStatusStore::clearActive() {
    return removeIf([](const ProgramStatus &record) {
        return record.state == ProgramStatus::State::Working || record.state == ProgramStatus::State::Blocked
               || record.state == ProgramStatus::State::Idle;
    });
}

bool ProgramStatusStore::clearCompleted() {
    return removeIf([](const ProgramStatus &record) {
        return record.state == ProgramStatus::State::Done || record.state == ProgramStatus::State::Error;
    });
}

QList<ProgramStatus> ProgramStatusStore::records() const {
    QList<ProgramStatus> result = m_records.values();
    std::sort(result.begin(), result.end(), [](const ProgramStatus &a, const ProgramStatus &b) { return a.id < b.id; });
    return result;
}
