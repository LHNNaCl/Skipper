#pragma once

#include "application/session.hpp"

#include <QMainWindow>

#include <vector>

class QButtonGroup;
class QCheckBox;
class QCloseEvent;
class QFrame;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;

namespace skipper {

class Controller;

class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow();
  void showProgress(const Progress& progress);
  void showAnalyzeResult(const AnalyzeResult& result);
  void showExportResult(const ExportResult& result);

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  struct ConfirmRow {
    std::string path;
    double duration = 0;
    std::optional<Segment> intro;
    std::optional<Segment> credits;
    QCheckBox* exclude = nullptr;
    QLineEdit* introStart = nullptr;
    QLineEdit* introEnd = nullptr;
    QLineEdit* creditsStart = nullptr;
    QLineEdit* creditsEnd = nullptr;
    QWidget* sourceBox = nullptr;
    QLabel* nameLabel = nullptr;
  };

  void buildUi();
  void applyStyle();
  QWidget* buildSelectPage();
  QWidget* buildConfirmPage();
  QWidget* buildProgressPage();
  void setStep(int active);
  void chooseDirectory();
  void fillFiles(const std::vector<std::string>& paths);
  void clearStandard();
  void refreshStandard();
  void startAnalyze();
  void startExport();
  void backToSelect();
  void finishToSelect();
  void showSelectMessage(const QString& text);
  void highlightStandard(const std::vector<FieldError>& errors);
  void buildConfirm(const AnalyzeResult& result);
  void updateSource(int row);
  void markConfirmErrors(const std::vector<FieldError>& errors);
  void setFieldError(QLineEdit* edit, bool error, const QString& tip);
  QWidget* fileCell(const QString& name, const QString& sub, QLabel** nameLabel);
  QLineEdit* timeEdit();

  Controller* controller_ = nullptr;
  QStackedWidget* stack_ = nullptr;
  QLabel* stepBadges_[3] = {};
  QLabel* stepTexts_[3] = {};

  QLabel* pathLabel_ = nullptr;
  QWidget* extChips_ = nullptr;
  QLabel* selectMessage_ = nullptr;
  QTableWidget* fileTable_ = nullptr;
  QButtonGroup* standardGroup_ = nullptr;
  QLabel* standardChip_ = nullptr;
  QLineEdit* stdIntroStart_ = nullptr;
  QLineEdit* stdIntroEnd_ = nullptr;
  QLineEdit* stdCreditsStart_ = nullptr;
  QLineEdit* stdCreditsEnd_ = nullptr;
  QPushButton* analyzeButton_ = nullptr;
  QLabel* selectCount_ = nullptr;

  QLabel* audioBanner_ = nullptr;
  QLabel* confirmMessage_ = nullptr;
  QTableWidget* planTable_ = nullptr;
  QLabel* failureLabel_ = nullptr;
  QLabel* exportHint_ = nullptr;
  QPushButton* exportButton_ = nullptr;
  std::vector<ConfirmRow> rows_;

  QLabel* progressTitle_ = nullptr;
  QLabel* progressChip_ = nullptr;
  QProgressBar* bar_ = nullptr;
  QLabel* progressLabel_ = nullptr;
  QLabel* outputPathLabel_ = nullptr;
  QFrame* resultPanel_ = nullptr;
  QTableWidget* resultTable_ = nullptr;
  QLabel* resultNote_ = nullptr;
  QLabel* resultSummary_ = nullptr;
  QPushButton* cancelButton_ = nullptr;
  QPushButton* doneButton_ = nullptr;

  std::string directory_;
  std::vector<std::string> files_;
  bool sawWork_ = false;
};

}  // namespace skipper
