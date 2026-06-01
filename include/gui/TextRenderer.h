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
inline constexpr int kLineHeightPadding = 5;   ///< Extra pixels added to each line's height.
inline constexpr int kGutterTextPadding = 10;  ///< Horizontal padding around gutter/text.
inline constexpr int kGutterDigits = 6;        ///< Line-number digits sizing the gutter width.
}  // namespace editor_layout

/**
 * @struct ViewMetrics
 * @brief Monospaced cell metrics derived from the current font.
 */
struct ViewMetrics {
    int lineHeight_;  ///< Height of one text row in pixels.
    int charWidth_;   ///< Width of one monospaced cell in pixels.
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
    PieceTable* pieceTable_{ nullptr };    ///< Document backend (not owned).
    LineManager* lineManager_{ nullptr };  ///< Line-offset cache (not owned).
    int startLine_{ 0 };                   ///< First visible virtual line.
    int hScrollPx_{ 0 };                   ///< Horizontal scroll offset in pixels.
    int gutterWidth_{ 0 };                 ///< Gutter width in pixels.
    QSize viewportSize_;                   ///< Viewport size in pixels.
    QFont font_;                           ///< Monospaced font to paint with.
    int cursorLine_{ 0 };                  ///< Cursor virtual line.
    int cursorCol_{ 0 };                   ///< Cursor byte column.
    bool cursorVisible_{ false };          ///< Whether the caret is shown this frame.
    bool hasFocus_{ false };               ///< Whether the viewer has focus.
    const std::vector<uint64_t>* searchResults_{ nullptr };  ///< Match offsets (not owned).
    int activeSearchIndex_{ -1 };                            ///< Index of the active match.
    int searchLength_{ 0 };                                  ///< Length of the search term.
    const QStringList* mockHighlights_{ nullptr };           ///< Mock highlight words (not owned).
    bool renderBusy_{ false };                               ///< True while a background task runs.
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
    static auto paint( QPainter& painter, const QRect& eventRect, const RenderContext& ctx )
        -> void;
};
