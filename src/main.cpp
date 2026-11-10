#include "ui/main_window.hpp"

#include <QApplication>
#include <QFont>

int main(int argc, char* argv[]) {
  QApplication app(argc, argv);
  QApplication::setApplicationName("Skipper");
  QApplication::setStyle("Fusion");
  QFont font("Segoe UI", 10);
  font.setFamilies({"Segoe UI", "Microsoft YaHei UI", "Microsoft YaHei"});
  app.setFont(font);

  skipper::MainWindow window;
  window.show();
  return app.exec();
}
