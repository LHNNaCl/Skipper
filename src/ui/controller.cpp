#include "ui/controller.hpp"

#include "infrastructure/ffmpeg_export.hpp"
#include "infrastructure/ffmpeg_fingerprint.hpp"
#include "infrastructure/ffmpeg_probe.hpp"
#include "infrastructure/ffmpeg_visual.hpp"
#include "infrastructure/file_listing.hpp"
#include "infrastructure/output_files.hpp"
#include "ui/main_window.hpp"

#include <QMetaObject>
#include <QThread>

namespace skipper {

class Controller::Worker : public QObject {
 public:
  Worker()
      : probe(runner, tools),
        finger(runner, tools),
        visual(runner, tools),
        exporter(runner, tools),
        session(listing, probe, finger, visual, exporter, files) {}

  ProcessRunner runner;
  ExternalTools tools;
  FileListingFs listing;
  FfmpegProbe probe;
  FfmpegFingerprint finger;
  FfmpegVisual visual;
  FfmpegExport exporter;
  OutputFilesFs files;
  Session session;
};

Controller::Controller(MainWindow* window) : QObject(window), window_(window) {
  worker_ = new Worker();
  thread_ = new QThread(this);
  worker_->moveToThread(thread_);
  thread_->start();
}

Controller::~Controller() { shutdown(); }

std::vector<std::string> Controller::listDirectory(const std::string& directory) {
  FileListingFs listing;
  return listing.list(directory);
}

void Controller::analyze(const std::string& directory,
                         const std::optional<StandardMark>& standard,
                         const DetectionMethods& methods) {
  if (busy_ || closing_.load()) return;
  busy_ = true;
  token_.reset();
  QMetaObject::invokeMethod(
      worker_,
      [this, directory, standard, methods] {
        auto result = worker_->session.analyze(
            directory, standard, methods,
            [this](const Progress& progress) {
              const Progress copy = progress;
              QMetaObject::invokeMethod(
                  this,
                  [this, copy] {
                    if (!closing_.load()) window_->showProgress(copy);
                  },
                  Qt::QueuedConnection);
            },
            token_);
        QMetaObject::invokeMethod(
            this,
            [this, result] {
              busy_ = false;
              if (!closing_.load()) window_->showAnalyzeResult(result);
            },
            Qt::QueuedConnection);
      },
      Qt::QueuedConnection);
}

void Controller::exportConfirmed(const std::vector<ConfirmedEpisode>& episodes) {
  if (busy_ || closing_.load()) return;
  busy_ = true;
  token_.reset();
  QMetaObject::invokeMethod(
      worker_,
      [this, episodes] {
        auto result = worker_->session.exportConfirmed(
            episodes,
            [this](const Progress& progress) {
              const Progress copy = progress;
              QMetaObject::invokeMethod(
                  this,
                  [this, copy] {
                    if (!closing_.load()) window_->showProgress(copy);
                  },
                  Qt::QueuedConnection);
            },
            token_);
        QMetaObject::invokeMethod(
            this,
            [this, result] {
              busy_ = false;
              if (!closing_.load()) window_->showExportResult(result);
            },
            Qt::QueuedConnection);
      },
      Qt::QueuedConnection);
}

void Controller::cancel() {
  token_.cancel();
  if (worker_ != nullptr) worker_->runner.terminate();
}

void Controller::shutdown() {
  if (stopped_) return;
  stopped_ = true;
  closing_.store(true);
  token_.cancel();
  if (worker_ != nullptr) worker_->runner.terminate();
  if (thread_ != nullptr) {
    thread_->quit();
    thread_->wait(8000);
  }
  delete worker_;
  worker_ = nullptr;
}

}  // namespace skipper

