#include "gui/LargeFileViewer.h"

#include <QCursor>
#include <QPainter>
#include <QScrollBar>
#include <QStyle>
#include <QToolTip>
#include <QWheelEvent>
#include <algorithm>

static constexpr int kCursorBlinkRateMs = 500;
static constexpr int kLineOffsetDelayMs = 300;
static constexpr int kLineHeightPadding = 5;
static constexpr int kGutterTextPadding = 10;
static constexpr int kScrollStepDivisor = 120;
static constexpr int kWheelMultiplier = 3;

LargeFileViewer::LargeFileViewer( QWidget* parent ) : QAbstractScrollArea( parent )
{
    viewport()->setBackgroundRole( QPalette::Base );
    setFocusPolicy( Qt::StrongFocus );

    setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
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
    line_cache_.clear();
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

auto LargeFileViewer::jumpToLogicalPosition( uint64_t pos ) -> void
{
    if( piece_table_ == nullptr || !line_manager_ ) {
        return;
    }
    int line = line_manager_->getVirtualLineFromOffset( pos );
    int col = static_cast<int>( pos - line_manager_->getLineOffset( line ) );
    setCursorPosition( line, col );
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

auto LargeFileViewer::refreshLineOffsets() -> void
{
    if( piece_table_ == nullptr || !line_manager_ ) {
        return;
    }

    int totalLines = line_manager_->getLineCount();
    QFontMetrics fontMetrics( font() );
    int lineHeight = fontMetrics.height() + kLineHeightPadding;
    int visibleLines = viewport()->height() / lineHeight;

    verticalScrollBar()->setSingleStep( 1 );
    verticalScrollBar()->setPageStep( visibleLines );
    verticalScrollBar()->setRange( 0, std::max( 0, totalLines - visibleLines ) );

    int maxChars = 0;
    int startLine = verticalScrollBar()->value();
    for( int i = 0; i <= visibleLines && startLine + i < totalLines; ++i ) {
        maxChars = std::max(
            maxChars, static_cast<int>( line_manager_->getVirtualLineLength( startLine + i ) ) );
    }

    int maxPixels = maxChars * fontMetrics.averageCharWidth();
    int visibleWidth = viewport()->width() - gutter_width_ - kGutterTextPadding;
    horizontalScrollBar()->setSingleStep( fontMetrics.averageCharWidth() * 4 );
    horizontalScrollBar()->setPageStep( visibleWidth );
    horizontalScrollBar()->setRange( 0, std::max( 0, maxPixels - visibleWidth ) );
}

auto LargeFileViewer::setPieceTable( PieceTable* pieceTable ) -> void
{
    piece_table_ = pieceTable;
    if( piece_table_ ) {
        line_manager_ = std::make_unique<LineManager>( piece_table_ );
    } else {
        line_manager_.reset();
    }
    cursor_line_ = 0;
    cursor_col_ = 0;
    invalidateCache();
    refreshLineOffsets();
    verticalScrollBar()->setValue( 0 );
    horizontalScrollBar()->setValue( 0 );
    viewport()->update();
}

auto LargeFileViewer::getLineText( int line ) const -> QString
{
    if( piece_table_ == nullptr || !line_manager_ || line < 0 ||
        line >= line_manager_->getLineCount() ) {
        return "";
    }

    uint64_t startByte = line_manager_->getLineOffset( line );
    uint64_t length = line_manager_->getVirtualLineLength( line );

    if( length == 0 ) {
        return "";
    }

    std::string rawLine = piece_table_->getSubstr( startByte, length );

    QString qline = QString::fromUtf8( rawLine.data(), rawLine.size() );

    for( int i = 0; i < qline.length(); ++i ) {
        ushort unicode = qline[i].unicode();
        if( ( unicode < 32 && unicode != '\t' && unicode != '\n' && unicode != '\r' ) ||
            unicode == 127 ) {
            qline[i] = QChar( 0xFFFD );  // Unicode replacement character
        }
    }

    return qline;
}

auto LargeFileViewer::getLineTextCached( int line ) -> QString
{
    auto it = std::find_if( line_cache_.begin(), line_cache_.end(),
                            [line]( const auto& cls ) { return cls.line_ == line; } );
    if( it != line_cache_.end() ) {
        return it->text_;
    }
    QString text = getLineText( line );
    line_cache_.push_back( { line, text } );
    return text;
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
    int lineLen = static_cast<int>( getLineTextCached( cursor_line_ ).length() );
    cursor_col_ = std::max( 0, std::min( col, lineLen ) );
    cursor_visible_ = true;
    scrollToCursor();
    viewport()->update();
    emit cursorPositionChanged( cursor_line_, cursor_col_ );
}

auto LargeFileViewer::scrollToCursor() -> void
{
    int currentScroll = verticalScrollBar()->value();
    QFontMetrics fontMetrics( font() );
    int lineHeight = fontMetrics.height() + kLineHeightPadding;
    int visibleLinesCount = viewport()->height() / lineHeight;

    if( cursor_line_ < currentScroll ) {
        verticalScrollBar()->setValue( cursor_line_ );
    } else if( cursor_line_ >= currentScroll + visibleLinesCount ) {
        verticalScrollBar()->setValue( cursor_line_ - visibleLinesCount + 1 );
    }

    int currentHScroll = horizontalScrollBar()->value();
    int visibleWidth = viewport()->width() - gutter_width_ - kGutterTextPadding;
    
    // NOWE: Zamiana indeksu znaku na jego pozycję w pikselach
    QString lineText = getLineTextCached( cursor_line_ );
    int cursorPx = fontMetrics.horizontalAdvance( lineText.left( cursor_col_ ) );
    int margin = fontMetrics.averageCharWidth() * 2; // Margines dla lepszej widoczności kursora

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
        } else if( cursor_line_ > 0 ) {
            cursor_line_--;
            cursor_col_ = static_cast<int>( getLineTextCached( cursor_line_ ).length() );
        }
    } else if( key == Qt::Key_Right ) {
        if( cursor_col_ < getLineTextCached( cursor_line_ ).length() ) {
            cursor_col_++;
        } else if( cursor_line_ < line_manager_->getLineCount() - 1 ) {
            cursor_line_++;
            cursor_col_ = 0;
        }
    } else if( key == Qt::Key_Up ) {
        if( cursor_line_ > 0 ) {
            cursor_line_--;
            cursor_col_ = std::min(
                cursor_col_, static_cast<int>( getLineTextCached( cursor_line_ ).length() ) );
        }
    } else if( key == Qt::Key_Down ) {
        if( cursor_line_ < line_manager_->getLineCount() - 1 ) {
            cursor_line_++;
            cursor_col_ = std::min(
                cursor_col_, static_cast<int>( getLineTextCached( cursor_line_ ).length() ) );
        }
    } else if( key == Qt::Key_Backspace ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        if( pos > 0 ) {
            piece_table_->remove( pos - 1, 1 );
            invalidateCache( pos - 1 );
            tableModified = true;
            if( cursor_col_ > 0 ) {
                cursor_col_--;
            } else {
                cursor_line_--;
                cursor_col_ = static_cast<int>( getLineTextCached( cursor_line_ ).length() );
            }
        }
    } else if( key == Qt::Key_Delete ) {
        uint64_t pos = getLogicalPosition( cursor_line_, cursor_col_ );
        if( pos < piece_table_->size() ) {
            piece_table_->remove( pos, 1 );
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

    int clickedLineOffset = static_cast<int>( event->position().y() ) / lineHeight;
    int targetLine = verticalScrollBar()->value() + clickedLineOffset;

    if( targetLine < line_manager_->getLineCount() ) {
        cursor_line_ = targetLine;
        QString lineText = getLineTextCached( cursor_line_ );

        int scrollX = horizontalScrollBar()->value();
        int textX = static_cast<int>( event->position().x() ) - gutter_width_ - kGutterTextPadding;

        int col = scrollX;
        int currentX = 0;

        while( col < lineText.length() ) {
            int charWidth = fontMetrics.horizontalAdvance( lineText.at( col ) );
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

    emit cursorPositionChanged( cursor_line_, cursor_col_ );
}

auto LargeFileViewer::paintViewport( QPaintEvent* event ) -> void
{
    QPainter painter( viewport() );
    painter.fillRect( event->rect(), Qt::white );

    painter.fillRect( 0, 0, gutter_width_, viewport()->height(), QColor( "#f0f0f0" ) );
    painter.setPen( QColor( "#d0d0d0" ) );
    painter.drawLine( gutter_width_, 0, gutter_width_, viewport()->height() );

    if( piece_table_ == nullptr ) {
        return;
    }

    QFontMetrics fontMetrics( painter.font() );
    int lineHeight = fontMetrics.height() + kLineHeightPadding;
    int startLine = verticalScrollBar()->value();
    int visibleLinesCount = ( viewport()->height() / lineHeight ) + 1;
    int totalLines = line_manager_->getLineCount();

    int scrollXPx = horizontalScrollBar()->value();
    int visibleWidth = viewport()->width() - gutter_width_ - kGutterTextPadding;

    for( int idx = 0; idx < visibleLinesCount; ++idx ) {
        int currentLineIndex = startLine + idx;
        if( currentLineIndex >= totalLines ) {
            break;
        }

        uint64_t lineStart = line_manager_->getLineOffset( currentLineIndex );
        uint64_t lineLen = line_manager_->getVirtualLineLength( currentLineIndex );

        int estimatedCharOffset = scrollXPx / fontMetrics.averageCharWidth();
        int charOffset = std::max( 0, estimatedCharOffset - 50 );
        charOffset = std::min( charOffset, static_cast<int>( lineLen ) );

        if( charOffset > 0 ) {
            int checkStart = std::max( 0, charOffset - 4 );
            int checkLen = charOffset - checkStart + 1;
            std::string bytes = piece_table_->getSubstr( lineStart + checkStart, checkLen );
            int bIdx = checkLen - 1;
            while( bIdx >= 0 && charOffset > 0 ) {
                unsigned char b = static_cast<unsigned char>( bytes[bIdx] );
                if( ( b & 0xC0 ) != 0x80 )
                    break;
                charOffset--;
                bIdx--;
            }
        }

        int prefixWidth = 0;
        if( charOffset > 0 ) {
            std::string rawPrefix = piece_table_->getSubstr( lineStart, charOffset );
            QString prefixStr = QString::fromUtf8( rawPrefix.data(), rawPrefix.size() );
            for( int i = 0; i < prefixStr.length(); ++i ) {
                ushort unicode = prefixStr[i].unicode();
                if( ( unicode < 32 && unicode != '\t' && unicode != '\n' && unicode != '\r' ) ||
                    unicode == 127 ) {
                    prefixStr[i] = QChar( 0xFFFD );
                }
            }
            prefixWidth = fontMetrics.horizontalAdvance( prefixStr );
        }

        int textX_ = gutter_width_ + kGutterTextPadding - scrollXPx + prefixWidth;

        int neededWidthPx = viewport()->width() - textX_;
        if( neededWidthPx < 0 ) {
            neededWidthPx = visibleWidth;
        }

        int charsToFetch = ( neededWidthPx / fontMetrics.averageCharWidth() ) * 4 + 100;
        charsToFetch = std::min( charsToFetch, static_cast<int>( lineLen - charOffset ) );

        if( charOffset + charsToFetch < lineLen ) {
            std::string nextBytes =
                piece_table_->getSubstr( lineStart + charOffset + charsToFetch, 4 );
            if( nextBytes.size() > 0 ) {
                unsigned char firstB = static_cast<unsigned char>( nextBytes[0] );
                if( ( firstB & 0xC0 ) == 0x80 ) {
                    int extraBytes = 1;
                    while( extraBytes < nextBytes.size() ) {
                        unsigned char b = static_cast<unsigned char>( nextBytes[extraBytes] );
                        if( ( b & 0xC0 ) != 0x80 )
                            break;
                        extraBytes++;
                    }
                    charsToFetch += extraBytes;
                }
            }
        }

        std::string rawChunk = piece_table_->getSubstr( lineStart + charOffset, charsToFetch );
        QString lineText = QString::fromUtf8( rawChunk.data(), rawChunk.size() );
        for( int i = 0; i < lineText.length(); ++i ) {
            ushort unicode = lineText[i].unicode();
            if( ( unicode < 32 && unicode != '\t' && unicode != '\n' && unicode != '\r' ) ||
                unicode == 127 ) {
                lineText[i] = QChar( 0xFFFD );
            }
        }

        int yBase = idx * lineHeight;
        int yText = yBase + fontMetrics.ascent() + 2;

        painter.setPen( QColor( "#808080" ) );
        painter.drawText( QRect( 0, yBase, gutter_width_ - kLineHeightPadding, lineHeight ),
                          static_cast<int>( Qt::AlignRight | Qt::AlignVCenter ),
                          QString::number( currentLineIndex + 1 ) );

        painter.save();
        painter.setClipRect( gutter_width_ + 1, yBase, viewport()->width() - gutter_width_ - 1,
                             lineHeight );

        int textX = gutter_width_ + kGutterTextPadding - scrollXPx + prefixWidth;

        // Search Highlighting
        if( !search_results_.empty() && search_length_ > 0 ) {
            auto it = std::lower_bound( search_results_.begin(), search_results_.end(), lineStart );
            int indexOffset = std::distance( search_results_.begin(), it );

            while( it != search_results_.end() && *it < lineStart + lineLen ) {
                uint64_t matchPos = *it;
                int matchCol = static_cast<int>( matchPos - lineStart );

                // Only draw if it's within our fetched window
                if( matchCol + search_length_ > charOffset &&
                    matchCol < charOffset + charsToFetch ) {
                    int visMatchStart = std::max( 0, matchCol - charOffset );
                    int visMatchEnd =
                        std::min( charsToFetch, matchCol + search_length_ - charOffset );
                    int matchLen = visMatchEnd - visMatchStart;

                    if( matchLen > 0 ) {
                        int startX =
                            textX + fontMetrics.horizontalAdvance( lineText.left( visMatchStart ) );
                        int wordWidth = fontMetrics.horizontalAdvance(
                            lineText.mid( visMatchStart, matchLen ) );

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
                if( word.isEmpty() ) {
                    continue;
                }
                int matchIdx = 0;
                while( ( matchIdx = lineText.indexOf( word, matchIdx, Qt::CaseInsensitive ) ) !=
                       -1 ) {
                    int startX = textX + fontMetrics.horizontalAdvance( lineText.left( matchIdx ) );
                    int wordWidth = fontMetrics.horizontalAdvance(
                        lineText.mid( matchIdx, static_cast<int>( word.length() ) ) );
                    painter.fillRect( startX, yBase + 2, wordWidth, lineHeight - 4,
                                      QColor( 255, 255, 0, 150 ) );
                    matchIdx += static_cast<int>( word.length() );
                }
            }
        }

        painter.setPen( Qt::black );
        painter.drawText( textX, yText, lineText );

        if( currentLineIndex == cursor_line_ && hasFocus() && cursor_visible_ ) {
            if( cursor_col_ >= charOffset && cursor_col_ <= charOffset + charsToFetch ) {
                int cursorVisOffset = cursor_col_ - charOffset;
                int cursorX =
                    textX + fontMetrics.horizontalAdvance( lineText.left( cursorVisOffset ) );
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
