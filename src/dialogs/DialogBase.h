#ifndef DIALOGBASE_H
#define DIALOGBASE_H

#include <QDialog>
#include <QIcon>
#include <QString>

/**
 * @brief Base class for all TStar/AstroSpike dialogs with common functionality
 */
class DialogBase : public QDialog {
    Q_OBJECT

public:
    explicit DialogBase(QWidget* parent = nullptr,
                       const QString& title = QString(),
                       int defaultWidth = 0,
                       int defaultHeight = 0,
                       bool deleteOnClose = false,
                       bool showIcon = true);
    
    virtual ~DialogBase() = default;
    
    void setWindowProperties(const QString& title, int width = 0, int height = 0);
    void setDeleteOnClose(bool enabled);
    
    static QIcon getStandardIcon();
    
protected:
    virtual void setupDialogUI() {}
    
    void restoreWindowGeometry(const QString& settingsKey = QString());
    void saveWindowGeometry(const QString& settingsKey = QString());

private:
    void initialize(const QString& title, int width, int height, bool deleteOnClose, bool showIcon);
};

#endif // DIALOGBASE_H
