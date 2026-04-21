/**
 * Authors: Jan Szwagierczak
 * Description: Implementation of the custom widget for displaying large file content.
 */

#include "gui/LargeFileViewer.h"

#include <QPainter>
#include <QScrollBar>
#include <QStyle>
#include <QToolTip>

LargeFileViewer::LargeFileViewer( QWidget* parent ) : QAbstractScrollArea( parent )
{
    viewport()->setBackgroundRole( QPalette::Base );

    QScrollBar* vScrollBar = verticalScrollBar();

    // Enforce a minimum scrollbar  height (30px)
    vScrollBar->setStyleSheet(
        "QScrollBar:vertical {"
        "    background: #f0f0f0;"
        "    width: 15px;"
        "    margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "    background: #c0c0c0;"
        "    min-height: 30px;"
        "    border-radius: 4px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
        "    background: #a0a0a0;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "    height: 0px;"
        "}" );

    // mockup range
    vScrollBar->setRange( 0, 1000000 );

    // Connect slider movement to tooltip display
    connect( vScrollBar, &QScrollBar::sliderMoved, this, &LargeFileViewer::onScrollbarMoved );
}

void LargeFileViewer::setMockHighlights( const QList<QRect>& highlights )
{
    mock_highlights_ = highlights;
    viewport()->update();
}

void LargeFileViewer::onScrollbarMoved( int value )
{
    QScrollBar* vBar = verticalScrollBar();
    double percent = ( static_cast<double>( value ) / vBar->maximum() ) * 100.0;

    int mockLineNumber = value;

    QString tip = QString( "Line: %1\n%2%" ).arg( mockLineNumber ).arg( percent, 0, 'f', 1 );

    // Position tooltip near the cursor/scrollbar
    QToolTip::showText( QCursor::pos(), tip, this );
}

void LargeFileViewer::paintEvent( QPaintEvent* event )
{
    QPainter painter( viewport() );
    painter.setRenderHint( QPainter::Antialiasing );

    // Fill background
    painter.fillRect( event->rect(), Qt::white );

    // Draw mock search highlights
    if( !mock_highlights_.isEmpty() ) {
        painter.setPen( Qt::NoPen );
        painter.setBrush( QColor( 255, 255, 0, 150 ) );  // Semi-transparent yellow
        for( const QRect& rect : mock_highlights_ ) {
            painter.drawRect( rect );
        }
    }

    // Draw minimalistic text
    painter.setPen( Qt::black );
    painter.drawText( viewport()->rect(), Qt::AlignCenter,
                      "Minimalistic Large File Text Rendering Area\n(Scroll for Tooltip Mockup)" );
}
