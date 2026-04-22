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
#include <vector>

#include "backend/PieceTable.h"

class LargeFileViewer : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit LargeFileViewer( QWidget* parent = nullptr );
    ~LargeFileViewer() override = default;

    void setMockHighlights( const QStringList& words );
    void setPieceTable( PieceTable* pieceTable );
    void setCursorPosition( int line, int col );
    void scrollToCursor();
    void jumpToLogicalPosition( uint64_t pos );
    void refreshView();

protected:
    auto eventFilter( QObject* obj, QEvent* event ) -> bool override;
    void resizeEvent( QResizeEvent* event ) override;
    void wheelEvent( QWheelEvent* event ) override;
    void keyPressEvent( QKeyEvent* event ) override;
    void mousePressEvent( QMouseEvent* event ) override;

private slots:
    void blinkCursor();
    void onScrollbarMoved( int value );

private:
    void paintViewport( QPaintEvent* event );
    [[nodiscard]] auto getLogicalPosition( int line, int col ) const -> uint64_t;
    [[nodiscard]] auto getLineText( int line ) const -> QString;
    void refreshLineOffsets();

    PieceTable* piece_table_{ nullptr };
    QStringList mock_highlight_words_;
    std::vector<uint64_t> line_offsets_;

    int cursor_line_{ 0 };
    int cursor_col_{ 0 };
    bool cursor_visible_{ true };
    QTimer* cursor_timer_{ nullptr };
    QTimer* line_offset_timer_{ nullptr };
    int gutter_width_{ 50 };
};