/**
 * @file LargeFileViewer.h
 * @author Tomasz Okon
 * @brief GUI widget for viewing and editing immense files without loading them fully into RAM.
 */

#pragma once

#include <QAbstractScrollArea>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <vector>

#include "backend/PieceTable.h"

/**
 * @class LargeFileViewer
 * @brief Custom scroll area rendering virtualized text views based on the PieceTable.
 */
class LargeFileViewer : public QAbstractScrollArea {
    Q_OBJECT

public:
    /**
     * @brief Constructs the viewer widget.
     * @param parent Pointer to parent QWidget.
     */
    explicit LargeFileViewer( QWidget* parent = nullptr );
    ~LargeFileViewer() override = default;

    /**
     * @brief Provides a list of words to be mock-highlighted.
     * @param words List of strings to emphasize visually.
     */
    auto setMockHighlights( const QStringList& words ) -> void;

    /**
     * @brief Attaches a backend PieceTable instance to this viewer.
     * @param pieceTable Pointer to initialized piece table.
     */
    auto setPieceTable( PieceTable* pieceTable ) -> void;

    /**
     * @brief Moves the internal cursor.
     * @param line Zero-based line number.
     * @param col Zero-based column index.
     */
    auto setCursorPosition( int line, int col ) -> void;

    /**
     * @brief Adjusts the vertical scrollbar to ensure the cursor is visible.
     */
    auto scrollToCursor() -> void;

    /**
     * @brief Translates a logical byte position to view coordinates and jumps to it.
     * @param pos Logical byte position.
     */
    auto jumpToLogicalPosition( uint64_t pos ) -> void;

    /**
     * @brief Forces a complete repaint and cache invalidation.
     */
    auto refreshView() -> void;

protected:
    auto eventFilter( QObject* obj, QEvent* event ) -> bool override;
    auto resizeEvent( QResizeEvent* event ) -> void override;
    auto wheelEvent( QWheelEvent* event ) -> void override;
    auto keyPressEvent( QKeyEvent* event ) -> void override;
    auto mousePressEvent( QMouseEvent* event ) -> void override;

private:
    auto blinkCursor() -> void;
    auto onScrollbarMoved( int value ) -> void;
    auto paintViewport( QPaintEvent* event ) -> void;

    [[nodiscard]] auto getLogicalPosition( int line, int col ) const -> uint64_t;
    [[nodiscard]] auto getLineText( int line ) const -> QString;
    [[nodiscard]] auto getLineTextCached( int line ) -> QString;

    auto refreshLineOffsets() -> void;
    auto invalidateCache() -> void;

    PieceTable* piece_table_{ nullptr };
    QStringList mock_highlight_words_;
    std::vector<uint64_t> line_offsets_;

    struct CachedLine {
        int line_;
        QString text_;
    };
    std::vector<CachedLine> line_cache_;

    int cursor_line_{ 0 };
    int cursor_col_{ 0 };
    bool cursor_visible_{ true };
    QTimer* cursor_timer_{ nullptr };
    QTimer* line_offset_timer_{ nullptr };

    static constexpr int kDefaultGutterWidth = 50;
    int gutter_width_{ kDefaultGutterWidth };
};
