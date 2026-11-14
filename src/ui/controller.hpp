#pragma once

#include "application/session.hpp"

#include <QObject>

#include <atomic>
#include <optional>

class QThread;

namespace skipper {

class MainWindow;

class Controller : public QObject {
  Q_OBJECT

 public:
  explicit Controller(MainWindow* window);
  ~Controller() override;

  std::vector<std::string> listDirectory(const std::string& directory);
  void analyze(const std::string& directory, const std::optional<StandardMark>& standard);
  void exportConfirmed(const std::vector<ConfirmedEpisode>& episodes);
  void cancel();
  void shutdown();

 private:
  class Worker;

  MainWindow* window_ = nullptr;
  Worker* worker_ = nullptr;
  QThread* thread_ = nullptr;
  CancelToken token_;
  std::atomic<bool> closing_{false};
  bool busy_ = false;
  bool stopped_ = false;
};

}  // namespace skipper
