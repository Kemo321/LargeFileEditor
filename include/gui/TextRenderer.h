/**
 * @file TextRenderer.h
 * @brief Renders the virtualized text grid (gutter, text, highlights, cursor) for the viewer.
 */

#pragma once

#include <QFont>
#include <QRect>
#include <QSize>
#include <QStringList>
#include <cstdint>
#include <vector>

class QPainter;
class PieceTable;
class LineManager;

/**
 * @namespace editor_layout
 * @brief Layout constants shared by the viewer geometry and the renderer.
 */
namespace editor_layout {
inline constexpr int kLineHeightPadding = 5;
inline constexpr int kGutterTextPadding = 10;
inline constexpr int kGutterDigits = 6;
}  // namespace editor_layout

/**
 * @struct ViewMetrics
 * @brief Monospaced cell metrics derived from the current font.
 */
struct ViewMetrics {
    int lineHeight;
    int charWidth;
};

/**
 * @brief Computes the (line height, char width) metrics for a monospaced font.
 */
[[nodiscard]] auto computeViewMetrics( const QFont& font ) -> ViewMetrics;

/**
 * @struct RenderContext
 * @brief All state the renderer needs for a single repaint (read-only snapshot).
 */
struct RenderContext {
    PieceTable* pieceTable{ nullptr };
    LineManager* lineManager{ nullptr };
    int startLine{ 0 };
    int hScrollPx{ 0 };
    int gutterWidth{ 0 };
    QSize viewportSize;
    QFont font;
    int cursorLine{ 0 };
    int cursorCol{ 0 };
    bool cursorVisible{ false };
    bool hasFocus{ false };
    const std::vector<uint64_t>* searchResults{ nullptr };
    int activeSearchIndex{ -1 };
    int searchLength{ 0 };
    const QStringList* mockHighlights{ nullptr };
    bool renderBusy{ false };
};

/**
 * @class TextRenderer
 * @brief Stateless painter for the viewport grid.
 */
class TextRenderer {
public:
    /**
     * @brief Paints the visible portion of the document.
     * @param painter Painter bound to the viewport.
     * @param eventRect The region requiring repaint.
     * @param ctx Read-only snapshot of view state.
     */
    auto paint( QPainter& painter, const QRect& eventRect, const RenderContext& ctx ) -> void;
};
