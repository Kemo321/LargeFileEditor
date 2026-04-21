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

void LargeFileViewer::setMockHighlights( const QStringList& words )
{
    mock_highlight_words_ = words;
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

    // Draw Line Number Margin
    int gutterWidth = 50;
    painter.fillRect( 0, 0, gutterWidth, viewport()->height(), QColor( "#f0f0f0" ) );
    painter.setPen( QColor( "#d0d0d0" ) );
    painter.drawLine( gutterWidth, 0, gutterWidth, viewport()->height() );

    // Mock data for testing alignment
    QStringList mockText = { "Lorem ipsum dolor sit amet, consectetur adipiscing elit.",
                             "Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.",
                             "Ut enim ad minim veniam, quis nostrud exercitation ullamco",
                             "laboris nisi ut aliquip ex ea commodo consequat.",
                             "Duis aute irure dolor in reprehenderit in voluptate velit",
                             "esse cillum dolore eu fugiat nulla pariatur.",
                             "Excepteur sint occaecat cupidatat non proident, sunt in",
                             "culpa qui officia deserunt mollit anim id est laborum.",
                             "Sed ut perspiciatis unde omnis iste natus error sit voluptatem",
                             "accusantium doloremque laudantium, totam rem aperiam.",
                             "Minimalistic Large File Text Rendering Area (Mockup)",
                             "Scroll for Tooltip Mockup" };

    QFontMetrics fm( painter.font() );
    int y = fm.ascent() + 5;

    for( int i = 0; i < mockText.size(); ++i ) {
        // Draw Line number
        painter.setPen( QColor( "#808080" ) );
        painter.drawText( QRect( 0, y - fm.ascent(), gutterWidth - 5, fm.height() ),
                          Qt::AlignRight | Qt::AlignVCenter, QString::number( i + 1 ) );

        int textX = gutterWidth + 10;
        QString lineText = mockText[i];

        // Draw highlights if they match any word
        if( !mock_highlight_words_.isEmpty() ) {
            for( const QString& word : mock_highlight_words_ ) {
                int idx = 0;
                while( ( idx = lineText.indexOf( word, idx, Qt::CaseInsensitive ) ) != -1 ) {
                    int startX = textX + fm.horizontalAdvance( lineText.left( idx ) );
                    int wordWidth = fm.horizontalAdvance( lineText.mid( idx, word.length() ) );
                    painter.setPen( Qt::NoPen );
                    painter.setBrush( QColor( 255, 255, 0, 150 ) );  // Semi-transparent yellow
                    painter.drawRect( startX, y - fm.ascent(), wordWidth, fm.height() );
                    idx += word.length();
                }
            }
        }

        // Draw Text
        painter.setPen( Qt::black );
        painter.drawText( textX, y, lineText );

        y += fm.height() + 5;  // move down for next line
    }
}
