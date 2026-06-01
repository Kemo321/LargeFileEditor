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

// Per-repaint, per-line drawing state shared by the render pipeline helpers below. The leading
// members are loop invariants set once in paint(); the trailing members are recomputed per line by
// prepareLine().
struct LineDrawContext {
    QPainter& painter;
    const RenderContext& ctx;
    const QFontMetrics& fontMetrics;
    int lineHeight;
    int charWidth;
    int gutterWidth;
    int startColByte;
    int pxOffset;
    int visibleCols;
    const std::function<unsigned char( uint64_t )>& byteAt;

    int currentLineIndex{ 0 };
    uint64_t lineStart{ 0 };
    std::string rawChunk;
    QString lineText;
    int yBase{ 0 };
    int yText{ 0 };
    int textX{ 0 };
    uint64_t chunkStartByte{ 0 };
    uint64_t lineLen{ 0 };
};

// Paints the page background, the open/busy placeholder messages, and the empty gutter column.
// Returns false when a placeholder was drawn and no document content should follow.
auto setupBackground( QPainter& painter, const QRect& eventRect, const RenderContext& ctx,
                      int gutterWidth ) -> bool
{
    const QRect viewportRect( 0, 0, ctx.viewportSize.width(), ctx.viewportSize.height() );

    painter.fillRect( eventRect, Qt::white );

    if( ctx.renderBusy ) {
        painter.setPen( Qt::gray );
        painter.drawText( viewportRect, Qt::AlignCenter, "Replacing…" );
        return false;
    }

    if( ctx.pieceTable == nullptr || ctx.lineManager == nullptr ) {
        painter.setPen( Qt::gray );
        painter.drawText( viewportRect, Qt::AlignCenter,
                          "Please open a file (File -> Open or Ctrl+O)" );
        return false;
    }

    painter.fillRect( 0, 0, gutterWidth, ctx.viewportSize.height(), QColor( "#f0f0f0" ) );
    painter.setPen( QColor( "#d0d0d0" ) );
    painter.drawLine( gutterWidth, 0, gutterWidth, ctx.viewportSize.height() );
    return true;
}

// Fetches and sanitizes the visible chunk for line @p idx and computes its layout (y positions,
// UTF-8-snapped chunk start, and horizontal text origin).
auto prepareLine( LineDrawContext& lc, int idx ) -> void
{
    const RenderContext& ctx = lc.ctx;
    lc.currentLineIndex = ctx.startLine + idx;
    lc.lineStart = ctx.lineManager->getLineOffset( lc.currentLineIndex );

    int fetch_length = lc.visibleCols + 10;
    lc.rawChunk =
        ctx.lineManager->getLineChunk( lc.currentLineIndex, lc.startColByte, fetch_length );

    lc.lineText = QString::fromUtf8( lc.rawChunk.data(), lc.rawChunk.size() );
    for( int i = 0; i < lc.lineText.length(); ++i ) {
        ushort unicode = lc.lineText[i].unicode();
        if( ( unicode < 32 && unicode != '\t' && unicode != '\n' && unicode != '\r' ) ||
            unicode == 127 ) {
            lc.lineText[i] = QChar( 0xFFFD );
        }
    }

    lc.yBase = idx * lc.lineHeight;
    lc.yText = lc.yBase + lc.fontMetrics.ascent() + 2;

    lc.lineLen = ctx.lineManager->getVirtualLineLength( lc.currentLineIndex );
    lc.chunkStartByte = lc.startColByte;
    if( lc.chunkStartByte > lc.lineLen ) {
        lc.chunkStartByte = lc.lineLen;
    }

    if( lc.chunkStartByte > 0 ) {
        uint64_t snapped = Utf8Utils::snapToCharacterBoundary( lc.byteAt, lc.lineStart,
                                                               lc.lineStart + lc.chunkStartByte );
        lc.chunkStartByte = snapped - lc.lineStart;
    }

    lc.textX = lc.gutterWidth + editor_layout::kGutterTextPadding - lc.pxOffset;

    if( lc.chunkStartByte < static_cast<uint64_t>( lc.startColByte ) ) {
        std::string prefix = lc.rawChunk.substr( 0, lc.startColByte - lc.chunkStartByte );
        QString qPrefix = QString::fromUtf8( prefix.data(), prefix.size() );
        lc.textX -= lc.fontMetrics.horizontalAdvance( qPrefix );
    }
}

// Draws the right-aligned line number in the gutter for the current line.
auto renderGutterAndLineNumbers( LineDrawContext& lc ) -> void
{
    lc.painter.setPen( QColor( "#808080" ) );
    lc.painter.drawText(
        QRect( 0, lc.yBase, lc.gutterWidth - editor_layout::kGutterTextPadding, lc.lineHeight ),
        static_cast<int>( Qt::AlignRight | Qt::AlignVCenter ),
        formatLineNumber( lc.currentLineIndex + 1 ) );
}

// Paints the highlight rectangles for search matches intersecting the current line.
auto renderSearchHighlights( LineDrawContext& lc ) -> void
{
    const RenderContext& ctx = lc.ctx;
    if( ctx.searchResults == nullptr || ctx.searchResults->empty() || ctx.searchLength <= 0 ) {
        return;
    }

    auto it =
        std::lower_bound( ctx.searchResults->begin(), ctx.searchResults->end(), lc.lineStart );
    int indexOffset = std::distance( ctx.searchResults->begin(), it );

    while( it != ctx.searchResults->end() && *it < lc.lineStart + lc.lineLen ) {
        int matchCol = static_cast<int>( *it - lc.lineStart );

        if( matchCol + ctx.searchLength > lc.chunkStartByte &&
            matchCol < lc.chunkStartByte + lc.rawChunk.size() ) {
            int visMatchStart = std::max( 0, matchCol - static_cast<int>( lc.chunkStartByte ) );
            int visMatchEnd =
                std::min( static_cast<int>( lc.rawChunk.size() ),
                          matchCol + ctx.searchLength - static_cast<int>( lc.chunkStartByte ) );
            int matchLen = visMatchEnd - visMatchStart;

            if( matchLen > 0 ) {
                std::string prefixRaw = lc.rawChunk.substr( 0, visMatchStart );
                std::string matchRaw = lc.rawChunk.substr( visMatchStart, matchLen );
                int startX = lc.textX + lc.fontMetrics.horizontalAdvance( QString::fromUtf8(
                                            prefixRaw.data(), prefixRaw.size() ) );
                int wordWidth = lc.fontMetrics.horizontalAdvance(
                    QString::fromUtf8( matchRaw.data(), matchRaw.size() ) );

                bool isActive = ( indexOffset == ctx.activeSearchIndex );
                QColor bgColor = isActive ? QColor( 255, 215, 0 ) : QColor( 255, 255, 200 );
                lc.painter.fillRect( startX, lc.yBase + 2, wordWidth, lc.lineHeight - 4, bgColor );
            }
        }
        ++it;
        ++indexOffset;
    }
}

// Paints the mock highlight rectangles for configured words on the current line.
auto renderMockHighlights( LineDrawContext& lc ) -> void
{
    const RenderContext& ctx = lc.ctx;
    if( ctx.mockHighlights == nullptr || ctx.mockHighlights->isEmpty() ) {
        return;
    }

    for( const QString& word : *ctx.mockHighlights ) {
        if( word.isEmpty() ) {
            continue;
        }
        int matchIdx = 0;
        while( ( matchIdx = lc.lineText.indexOf( word, matchIdx, Qt::CaseInsensitive ) ) != -1 ) {
            int startX =
                lc.textX + lc.fontMetrics.horizontalAdvance( lc.lineText.left( matchIdx ) );
            int wordWidth =
                lc.fontMetrics.horizontalAdvance( lc.lineText.mid( matchIdx, word.length() ) );
            lc.painter.fillRect( startX, lc.yBase + 2, wordWidth, lc.lineHeight - 4,
                                 QColor( 255, 255, 0, 150 ) );
            matchIdx += word.length();
        }
    }
}

// Draws the sanitized text of the current line at its computed origin.
auto renderTextLine( LineDrawContext& lc ) -> void
{
    lc.painter.setPen( Qt::black );
    lc.painter.drawText( lc.textX, lc.yText, lc.lineText );
}

// Draws the blinking caret when it falls on the current visible line.
auto renderCursor( LineDrawContext& lc ) -> void
{
    const RenderContext& ctx = lc.ctx;
    if( lc.currentLineIndex == ctx.cursorLine && ctx.hasFocus && ctx.cursorVisible ) {
        if( ctx.cursorCol >= lc.chunkStartByte &&
            ctx.cursorCol <= lc.chunkStartByte + lc.rawChunk.size() ) {
            int cursorVisOffset = ctx.cursorCol - lc.chunkStartByte;
            std::string prefixRaw = lc.rawChunk.substr( 0, cursorVisOffset );
            int cursorX = lc.textX + lc.fontMetrics.horizontalAdvance(
                                         QString::fromUtf8( prefixRaw.data(), prefixRaw.size() ) );
            lc.painter.setPen( Qt::black );
            lc.painter.drawLine( cursorX, lc.yBase + 2, cursorX, lc.yBase + lc.lineHeight - 2 );
        }
    }
}

}  // namespace

auto computeViewMetrics( const QFont& font ) -> ViewMetrics
{
    QFontMetrics fontMetrics( font );
    int lineHeight = fontMetrics.height() + editor_layout::kLineHeightPadding;
    int charWidth = fontMetrics.horizontalAdvance( 'A' );
    if( charWidth <= 0 ) {
        charWidth = 1;
    }
    return { lineHeight, charWidth };
}

auto TextRenderer::paint( QPainter& painter, const QRect& eventRect,
                          const RenderContext& ctx ) -> void
{
    const ViewMetrics metrics = computeViewMetrics( ctx.font );
    const int gutterWidth =
        editor_layout::kGutterDigits * metrics.charWidth + editor_layout::kGutterTextPadding * 2;

    if( !setupBackground( painter, eventRect, ctx, gutterWidth ) ) {
        return;
    }

    const QFontMetrics fontMetrics( ctx.font );
    const std::function<unsigned char( uint64_t )> byteAt = [&ctx]( uint64_t pos ) {
        return static_cast<unsigned char>( ctx.pieceTable->getSubstr( pos, 1 )[0] );
    };

    LineDrawContext lc{ painter,
                        ctx,
                        fontMetrics,
                        metrics.lineHeight,
                        metrics.charWidth,
                        gutterWidth,
                        ctx.hScrollPx / metrics.charWidth,
                        ctx.hScrollPx % metrics.charWidth,
                        ( ctx.viewportSize.width() - gutterWidth ) / metrics.charWidth,
                        byteAt };

    const int visibleLinesCount = ( ctx.viewportSize.height() / metrics.lineHeight ) + 1;
    const int totalLines = ctx.lineManager->getLineCount();

    for( int idx = 0; idx < visibleLinesCount; ++idx ) {
        if( ctx.startLine + idx >= totalLines ) {
            break;
        }

        prepareLine( lc, idx );
        renderGutterAndLineNumbers( lc );

        painter.save();
        painter.setClipRect( gutterWidth + 1, lc.yBase, ctx.viewportSize.width() - gutterWidth - 1,
                             metrics.lineHeight );
        renderSearchHighlights( lc );
        renderMockHighlights( lc );
        renderTextLine( lc );
        renderCursor( lc );
        painter.restore();
    }
}
