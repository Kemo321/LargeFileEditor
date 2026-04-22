#include "gui/LargeFileViewer.h"

#include <QCursor>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>

LargeFileViewer::LargeFileViewer( QWidget* parent ) : QAbstractScrollArea( parent )
{
    viewport()->setBackgroundRole( QPalette::Base );
    setFocusPolicy( Qt::StrongFocus );

    setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
    setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

    cursor_timer_ = new QTimer( this );
    connect( cursor_timer_, &QTimer::timeout, this, &LargeFileViewer::blinkCursor );
    cursor_timer_->start( 500 );

    line_offset_timer_ = new QTimer( this );
    line_offset_timer_->setSingleShot( true );
    connect( line_offset_timer_, &QTimer::timeout, this, &LargeFileViewer::refreshLineOffsets );

    viewport()->installEventFilter( this );

    connect( verticalScrollBar(), &QScrollBar::valueChanged, this,
             [this]( int ) { viewport()->update(); } );
}

auto LargeFileViewer::eventFilter( QObject* obj, QEvent* event ) -> bool
{
    if( obj == viewport() && event->type() == QEvent::Paint ) {
        paintViewport( static_cast<QPaintEvent*>( event ) );
        return true;
    }
    return QAbstractScrollArea::eventFilter( obj, event );
}

void LargeFileViewer::resizeEvent( QResizeEvent* event )
{
    QAbstractScrollArea::resizeEvent( event );
    refreshLineOffsets();
}

void LargeFileViewer::invalidateCache()
{
    line_cache_.clear();
}

void LargeFileViewer::refreshView()
{
    invalidateCache();
    refreshLineOffsets();
    viewport()->update();
}

void LargeFileViewer::jumpToLogicalPosition( uint64_t pos )
{
    if( piece_table_ == nullptr ) {
        return;
    }
    int line = piece_table_->getLineFromPosition( pos );
    int col = static_cast<int>( pos - piece_table_->getLineStart( line ) );
    setCursorPosition( line, col );
}

void LargeFileViewer::blinkCursor()
{
    cursor_visible_ = !cursor_visible_;
    viewport()->update();
}

void LargeFileViewer::setMockHighlights( const QStringList& words )
{
    mock_highlight_words_ = words;
    viewport()->update();
}

void LargeFileViewer::onScrollbarMoved( int value )
{
    QScrollBar* vBar = verticalScrollBar();
    if( vBar->maximum() > 0 ) {
        double percent = ( static_cast<double>( value ) / vBar->maximum() ) * 100.0;
        QString tip = QString( "Line: %1\n%2%" ).arg( value + 1 ).arg( percent, 0, 'f', 1 );
        QToolTip::showText( QCursor::pos(), tip, this );
    }
}

void LargeFileViewer::refreshLineOffsets()
{
    if( piece_table_ == nullptr ) {
        return;
    }

    int totalLines = piece_table_->getLineCount();
    QFontMetrics fm( font() );
    int lineHeight = fm.height() + 5;
    int visibleLines = viewport()->height() / lineHeight;

    verticalScrollBar()->setSingleStep( 1 );
    verticalScrollBar()->setPageStep( visibleLines );
    verticalScrollBar()->setRange( 0, std::max( 0, totalLines - visibleLines ) );
}

void LargeFileViewer::setPieceTable( PieceTable* pieceTable )
{
    piece_table_ = pieceTable;
    cursor_line_ = 0;
    cursor_col_ = 0;
    invalidateCache();
    refreshLineOffsets();
    verticalScrollBar()->setValue( 0 );
    viewport()->update();
}

auto LargeFileViewer::getLineText( int line ) const -> QString
{
    if( ( piece_table_ == nullptr ) || line < 0 || line >= piece_table_->getLineCount() ) {
        return "";
    }

    uint64_t startByte = piece_table_->getLineStart( line );
    uint64_t endByte = piece_table_->getLineStart( line + 1 );

    if( endByte <= startByte ) {
        return "";
    }

    std::string rawLine = piece_table_->getSubstr( startByte, endByte - startByte );
    QString qline = QString::fromStdString( rawLine );
    if( qline.endsWith( '\n' ) ) {
        qline.chop( 1 );
    }
    if( qline.endsWith( '\r' ) ) {
        qline.chop( 1 );
    }
    return qline;
}

auto LargeFileViewer::getLineTextCached( int line ) -> QString
{
    for( const auto& cl : line_cache_ ) {
        if( cl.line == line ) {
            return cl.text;
        }
    }
    QString text = getLineText( line );
    line_cache_.push_back( { line, text } );
    return text;
}

auto LargeFileViewer::getLogicalPosition( int line, int col ) const -> uint64_t
{
    if( piece_table_ == nullptr ) {
        return 0;
    }
    return piece_table_->getLineStart( line ) + col;
}

void LargeFileViewer::setCursorPosition( int line, int col )
{
    if( piece_table_ == nullptr ) {
        return;
    }
    cursor_line_ = std::max( 0, std::min( line, piece_table_->getLineCount() - 1 ) );
    int lineLen = getLineTextCached( cursor_line_ ).length();
    cursor_col_ = std::max( 0, std::min( col, lineLen ) );
    cursor_visible_ = true;
    scrollToCursor();
    viewport()->update();
}

void LargeFileViewer::scrollToCursor()
{
    int currentScroll = verticalScrollBar()->value();
    QFontMetrics fm( font() );
    int lineHeight = fm.height() + 5;
    int visibleLinesCount = viewport()->height() / lineHeight;

    if( cursor_line_ < currentScroll ) {
        verticalScrollBar()->setValue( cursor_line_ );
    } else if( cursor_line_ >= currentScroll + visibleLinesCount ) {
        verticalScrollBar()->setValue( cursor_line_ - visibleLinesCount + 1 );
    }
}

void LargeFileViewer::keyPressEvent( QKeyEvent* event )
{
    if( piece_table_ == nullptr ) {
        return;
    }

    QString text = event->text();
    int key = event->key();
    bool tableModified = false;

    if( key == Qt::Key_Left ) {
        if( cursor_col_ > 0 ) {
            cursor_col_--;
        } else if( cursor_line_ > 0 ) {
            cursor_line_--;
            cursor_col_ = getLineTextCached( cursor_line_ ).length();
        }
    } else if( key == Qt::Key_Right ) {
        if( cursor_col_ < getLineTextCached( cursor_line_ ).length() ) {
            cursor_col_++;
        } else if( cursor_line_ < piece_table_->getLineCount() - 1 ) {
            cursor_line_++;
            cursor_col_ = 0;
        }
    } else if( key == Qt::Key_Up ) {
        if( cursor_line_ > 0 ) {
            cursor_line_--;
            cursor_col_ =
                std::min( cursor_col_, static_cast<int>( getLineTextCached( cursor_line_ ).length() ) );
        }
    } else if( key == Qt::Key_Down ) {
        if( cursor_line_ < piece_table_->getLineCount() - 1 ) {
            cursor_line_++;
            cursor_col_ =
                std::min( cursor_col_, static_cast<int>( getLineTextCached( cursor_line_ ).length() ) );
        }
    } else if( key == Qt::Key_Backspace ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        if( pos > 0 ) {
            piece_table_->remove( pos - 1, 1 );
            tableModified = true;
            if( cursor_col_ > 0 ) {
                cursor_col_--;
            } else {
                cursor_line_--;
                cursor_col_ = getLineTextCached( cursor_line_ ).length();
            }
        }
    } else if( key == Qt::Key_Delete ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        if( pos < piece_table_->size() ) {
            piece_table_->remove( pos, 1 );
            tableModified = true;
        }
    } else if( key == Qt::Key_Return || key == Qt::Key_Enter ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        piece_table_->insert( pos, "\n" );
        tableModified = true;
        cursor_line_++;
        cursor_col_ = 0;
    } else if( !text.isEmpty() && text.at( 0 ).isPrint() ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        piece_table_->insert( pos, text.toStdString() );
        tableModified = true;
        cursor_col_ += text.length();
    }

    if (tableModified) {
        invalidateCache();
    }

    cursor_visible_ = true;
    scrollToCursor();
    refreshLineOffsets();
    viewport()->update();

    line_offset_timer_->start( 300 );
}

void LargeFileViewer::mousePressEvent( QMouseEvent* event )
{
    if( piece_table_ == nullptr ) {
        return;
    }

    QFontMetrics fm( font() );
    int lineHeight = fm.height() + 5;

    int clickedLineOffset = event->position().y() / lineHeight;
    int targetLine = verticalScrollBar()->value() + clickedLineOffset;

    if( targetLine < piece_table_->getLineCount() ) {
        cursor_line_ = targetLine;
        QString lineText = getLineTextCached( cursor_line_ );

        int textX = event->position().x() - gutter_width_ - 10;
        int col = 0;
        int currentX = 0;

        while( col < lineText.length() ) {
            int charWidth = fm.horizontalAdvance( lineText.at( col ) );
            if( currentX + charWidth / 2 > textX ) {
                break;
            }
            currentX += charWidth;
            col++;
        }
        cursor_col_ = col;
    }

    cursor_visible_ = true;
    viewport()->update();
}

void LargeFileViewer::paintViewport( QPaintEvent* event )
{
    QPainter painter( viewport() );
    painter.fillRect( event->rect(), Qt::white );

    painter.fillRect( 0, 0, gutter_width_, viewport()->height(), QColor( "#f0f0f0" ) );
    painter.setPen( QColor( "#d0d0d0" ) );
    painter.drawLine( gutter_width_, 0, gutter_width_, viewport()->height() );

    if( piece_table_ == nullptr ) {
        return;
    }

    QFontMetrics fm( painter.font() );
    int lineHeight = fm.height() + 5;
    int startLine = verticalScrollBar()->value();
    int visibleLinesCount = ( viewport()->height() / lineHeight ) + 1;
    int totalLines = piece_table_->getLineCount();

    for( int i = 0; i < visibleLinesCount; ++i ) {
        int currentLineIndex = startLine + i;
        if( currentLineIndex >= totalLines ) {
            break;
        }

        QString lineText = getLineTextCached( currentLineIndex );
        int yBase = i * lineHeight;
        int yText = yBase + fm.ascent() + 2;

        painter.setPen( QColor( "#808080" ) );
        painter.drawText( QRect( 0, yBase, gutter_width_ - 5, lineHeight ),
                          Qt::AlignRight | Qt::AlignVCenter,
                          QString::number( currentLineIndex + 1 ) );

        int textX = gutter_width_ + 10;

        if( !mock_highlight_words_.isEmpty() ) {
            for( const QString& word : mock_highlight_words_ ) {
                if( word.isEmpty() ) {
                    continue;
                }
                int idx = 0;
                while( ( idx = lineText.indexOf( word, idx, Qt::CaseInsensitive ) ) != -1 ) {
                    int startX = textX + fm.horizontalAdvance( lineText.left( idx ) );
                    int wordWidth = fm.horizontalAdvance( lineText.mid( idx, word.length() ) );
                    painter.fillRect( startX, yBase + 2, wordWidth, lineHeight - 4,
                                      QColor( 255, 255, 0, 150 ) );
                    idx += word.length();
                }
            }
        }

        painter.setPen( Qt::black );
        painter.drawText( textX, yText, lineText );

        if( currentLineIndex == cursor_line_ && hasFocus() && cursor_visible_ ) {
            int cursorX = textX + fm.horizontalAdvance( lineText.left( cursor_col_ ) );
            painter.setPen( Qt::black );
            painter.drawLine( cursorX, yBase + 2, cursorX, yBase + lineHeight - 2 );
        }
    }
}

void LargeFileViewer::wheelEvent( QWheelEvent* event )
{
    int numSteps = event->angleDelta().y() / 120;
    verticalScrollBar()->setValue( verticalScrollBar()->value() - ( numSteps * 3 ) );
    event->accept();
}