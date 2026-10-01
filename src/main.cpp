#include "mainwindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Ollama Chat"));
    app.setOrganizationName(QStringLiteral("OllamaChat"));

    QFont uiFont(QStringLiteral("Segoe UI"), 10);
    app.setFont(uiFont);

    QFile styleFile(QStringLiteral(":/styles/app.qss"));
    if (styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(styleFile.readAll()));
    }

    MainWindow window;
    window.show();

    return app.exec();
}
