#include <QApplication>
#include <windows.h>  // Windows系统API，设置控制台编码
#include "MusicPlayer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 仅保留：设置Windows控制台输出编码为UTF-8（修复乱码）
    SetConsoleOutputCP(CP_UTF8);

    MusicPlayer w;
    w.show();
    return a.exec();
}
