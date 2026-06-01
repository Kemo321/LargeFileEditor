/**
 * @file CursorManager.h
 * @brief Cursor coordinate and blink/visibility state for the editor viewport.
 */

#pragma once

#include <QObject>
#include <QTimer>

/**
 * @class CursorManager
 * @brief Owns the text cursor's (line, column) position and its blink/visibility state.
 *
 * Coordinate clamping lives with the view (which knows line counts); this class only stores
 * the position and drives the blink timer, emitting @ref blinkToggled so the viewport repaints.
 */
class CursorManager : public QObject {
    Q_OBJECT

public:
    explicit CursorManager( QObject* parent = nullptr );

    [[nodiscard]] auto line() const -> int;
    [[nodiscard]] auto col() const -> int;

    /// Sets the cursor coordinates verbatim (no clamping).
    auto setPosition( int line, int col ) -> void;

    [[nodiscard]] auto isVisible() const -> bool;

    /// Forces the cursor visible (e.g. right after a move or edit).
    auto setVisible( bool visible ) -> void;

    /// Resumes/suspends the blink animation.
    auto startBlink() -> void;
    auto stopBlink() -> void;

signals:
    /// Emitted on each blink toggle so the viewport can repaint.
    void blinkToggled();

private:
    auto toggleVisible() -> void;

    int line_{ 0 };
    int col_{ 0 };
    bool visible_{ true };
    QTimer* timer_{ nullptr };
};
