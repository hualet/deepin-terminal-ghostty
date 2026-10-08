#pragma once

#include <QHash>
#include <QList>
#include <QString>

// One record of the program status protocol (OSC 7501).
// https://www.superlogical.com/rex/docs/build/program-status
struct ProgramStatus {
    enum class State { Idle = 0, Working = 1, Done = 2, Blocked = 3, Error = 4 };
    enum class Kind { None = 0, Permission = 1, Question = 2, Auth = 3 };

    // Empty for the root record. "/" separates hierarchical segments.
    QString id;
    State state = State::Idle;
    Kind kind = Kind::None;
    // 0-100, or -1 when the program did not report one.
    int progress = -1;
    QString app;
    QString title;
    QString message;
    // Increases on every update; larger means more recently updated.
    quint64 sequence = 0;

    bool operator==(const ProgramStatus &other) const;
    bool operator!=(const ProgramStatus &other) const { return !(*this == other); }
};

// Keeps program status records and applies the protocol's lifetime rules.
// Every mutator returns true when the set of records changed.
class ProgramStatusStore {
public:
    static constexpr int kMaxRecords = 256;

    // Replaces the record with the report's id, evicting the least recently
    // updated record when the store is full.
    bool apply(const ProgramStatus &report);
    // Removes the record with this id and every record beneath it. An empty
    // id removes every record.
    bool clear(const QString &id);
    // Removes working and blocked records, and idle records too. Used when a
    // new shell prompt starts or the program in the terminal exits.
    bool clearActive();
    // Removes done and error records once the user has seen them.
    bool clearCompleted();

    bool isEmpty() const { return m_records.isEmpty(); }
    int size() const { return static_cast<int>(m_records.size()); }
    // Records sorted by id, so the root record comes first.
    QList<ProgramStatus> records() const;

private:
    template <typename Predicate> bool removeIf(Predicate predicate);

    QHash<QString, ProgramStatus> m_records;
    quint64 m_nextSequence = 1;
};
