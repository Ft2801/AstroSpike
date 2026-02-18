#pragma once
#include <QMainWindow>
#include <memory>
class ImageBuffer;
class AstroSpikeDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void openImage();
    void saveImage();

private:
    std::shared_ptr<ImageBuffer> m_buffer;
    AstroSpikeDialog* m_spikeWidget;
    
    void loadFromQImage(const QString& path);
};
