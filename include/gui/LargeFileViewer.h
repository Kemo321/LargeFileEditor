/**
 * Authors: Jan Szwagierczak
 * Description: Header of the custom widget for displaying large file content.
 */

#pragma once

#include <QAbstractScrollArea>
#include <QPaintEvent>
#include <QPainter>
#include <QResizeEvent>

class LargeFileViewer : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit LargeFileViewer(QWidget* parent = nullptr);
    ~LargeFileViewer() override = default;

protected:
    void paintEvent(QPaintEvent* event) override;
};
