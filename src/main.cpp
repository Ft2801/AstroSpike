#include <QApplication>
#include <QStyleFactory>
#include "MainWindow.h"
int main(int argc, char *argv[])
{
    // Note: High DPI support is enabled by default in Qt6
    QApplication app(argc, argv);
    
    // Set Dark Theme
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QPalette p = app.palette();
    p.setColor(QPalette::Window, QColor(53, 53, 53));
    p.setColor(QPalette::WindowText, Qt::white);
    p.setColor(QPalette::Base, QColor(25, 25, 25));
    p.setColor(QPalette::AlternateBase, QColor(53, 53, 53));
    p.setColor(QPalette::ToolTipBase, Qt::white);
    p.setColor(QPalette::ToolTipText, Qt::white);
    p.setColor(QPalette::Text, Qt::white);
    p.setColor(QPalette::Button, QColor(53, 53, 53));
    p.setColor(QPalette::ButtonText, Qt::white);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, QColor(42, 130, 218));
    p.setColor(QPalette::Highlight, QColor(42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::black);
    app.setPalette(p);

    // Tooltip and control styling
    app.setStyleSheet(
        "QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }"
        "QScrollBar:vertical { border: 0px; background: #2b2b2b; width: 10px; margin: 0px 0px 0px 0px; border-radius: 5px; }"
        "QScrollBar::handle:vertical { background: #555; min-height: 20px; border-radius: 5px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }"
        "QScrollBar:horizontal { border: 0px; background: #2b2b2b; height: 10px; margin: 0px 0px 0px 0px; border-radius: 5px; }"
        "QScrollBar::handle:horizontal { background: #555; min-width: 20px; border-radius: 5px; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }"
        "QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: none; }"
        "QComboBox { color: white; background-color: #353535; border: 1px solid #555; padding: 2px; }"
        "QComboBox:hover { border: 1px solid #2a82da; }"
        "QComboBox QAbstractItemView { background-color: #2a2a2a; color: white; selection-background-color: #4a7ba7; selection-color: white; outline: none; }"
        "QComboBox QAbstractItemView::item { padding: 5px; margin: 0px; color: white; }"
        "QComboBox QAbstractItemView::item:hover { background-color: #4a7ba7; color: white; }"
        "QComboBox QAbstractItemView::item:selected { background-color: #4a7ba7; color: white; }"
    );

    MainWindow w;
    w.show();
    
    return app.exec();
}
