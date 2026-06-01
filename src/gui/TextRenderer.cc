// Author: Jan Szwagierczak

#include "gui/TextRenderer.h"

#include <QFontMetrics>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

#include "backend/PieceTable.h"
#include "gui/LineManager.h"
#include "util/Utf8Utils.h"

namespace {

constexpr int kOneMillion = 1'000'000;
constexpr int kOneBillion = 1'000'000'000;

auto formatLineNumber( int lineNum ) -> QString
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

// Per-line drawing state: leading members are loop invariants set in paint(); trailing members are
// recomputed per line by prepareLine().
struct LineDrawContext {
    QPainter& painter_;
    const RenderContext& ctx_;
    const QFontMetrics& fontMetrics_;
    int lineHeight_;
    int gutterWidth_;
    int startColByte_;
    int pxOffset_;
    int visibleCols_;
    const std::function<unsigned char( uint64_t )>& byteAt_;

    int currentLineIndex_{ 0 };
    uint64_t lineStart_{ 0 };
    std::string rawChunk_;
    QString lineText_;
    int yBase_{ 0 };
    int yText_{ 0 };
    int textX_{ 0 };
    uint64_t chunkStartByte_{ 0 };
    uint64_t lineLen_{ 0 };
};

// Paints the page background, the open/busy placeholder messages, and the empty gutter column.
// Returns false when a placeholder was drawn and no document content should follow.
auto setupBackground( QPainter& painter, const QRect& eventRect, const RenderContext& ctx,
                      int gutterWidth_ ) -> bool
{
    const QRect viewportRect( 0, 0, ctx.viewportSize_.width(), ctx.viewportSize_.height() );

    painter.fillRect( eventRect, Qt::white );

    if( ctx.renderBusy_ ) {
        painter.setPen( Qt::gray );
        painter.drawText( viewportRect, Qt::AlignCenter, "Zamienianie…" );
        return false;
    }

    if( ctx.pieceTable_ == nullptr || ctx.lineManager_ == nullptr ) {
        painter.setPen( Qt::gray );
        painter.drawText( viewportRect, Qt::AlignCenter,
                          "Otwórz plik (Plik -> Otwórz lub Ctrl+O)" );
        return false;
    }

    painter.fillRect( 0, 0, gutterWidth_, ctx.viewportSize_.height(), QColor( "#f0f0f0" ) );
    painter.setPen( QColor( "#d0d0d0" ) );
    painter.drawLine( gutterWidth_, 0, gutterWidth_, ctx.viewportSize_.height() );
    return true;
}

// Fetches and sanitizes the visible chunk for line @p idx and computes its layout (y positions,
// UTF-8-snapped chunk start, and horizontal text origin).
auto prepareLine( LineDrawContext& lc, int idx ) -> void
{
    const RenderContext& ctx = lc.ctx_;
    lc.currentLineIndex_ = ctx.startLine_ + idx;
    lc.lineStart_ = ctx.lineManager_->getLineOffset( lc.currentLineIndex_ );

    int fetch_length = lc.visibleCols_ + 10;
    lc.rawChunk_ =
        ctx.lineManager_->getLineChunk( lc.currentLineIndex_, lc.startColByte_, fetch_length );

    lc.lineText_ = QString::fromUtf8( lc.rawChunk_.data(), lc.rawChunk_.size() );
    for( int i = 0; i < lc.lineText_.length(); ++i ) {
        ushort unicode = lc.lineText_[i].unicode();
        if( ( unicode < 32 && unicode != '\t' && unicode != '\n' && unicode != '\r' ) ||
            unicode == 127 ) {
            lc.lineText_[i] = QChar( 0xFFFD );
        }
    }

    lc.yBase_ = idx * lc.lineHeight_;
    lc.yText_ = lc.yBase_ + lc.fontMetrics_.ascent() + 2;

    lc.lineLen_ = ctx.lineManager_->getVirtualLineLength( lc.currentLineIndex_ );
    lc.chunkStartByte_ = lc.startColByte_;
    if( lc.chunkStartByte_ > lc.lineLen_ ) {
        lc.chunkStartByte_ = lc.lineLen_;
    }

    if( lc.chunkStartByte_ > 0 ) {
        uint64_t snapped = Utf8Utils::snapToCharacterBoundary( lc.byteAt_, lc.lineStart_,
                                                               lc.lineStart_ + lc.chunkStartByte_ );
        lc.chunkStartByte_ = snapped - lc.lineStart_;
    }

    lc.textX_ = lc.gutterWidth_ + editor_layout::kGutterTextPadding - lc.pxOffset_;

    if( lc.chunkStartByte_ < static_cast<uint64_t>( lc.startColByte_ ) ) {
        std::string prefix = lc.rawChunk_.substr( 0, lc.startColByte_ - lc.chunkStartByte_ );
        QString qPrefix = QString::fromUtf8( prefix.data(), prefix.size() );
        lc.textX_ -= lc.fontMetrics_.horizontalAdvance( qPrefix );
    }
}

// Draws the right-aligned line number in the gutter for the current line.
auto renderGutterAndLineNumbers( LineDrawContext& lc ) -> void
{
    lc.painter_.setPen( QColor( "#808080" ) );
    lc.painter_.drawText(
        QRect( 0, lc.yBase_, lc.gutterWidth_ - editor_layout::kGutterTextPadding, lc.lineHeight_ ),
        static_cast<int>( Qt::AlignRight | Qt::AlignVCenter ),
        formatLineNumber( lc.currentLineIndex_ + 1 ) );
}

// Paints the highlight rectangles for search matches intersecting the current line.
auto renderSearchHighlights( LineDrawContext& lc ) -> void
{
    const RenderContext& ctx = lc.ctx_;
    if( ctx.searchResults_ == nullptr || ctx.searchResults_->empty() || ctx.searchLength_ <= 0 ) {
        return;
    }

    auto it =
        std::lower_bound( ctx.searchResults_->begin(), ctx.searchResults_->end(), lc.lineStart_ );
    int indexOffset = std::distance( ctx.searchResults_->begin(), it );

    while( it != ctx.searchResults_->end() && *it < lc.lineStart_ + lc.lineLen_ ) {
        int matchCol = static_cast<int>( *it - lc.lineStart_ );

        if( matchCol + ctx.searchLength_ > lc.chunkStartByte_ &&
            matchCol < lc.chunkStartByte_ + lc.rawChunk_.size() ) {
            int visMatchStart = std::max( 0, matchCol - static_cast<int>( lc.chunkStartByte_ ) );
            int visMatchEnd =
                std::min( static_cast<int>( lc.rawChunk_.size() ),
                          matchCol + ctx.searchLength_ - static_cast<int>( lc.chunkStartByte_ ) );
            int matchLen = visMatchEnd - visMatchStart;

            if( matchLen > 0 ) {
                std::string prefixRaw = lc.rawChunk_.substr( 0, visMatchStart );
                std::string matchRaw = lc.rawChunk_.substr( visMatchStart, matchLen );
                int startX = lc.textX_ + lc.fontMetrics_.horizontalAdvance( QString::fromUtf8(
                                             prefixRaw.data(), prefixRaw.size() ) );
                int wordWidth = lc.fontMetrics_.horizontalAdvance(
                    QString::fromUtf8( matchRaw.data(), matchRaw.size() ) );

                bool isActive = ( indexOffset == ctx.activeSearchIndex_ );
                QColor bgColor = isActive ? QColor( 255, 215, 0 ) : QColor( 255, 255, 200 );
                lc.painter_.fillRect( startX, lc.yBase_ + 2, wordWidth, lc.lineHeight_ - 4,
                                      bgColor );
            }
        }
        ++it;
        ++indexOffset;
    }
}

// Paints the mock highlight rectangles for configured words on the current line.
auto renderMockHighlights( LineDrawContext& lc ) -> void
{
    const RenderContext& ctx = lc.ctx_;
    if( ctx.mockHighlights_ == nullptr || ctx.mockHighlights_->isEmpty() ) {
        return;
    }

    for( const QString& word : *ctx.mockHighlights_ ) {
        if( word.isEmpty() ) {
            continue;
        }
        int matchIdx = 0;
        while( ( matchIdx = lc.lineText_.indexOf( word, matchIdx, Qt::CaseInsensitive ) ) != -1 ) {
            int startX =
                lc.textX_ + lc.fontMetrics_.horizontalAdvance( lc.lineText_.left( matchIdx ) );
            int wordWidth =
                lc.fontMetrics_.horizontalAdvance( lc.lineText_.mid( matchIdx, word.length() ) );
            lc.painter_.fillRect( startX, lc.yBase_ + 2, wordWidth, lc.lineHeight_ - 4,
                                  QColor( 255, 255, 0, 150 ) );
            matchIdx += word.length();
        }
    }
}

// Draws the sanitized text of the current line at its computed origin.
auto renderTextLine( LineDrawContext& lc ) -> void
{
    lc.painter_.setPen( Qt::black );
    lc.painter_.drawText( lc.textX_, lc.yText_, lc.lineText_ );
}

// Draws the blinking caret when it falls on the current visible line.
auto renderCursor( LineDrawContext& lc ) -> void
{
    const RenderContext& ctx = lc.ctx_;
    if( lc.currentLineIndex_ == ctx.cursorLine_ && ctx.hasFocus_ && ctx.cursorVisible_ ) {
        if( ctx.cursorCol_ >= lc.chunkStartByte_ &&
            ctx.cursorCol_ <= lc.chunkStartByte_ + lc.rawChunk_.size() ) {
            int cursorVisOffset = ctx.cursorCol_ - lc.chunkStartByte_;
            std::string prefixRaw = lc.rawChunk_.substr( 0, cursorVisOffset );
            int cursorX = lc.textX_ + lc.fontMetrics_.horizontalAdvance(
                                          QString::fromUtf8( prefixRaw.data(), prefixRaw.size() ) );
            lc.painter_.setPen( Qt::black );
            lc.painter_.drawLine( cursorX, lc.yBase_ + 2, cursorX, lc.yBase_ + lc.lineHeight_ - 2 );
        }
    }
}

}  // namespace

auto computeViewMetrics( const QFont& font_ ) -> ViewMetrics
{
    QFontMetrics fontMetrics( font_ );
    int lineHeight_ = fontMetrics.height() + editor_layout::kLineHeightPadding;
    int charWidth_ = fontMetrics.horizontalAdvance( 'A' );
    if( charWidth_ <= 0 ) {
        charWidth_ = 1;
    }
    return { lineHeight_, charWidth_ };
}

auto TextRenderer::paint( QPainter& painter, const QRect& eventRect, const RenderContext& ctx )
    -> void
{
    const ViewMetrics metrics = computeViewMetrics( ctx.font_ );
    const int gutterWidth_ =
        editor_layout::kGutterDigits * metrics.charWidth_ + editor_layout::kGutterTextPadding * 2;

    if( !setupBackground( painter, eventRect, ctx, gutterWidth_ ) ) {
        return;
    }

    const QFontMetrics fontMetrics( ctx.font_ );
    const std::function<unsigned char( uint64_t )> byteAt = [&ctx]( uint64_t pos ) {
        return static_cast<unsigned char>( ctx.pieceTable_->getSubstr( pos, 1 )[0] );
    };

    LineDrawContext lc{ painter,
                        ctx,
                        fontMetrics,
                        metrics.lineHeight_,
                        gutterWidth_,
                        ctx.hScrollPx_ / metrics.charWidth_,
                        ctx.hScrollPx_ % metrics.charWidth_,
                        ( ctx.viewportSize_.width() - gutterWidth_ ) / metrics.charWidth_,
                        byteAt };

    const int visibleLinesCount = ( ctx.viewportSize_.height() / metrics.lineHeight_ ) + 1;
    const int totalLines = ctx.lineManager_->getLineCount();

    for( int idx = 0; idx < visibleLinesCount; ++idx ) {
        if( ctx.startLine_ + idx >= totalLines ) {
            break;
        }

        prepareLine( lc, idx );
        renderGutterAndLineNumbers( lc );

        painter.save();
        painter.setClipRect( gutterWidth_ + 1, lc.yBase_,
                             ctx.viewportSize_.width() - gutterWidth_ - 1, metrics.lineHeight_ );
        renderSearchHighlights( lc );
        renderMockHighlights( lc );
        renderTextLine( lc );
        renderCursor( lc );
        painter.restore();
    }
}
