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

static constexpr int kLineOffsetDelayMs = 300;
static constexpr int kScrollStepDivisor = 120;
static constexpr int kWheelMultiplier = 3;

LargeFileViewer::LargeFileViewer( QWidget* parent ) : QAbstractScrollArea( parent )
{
    viewport()->setBackgroundRole( QPalette::Base );
    setFocusPolicy( Qt::StrongFocus );

    QFont fixedFont = QFontDatabase::systemFont( QFontDatabase::FixedFont );
    setFont( fixedFont );

    setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    setHorizontalScrollBarPolicy( Qt::ScrollBarAsNeeded );

    cursor_ = new CursorManager( this );
    connect( cursor_, &CursorManager::blinkToggled, this, [this]() { viewport()->update(); } );

    controller_ = new EditorController( cursor_, this );
    connect( controller_, &EditorController::documentEdited, this, [this]( uint64_t offset ) {
        invalidateCache( offset );
        emit documentModified();
    } );

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

auto LargeFileViewer::setBusy( bool busy ) -> void
{
    render_busy_ = busy;
    if( cursor_ != nullptr ) {
        if( busy ) {
            cursor_->stopBlink();
        } else {
            cursor_->startBlink();
        }
    }
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
        int visibleWidth = viewport()->width() - gutter_width_ - editor_layout::kGutterTextPadding;
        int currentHScroll = horizontalScrollBar()->value();

        if( matchStartPx < currentHScroll || matchEndPx > currentHScroll + visibleWidth ) {
            int matchCenterPx = ( matchStartPx + matchEndPx ) / 2;
            int newScroll = std::max( 0, matchCenterPx - visibleWidth / 2 );
            horizontalScrollBar()->setValue( newScroll );
        }
    }
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
    if( piece_table_ == nullptr || !line_manager_ || render_busy_ ) {
        return;
    }

    ViewMetrics metrics = computeViewMetrics( font() );
    int lineHeight = metrics.lineHeight;
    int charWidth = metrics.charWidth;
    gutter_width_ =
        editor_layout::kGutterDigits * charWidth + editor_layout::kGutterTextPadding * 2;

    int totalLines = line_manager_->getLineCount();
    int visibleLines = viewport()->height() / lineHeight;

    verticalScrollBar()->setSingleStep( 1 );
    verticalScrollBar()->setPageStep( visibleLines );
    verticalScrollBar()->setRange( 0, std::max( 0, totalLines - visibleLines ) );

    int maxChars = static_cast<int>( line_manager_->getGlobalMaxLineLength() );
    int maxPixels = maxChars * charWidth;
    int visibleWidth = viewport()->width() - gutter_width_ - editor_layout::kGutterTextPadding;

    horizontalScrollBar()->setSingleStep( charWidth * 4 );
    horizontalScrollBar()->setPageStep( visibleWidth );
    horizontalScrollBar()->setRange( 0, std::max( 0, maxPixels - visibleWidth ) );
}

auto LargeFileViewer::setPieceTable( PieceTable* pieceTable ) -> void
{
    piece_table_ = pieceTable;
    if( piece_table_ != nullptr ) {
        line_manager_ = std::make_unique<LineManager>( piece_table_ );
        setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
    } else {
        line_manager_.reset();
        setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    }
    cursor_->setPosition( 0, 0 );
    controller_->setContext( piece_table_, line_manager_.get() );
    invalidateCache();
    refreshLineOffsets();
    verticalScrollBar()->setValue( 0 );
    horizontalScrollBar()->setValue( 0 );
    viewport()->update();
}

auto LargeFileViewer::setCursorPosition( int line, int col ) -> void
{
    if( piece_table_ == nullptr || !line_manager_ ) {
        return;
    }
    int clampedLine = std::max( 0, std::min( line, line_manager_->getLineCount() - 1 ) );
    int lineLen = static_cast<int>( line_manager_->getVirtualLineLength( clampedLine ) );
    int clampedCol = std::max( 0, std::min( col, lineLen ) );
    cursor_->setPosition( clampedLine, clampedCol );
    cursor_->setVisible( true );
    scrollToCursor();
    viewport()->update();
    emit cursorPositionChanged( cursor_->line(), cursor_->col() );
}

auto LargeFileViewer::scrollToCursor() -> void
{
    ViewMetrics metrics = computeViewMetrics( font() );
    int lineHeight = metrics.lineHeight;
    int charWidth = metrics.charWidth;

    int currentScroll = verticalScrollBar()->value();
    int visibleLinesCount = viewport()->height() / lineHeight;

    if( cursor_->line() < currentScroll ) {
        verticalScrollBar()->setValue( cursor_->line() );
    } else if( cursor_->line() >= currentScroll + visibleLinesCount ) {
        verticalScrollBar()->setValue( cursor_->line() - visibleLinesCount + 1 );
    }

    int currentHScroll = horizontalScrollBar()->value();
    int visibleWidth = viewport()->width() - gutter_width_ - editor_layout::kGutterTextPadding;

    int cursorPx = cursor_->col() * charWidth;
    int margin = charWidth * 2;

    if( cursorPx < currentHScroll ) {
        horizontalScrollBar()->setValue( std::max( 0, cursorPx - margin ) );
    } else if( cursorPx > currentHScroll + visibleWidth - margin ) {
        horizontalScrollBar()->setValue( cursorPx - visibleWidth + margin );
    }
}

auto LargeFileViewer::keyPressEvent( QKeyEvent* event ) -> void
{
    if( piece_table_ == nullptr ) {
        return;
    }

    controller_->handleKeyPress( event );

    cursor_->setVisible( true );
    scrollToCursor();
    refreshLineOffsets();
    viewport()->update();

    emit cursorPositionChanged( cursor_->line(), cursor_->col() );

    line_offset_timer_->start( kLineOffsetDelayMs );
}

auto LargeFileViewer::mousePressEvent( QMouseEvent* event ) -> void
{
    if( piece_table_ == nullptr ) {
        return;
    }

    ViewMetrics metrics = computeViewMetrics( font() );
    int lineHeight = metrics.lineHeight;
    int charWidth = metrics.charWidth;

    int clickedLineOffset = static_cast<int>( event->position().y() ) / lineHeight;
    int targetLine = verticalScrollBar()->value() + clickedLineOffset;

    if( targetLine < line_manager_->getLineCount() ) {
        int textX = static_cast<int>( event->position().x() ) - gutter_width_ -
                    editor_layout::kGutterTextPadding;
        int scrollXPx = horizontalScrollBar()->value();
        int totalPx = textX + scrollXPx;

        int approx_col = totalPx / charWidth;
        if( approx_col < 0 ) {
            approx_col = 0;
        }

        uint64_t lineLen = line_manager_->getVirtualLineLength( targetLine );
        if( approx_col > static_cast<int>( lineLen ) ) {
            approx_col = static_cast<int>( lineLen );
        }

        controller_->handleMouseClick( targetLine, approx_col );
    }

    cursor_->setVisible( true );
    viewport()->update();

    emit cursorPositionChanged( cursor_->line(), cursor_->col() );
}

auto LargeFileViewer::paintViewport( QPaintEvent* event ) -> void
{
    QPainter painter( viewport() );
    gutter_width_ = editor_layout::kGutterDigits * computeViewMetrics( font() ).charWidth +
                    editor_layout::kGutterTextPadding * 2;

    RenderContext ctx;
    ctx.pieceTable = piece_table_;
    ctx.lineManager = line_manager_.get();
    ctx.startLine = verticalScrollBar()->value();
    ctx.hScrollPx = horizontalScrollBar()->value();
    ctx.gutterWidth = gutter_width_;
    ctx.viewportSize = viewport()->size();
    ctx.font = font();
    ctx.cursorLine = cursor_->line();
    ctx.cursorCol = cursor_->col();
    ctx.cursorVisible = cursor_->isVisible();
    ctx.hasFocus = hasFocus();
    ctx.searchResults = &search_results_;
    ctx.activeSearchIndex = active_search_index_;
    ctx.searchLength = search_length_;
    ctx.mockHighlights = &mock_highlight_words_;
    ctx.renderBusy = render_busy_;
    renderer_.paint( painter, event->rect(), ctx );
}

auto LargeFileViewer::wheelEvent( QWheelEvent* event ) -> void
{
    int numSteps = event->angleDelta().y() / kScrollStepDivisor;
    verticalScrollBar()->setValue( verticalScrollBar()->value() - ( numSteps * kWheelMultiplier ) );
    event->accept();
}
