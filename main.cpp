#include "BrainFlyer.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setStyle(QStyleFactory::create("Fusion"));

    BrainFlyer::BrainFlyer window; // Указываем пространство имен BrainFlyer
    window.show();

    return app.exec();
}