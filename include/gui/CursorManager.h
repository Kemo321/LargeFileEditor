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
    /// Constructs the cursor and starts its blink timer.
    explicit CursorManager( QObject* parent = nullptr );

    /// Current cursor line.
    [[nodiscard]] auto line() const -> int;
    /// Current cursor column.
    [[nodiscard]] auto col() const -> int;

    /// Sets the cursor coordinates verbatim (no clamping).
    auto setPosition( int line, int col ) -> void;

    /// Whether the caret is currently visible.
    [[nodiscard]] auto isVisible() const -> bool;

    /// Forces the cursor visible (e.g. right after a move or edit).
    auto setVisible( bool visible ) -> void;

    /// Resumes/suspends the blink animation.
    auto startBlink() -> void;
    /// Stops the blink animation.
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
