#include "MainWindow.h"
#include "AstroSpikeDialog.h"
#include "ImageBuffer.h"
#include "io/FitsLoader.h"
#include "io/SimpleTiffReader.h"
#include "io/SimpleTiffWriter.h"
#include "io/FitsWriter.h"
#include <QMenuBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle("AstroSpike Standalone");
    setWindowIcon(QIcon(":/images/Logo.png"));
    resize(1400, 900);
    
    QMenu* fileMenu = menuBar()->addMenu("File");
    fileMenu->addAction("Open...", this, &MainWindow::openImage);
    fileMenu->addAction("Save As...", this, &MainWindow::saveImage);
    fileMenu->addAction("Exit", this, &QMainWindow::close);
    
    m_spikeWidget = new AstroSpikeDialog(this);
    // Behave as a widget inside the window
    m_spikeWidget->setWindowFlags(Qt::Widget);
    setCentralWidget(m_spikeWidget);
}

MainWindow::~MainWindow() {}

void MainWindow::openImage() {
    QString path = QFileDialog::getOpenFileName(this, "Open Image", "", "All Images (*.fits *.fit *.tif *.tiff *.png *.jpg *.jpeg *.bmp);;FITS (*.fits *.fit);;TIFF (*.tif *.tiff);;PNG (*.png);;JPG (*.jpg *.jpeg)");
    if (path.isEmpty()) return;
    
    QString ext = QFileInfo(path).suffix().toLower();
    bool ok = false;
    QString err;
    
    m_buffer = std::make_shared<ImageBuffer>();

    if (ext == "fits" || ext == "fit") {
        ok = FitsLoader::load(path, *m_buffer, &err);
    } else if (ext == "tif" || ext == "tiff") {
        int w, h, c;
        std::vector<float> d;
        ok = SimpleTiffReader::readFloat32(path, w, h, c, d, &err, nullptr);
        if (ok) m_buffer->setData(w, h, c, d);
    } else {
        loadFromQImage(path);
        ok = m_buffer->isValid();
        if (!ok) err = "Failed to load image via Qt.";
    }
    
    if (ok) {
        m_spikeWidget->setImageBuffer(m_buffer.get());
        // Reset window title to file name
        setWindowTitle("AstroSpike - " + QFileInfo(path).fileName());
    } else {
        QMessageBox::critical(this, "Error", "Could not load image:\n" + err);
    }
}

void MainWindow::loadFromQImage(const QString& path) {
    QImage img(path);
    if (img.isNull()) return;
    
    img = img.convertToFormat(QImage::Format_RGB32);
    int w = img.width();
    int h = img.height();
    int c = 3;
    
    std::vector<float> data(w * h * c);
    
    for (int y = 0; y < h; ++y) {
        const QRgb* line = (const QRgb*)img.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            int idx = (y * w + x) * c;
            data[idx+0] = qRed(line[x]) / 255.0f;
            data[idx+1] = qGreen(line[x]) / 255.0f;
            data[idx+2] = qBlue(line[x]) / 255.0f;
        }
    }
    m_buffer->setData(w, h, c, data);
}

void MainWindow::saveImage() {
    if (!m_buffer || !m_buffer->isValid()) return;
    
    QString path = QFileDialog::getSaveFileName(this, "Save Image", "", "PNG (*.png);;TIFF 16-bit (*.tif);;TIFF 32-bit (*.tif);;FITS (*.fits)");
    if (path.isEmpty()) return;
    
    // Commit changes to buffer
    m_spikeWidget->applyToDocument();
    
    QString ext = QFileInfo(path).suffix().toLower();
    bool ok = false;
    QString err;
    
    if (ext == "tif" || ext == "tiff") {
         int bits = 16;
         ok = SimpleTiffWriter::write(path, m_buffer->width(), m_buffer->height(),
                                     m_buffer->channels(), SimpleTiffWriter::Format_float32,
                                     m_buffer->data(), &err);
    } else if (ext == "fits" || ext == "fit") {
        ok = FitsWriter::write(path, *m_buffer, &err);
    } else {
        // Default to Qt Save (PNG, JPG)
         QImage out = m_buffer->getDisplayImage(ImageBuffer::Display_Linear);
         if (out.format() != QImage::Format_RGB32 && out.format() != QImage::Format_ARGB32) {
             out = out.convertToFormat(QImage::Format_RGB32);
         }
         ok = out.save(path);
         if (!ok) {
             QFileInfo fi(path);
             QString format = fi.suffix().toUpper();
             err = QString("Qt could not save the image as %1. The format might be unsupported or the path invalid.").arg(format);
         }
    }
    
    if (!ok) {
        QMessageBox::critical(this, "Error", "Save failed: " + err);
    } else {
        QMessageBox::information(this, "Saved", "Image saved successfully.");
    }
}
