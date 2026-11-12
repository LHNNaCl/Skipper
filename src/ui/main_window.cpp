#include "ui/main_window.hpp"

#include "domain/edit_check.hpp"
#include "domain/output_name.hpp"
#include "ui/controller.hpp"
#include "ui/text.hpp"
#include "ui/time_format.hpp"

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColor>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <map>

namespace skipper {
namespace {

QString u8(const char* text) { return QString::fromUtf8(text); }

QString fromStd(const std::string& text) { return QString::fromUtf8(text.c_str()); }

QFrame* panel() {
  auto* frame = new QFrame;
  frame->setObjectName("panel");
  return frame;
}

QLabel* caption(const char* text) {
  auto* label = new QLabel(u8(text));
  label->setObjectName("caption");
  return label;
}

QPushButton* button(const char* text, bool primary) {
  auto* widget = new QPushButton(u8(text));
  widget->setObjectName(primary ? "primary" : "secondary");
  widget->setCursor(Qt::PointingHandCursor);
  return widget;
}

class ElideLabel : public QLabel {
 public:
  explicit ElideLabel(QWidget* parent = nullptr) : QLabel(parent) {
    setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
  }

  void setFullText(const QString& text) {
    full_ = text;
    setToolTip(text);
    updateText();
  }

 protected:
  void resizeEvent(QResizeEvent* event) override {
    QLabel::resizeEvent(event);
    updateText();
  }

 private:
  void updateText() {
    const QString next = QFontMetrics(font()).elidedText(full_, Qt::ElideMiddle, qMax(8, width()));
    if (text() != next) QLabel::setText(next);
  }

  QString full_;
};

void restyle(QWidget* widget);

QWidget* chipLabel(const QString& text, const QString& kind) {
  auto* chip = new QWidget;
  chip->setObjectName("chip");
  chip->setProperty("kind", kind);
  chip->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  auto* layout = new QHBoxLayout(chip);
  layout->setContentsMargins(9, 2, 9, 2);
  layout->setSpacing(6);
  auto* dot = new QLabel;
  dot->setObjectName("chipDot");
  dot->setFixedSize(7, 7);
  if (kind == QStringLiteral("plain")) dot->hide();
  auto* label = new QLabel(text);
  label->setObjectName("chipText");
  layout->addWidget(dot);
  layout->addWidget(label);
  return chip;
}

void setChip(QWidget* chip, const QString& text, const QString& kind) {
  chip->setProperty("kind", kind);
  if (auto* label = chip->findChild<QLabel*>("chipText")) label->setText(text);
  restyle(chip);
  for (auto* child : chip->findChildren<QWidget*>()) restyle(child);
}

void setElided(QLabel* label, const QString& text) {
  if (auto* elide = dynamic_cast<ElideLabel*>(label)) elide->setFullText(text);
  else label->setText(text);
}

void restyle(QWidget* widget) {
  widget->style()->unpolish(widget);
  widget->style()->polish(widget);
  widget->update();
}

QString parentDirectory(const std::string& path) {
  const QString full = fromStd(path);
  const int slash = full.lastIndexOf(QRegularExpression("[\\\\/]"));
  return slash < 0 ? QString() : full.left(slash);
}

const char* fieldTip(FieldCode code) {
  switch (code) {
    case FieldCode::HalfEmpty: return text::kHalfEmpty;
    case FieldCode::Order: return text::kOrder;
    case FieldCode::OutOfRange: return text::kOutOfRange;
    case FieldCode::Overlap: return text::kOverlap;
  }
  return text::kEditInvalid;
}

}  // namespace

MainWindow::MainWindow() {
  setWindowTitle("Skipper");
  setMinimumSize(960, 640);
  resize(1100, 760);
  controller_ = new Controller(this);
  buildUi();
  applyStyle();
  setStep(0);
  setupTray();
}

void MainWindow::setupTray() {
  QIcon icon(QStringLiteral(":/icons/skipper.png"));
  if (icon.isNull()) icon = style()->standardIcon(QStyle::SP_DesktopIcon);
  setWindowIcon(icon);
  if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

  tray_ = new QSystemTrayIcon(icon, qApp);

  auto* menu = new QMenu(this);
  auto* showAction = menu->addAction(u8(text::kShowMainWindow));
  auto* quitAction = menu->addAction(u8(text::kQuit));
  connect(showAction, &QAction::triggered, this, &MainWindow::showFromTray);
  connect(quitAction, &QAction::triggered, this, &MainWindow::quitFromTray);
  connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) showFromTray();
  });
  tray_->setContextMenu(menu);
  tray_->show();
  // Windows 11 drops a tip registered before the tray icon's version is set.
  tray_->setToolTip(u8(text::kTrayTip));
}

void MainWindow::showFromTray() {
  if (isMinimized()) showNormal();
  show();
  raise();
  activateWindow();
}

void MainWindow::quitFromTray() {
  quitting_ = true;
  controller_->shutdown();
  QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (!quitting_ && tray_ != nullptr) {
    event->ignore();
    hide();
    tray_->show();
    return;
  }
  controller_->shutdown();
  event->accept();
  QApplication::quit();
}

void MainWindow::buildUi() {
  auto* root = new QWidget;
  auto* layout = new QVBoxLayout(root);
  layout->setContentsMargins(20, 16, 20, 18);
  layout->setSpacing(12);

  auto* steps = new QWidget;
  auto* stepLayout = new QHBoxLayout(steps);
  stepLayout->setContentsMargins(0, 0, 0, 0);
  stepLayout->setSpacing(8);
  const char* names[] = {text::kStepSelect, text::kStepConfirm, text::kStepResult};
  for (int i = 0; i < 3; ++i) {
    if (i != 0) {
      auto* line = new QFrame;
      line->setObjectName("stepLine");
      line->setFixedSize(34, 1);
      stepLayout->addWidget(line);
    }
    stepBadges_[i] = new QLabel(QString::number(i + 1));
    stepBadges_[i]->setObjectName("stepBadge");
    stepBadges_[i]->setAlignment(Qt::AlignCenter);
    stepBadges_[i]->setFixedSize(22, 22);
    stepTexts_[i] = new QLabel(u8(names[i]));
    stepTexts_[i]->setObjectName("stepText");
    stepLayout->addWidget(stepBadges_[i]);
    stepLayout->addWidget(stepTexts_[i]);
  }
  stepLayout->addStretch();
  layout->addWidget(steps);

  stack_ = new QStackedWidget;
  stack_->addWidget(buildSelectPage());
  stack_->addWidget(buildConfirmPage());
  stack_->addWidget(buildProgressPage());
  layout->addWidget(stack_, 1);
  setCentralWidget(root);
}

QWidget* MainWindow::buildSelectPage() {
  auto* page = new QWidget;
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);

  auto* dirPanel = panel();
  auto* dirLayout = new QVBoxLayout(dirPanel);
  dirLayout->addWidget(caption(text::kDirectory));
  auto* dirRow = new QHBoxLayout;
  auto* choose = button(text::kChooseDirectory, false);
  connect(choose, &QPushButton::clicked, this, &MainWindow::chooseDirectory);
  pathLabel_ = new QLabel(u8(text::kNoDirectory));
  pathLabel_->setObjectName("path");
  extChips_ = new QWidget;
  auto* chips = new QHBoxLayout(extChips_);
  chips->setContentsMargins(0, 0, 0, 0);
  chips->setSpacing(6);
  dirRow->addWidget(choose);
  dirRow->addWidget(pathLabel_, 1);
  dirRow->addWidget(extChips_);
  dirLayout->addLayout(dirRow);
  auto* note = new QLabel(u8(text::kTopLevelOnly));
  note->setObjectName("note");
  dirLayout->addWidget(note);
  selectMessage_ = new QLabel;
  selectMessage_->setObjectName("message");
  selectMessage_->setWordWrap(true);
  selectMessage_->hide();
  dirLayout->addWidget(selectMessage_);
  layout->addWidget(dirPanel);

  auto* listPanel = panel();
  auto* listLayout = new QVBoxLayout(listPanel);
  auto* listHead = new QHBoxLayout;
  listHead->addWidget(caption(text::kVideoList));
  listHead->addStretch();
  auto* listHint = new QLabel(u8(text::kStandardHint));
  listHint->setObjectName("note");
  listHead->addWidget(listHint);
  listLayout->addLayout(listHead);
  fileTable_ = new QTableWidget(0, 2);
  fileTable_->setHorizontalHeaderLabels({u8(text::kStandard), u8(text::kFile)});
  fileTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  fileTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  fileTable_->setColumnWidth(0, 64);
  fileTable_->verticalHeader()->hide();
  fileTable_->setSelectionMode(QAbstractItemView::NoSelection);
  fileTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  fileTable_->setFocusPolicy(Qt::NoFocus);
  fileTable_->setShowGrid(false);
  standardGroup_ = new QButtonGroup(this);
  standardGroup_->setExclusive(true);
  connect(standardGroup_, &QButtonGroup::idClicked, this, [this](int) { refreshStandard(); });
  connect(fileTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
    if (auto* radio = standardGroup_->button(row)) radio->setChecked(true);
    refreshStandard();
  });
  listLayout->addWidget(fileTable_, 1);
  selectCount_ = new QLabel;
  selectCount_->setObjectName("note");
  listLayout->addWidget(selectCount_);
  layout->addWidget(listPanel, 1);

  auto* stdPanel = panel();
  auto* stdLayout = new QVBoxLayout(stdPanel);
  auto* head = new QHBoxLayout;
  standardChip_ = chipLabel(u8(text::kNoStandard), "muted");
  auto* help = new QLabel(u8(text::kStandardHelp));
  help->setObjectName("note");
  head->addWidget(standardChip_);
  head->addWidget(help, 1);
  stdLayout->addLayout(head);
  auto* times = new QGridLayout;
  const char* labels[] = {text::kIntroStart, text::kIntroEnd, text::kCreditsStart, text::kCreditsEnd};
  QLineEdit** edits[] = {&stdIntroStart_, &stdIntroEnd_, &stdCreditsStart_, &stdCreditsEnd_};
  for (int i = 0; i < 4; ++i) {
    times->addWidget(caption(labels[i]), 0, i);
    *edits[i] = timeEdit();
    times->addWidget(*edits[i], 1, i);
  }
  times->setColumnStretch(4, 1);
  stdLayout->addLayout(times);
  auto* methods = new QHBoxLayout;
  methods->setSpacing(16);
  methods->addWidget(caption(text::kMethods));
  methodStandard_ = new QCheckBox(u8(text::kMethodStandard));
  methodChapter_ = new QCheckBox(u8(text::kMethodChapter));
  methodAudio_ = new QCheckBox(u8(text::kMethodAudio));
  methodBlack_ = new QCheckBox(u8(text::kMethodBlack));
  for (QCheckBox* box : {methodStandard_, methodChapter_, methodAudio_, methodBlack_}) {
    box->setChecked(true);
    methods->addWidget(box);
  }
  connect(methodStandard_, &QCheckBox::toggled, this, [this](bool) { refreshStandard(); });
  methods->addStretch();
  stdLayout->addLayout(methods);
  auto* footer = new QHBoxLayout;
  auto* hint = new QLabel(u8(text::kAnalyzeHint));
  hint->setObjectName("note");
  footer->addWidget(hint, 1);
  auto* clear = button(text::kClearStandard, false);
  connect(clear, &QPushButton::clicked, this, &MainWindow::clearStandard);
  analyzeButton_ = button(text::kAnalyze, true);
  analyzeButton_->setEnabled(false);
  connect(analyzeButton_, &QPushButton::clicked, this, &MainWindow::startAnalyze);
  footer->addWidget(clear);
  footer->addWidget(analyzeButton_);
  stdLayout->addLayout(footer);
  layout->addWidget(stdPanel);
  refreshStandard();
  return page;
}

QWidget* MainWindow::buildConfirmPage() {
  auto* page = new QWidget;
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  auto* box = panel();
  auto* boxLayout = new QVBoxLayout(box);
  audioBanner_ = new QLabel(u8(text::kAudioSkipped));
  audioBanner_->setObjectName("banner");
  audioBanner_->setWordWrap(true);
  audioBanner_->hide();
  boxLayout->addWidget(audioBanner_);
  confirmMessage_ = new QLabel;
  confirmMessage_->setObjectName("message");
  confirmMessage_->setWordWrap(true);
  confirmMessage_->hide();
  boxLayout->addWidget(confirmMessage_);
  auto* head = new QHBoxLayout;
  head->addWidget(caption(text::kResults));
  head->addStretch();
  auto* hint = new QLabel(u8(text::kManualHint));
  hint->setObjectName("note");
  head->addWidget(hint);
  boxLayout->addLayout(head);
  planTable_ = new QTableWidget(0, 8);
  planTable_->setHorizontalHeaderLabels({u8(text::kExclude), u8(text::kFile), u8(text::kDuration), u8(text::kIntroStart),
                                         u8(text::kIntroEnd), u8(text::kCreditsStart), u8(text::kCreditsEnd),
                                         u8(text::kSource)});
  planTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  planTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  planTable_->setColumnWidth(0, 56);
  for (int column : {2, 3, 4, 5, 6}) {
    planTable_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::Fixed);
    planTable_->setColumnWidth(column, column == 2 ? 88 : 108);
  }
  planTable_->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Fixed);
  planTable_->setColumnWidth(7, 150);
  planTable_->verticalHeader()->hide();
  planTable_->setSelectionMode(QAbstractItemView::NoSelection);
  planTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  planTable_->setFocusPolicy(Qt::NoFocus);
  planTable_->setShowGrid(false);
  boxLayout->addWidget(planTable_, 1);
  failureLabel_ = new QLabel;
  failureLabel_->setObjectName("note");
  failureLabel_->setWordWrap(true);
  failureLabel_->hide();
  boxLayout->addWidget(failureLabel_);
  auto* note = new QLabel(u8(text::kConfirmHint));
  note->setObjectName("note");
  boxLayout->addWidget(note);
  auto* footer = new QHBoxLayout;
  exportHint_ = new QLabel;
  exportHint_->setObjectName("note");
  footer->addWidget(exportHint_, 1);
  auto* back = button(text::kBack, false);
  connect(back, &QPushButton::clicked, this, &MainWindow::backToSelect);
  exportButton_ = button(text::kExport, true);
  connect(exportButton_, &QPushButton::clicked, this, &MainWindow::startExport);
  footer->addWidget(back);
  footer->addWidget(exportButton_);
  boxLayout->addLayout(footer);
  layout->addWidget(box, 1);
  return page;
}

QWidget* MainWindow::buildProgressPage() {
  auto* page = new QWidget;
  auto* layout = new QVBoxLayout(page);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(12);
  auto* top = panel();
  top->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
  auto* topLayout = new QVBoxLayout(top);
  topLayout->setContentsMargins(16, 14, 16, 14);
  topLayout->setSpacing(8);
  auto* head = new QHBoxLayout;
  head->setAlignment(Qt::AlignVCenter);
  progressTitle_ = caption(text::kProgress);
  progressChip_ = chipLabel(u8(text::kRunning), "info");
  head->addWidget(progressTitle_);
  head->addStretch();
  head->addWidget(progressChip_);
  topLayout->addLayout(head);
  bar_ = new QProgressBar;
  bar_->setRange(0, 100);
  bar_->setTextVisible(false);
  bar_->setFixedHeight(8);
  topLayout->addWidget(bar_);
  auto* row = new QHBoxLayout;
  row->setSpacing(16);
  progressLabel_ = new ElideLabel;
  progressLabel_->setObjectName("progressLine");
  outputPathLabel_ = new ElideLabel;
  outputPathLabel_->setObjectName("note");
  row->addWidget(progressLabel_, 1);
  row->addWidget(outputPathLabel_, 1);
  cancelButton_ = button(text::kCancel, false);
  connect(cancelButton_, &QPushButton::clicked, this, [this] { controller_->cancel(); });
  row->addWidget(cancelButton_);
  topLayout->addLayout(row);
  layout->addWidget(top);

  resultPanel_ = panel();
  resultPanel_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
  auto* resultLayout = new QVBoxLayout(resultPanel_);
  resultLayout->setContentsMargins(16, 14, 16, 14);
  resultLayout->setSpacing(12);
  resultTable_ = new QTableWidget(0, 3);
  resultTable_->setHorizontalHeaderLabels({u8("状态"), u8(text::kFile), u8("说明")});
  resultTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
  resultTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
  resultTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
  resultTable_->setColumnWidth(0, 110);
  resultTable_->setColumnWidth(2, 220);
  resultTable_->verticalHeader()->hide();
  resultTable_->setSelectionMode(QAbstractItemView::NoSelection);
  resultTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  resultTable_->setFocusPolicy(Qt::NoFocus);
  resultTable_->setShowGrid(false);
  resultLayout->addWidget(resultTable_, 1);
  resultNote_ = new QLabel;
  resultNote_->setObjectName("message");
  resultNote_->hide();
  resultLayout->addWidget(resultNote_);
  auto* footer = new QHBoxLayout;
  footer->setSpacing(16);
  resultSummary_ = new QLabel;
  resultSummary_->setObjectName("progressLine");
  resultSummary_->setTextFormat(Qt::RichText);
  doneButton_ = button(text::kDone, true);
  doneButton_->hide();
  connect(doneButton_, &QPushButton::clicked, this, &MainWindow::finishToSelect);
  footer->addWidget(resultSummary_, 1);
  footer->addWidget(doneButton_);
  resultLayout->addLayout(footer);
  layout->addWidget(resultPanel_, 1);
  resultPanel_->hide();
  return page;
}

void MainWindow::setStep(int active) {
  for (int i = 0; i < 3; ++i) {
    const char* state = i < active ? "done" : (i == active ? "on" : "off");
    stepBadges_[i]->setText(i < active ? QString::fromUtf8("✓") : QString::number(i + 1));
    stepBadges_[i]->setProperty("state", state);
    stepTexts_[i]->setProperty("state", state);
    restyle(stepBadges_[i]);
    restyle(stepTexts_[i]);
  }
}

void MainWindow::chooseDirectory() {
  const QString picked = QFileDialog::getExistingDirectory(this, u8(text::kChooseDirectory));
  if (picked.isEmpty()) return;
  directory_ = picked.toUtf8().toStdString();
  pathLabel_->setText(picked);
  pathLabel_->setToolTip(picked);
  showSelectMessage({});
  fillFiles(controller_->listDirectory(directory_));
}

void MainWindow::fillFiles(const std::vector<std::string>& paths) {
  files_ = paths;
  for (auto* radio : standardGroup_->buttons()) standardGroup_->removeButton(radio);
  fileTable_->setRowCount(0);
  std::map<std::string, int> extensions;
  for (int row = 0; row < static_cast<int>(files_.size()); ++row) {
    fileTable_->insertRow(row);
    fileTable_->setRowHeight(row, 52);
    auto* radio = new QRadioButton;
    radio->setObjectName("choice");
    auto* holder = new QWidget;
    auto* holderLayout = new QHBoxLayout(holder);
    holderLayout->setContentsMargins(0, 0, 0, 0);
    holderLayout->setAlignment(Qt::AlignCenter);
    holderLayout->addWidget(radio);
    fileTable_->setCellWidget(row, 0, holder);
    standardGroup_->addButton(radio, row);
    const QString name = fromStd(fileNameOf(files_[static_cast<std::size_t>(row)]));
    fileTable_->setCellWidget(row, 1, fileCell(name, parentDirectory(files_[static_cast<std::size_t>(row)]), nullptr));
    const auto dot = name.lastIndexOf('.');
    if (dot >= 0) {
      auto ext = name.mid(dot + 1).toLower().toStdString();
      extensions[ext] += 1;
    }
  }
  auto* chips = qobject_cast<QHBoxLayout*>(extChips_->layout());
  while (QLayoutItem* item = chips->takeAt(0)) {
    delete item->widget();
    delete item;
  }
  for (const auto& [ext, count] : extensions) {
    chips->addWidget(chipLabel(QString::fromStdString(ext) + u8(" · ") + QString::number(count), "muted"));
  }
  if (!files_.empty()) chips->addWidget(chipLabel(u8("共 ") + QString::number(static_cast<int>(files_.size())) + u8(" 个"), "plain"));
  selectCount_->setText(files_.empty() ? u8(text::kNoVideos) : QString());
  analyzeButton_->setEnabled(!files_.empty());
  clearStandard();
}

void MainWindow::clearStandard() {
  standardGroup_->setExclusive(false);
  for (auto* radio : standardGroup_->buttons()) radio->setChecked(false);
  standardGroup_->setExclusive(true);
  for (QLineEdit* edit : {stdIntroStart_, stdIntroEnd_, stdCreditsStart_, stdCreditsEnd_}) {
    edit->clear();
    setFieldError(edit, false, {});
  }
  refreshStandard();
}

void MainWindow::refreshStandard() {
  const int row = standardGroup_->checkedId();
  const bool chosen = row >= 0 && row < static_cast<int>(files_.size());
  const bool useStandard = methodStandard_ && methodStandard_->isChecked();
  for (QLineEdit* edit : {stdIntroStart_, stdIntroEnd_, stdCreditsStart_, stdCreditsEnd_}) {
    edit->setEnabled(chosen && useStandard);
  }
  if (!chosen) setChip(standardChip_, u8(text::kNoStandard), "muted");
  else setChip(standardChip_, u8(text::kStandardPrefix) + fromStd(fileNameOf(files_[static_cast<std::size_t>(row)])), "std");
}

void MainWindow::startAnalyze() {
  if (files_.empty()) return;
  showSelectMessage({});
  for (QLineEdit* edit : {stdIntroStart_, stdIntroEnd_, stdCreditsStart_, stdCreditsEnd_}) setFieldError(edit, false, {});
  const int row = standardGroup_->checkedId();
  std::optional<StandardMark> mark;
  if (methodStandard_->isChecked() && row >= 0) {
    QLineEdit* edits[] = {stdIntroStart_, stdIntroEnd_, stdCreditsStart_, stdCreditsEnd_};
    bool bad = false;
    for (QLineEdit* edit : edits) {
      const auto parsed = parseTime(edit->text().toUtf8().toStdString());
      if (!parsed.ok) {
        setFieldError(edit, true, u8(text::kBadTime));
        bad = true;
      }
    }
    if (bad) {
      showSelectMessage(u8(text::kBadTime));
      return;
    }
    StandardMark standard;
    standard.path = files_[static_cast<std::size_t>(row)];
    const auto introStart = parseTime(stdIntroStart_->text().toUtf8().toStdString());
    const auto introEnd = parseTime(stdIntroEnd_->text().toUtf8().toStdString());
    const auto creditsStart = parseTime(stdCreditsStart_->text().toUtf8().toStdString());
    const auto creditsEnd = parseTime(stdCreditsEnd_->text().toUtf8().toStdString());
    if (!introStart.empty) standard.intro.startSec = introStart.seconds;
    if (!introEnd.empty) standard.intro.endSec = introEnd.seconds;
    if (!creditsStart.empty) standard.credits.startSec = creditsStart.seconds;
    if (!creditsEnd.empty) standard.credits.endSec = creditsEnd.seconds;
    mark = std::move(standard);
  }
  analyzeButton_->setEnabled(false);
  exportButton_->setEnabled(false);
  sawWork_ = false;
  liveListReady_ = false;
  resultPanel_->hide();
  resultNote_->hide();
  DetectionMethods methods;
  methods.standard = methodStandard_->isChecked();
  methods.chapter = methodChapter_->isChecked();
  methods.audio = methodAudio_->isChecked();
  methods.blackFrame = methodBlack_->isChecked();
  controller_->analyze(directory_, mark, methods);
}

void MainWindow::showProgress(const Progress& progress) {
  sawWork_ = true;
  stack_->setCurrentIndex(2);
  setStep(progress.stage == Stage::Export ? 2 : 1);
  progressTitle_->setText(u8(progress.stage == Stage::Export ? text::kProgress : text::kAnalyzing));
  setChip(progressChip_, u8(text::kRunning), "info");
  setElided(progressLabel_, u8(text::stage(progress.stage)) + QString("  %1/%2  %3")
                                                              .arg(progress.index)
                                                              .arg(progress.count)
                                                              .arg(fromStd(progress.name)));
  bar_->setObjectName("");
  bar_->setValue(progress.count <= 0 ? 0 : progress.index * 100 / progress.count);
  restyle(bar_);
  setElided(outputPathLabel_, u8(text::kOutputDir) + "  " + fromStd(directory_) + "\\output");
  cancelButton_->show();
  doneButton_->hide();
  resultPanel_->show();
  syncLiveResults(progress);
}

void MainWindow::syncLiveResults(const Progress& progress) {
  std::vector<std::string> names;
  if (progress.stage == Stage::Export) {
    for (const ConfirmRow& row : rows_) names.push_back(fileNameOf(row.path));
  }
  if (names.empty()) {
    for (const std::string& path : files_) names.push_back(fileNameOf(path));
  }
  const bool rebuild = !liveListReady_ || liveStage_ != progress.stage ||
                       resultTable_->rowCount() != static_cast<int>(names.size());
  if (rebuild) {
    liveStage_ = progress.stage;
    liveListReady_ = true;
    resultTable_->setRowCount(0);
    for (int row = 0; row < static_cast<int>(names.size()); ++row) {
      resultTable_->insertRow(row);
      resultTable_->setRowHeight(row, 52);
      auto* status = chipLabel(u8(text::kWaiting), "muted");
      auto* holder = new QWidget;
      auto* holderLayout = new QHBoxLayout(holder);
      holderLayout->setContentsMargins(8, 0, 0, 0);
      holderLayout->addWidget(status);
      holderLayout->addStretch();
      resultTable_->setCellWidget(row, 0, holder);
      resultTable_->setCellWidget(row, 1, fileCell(fromStd(names[static_cast<std::size_t>(row)]), u8(text::kUntouched), nullptr));
      auto* detail = new QTableWidgetItem;
      detail->setFlags(Qt::ItemIsEnabled);
      detail->setForeground(QColor("#98a0a9"));
      resultTable_->setItem(row, 2, detail);
    }
  }

  int current = -1;
  for (int row = 0; row < static_cast<int>(names.size()); ++row) {
    if (names[static_cast<std::size_t>(row)] == progress.name) {
      current = row;
      break;
    }
  }
  if (current < 0 && progress.index >= 1 && progress.index <= static_cast<int>(names.size())) current = progress.index - 1;

  int done = 0;
  int waiting = 0;
  for (int row = 0; row < resultTable_->rowCount(); ++row) {
    auto* holder = resultTable_->cellWidget(row, 0);
    auto* chip = holder == nullptr ? nullptr : holder->findChild<QWidget*>("chip");
    auto* detail = resultTable_->item(row, 2);
    if (chip == nullptr) continue;
    if (row == current) {
      setChip(chip, u8(text::kRunning), "info");
      if (detail != nullptr) detail->setText(u8(text::stage(progress.stage)));
    } else if (current >= 0 && row < current) {
      setChip(chip, u8(text::kProcessed), "muted");
      if (detail != nullptr) detail->setText(u8(text::stage(progress.stage)));
      ++done;
    } else {
      setChip(chip, u8(text::kWaiting), "muted");
      if (detail != nullptr) detail->setText({});
      ++waiting;
    }
  }
  if (current >= 0 && resultTable_->item(current, 2) != nullptr) {
    resultTable_->scrollToItem(resultTable_->item(current, 2), QAbstractItemView::PositionAtCenter);
  }
  resultNote_->hide();
  resultSummary_->setText(
      QString::fromUtf8("已处理 <b>%1</b>    进行中 <b style='color:#4f8cff'>%2</b>    等待 <b>%3</b>")
          .arg(done)
          .arg(current >= 0 ? 1 : 0)
          .arg(waiting));
}

void MainWindow::showAnalyzeResult(const AnalyzeResult& result) {
  analyzeButton_->setEnabled(!files_.empty());
  exportButton_->setEnabled(true);
  if (result.reason == Reason::NoVideos) {
    stack_->setCurrentIndex(0);
    setStep(0);
    showSelectMessage(u8(text::kNoVideos));
    return;
  }
  if (result.reason == Reason::StandardInvalid) {
    stack_->setCurrentIndex(0);
    setStep(0);
    showSelectMessage(u8(result.fieldErrors.empty() ? text::kStandardUnreadable : text::kStandardInvalid));
    highlightStandard(result.fieldErrors);
    return;
  }
  if (result.reason == Reason::Cancelled) {
    stack_->setCurrentIndex(2);
    setStep(1);
    setChip(progressChip_, u8(text::kCancelled), "warn");
    resultNote_->setText(u8(text::kCancelledNote));
    resultNote_->show();
    resultPanel_->show();
    resultTable_->setRowCount(0);
    resultSummary_->clear();
    cancelButton_->hide();
    doneButton_->show();
    return;
  }
  buildConfirm(result);
  stack_->setCurrentIndex(1);
  setStep(1);
}

void MainWindow::showExportResult(const ExportResult& result) {
  analyzeButton_->setEnabled(!files_.empty());
  exportButton_->setEnabled(true);
  if (result.validationFailed) {
    stack_->setCurrentIndex(1);
    setStep(1);
    confirmMessage_->setText(u8(text::kEditInvalid));
    confirmMessage_->show();
    markConfirmErrors(result.fieldErrors);
    return;
  }
  stack_->setCurrentIndex(2);
  setStep(2);
  liveListReady_ = false;
  resultPanel_->show();
  cancelButton_->hide();
  doneButton_->show();
  bar_->setValue(100);
  bar_->setObjectName(result.cancelled ? "" : "done");
  restyle(bar_);
  setChip(progressChip_, u8(result.cancelled ? text::kCancelled : text::kAllDone), result.cancelled ? "warn" : "ok");
  resultNote_->setVisible(result.cancelled || (result.batchReason == Reason::ExportFailed && result.items.empty()));
  if (result.batchReason == Reason::ExportFailed && result.items.empty()) resultNote_->setText(u8(text::kCannotCreateOutput));
  else if (result.cancelled) resultNote_->setText(u8(text::kCancelledNote));

  resultTable_->setRowCount(0);
  int exported = 0;
  int excluded = 0;
  int skipped = 0;
  int failed = 0;
  for (int row = 0; row < static_cast<int>(result.items.size()); ++row) {
    const ItemResult& item = result.items[static_cast<std::size_t>(row)];
    resultTable_->insertRow(row);
    resultTable_->setRowHeight(row, 52);
    auto* status = chipLabel(u8(text::status(item.status)), text::statusKind(item.status));
    auto* holder = new QWidget;
    auto* holderLayout = new QHBoxLayout(holder);
    holderLayout->setContentsMargins(8, 0, 0, 0);
    holderLayout->addWidget(status);
    holderLayout->addStretch();
    resultTable_->setCellWidget(row, 0, holder);
    const QString subtitle = item.status == ItemStatus::Exported
                                 ? QString("output\\") + fromStd(outputFileName(item.name))
                                 : u8(text::kUntouched);
    resultTable_->setCellWidget(row, 1, fileCell(fromStd(item.name), subtitle, nullptr));
    auto* detail = new QTableWidgetItem(u8(text::detail(item.status, item.reason)));
    detail->setFlags(Qt::ItemIsEnabled);
    detail->setForeground(QColor("#98a0a9"));
    resultTable_->setItem(row, 2, detail);
    if (item.status == ItemStatus::Exported) ++exported;
    else if (item.status == ItemStatus::Excluded) ++excluded;
    else if (item.status == ItemStatus::Skipped) ++skipped;
    else ++failed;
  }
  resultSummary_->setText(
      QString::fromUtf8("已导出 <b style='color:#45c97a'>%1</b>    已排除 <b>%2</b>    已跳过 <b style='color:#e0b34f'>%3</b>    失败 <b style='color:#e5645f'>%4</b>")
          .arg(exported)
          .arg(excluded)
          .arg(skipped)
          .arg(failed));
}

void MainWindow::buildConfirm(const AnalyzeResult& result) {
  audioBanner_->setVisible(result.audioUnavailable);
  confirmMessage_->hide();
  planTable_->setRowCount(0);
  rows_.clear();
  rows_.reserve(result.plans.size());
  for (int row = 0; row < static_cast<int>(result.plans.size()); ++row) {
    const EpisodePlan& plan = result.plans[static_cast<std::size_t>(row)];
    planTable_->insertRow(row);
    planTable_->setRowHeight(row, 48);
    ConfirmRow item;
    item.path = plan.path;
    item.duration = plan.durationSec;
    item.intro = plan.intro;
    item.credits = plan.credits;
    item.exclude = new QCheckBox;
    auto* checkHolder = new QWidget;
    auto* checkLayout = new QHBoxLayout(checkHolder);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setAlignment(Qt::AlignCenter);
    checkLayout->addWidget(item.exclude);
    planTable_->setCellWidget(row, 0, checkHolder);
    item.nameLabel = nullptr;
    planTable_->setCellWidget(row, 1, fileCell(fromStd(plan.name), {}, &item.nameLabel));
    auto* duration = new QTableWidgetItem(fromStd(formatTime(plan.durationSec)));
    duration->setTextAlignment(Qt::AlignCenter);
    duration->setFlags(Qt::ItemIsEnabled);
    planTable_->setItem(row, 2, duration);
    item.introStart = timeEdit();
    item.introEnd = timeEdit();
    item.creditsStart = timeEdit();
    item.creditsEnd = timeEdit();
    if (plan.intro) {
      item.introStart->setText(fromStd(formatTime(plan.intro->startSec)));
      item.introEnd->setText(fromStd(formatTime(plan.intro->endSec)));
    }
    if (plan.credits) {
      item.creditsStart->setText(fromStd(formatTime(plan.credits->startSec)));
      item.creditsEnd->setText(fromStd(formatTime(plan.credits->endSec)));
    }
    QLineEdit* edits[] = {item.introStart, item.introEnd, item.creditsStart, item.creditsEnd};
    for (int column = 0; column < 4; ++column) {
      auto* holder = new QWidget;
      auto* holderLayout = new QHBoxLayout(holder);
      holderLayout->setContentsMargins(4, 0, 4, 0);
      holderLayout->addWidget(edits[column]);
      planTable_->setCellWidget(row, 3 + column, holder);
      connect(edits[column], &QLineEdit::textChanged, this, [this, row, edit = edits[column]] {
        setFieldError(edit, false, {});
        if (row >= 0 && row < static_cast<int>(rows_.size())) updateSource(row);
      });
    }
    item.sourceBox = new QWidget;
    auto* sourceLayout = new QHBoxLayout(item.sourceBox);
    sourceLayout->setContentsMargins(6, 0, 6, 0);
    sourceLayout->setSpacing(4);
    planTable_->setCellWidget(row, 7, item.sourceBox);
    connect(item.exclude, &QCheckBox::toggled, this, [this, row](bool excluded) {
      if (row < 0 || row >= static_cast<int>(rows_.size()) || rows_[static_cast<std::size_t>(row)].nameLabel == nullptr) return;
      rows_[static_cast<std::size_t>(row)].nameLabel->setProperty("dim", excluded);
      restyle(rows_[static_cast<std::size_t>(row)].nameLabel);
    });
    rows_.push_back(item);
    updateSource(row);
  }
  QStringList failures;
  for (const FailedProbe& failure : result.failures) {
    failures << fromStd(failure.name) + u8("    ") + u8(text::kCannotRead);
  }
  failureLabel_->setText(failures.join("\n"));
  failureLabel_->setVisible(!failures.isEmpty());
  exportHint_->setText(u8(text::kExportTo) + "  " + fromStd(directory_) + "\\output\\");
  exportButton_->setEnabled(!result.plans.empty());
}

void MainWindow::updateSource(int row) {
  if (row < 0 || row >= static_cast<int>(rows_.size())) return;
  const ConfirmRow& item = rows_[static_cast<std::size_t>(row)];
  auto describe = [](const std::optional<Segment>& original, QLineEdit* start, QLineEdit* end, QString& kind) {
    const auto parsedStart = parseTime(start->text().toUtf8().toStdString());
    const auto parsedEnd = parseTime(end->text().toUtf8().toStdString());
    Endpoint edited;
    if (parsedStart.ok && !parsedStart.empty) edited.startSec = parsedStart.seconds;
    if (parsedEnd.ok && !parsedEnd.empty) edited.endSec = parsedEnd.seconds;
    if (!edited.startSec && !edited.endSec) {
      kind = "none";
      return u8(text::kNotFound);
    }
    const auto source = sourceAfterEdit(original, edited);
    if (!source) {
      kind = "none";
      return u8(text::kNotFound);
    }
    kind = text::sourceKind(*source);
    return u8(text::source(*source));
  };
  QString introKind;
  QString creditsKind;
  const QString intro = describe(item.intro, item.introStart, item.introEnd, introKind);
  const QString credits = describe(item.credits, item.creditsStart, item.creditsEnd, creditsKind);
  auto* layout = qobject_cast<QHBoxLayout*>(item.sourceBox->layout());
  while (QLayoutItem* child = layout->takeAt(0)) {
    delete child->widget();
    delete child;
  }
  if (intro == credits) {
    layout->addWidget(chipLabel(intro, introKind));
  } else {
    layout->addWidget(chipLabel(intro, introKind));
    layout->addWidget(chipLabel(credits, creditsKind));
  }
  layout->addStretch();
}

void MainWindow::startExport() {
  confirmMessage_->hide();
  std::vector<ConfirmedEpisode> episodes;
  bool badFormat = false;
  for (const ConfirmRow& row : rows_) {
    QLineEdit* edits[] = {row.introStart, row.introEnd, row.creditsStart, row.creditsEnd};
    bool rowBad = false;
    for (QLineEdit* edit : edits) {
      const auto parsed = parseTime(edit->text().toUtf8().toStdString());
      const bool invalid = !parsed.ok;
      if (invalid && !row.exclude->isChecked()) {
        setFieldError(edit, true, u8(text::kBadTime));
        rowBad = true;
      } else if (!invalid) {
        setFieldError(edit, false, {});
      }
    }
    badFormat = badFormat || rowBad;
    ConfirmedEpisode episode;
    episode.path = row.path;
    episode.excluded = row.exclude->isChecked();
    const auto introStart = parseTime(row.introStart->text().toUtf8().toStdString());
    const auto introEnd = parseTime(row.introEnd->text().toUtf8().toStdString());
    const auto creditsStart = parseTime(row.creditsStart->text().toUtf8().toStdString());
    const auto creditsEnd = parseTime(row.creditsEnd->text().toUtf8().toStdString());
    if (introStart.ok && !introStart.empty) episode.intro.startSec = introStart.seconds;
    if (introEnd.ok && !introEnd.empty) episode.intro.endSec = introEnd.seconds;
    if (creditsStart.ok && !creditsStart.empty) episode.credits.startSec = creditsStart.seconds;
    if (creditsEnd.ok && !creditsEnd.empty) episode.credits.endSec = creditsEnd.seconds;
    episodes.push_back(std::move(episode));
  }
  if (badFormat) {
    confirmMessage_->setText(u8(text::kBadTime));
    confirmMessage_->show();
    return;
  }
  analyzeButton_->setEnabled(false);
  exportButton_->setEnabled(false);
  sawWork_ = false;
  controller_->exportConfirmed(episodes);
}

void MainWindow::markConfirmErrors(const std::vector<FieldError>& errors) {
  for (const ConfirmRow& row : rows_) {
    for (QLineEdit* edit : {row.introStart, row.introEnd, row.creditsStart, row.creditsEnd}) setFieldError(edit, false, {});
  }
  for (const FieldError& error : errors) {
    const auto found = std::find_if(rows_.begin(), rows_.end(), [&](const ConfirmRow& row) { return row.path == error.path; });
    if (found == rows_.end()) continue;
    const QString tip = u8(fieldTip(error.code));
    auto mark = [&](QLineEdit* edit) { setFieldError(edit, true, tip); };
    switch (error.field) {
      case Field::Intro: mark(found->introStart); mark(found->introEnd); break;
      case Field::Credits: mark(found->creditsStart); mark(found->creditsEnd); break;
      case Field::IntroStart: mark(found->introStart); break;
      case Field::IntroEnd: mark(found->introEnd); break;
      case Field::CreditsStart: mark(found->creditsStart); break;
      case Field::CreditsEnd: mark(found->creditsEnd); break;
      case Field::Pair: mark(found->introEnd); mark(found->creditsStart); break;
    }
  }
}

void MainWindow::highlightStandard(const std::vector<FieldError>& errors) {
  for (QLineEdit* edit : {stdIntroStart_, stdIntroEnd_, stdCreditsStart_, stdCreditsEnd_}) setFieldError(edit, false, {});
  for (const FieldError& error : errors) {
    const QString tip = u8(fieldTip(error.code));
    auto mark = [&](QLineEdit* edit) { setFieldError(edit, true, tip); };
    switch (error.field) {
      case Field::Intro: mark(stdIntroStart_); mark(stdIntroEnd_); break;
      case Field::Credits: mark(stdCreditsStart_); mark(stdCreditsEnd_); break;
      case Field::IntroStart: mark(stdIntroStart_); break;
      case Field::IntroEnd: mark(stdIntroEnd_); break;
      case Field::CreditsStart: mark(stdCreditsStart_); break;
      case Field::CreditsEnd: mark(stdCreditsEnd_); break;
      case Field::Pair: mark(stdIntroEnd_); mark(stdCreditsStart_); break;
    }
  }
}

void MainWindow::setFieldError(QLineEdit* edit, bool error, const QString& tip) {
  edit->setProperty("error", error);
  edit->setToolTip(tip);
  restyle(edit);
}

void MainWindow::showSelectMessage(const QString& text) {
  selectMessage_->setText(text);
  selectMessage_->setVisible(!text.isEmpty());
}

void MainWindow::backToSelect() {
  stack_->setCurrentIndex(0);
  setStep(0);
  analyzeButton_->setEnabled(!files_.empty());
}

void MainWindow::finishToSelect() {
  resultNote_->hide();
  resultPanel_->hide();
  backToSelect();
}

QWidget* MainWindow::fileCell(const QString& name, const QString& sub, QLabel** nameLabel) {
  auto* cell = new QWidget;
  auto* layout = new QHBoxLayout(cell);
  layout->setContentsMargins(8, 4, 8, 4);
  layout->setSpacing(8);
  auto* icon = new QLabel(QString::fromUtf8("▶"));
  icon->setObjectName("fileIcon");
  icon->setAlignment(Qt::AlignCenter);
  icon->setFixedSize(26, 26);
  auto* text = new QWidget;
  auto* textLayout = new QVBoxLayout(text);
  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->setSpacing(0);
  auto* title = new QLabel(name);
  title->setObjectName("fileName");
  textLayout->addWidget(title);
  if (!sub.isEmpty()) {
    auto* detail = new QLabel(sub);
    detail->setObjectName("note");
    textLayout->addWidget(detail);
  }
  layout->addWidget(icon);
  layout->addWidget(text, 1);
  if (nameLabel != nullptr) *nameLabel = title;
  return cell;
}

QLineEdit* MainWindow::timeEdit() {
  auto* edit = new QLineEdit;
  edit->setObjectName("time");
  edit->setPlaceholderText(u8(text::kNotFound));
  edit->setAlignment(Qt::AlignCenter);
  edit->setFixedHeight(30);
  return edit;
}

void MainWindow::applyStyle() {
  setStyleSheet(QString::fromUtf8(R"(
    QMainWindow, QWidget { background: #14171b; color: #e8eaec; }
    QMenu { background: #1a1e23; color: #e8eaec; border: 1px solid #2b3138; }
    QMenu::item { padding: 6px 28px 6px 16px; background: transparent; }
    QMenu::item:selected { background: #2a3038; }
    QFrame#panel { background: #1a1e23; border: 1px solid #2b3138; border-radius: 10px; }
    QLabel#caption { color: #98a0a9; font-size: 12px; background: transparent; }
    QLabel#progressLine { color: #98a0a9; font-size: 12px; background: transparent; }
    QLabel#note { color: #6b727b; font-size: 12px; }
    QLabel#body { color: #c9ced4; }
    QLabel#path { background: #20242a; border: 1px solid #2b3138; border-radius: 7px; padding: 6px 10px; color: #c9ced4; }
    QLabel#message { color: #f0a19d; }
    QLabel#banner { color: #ecd29a; background: rgba(224,179,79,0.10); border: 1px solid rgba(224,179,79,0.35); border-radius: 8px; padding: 8px 12px; }
    QLabel#stepBadge { border-radius: 11px; font-weight: 600; }
    QLabel#stepBadge[state="on"] { background: #4f8cff; color: white; }
    QLabel#stepBadge[state="done"] { background: rgba(69,201,122,0.15); color: #45c97a; border: 1px solid rgba(69,201,122,0.45); }
    QLabel#stepBadge[state="off"] { background: #20242a; color: #98a0a9; border: 1px solid #343b44; }
    QLabel#stepText[state="on"] { color: #e8eaec; font-weight: 600; }
    QLabel#stepText[state="off"], QLabel#stepText[state="done"] { color: #98a0a9; }
    QFrame#stepLine { background: #343b44; border: none; }
    QCheckBox { color: #e8eaec; spacing: 6px; background: transparent; }
    QPushButton:hover { background: #2a3038; }
    QPushButton:disabled { color: #6b727b; background: #1b1f24; }
    QPushButton#primary { background: #3f7ef2; border-color: #3f7ef2; color: white; font-weight: 600; }
    QPushButton#primary:hover { background: #5a94ff; }
    QPushButton#primary:disabled { background: #2a3038; border-color: #343b44; color: #6b727b; }
    QLineEdit#time { background: #20242a; border: 1px solid #2b3138; border-radius: 7px; color: #dfe3e7; padding: 4px 8px; }
    QLineEdit#time:focus { border-color: #4f8cff; }
    QLineEdit#time:disabled { color: #565d66; }
    QLineEdit#time[error="true"] { border: 1px solid #e5645f; }
    QHeaderView::section { background: #161a1f; color: #98a0a9; border: none; border-bottom: 1px solid #2b3138; padding: 8px; font-weight: 600; }
    QTableWidget { background: #1a1e23; border: 1px solid #2b3138; border-radius: 8px; alternate-background-color: #1a1e23; }
    QTableWidget::item { color: #c9ced4; border-bottom: 1px solid #23282e; }
    QLabel#fileIcon { background: #23272d; border: 1px solid #2b3138; border-radius: 6px; color: #c8cdd2; }
    QLabel#fileName { color: #e8eaec; font-weight: 600; }
    QLabel#fileName[dim="true"] { color: #6b727b; }
    QWidget#chip { border-radius: 11px; background: #23272d; border: 1px solid #2b3138; }
    QWidget#chip QLabel { background: transparent; border: none; color: #d3d7db; font-size: 12px; }
    QLabel#chipDot { border-radius: 3px; background: #6b727b; }
    QWidget#chip[kind="muted"] QLabel#chipText { color: #9aa1a8; }
    QWidget#chip[kind="muted"] QLabel#chipDot { background: #9aa1a8; }
    QWidget#chip[kind="std"] { background: rgba(79,140,255,0.12); border-color: rgba(79,140,255,0.4); }
    QWidget#chip[kind="std"] QLabel#chipText { color: #a9c6ff; }
    QWidget#chip[kind="std"] QLabel#chipDot { background: #4f8cff; }
    QWidget#chip[kind="chap"] { background: rgba(106,208,255,0.10); border-color: rgba(106,208,255,0.35); }
    QWidget#chip[kind="chap"] QLabel#chipText { color: #9fdcff; }
    QWidget#chip[kind="chap"] QLabel#chipDot { background: #6ad0ff; }
    QWidget#chip[kind="audio"] { background: rgba(143,123,255,0.10); border-color: rgba(143,123,255,0.35); }
    QWidget#chip[kind="audio"] QLabel#chipText { color: #bfb1ff; }
    QWidget#chip[kind="audio"] QLabel#chipDot { background: #8f7bff; }
    QWidget#chip[kind="black"] { background: rgba(154,161,168,0.10); border-color: rgba(154,161,168,0.35); }
    QWidget#chip[kind="black"] QLabel#chipText { color: #c3c8ce; }
    QWidget#chip[kind="black"] QLabel#chipDot { background: #9aa1a8; }
    QWidget#chip[kind="both"] { background: rgba(143,123,255,0.10); border-color: rgba(143,123,255,0.35); }
    QWidget#chip[kind="both"] QLabel#chipText { color: #c3bcdd; }
    QWidget#chip[kind="both"] QLabel#chipDot { background: #8f7bff; }
    QWidget#chip[kind="manual"] { background: rgba(224,179,79,0.10); border-color: rgba(224,179,79,0.35); }
    QWidget#chip[kind="manual"] QLabel#chipText { color: #ecd29a; }
    QWidget#chip[kind="manual"] QLabel#chipDot { background: #e0b34f; }
    QWidget#chip[kind="none"] QLabel#chipText { color: #7d848d; }
    QWidget#chip[kind="none"] QLabel#chipDot { background: #7d848d; }
    QWidget#chip[kind="ok"] { background: rgba(69,201,122,0.10); border-color: rgba(69,201,122,0.35); }
    QWidget#chip[kind="ok"] QLabel#chipText { color: #8fe0b2; }
    QWidget#chip[kind="ok"] QLabel#chipDot { background: #45c97a; }
    QWidget#chip[kind="warn"] { background: rgba(224,179,79,0.10); border-color: rgba(224,179,79,0.35); }
    QWidget#chip[kind="warn"] QLabel#chipText { color: #ecd29a; }
    QWidget#chip[kind="warn"] QLabel#chipDot { background: #e0b34f; }
    QWidget#chip[kind="danger"] { background: rgba(229,100,95,0.10); border-color: rgba(229,100,95,0.35); }
    QWidget#chip[kind="danger"] QLabel#chipText { color: #f0a19d; }
    QWidget#chip[kind="danger"] QLabel#chipDot { background: #e5645f; }
    QWidget#chip[kind="info"] { background: rgba(79,140,255,0.12); border-color: rgba(79,140,255,0.4); }
    QWidget#chip[kind="info"] QLabel#chipText { color: #a9c6ff; }
    QWidget#chip[kind="info"] QLabel#chipDot { background: #4f8cff; }
    QWidget#chip[kind="plain"] QLabel#chipDot { background: transparent; }
    QProgressBar { background: #1b1f24; border: 1px solid #2b3138; border-radius: 5px; }
    QProgressBar::chunk { background: #4f8cff; border-radius: 4px; }
    QProgressBar#done::chunk { background: #45c97a; }
    QRadioButton::indicator, QCheckBox::indicator { width: 16px; height: 16px; }
    QRadioButton::indicator:checked { background: #4f8cff; border: 4px solid #4f8cff; border-radius: 8px; }
    QRadioButton::indicator:unchecked { background: transparent; border: 1px solid #4a525c; border-radius: 8px; }
    QCheckBox::indicator:checked { background: #4f8cff; border: 1px solid #4f8cff; border-radius: 4px; }
    QCheckBox::indicator:unchecked { background: transparent; border: 1px solid #4a525c; border-radius: 4px; }
    QScrollBar:vertical { background: #14171b; width: 10px; }
    QScrollBar::handle:vertical { background: #343b44; border-radius: 4px; }
  )"));
}

}  // namespace skipper
