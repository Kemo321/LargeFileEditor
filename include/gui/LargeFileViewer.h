/**
 * @file LargeFileViewer.h
 * @author Tomasz Okon, Jan Szwagierczak
 * @brief GUI widget for viewing and editing immense files without loading them fully into RAM.
 */

#pragma once

#include <QAbstractScrollArea>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStringList>
#include <QTimer>
#include <QWidget>
#include <memory>
#include <vector>

#include "backend/PieceTable.h"
#include "gui/LineManager.h"

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
     * @brief Sets search results and the currently active index to highlight.
     * @param searchResults List of logical byte offsets where the search string occurs.
     * @param activeIndex The index in searchResults that is currently active.
     * @param searchLength Length of the search string.
     */
    auto setSearchHighlights( const std::vector<uint64_t>& searchResults, int activeIndex,
                              int searchLength ) -> void;

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

signals:
    /**
     * @brief Emitted when the internal cursor position changes.
     * @param line The new 0-indexed line number.
     * @param col The new 0-indexed column number.
     */
    void cursorPositionChanged( int line, int col );

    /**
     * @brief Emitted when the document has been modified.
     */
    void documentModified();

protected:
    /**
     * @brief Filters events for the viewport to handle custom painting.
     * @param obj Object receiving the event.
     * @param event The event being received.
     * @return True if the event was handled, false otherwise.
     */
    auto eventFilter( QObject* obj, QEvent* event ) -> bool override;

    /**
     * @brief Handles widget resize events to recalculate visible lines.
     * @param event The resize event.
     */
    auto resizeEvent( QResizeEvent* event ) -> void override;

    /**
     * @brief Handles mouse wheel events for scrolling.
     * @param event The wheel event.
     */
    auto wheelEvent( QWheelEvent* event ) -> void override;

    /**
     * @brief Handles key presses for cursor movement and text editing.
     * @param event The key event.
     */
    auto keyPressEvent( QKeyEvent* event ) -> void override;

    /**
     * @brief Handles mouse presses for cursor positioning.
     * @param event The mouse event.
     */
    auto mousePressEvent( QMouseEvent* event ) -> void override;

private:
    auto blinkCursor() -> void;
    auto onScrollbarMoved( int value ) -> void;
    auto paintViewport( QPaintEvent* event ) -> void;

    [[nodiscard]] auto getLogicalPosition( int line, int col ) const -> uint64_t;
    [[nodiscard]] auto getLineText( int line ) const -> QString;
    [[nodiscard]] auto getLineTextCached( int line ) -> QString;

    auto refreshLineOffsets() -> void;
    auto invalidateCache( uint64_t offset = 0 ) -> void;

    PieceTable* piece_table_{ nullptr };
    std::unique_ptr<LineManager> line_manager_;
    QStringList mock_highlight_words_;

    std::vector<uint64_t> search_results_;
    int active_search_index_{ -1 };
    int search_length_{ 0 };

    QLabel* scrollbar_tooltip_{ nullptr };

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
