// Author: Jan Szwagierczak

#include "gui/CursorManager.h"

static constexpr int kCursorBlinkRateMs = 500;

CursorManager::CursorManager( QObject* parent ) : QObject( parent )
{
    timer_ = new QTimer( this );
    connect( timer_, &QTimer::timeout, this, &CursorManager::toggleVisible );
    timer_->start( kCursorBlinkRateMs );
}

auto CursorManager::line() const -> int
{
    return line_;
}

auto CursorManager::col() const -> int
{
    return col_;
}

auto CursorManager::setPosition( int line, int col ) -> void
{
    line_ = line;
    col_ = col;
}

auto CursorManager::isVisible() const -> bool
{
    return visible_;
}

auto CursorManager::setVisible( bool visible ) -> void
{
    visible_ = visible;
}

auto CursorManager::startBlink() -> void
{
    timer_->start( kCursorBlinkRateMs );
}

auto CursorManager::stopBlink() -> void
{
    timer_->stop();
}

auto CursorManager::toggleVisible() -> void
{
    visible_ = !visible_;
    emit blinkToggled();
}
