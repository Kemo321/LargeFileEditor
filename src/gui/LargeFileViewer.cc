#include "gui/LargeFileViewer.h"

#include <QCursor>
#include <QEvent>
#include <QFontDatabase>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>

static constexpr int kCursorBlinkRateMs = 500;
static constexpr int kLineOffsetDelayMs = 300;
static constexpr int kLineHeightPadding = 5;
static constexpr int kGutterTextPadding = 10;
static constexpr int kScrollStepDivisor = 120;
static constexpr int kWheelMultiplier = 3;
static constexpr int kGutterDigits = 6;
static constexpr int kOneMillion = 1'000'000;
static constexpr int kOneBillion = 1'000'000'000;

static auto formatLineNumber( int lineNum ) -> QString
{
    if( lineNum < kOneMillion ) {
        return QString::number( lineNum );
    }
    double value;
    char suffix;
    if( lineNum < kOneBillion ) {
        value = static_cast<double>( lineNum ) / kOneMillion;
        suffix = 'M';
    } else {
        value = static_cast<double>( lineNum ) / kOneBillion;
        suffix = 'B';
    }
    int intPart = static_cast<int>( value );
    int intDigits;
    if( intPart < 10 ) {
        intDigits = 1;
    } else if( intPart < 100 ) {
        intDigits = 2;
    } else if( intPart < 1000 ) {
        intDigits = 3;
    } else {
        intDigits = 4;
    }
    int decimals = std::max( 0, 4 - intDigits );
    double factor = 1.0;
    for( int i = 0; i < decimals; ++i ) {
        factor *= 10.0;
    }
    value = std::floor( value * factor ) / factor;
    return QString::number( value, 'f', decimals ) + QChar( suffix );
}

LargeFileViewer::LargeFileViewer( QWidget* parent ) : QAbstractScrollArea( parent )
{
    viewport()->setBackgroundRole( QPalette::Base );
    setFocusPolicy( Qt::StrongFocus );

    QFont fixedFont = QFontDatabase::systemFont( QFontDatabase::FixedFont );
    setFont( fixedFont );

    setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setHorizontalScrollBarPolicy( Qt::ScrollBarAsNeeded );

    cursor_timer_ = new QTimer( this );
    connect( cursor_timer_, &QTimer::timeout, this, &LargeFileViewer::blinkCursor );
    cursor_timer_->start( kCursorBlinkRateMs );

    line_offset_timer_ = new QTimer( this );
    line_offset_timer_->setSingleShot( true );
    connect( line_offset_timer_, &QTimer::timeout, this, &LargeFileViewer::refreshLineOffsets );

    viewport()->installEventFilter( this );

    scrollbar_tooltip_ = new QLabel( this );
    scrollbar_tooltip_->setWindowFlags( Qt::ToolTip );
    scrollbar_tooltip_->setStyleSheet(
        "QLabel { background-color: #ffffe0; color: black; border: 1px solid black; padding: 4px; "
        "}" );
    scrollbar_tooltip_->hide();

    connect( verticalScrollBar(), &QScrollBar::sliderPressed, this, [this]() {
        onScrollbarMoved( verticalScrollBar()->value() );
        scrollbar_tooltip_->show();
    } );
    connect( verticalScrollBar(), &QScrollBar::sliderReleased, this,
             [this]() { scrollbar_tooltip_->hide(); } );

    connect( verticalScrollBar(), &QScrollBar::valueChanged, this, [this]( int value ) {
        refreshLineOffsets();
        viewport()->update();
        if( verticalScrollBar()->isSliderDown() ) {
            onScrollbarMoved( value );
        }
    } );
}

auto LargeFileViewer::eventFilter( QObject* obj, QEvent* event ) -> bool
{
    if( obj == viewport() && event->type() == QEvent::Paint ) {
        paintViewport( static_cast<QPaintEvent*>( event ) );
        return true;
    }
    return QAbstractScrollArea::eventFilter( obj, event );
}

auto LargeFileViewer::resizeEvent( QResizeEvent* event ) -> void
{
    QAbstractScrollArea::resizeEvent( event );
    refreshLineOffsets();
}

auto LargeFileViewer::invalidateCache( uint64_t offset ) -> void
{
    if( line_manager_ ) {
        if( offset == 0 ) {
            line_manager_->reset();
        } else {
            line_manager_->invalidateCacheFromOffset( offset );
        }
    }
}

auto LargeFileViewer::refreshView() -> void
{
    invalidateCache();
    refreshLineOffsets();
    viewport()->update();
}

auto LargeFileViewer::jumpToLogicalPosition( uint64_t pos, int matchLength ) -> void
{
    if( piece_table_ == nullptr || !line_manager_ ) {
        return;
    }
    int line = line_manager_->getVirtualLineFromOffset( pos );
    int col = static_cast<int>( pos - line_manager_->getLineOffset( line ) );
    setCursorPosition( line, col );

    if( matchLength > 0 ) {
        QFontMetrics fontMetrics( font() );
        int charWidth = fontMetrics.horizontalAdvance( 'A' );
        if( charWidth <= 0 ) {
            charWidth = 1;
        }
        int matchStartPx = col * charWidth;
        int matchEndPx = ( col + matchLength ) * charWidth;
        int visibleWidth = viewport()->width() - gutter_width_ - kGutterTextPadding;
        int currentHScroll = horizontalScrollBar()->value();

        if( matchStartPx < currentHScroll || matchEndPx > currentHScroll + visibleWidth ) {
            int matchCenterPx = ( matchStartPx + matchEndPx ) / 2;
            int newScroll = std::max( 0, matchCenterPx - visibleWidth / 2 );
            horizontalScrollBar()->setValue( newScroll );
        }
    }
}

auto LargeFileViewer::blinkCursor() -> void
{
    cursor_visible_ = !cursor_visible_;
    viewport()->update();
}

auto LargeFileViewer::setMockHighlights( const QStringList& words ) -> void
{
    mock_highlight_words_ = words;
    viewport()->update();
}

auto LargeFileViewer::setSearchHighlights( const std::vector<uint64_t>& searchResults,
                                           int activeIndex, int searchLength ) -> void
{
    search_results_ = searchResults;
    active_search_index_ = activeIndex;
    search_length_ = searchLength;
    viewport()->update();
}

auto LargeFileViewer::onScrollbarMoved( int value ) -> void
{
    QScrollBar* vBar = verticalScrollBar();
    if( vBar->maximum() > 0 ) {
        double percent = ( static_cast<double>( value ) / vBar->maximum() ) * 100.0;
        QString tip = QString( "Line: %1\n%2%" ).arg( value + 1 ).arg( percent, 0, 'f', 1 );

        if( scrollbar_tooltip_ != nullptr ) {
            scrollbar_tooltip_->setText( tip );
            scrollbar_tooltip_->adjustSize();

            double proportion = static_cast<double>( value ) / vBar->maximum();
            int handleCenterY =
                static_cast<int>( proportion * ( vBar->height() - scrollbar_tooltip_->height() ) );
            QPoint pos =
                vBar->mapToGlobal( QPoint( -scrollbar_tooltip_->width() - 5, handleCenterY ) );

            scrollbar_tooltip_->move( pos );
        }
    }
}

auto LargeFileViewer::changeEvent( QEvent* event ) -> void
{
    QAbstractScrollArea::changeEvent( event );
    if( event->type() == QEvent::FontChange ) {
        refreshLineOffsets();
    }
}

auto LargeFileViewer::refreshLineOffsets() -> void
{
    if( piece_table_ == nullptr || !line_manager_ ) {
        return;
    }

    QFontMetrics fontMetrics( font() );
    int lineHeight = fontMetrics.height() + kLineHeightPadding;
    int charWidth = fontMetrics.horizontalAdvance( 'A' );
    if( charWidth <= 0 ) {
        charWidth = 1;
    }
    gutter_width_ = kGutterDigits * charWidth + kGutterTextPadding * 2;

    int totalLines = line_manager_->getLineCount();
    int visibleLines = viewport()->height() / lineHeight;

    verticalScrollBar()->setSingleStep( 1 );
    verticalScrollBar()->setPageStep( visibleLines );
    verticalScrollBar()->setRange( 0, std::max( 0, totalLines - visibleLines ) );

    int maxChars = static_cast<int>( line_manager_->getGlobalMaxLineLength() );
    int maxPixels = maxChars * charWidth;
    int visibleWidth = viewport()->width() - gutter_width_ - kGutterTextPadding;
    
    horizontalScrollBar()->setSingleStep( charWidth * 4 );
    horizontalScrollBar()->setPageStep( visibleWidth );
    horizontalScrollBar()->setRange( 0, std::max( 0, maxPixels - visibleWidth ) );
}

auto LargeFileViewer::setPieceTable( PieceTable* pieceTable ) -> void
{
    piece_table_ = pieceTable;
    if( piece_table_ ) {
        line_manager_ = std::make_unique<LineManager>( piece_table_ );
        setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
    } else {
        line_manager_.reset();
        setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    }
    cursor_line_ = 0;
    cursor_col_ = 0;
    invalidateCache();
    refreshLineOffsets();
    verticalScrollBar()->setValue( 0 );
    horizontalScrollBar()->setValue( 0 );
    viewport()->update();
}

auto LargeFileViewer::getLogicalPosition( int line, int col ) const -> uint64_t
{
    if( piece_table_ == nullptr || !line_manager_ ) {
        return 0;
    }
    return line_manager_->getLineOffset( line ) + col;
}

auto LargeFileViewer::setCursorPosition( int line, int col ) -> void
{
    if( piece_table_ == nullptr || !line_manager_ ) {
        return;
    }
    cursor_line_ = std::max( 0, std::min( line, line_manager_->getLineCount() - 1 ) );
    int lineLen = static_cast<int>( line_manager_->getVirtualLineLength( cursor_line_ ) );
    cursor_col_ = std::max( 0, std::min( col, lineLen ) );
    cursor_visible_ = true;
    scrollToCursor();
    viewport()->update();
    emit cursorPositionChanged( cursor_line_, cursor_col_ );
}

auto LargeFileViewer::scrollToCursor() -> void
{
    QFontMetrics fontMetrics( font() );
    int lineHeight = fontMetrics.height() + kLineHeightPadding;
    int charWidth = fontMetrics.horizontalAdvance( 'A' );
    if( charWidth <= 0 ) {
        charWidth = 1;
    }

    int currentScroll = verticalScrollBar()->value();
    int visibleLinesCount = viewport()->height() / lineHeight;

    if( cursor_line_ < currentScroll ) {
        verticalScrollBar()->setValue( cursor_line_ );
    } else if( cursor_line_ >= currentScroll + visibleLinesCount ) {
        verticalScrollBar()->setValue( cursor_line_ - visibleLinesCount + 1 );
    }

    int currentHScroll = horizontalScrollBar()->value();
    int visibleWidth = viewport()->width() - gutter_width_ - kGutterTextPadding;
    
    int cursorPx = cursor_col_ * charWidth;
    int margin = charWidth * 2; 

    if( cursorPx < currentHScroll ) {
        horizontalScrollBar()->setValue( std::max(0, cursorPx - margin) );
    } else if( cursorPx > currentHScroll + visibleWidth - margin ) {
        horizontalScrollBar()->setValue( cursorPx - visibleWidth + margin );
    }
}

// NOLINTBEGIN
auto LargeFileViewer::keyPressEvent( QKeyEvent* event ) -> void
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
            uint64_t lineStart = line_manager_->getLineOffset( cursor_line_ );
            while( cursor_col_ > 0 ) {
                std::string b = piece_table_->getSubstr( lineStart + cursor_col_, 1 );
                if( b.empty() )
                    break;
                unsigned char byte = static_cast<unsigned char>( b[0] );
                if( ( byte & 0xC0 ) != 0x80 )
                    break;
                cursor_col_--;
            }
        } else if( cursor_line_ > 0 ) {
            cursor_line_--;
            cursor_col_ = static_cast<int>( line_manager_->getVirtualLineLength( cursor_line_ ) );
        }
    } else if( key == Qt::Key_Right ) {
        uint64_t lineLen = line_manager_->getVirtualLineLength( cursor_line_ );
        if( cursor_col_ < static_cast<int>( lineLen ) ) {
            uint64_t lineStart = line_manager_->getLineOffset( cursor_line_ );
            std::string b = piece_table_->getSubstr( lineStart + cursor_col_, 1 );
            cursor_col_++;
            if( !b.empty() ) {
                unsigned char byte = static_cast<unsigned char>( b[0] );
                if( ( byte & 0xE0 ) == 0xC0 )
                    cursor_col_ += 1;
                else if( ( byte & 0xF0 ) == 0xE0 )
                    cursor_col_ += 2;
                else if( ( byte & 0xF8 ) == 0xF0 )
                    cursor_col_ += 3;
            }
            cursor_col_ = std::min( cursor_col_, static_cast<int>( lineLen ) );
        } else if( cursor_line_ < line_manager_->getLineCount() - 1 ) {
            cursor_line_++;
            cursor_col_ = 0;
        }
    } else if( key == Qt::Key_Up ) {
        if( cursor_line_ > 0 ) {
            cursor_line_--;
            cursor_col_ =
                std::min( cursor_col_,
                          static_cast<int>( line_manager_->getVirtualLineLength( cursor_line_ ) ) );
        }
    } else if( key == Qt::Key_Down ) {
        if( cursor_line_ < line_manager_->getLineCount() - 1 ) {
            cursor_line_++;
            cursor_col_ =
                std::min( cursor_col_,
                          static_cast<int>( line_manager_->getVirtualLineLength( cursor_line_ ) ) );
        }
    } else if( key == Qt::Key_Backspace ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        if( pos > 0 ) {
            int bytes_to_remove = 1;
            int check_col = cursor_col_ - 1;
            uint64_t lineStart = line_manager_->getLineOffset( cursor_line_ );
            while( check_col > 0 ) {
                std::string b = piece_table_->getSubstr( lineStart + check_col, 1 );
                if( b.empty() )
                    break;
                unsigned char byte = static_cast<unsigned char>( b[0] );
                if( ( byte & 0xC0 ) != 0x80 )
                    break;
                check_col--;
                bytes_to_remove++;
            }
            piece_table_->remove( pos - bytes_to_remove, bytes_to_remove );
            invalidateCache( pos - bytes_to_remove );
            tableModified = true;
            if( cursor_col_ > 0 ) {
                cursor_col_ -= bytes_to_remove;
            } else {
                cursor_line_--;
                cursor_col_ =
                    static_cast<int>( line_manager_->getVirtualLineLength( cursor_line_ ) );
            }
        }
    } else if( key == Qt::Key_Delete ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        if( pos < piece_table_->size() ) {
            int bytes_to_remove = 1;
            std::string b = piece_table_->getSubstr( pos, 1 );
            if( !b.empty() ) {
                unsigned char byte = static_cast<unsigned char>( b[0] );
                if( ( byte & 0xE0 ) == 0xC0 )
                    bytes_to_remove = 2;
                else if( ( byte & 0xF0 ) == 0xE0 )
                    bytes_to_remove = 3;
                else if( ( byte & 0xF8 ) == 0xF0 )
                    bytes_to_remove = 4;
            }
            piece_table_->remove( pos, bytes_to_remove );
            invalidateCache( pos );
            tableModified = true;
        }
    } else if( key == Qt::Key_Return || key == Qt::Key_Enter ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        piece_table_->insert( pos, "\n" );
        invalidateCache( pos );
        tableModified = true;
        cursor_line_++;
        cursor_col_ = 0;
    } else if( !text.isEmpty() && text.at( 0 ).isPrint() ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        piece_table_->insert( pos, text.toStdString() );
        invalidateCache( pos );
        tableModified = true;
        cursor_col_ += static_cast<int>( text.length() );
    }

    if( tableModified ) {
        emit documentModified();
    }

    cursor_visible_ = true;
    scrollToCursor();
    refreshLineOffsets();
    viewport()->update();

    emit cursorPositionChanged( cursor_line_, cursor_col_ );

    line_offset_timer_->start( kLineOffsetDelayMs );
}
// NOLINTEND

auto LargeFileViewer::mousePressEvent( QMouseEvent* event ) -> void
{
    if( piece_table_ == nullptr ) {
        return;
    }

    QFontMetrics fontMetrics( font() );
    int lineHeight = fontMetrics.height() + kLineHeightPadding;
    int charWidth = fontMetrics.horizontalAdvance( 'A' );
    if( charWidth <= 0 ) {
        charWidth = 1;
    }

    int clickedLineOffset = static_cast<int>( event->position().y() ) / lineHeight;
    int targetLine = verticalScrollBar()->value() + clickedLineOffset;

    if( targetLine < line_manager_->getLineCount() ) {
        cursor_line_ = targetLine;
        
        int textX = static_cast<int>( event->position().x() ) - gutter_width_ - kGutterTextPadding;
        int scrollXPx = horizontalScrollBar()->value();
        int totalPx = textX + scrollXPx;
        
        int approx_col = totalPx / charWidth;
        if (approx_col < 0) approx_col = 0;
        
        uint64_t lineLen = line_manager_->getVirtualLineLength( cursor_line_ );
        if (approx_col > static_cast<int>(lineLen)) approx_col = static_cast<int>(lineLen);

        uint64_t lineStart = line_manager_->getLineOffset( cursor_line_ );
        
        // Snap approx_col back to a UTF-8 character boundary
        if( approx_col > 0 && approx_col < static_cast<int>(lineLen) ) {
            int back_offset = 0;
            while( back_offset < 4 && approx_col >= back_offset ) {
                std::string b = piece_table_->getSubstr( lineStart + approx_col - back_offset, 1 );
                if( b.empty() ) break;
                unsigned char byte = static_cast<unsigned char>( b[0] );
                if( ( byte & 0xC0 ) != 0x80 ) {
                    approx_col -= back_offset;
                    break;
                }
                back_offset++;
            }
        }
        
        cursor_col_ = approx_col;
    }

    cursor_visible_ = true;
    viewport()->update();

    emit cursorPositionChanged( cursor_line_, cursor_col_ );
}

auto LargeFileViewer::paintViewport( QPaintEvent* event ) -> void
{
    QPainter painter( viewport() );
    painter.fillRect( event->rect(), Qt::white );

    if( piece_table_ == nullptr || !line_manager_ ) {
        painter.setPen( Qt::gray );
        painter.drawText( viewport()->rect(), Qt::AlignCenter,
                          "Please open a file (File -> Open or Ctrl+O)" );
        return;
    }

    QFontMetrics fontMetrics( font() );
    int lineHeight = fontMetrics.height() + kLineHeightPadding;
    int charWidth = fontMetrics.horizontalAdvance( 'A' );
    if( charWidth <= 0 ) {
        charWidth = 1;
    }
    gutter_width_ = kGutterDigits * charWidth + kGutterTextPadding * 2;

    painter.fillRect( 0, 0, gutter_width_, viewport()->height(), QColor( "#f0f0f0" ) );
    painter.setPen( QColor( "#d0d0d0" ) );
    painter.drawLine( gutter_width_, 0, gutter_width_, viewport()->height() );

    int startLine = verticalScrollBar()->value();
    int visibleLinesCount = ( viewport()->height() / lineHeight ) + 1;
    int totalLines = line_manager_->getLineCount();

    int scrollXPx = horizontalScrollBar()->value();
    int start_col_byte = scrollXPx / charWidth;
    int px_offset = scrollXPx % charWidth;
    int visible_cols = ( viewport()->width() - gutter_width_ ) / charWidth;

    for( int idx = 0; idx < visibleLinesCount; ++idx ) {
        int currentLineIndex = startLine + idx;
        if( currentLineIndex >= totalLines ) {
            break;
        }

        uint64_t lineStart = line_manager_->getLineOffset( currentLineIndex );
        
        int fetch_length = visible_cols + 10;
        std::string rawChunk = line_manager_->getLineChunk( currentLineIndex, start_col_byte, fetch_length );
        
        QString lineText = QString::fromUtf8( rawChunk.data(), rawChunk.size() );
        for( int i = 0; i < lineText.length(); ++i ) {
            ushort unicode = lineText[i].unicode();
            if( ( unicode < 32 && unicode != '\t' && unicode != '\n' && unicode != '\r' ) || unicode == 127 ) {
                lineText[i] = QChar( 0xFFFD );
            }
        }

        int yBase = idx * lineHeight;
        int yText = yBase + fontMetrics.ascent() + 2;

        painter.setPen( QColor( "#808080" ) );
        painter.drawText( QRect( 0, yBase, gutter_width_ - kGutterTextPadding, lineHeight ),
                          static_cast<int>( Qt::AlignRight | Qt::AlignVCenter ),
                          formatLineNumber( currentLineIndex + 1 ) );

        painter.save();
        painter.setClipRect( gutter_width_ + 1, yBase, viewport()->width() - gutter_width_ - 1, lineHeight );

        uint64_t line_len = line_manager_->getVirtualLineLength( currentLineIndex );
        uint64_t chunk_start_byte = start_col_byte;
        if (chunk_start_byte > line_len) chunk_start_byte = line_len;
        
        if( chunk_start_byte > 0 ) {
            int back_offset = 0;
            while( back_offset < 4 && chunk_start_byte >= back_offset ) {
                std::string b = piece_table_->getSubstr( lineStart + chunk_start_byte - back_offset, 1 );
                if( b.empty() ) break;
                unsigned char byte = static_cast<unsigned char>( b[0] );
                if( ( byte & 0xC0 ) != 0x80 ) {
                    chunk_start_byte -= back_offset;
                    break;
                }
                back_offset++;
            }
        }

        int textX = gutter_width_ + kGutterTextPadding - px_offset;
        
        if (chunk_start_byte < static_cast<uint64_t>(start_col_byte)) {
            std::string prefix = rawChunk.substr(0, start_col_byte - chunk_start_byte);
            QString qPrefix = QString::fromUtf8(prefix.data(), prefix.size());
            textX -= fontMetrics.horizontalAdvance(qPrefix);
        }

        if( !search_results_.empty() && search_length_ > 0 ) {
            auto it = std::lower_bound( search_results_.begin(), search_results_.end(), lineStart );
            int indexOffset = std::distance( search_results_.begin(), it );

            while( it != search_results_.end() && *it < lineStart + line_len ) {
                uint64_t matchPos = *it;
                int matchCol = static_cast<int>( matchPos - lineStart );

                if( matchCol + search_length_ > chunk_start_byte && matchCol < chunk_start_byte + rawChunk.size() ) {
                    int visMatchStart = std::max( 0, matchCol - static_cast<int>(chunk_start_byte) );
                    int visMatchEnd = std::min( static_cast<int>(rawChunk.size()), matchCol + search_length_ - static_cast<int>(chunk_start_byte) );
                    int matchLen = visMatchEnd - visMatchStart;

                    if( matchLen > 0 ) {
                        std::string prefixRaw = rawChunk.substr(0, visMatchStart);
                        std::string matchRaw = rawChunk.substr(visMatchStart, matchLen);
                        int startX = textX + fontMetrics.horizontalAdvance( QString::fromUtf8(prefixRaw.data(), prefixRaw.size()) );
                        int wordWidth = fontMetrics.horizontalAdvance( QString::fromUtf8(matchRaw.data(), matchRaw.size()) );

                        bool isActive = ( indexOffset == active_search_index_ );
                        QColor bgColor = isActive ? QColor( 255, 215, 0 ) : QColor( 255, 255, 200 );
                        painter.fillRect( startX, yBase + 2, wordWidth, lineHeight - 4, bgColor );
                    }
                }
                ++it;
                ++indexOffset;
            }
        }

        if( !mock_highlight_words_.isEmpty() ) {
            for( const QString& word : mock_highlight_words_ ) {
                if( word.isEmpty() ) continue;
                int matchIdx = 0;
                while( ( matchIdx = lineText.indexOf( word, matchIdx, Qt::CaseInsensitive ) ) != -1 ) {
                    int startX = textX + fontMetrics.horizontalAdvance( lineText.left( matchIdx ) );
                    int wordWidth = fontMetrics.horizontalAdvance( lineText.mid( matchIdx, word.length() ) );
                    painter.fillRect( startX, yBase + 2, wordWidth, lineHeight - 4, QColor( 255, 255, 0, 150 ) );
                    matchIdx += word.length();
                }
            }
        }

        painter.setPen( Qt::black );
        painter.drawText( textX, yText, lineText );

        if( currentLineIndex == cursor_line_ && hasFocus() && cursor_visible_ ) {
            if( cursor_col_ >= chunk_start_byte && cursor_col_ <= chunk_start_byte + rawChunk.size() ) {
                int cursorVisOffset = cursor_col_ - chunk_start_byte;
                std::string prefixRaw = rawChunk.substr(0, cursorVisOffset);
                int cursorX = textX + fontMetrics.horizontalAdvance( QString::fromUtf8(prefixRaw.data(), prefixRaw.size()) );
                painter.setPen( Qt::black );
                painter.drawLine( cursorX, yBase + 2, cursorX, yBase + lineHeight - 2 );
            }
        }

        painter.restore();
    }
}

auto LargeFileViewer::wheelEvent( QWheelEvent* event ) -> void
{
    int numSteps = event->angleDelta().y() / kScrollStepDivisor;
    verticalScrollBar()->setValue( verticalScrollBar()->value() - ( numSteps * kWheelMultiplier ) );
    event->accept();
}
