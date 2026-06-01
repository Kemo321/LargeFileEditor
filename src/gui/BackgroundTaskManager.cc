#include "gui/BackgroundTaskManager.h"

#include <QPromise>
#include <QtConcurrent/QtConcurrent>
#include <atomic>
#include <string>

#include "backend/PieceTable.h"

BackgroundTaskManager::BackgroundTaskManager( QObject* parent ) : QObject( parent )
{
    save_watcher_ = new QFutureWatcher<bool>( this );
    connect( save_watcher_, &QFutureWatcher<bool>::finished, this,
             &BackgroundTaskManager::onSaveFinished );

    find_watcher_ = new QFutureWatcher<std::vector<uint64_t>>( this );
    connect( find_watcher_, &QFutureWatcher<std::vector<uint64_t>>::finished, this,
             &BackgroundTaskManager::onFindFinished );

    replace_watcher_ = new QFutureWatcher<uint64_t>( this );
    connect( replace_watcher_, &QFutureWatcher<uint64_t>::progressValueChanged, this,
             &BackgroundTaskManager::replaceProgress );
    connect( replace_watcher_, &QFutureWatcher<uint64_t>::finished, this,
             &BackgroundTaskManager::onReplaceFinished );
}

auto BackgroundTaskManager::startSave( PieceTable* table, const QString& tempPath ) -> void
{
    QFuture<bool> future = QtConcurrent::run(
        [table, tempPath]() { return table->saveToFile( tempPath.toStdString() ); } );
    save_watcher_->setFuture( future );
}

auto BackgroundTaskManager::startFind( PieceTable* table, const QString& text, bool matchCase,
                                       bool matchWord ) -> void
{
    QFuture<std::vector<uint64_t>> future =
        QtConcurrent::run( [table, text, matchCase, matchWord]() {
            return table->findAll( text.toStdString(), matchCase, matchWord );
        } );
    find_watcher_->setFuture( future );
}

auto BackgroundTaskManager::startReplaceAll( PieceTable* table, const QString& findText,
                                             const QString& replaceText, bool matchCase,
                                             bool matchWord ) -> void
{
    replace_canceled_ = false;

    const std::string pat = findText.toUtf8().toStdString();
    const std::string repl = replaceText.toUtf8().toStdString();

    QFuture<uint64_t> future = QtConcurrent::run( [table, pat, repl, matchCase,
                                                   matchWord]( QPromise<uint64_t>& promise ) {
        promise.setProgressRange( 0, 100 );
        std::atomic<bool> cancel{ false };
        auto progress = [&promise, &cancel]( uint64_t done, uint64_t total ) {
            if( promise.isCanceled() ) {
                cancel.store( true );
            }
            promise.setProgressValue( total != 0 ? static_cast<int>( done * 100 / total ) : 100 );
        };
        promise.addResult( table->replaceAll( pat, repl, matchCase, matchWord, progress, cancel ) );
    } );

    replace_watcher_->setFuture( future );
}

auto BackgroundTaskManager::isSaveRunning() const -> bool
{
    return ( save_watcher_ != nullptr ) && save_watcher_->isRunning();
}

auto BackgroundTaskManager::isFindRunning() const -> bool
{
    return ( find_watcher_ != nullptr ) && find_watcher_->isRunning();
}

auto BackgroundTaskManager::isReplaceRunning() const -> bool
{
    return ( replace_watcher_ != nullptr ) && replace_watcher_->isRunning();
}

auto BackgroundTaskManager::cancelReplace() -> void
{
    if( isReplaceRunning() ) {
        replace_canceled_ = true;
        replace_watcher_->future().cancel();
    }
}

auto BackgroundTaskManager::waitForSave() -> void
{
    if( isSaveRunning() ) {
        save_watcher_->waitForFinished();
    }
}

auto BackgroundTaskManager::waitForReplace() -> void
{
    if( isReplaceRunning() ) {
        replace_watcher_->future().cancel();
        replace_watcher_->waitForFinished();
    }
}

auto BackgroundTaskManager::onSaveFinished() -> void
{
    emit this->saveFinished( save_watcher_->result() );
}

auto BackgroundTaskManager::onFindFinished() -> void
{
    emit this->findFinished( find_watcher_->result() );
}

auto BackgroundTaskManager::onReplaceFinished() -> void
{
    emit this->replaceFinished( replace_watcher_->result(), replace_canceled_ );
}
