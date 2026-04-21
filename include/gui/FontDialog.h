/**
 * Authors: Jan Szwagierczak
 * Description: Header of the Font dialog.
 */

#pragma once

#include <QComboBox>
#include <QDialog>

class FontDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the custom Font dialog.
     * @param parent The parent widget.
     */
    explicit FontDialog( QWidget* parent = nullptr );
    ~FontDialog() override = default;

private:
    QComboBox* size_combo_{};
};
