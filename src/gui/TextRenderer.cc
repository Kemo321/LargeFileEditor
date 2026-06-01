#include "gui/TextRenderer.h"

#include <QFontMetrics>
#include <QPainter>
#include <algorithm>
#include <cmath>
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
    const QRect viewportRect( 0, 0, ctx.viewportSize.width(), ctx.viewportSize.height() );

    painter.fillRect( eventRect, Qt::white );

    if( ctx.renderBusy ) {
        painter.setPen( Qt::gray );
        painter.drawText( viewportRect, Qt::AlignCenter, "Replacing…" );
        return;
    }

    if( ctx.pieceTable == nullptr || ctx.lineManager == nullptr ) {
        painter.setPen( Qt::gray );
        painter.drawText( viewportRect, Qt::AlignCenter,
                          "Please open a file (File -> Open or Ctrl+O)" );
        return;
    }

    QFontMetrics fontMetrics( ctx.font );
    ViewMetrics metrics = computeViewMetrics( ctx.font );
    int lineHeight = metrics.lineHeight;
    int charWidth = metrics.charWidth;
    int gutterWidth =
        editor_layout::kGutterDigits * charWidth + editor_layout::kGutterTextPadding * 2;

    painter.fillRect( 0, 0, gutterWidth, ctx.viewportSize.height(), QColor( "#f0f0f0" ) );
    painter.setPen( QColor( "#d0d0d0" ) );
    painter.drawLine( gutterWidth, 0, gutterWidth, ctx.viewportSize.height() );

    int startLine = ctx.startLine;
    int visibleLinesCount = ( ctx.viewportSize.height() / lineHeight ) + 1;
    int totalLines = ctx.lineManager->getLineCount();

    int scrollXPx = ctx.hScrollPx;
    int start_col_byte = scrollXPx / charWidth;
    int px_offset = scrollXPx % charWidth;
    int visible_cols = ( ctx.viewportSize.width() - gutterWidth ) / charWidth;

    auto byteAt = [&ctx]( uint64_t pos ) {
        return static_cast<unsigned char>( ctx.pieceTable->getSubstr( pos, 1 )[0] );
    };

    for( int idx = 0; idx < visibleLinesCount; ++idx ) {
        int currentLineIndex = startLine + idx;
        if( currentLineIndex >= totalLines ) {
            break;
        }

        uint64_t lineStart = ctx.lineManager->getLineOffset( currentLineIndex );

        int fetch_length = visible_cols + 10;
        std::string rawChunk =
            ctx.lineManager->getLineChunk( currentLineIndex, start_col_byte, fetch_length );

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
        painter.drawText(
            QRect( 0, yBase, gutterWidth - editor_layout::kGutterTextPadding, lineHeight ),
            static_cast<int>( Qt::AlignRight | Qt::AlignVCenter ),
            formatLineNumber( currentLineIndex + 1 ) );

        painter.save();
        painter.setClipRect( gutterWidth + 1, yBase, ctx.viewportSize.width() - gutterWidth - 1,
                             lineHeight );

        uint64_t line_len = ctx.lineManager->getVirtualLineLength( currentLineIndex );
        uint64_t chunk_start_byte = start_col_byte;
        if( chunk_start_byte > line_len ) {
            chunk_start_byte = line_len;
        }

        if( chunk_start_byte > 0 ) {
            uint64_t snapped = Utf8Utils::snapToCharacterBoundary( byteAt, lineStart,
                                                                   lineStart + chunk_start_byte );
            chunk_start_byte = snapped - lineStart;
        }

        int textX = gutterWidth + editor_layout::kGutterTextPadding - px_offset;

        if( chunk_start_byte < static_cast<uint64_t>( start_col_byte ) ) {
            std::string prefix = rawChunk.substr( 0, start_col_byte - chunk_start_byte );
            QString qPrefix = QString::fromUtf8( prefix.data(), prefix.size() );
            textX -= fontMetrics.horizontalAdvance( qPrefix );
        }

        if( ctx.searchResults != nullptr && !ctx.searchResults->empty() && ctx.searchLength > 0 ) {
            auto it =
                std::lower_bound( ctx.searchResults->begin(), ctx.searchResults->end(), lineStart );
            int indexOffset = std::distance( ctx.searchResults->begin(), it );

            while( it != ctx.searchResults->end() && *it < lineStart + line_len ) {
                uint64_t matchPos = *it;
                int matchCol = static_cast<int>( matchPos - lineStart );

                if( matchCol + ctx.searchLength > chunk_start_byte &&
                    matchCol < chunk_start_byte + rawChunk.size() ) {
                    int visMatchStart =
                        std::max( 0, matchCol - static_cast<int>( chunk_start_byte ) );
                    int visMatchEnd = std::min(
                        static_cast<int>( rawChunk.size() ),
                        matchCol + ctx.searchLength - static_cast<int>( chunk_start_byte ) );
                    int matchLen = visMatchEnd - visMatchStart;

                    if( matchLen > 0 ) {
                        std::string prefixRaw = rawChunk.substr( 0, visMatchStart );
                        std::string matchRaw = rawChunk.substr( visMatchStart, matchLen );
                        int startX = textX + fontMetrics.horizontalAdvance( QString::fromUtf8(
                                                 prefixRaw.data(), prefixRaw.size() ) );
                        int wordWidth = fontMetrics.horizontalAdvance(
                            QString::fromUtf8( matchRaw.data(), matchRaw.size() ) );

                        bool isActive = ( indexOffset == ctx.activeSearchIndex );
                        QColor bgColor = isActive ? QColor( 255, 215, 0 ) : QColor( 255, 255, 200 );
                        painter.fillRect( startX, yBase + 2, wordWidth, lineHeight - 4, bgColor );
                    }
                }
                ++it;
                ++indexOffset;
            }
        }

        if( ctx.mockHighlights != nullptr && !ctx.mockHighlights->isEmpty() ) {
            for( const QString& word : *ctx.mockHighlights ) {
                if( word.isEmpty() ) {
                    continue;
                }
                int matchIdx = 0;
                while( ( matchIdx = lineText.indexOf( word, matchIdx, Qt::CaseInsensitive ) ) !=
                       -1 ) {
                    int startX = textX + fontMetrics.horizontalAdvance( lineText.left( matchIdx ) );
                    int wordWidth =
                        fontMetrics.horizontalAdvance( lineText.mid( matchIdx, word.length() ) );
                    painter.fillRect( startX, yBase + 2, wordWidth, lineHeight - 4,
                                      QColor( 255, 255, 0, 150 ) );
                    matchIdx += word.length();
                }
            }
        }

        painter.setPen( Qt::black );
        painter.drawText( textX, yText, lineText );

        if( currentLineIndex == ctx.cursorLine && ctx.hasFocus && ctx.cursorVisible ) {
            if( ctx.cursorCol >= chunk_start_byte &&
                ctx.cursorCol <= chunk_start_byte + rawChunk.size() ) {
                int cursorVisOffset = ctx.cursorCol - chunk_start_byte;
                std::string prefixRaw = rawChunk.substr( 0, cursorVisOffset );
                int cursorX = textX + fontMetrics.horizontalAdvance(
                                          QString::fromUtf8( prefixRaw.data(), prefixRaw.size() ) );
                painter.setPen( Qt::black );
                painter.drawLine( cursorX, yBase + 2, cursorX, yBase + lineHeight - 2 );
            }
        }

        painter.restore();
    }
}
