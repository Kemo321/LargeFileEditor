/**
 * @file BackgroundTaskManager.h
 * @brief Orchestrates the asynchronous save/find/replace tasks for the editor.
 */

#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QString>
#include <cstdint>
#include <vector>

class PieceTable;

/**
 * @class BackgroundTaskManager
 * @brief Owns the QtConcurrent workers and QFutureWatchers for long-running operations.
 *
 * Keeps threading/cancellation plumbing out of @ref MainWindow. The caller-owned PieceTable is
 * held by the worker thread while a task runs, so the caller must keep it alive and not read it
 * until the matching finished signal fires.
 */
class BackgroundTaskManager : public QObject {
    Q_OBJECT

public:
    explicit BackgroundTaskManager( QObject* parent = nullptr );

    /// Saves @p table to @p tempPath on a worker thread; emits @ref saveFinished.
    auto startSave( PieceTable* table, const QString& tempPath ) -> void;

    /// Searches @p table for @p text on a worker thread; emits @ref findFinished.
    auto startFind( PieceTable* table, const QString& text, bool matchCase,
                    bool matchWord ) -> void;

    /// Replaces every match in @p table on a worker thread; emits progress and @ref
    /// replaceFinished.
    auto startReplaceAll( PieceTable* table, const QString& findText, const QString& replaceText,
                          bool matchCase, bool matchWord ) -> void;

    [[nodiscard]] auto isSaveRunning() const -> bool;
    [[nodiscard]] auto isFindRunning() const -> bool;
    [[nodiscard]] auto isReplaceRunning() const -> bool;

    /// Requests cancellation of a running Replace All (flag delivered via @ref replaceFinished).
    auto cancelReplace() -> void;

    /// Blocks until a running save completes (no-op otherwise).
    auto waitForSave() -> void;

    /// Cancels and blocks until a running Replace All completes (no-op otherwise).
    auto waitForReplace() -> void;

signals:
    /// Emitted when a background save completes; @p success is the backend write result.
    void saveFinished( bool success );

    /// Emitted when a background search completes with its match offsets.
    void findFinished( std::vector<uint64_t> results );

    /// Emitted as Replace All progresses; @p percent is 0..100.
    void replaceProgress( int percent );

    /// Emitted when Replace All completes; @p canceled reports whether it was cancelled.
    void replaceFinished( uint64_t replacedCount, bool canceled );

private:
    auto onSaveFinished() -> void;
    auto onFindFinished() -> void;
    auto onReplaceFinished() -> void;

    QFutureWatcher<bool>* save_watcher_{ nullptr };
    QFutureWatcher<std::vector<uint64_t>>* find_watcher_{ nullptr };
    QFutureWatcher<uint64_t>* replace_watcher_{ nullptr };

    bool replace_canceled_{ false };
};
