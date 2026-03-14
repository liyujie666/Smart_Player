#include "mainwindow.h"
#include <QApplication>

#undef main
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QString videoPath;

    if(argc > 1){
        videoPath = QString::fromLocal8Bit(argv[1]);
    }

    MainWindow w;
    w.show();

    if(!videoPath.isEmpty()){
        w.openVideoFromCommand(videoPath);
    }
    return a.exec();
}
