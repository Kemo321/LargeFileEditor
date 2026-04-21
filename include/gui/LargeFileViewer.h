/**
 * Authors: Jan Szwagierczak
 * Description: Header of the custom widget for displaying large file content.
 */

#pragma once

#include <QAbstractScrollArea>
#include <QList>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QResizeEvent>
#include <QScrollBar>

/**
 * @brief A custom minimalistic text rendering area designed to avoid
 * loading the entire file into RAM. Currently a purely decoupled UI mockup.
 */
class LargeFileViewer : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit LargeFileViewer( QWidget* parent = nullptr );
    ~LargeFileViewer() override = default;

    /**
     * @brief Mock method demonstrating how search result highlights will be visually applied.
     */
    void setMockHighlights( const QStringList& words );

protected:
    /**
     * @brief Custom paint event to render the visible portion of the file and highlights.
     */
    void paintEvent( QPaintEvent* event ) override;

private:
    void onScrollbarMoved( int value );

    QStringList mock_highlight_words_;
};
