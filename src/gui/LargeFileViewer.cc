/**
 * Authors: Jan Szwagierczak
 * Description: Implementation of the custom widget for displaying large file content.
 */

#include "gui/LargeFileViewer.h"
#include <QPainter>

LargeFileViewer::LargeFileViewer(QWidget* parent) : QAbstractScrollArea(parent) {
    viewport()->setBackgroundRole(QPalette::Base);
}

void LargeFileViewer::paintEvent(QPaintEvent* event) {
    QPainter painter(viewport());
    painter.fillRect(event->rect(), Qt::white);
    painter.drawText(viewport()->rect(), Qt::AlignCenter, "Large File Text Rendering Area\n(Mockup - Backend Disconnected)");
}
