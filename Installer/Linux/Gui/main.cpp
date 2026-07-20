#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTableWidget>
#include <QTemporaryFile>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>

namespace {

struct Candidate final {
  QString Name;
  QString LoaderPath;
  QString Icon;
};

[[nodiscard]] QString DetectEspRoot() {
  constexpr const char* kCandidates[] = {"/boot/efi", "/efi", "/boot"};
  for (const char* CandidatePath : kCandidates) {
    const QDir Directory(QString::fromUtf8(CandidatePath));
    if (Directory.exists("EFI")) {
      return Directory.absolutePath();
    }
  }
  return {};
}

[[nodiscard]] Candidate RecognizeLoader(
    const QString& EspRoot,
    const QString& AbsolutePath) {
  QString Relative = QDir(EspRoot).relativeFilePath(AbsolutePath);
  Relative.replace('/', '\\');
  const QString Lower = Relative.toLower();
  Candidate Result;
  Result.LoaderPath = QStringLiteral("\\") + Relative;
  Result.Icon = QStringLiteral("generic");

  if (Lower.contains(QStringLiteral("microsoft\\boot\\bootmgfw.efi"))) {
    Result.Name = QStringLiteral("WINDOWS BOOT MANAGER");
    Result.Icon = QStringLiteral("windows");
  } else if (Lower.contains(QStringLiteral("\\kali\\"))) {
    Result.Name = QStringLiteral("KALI LINUX");
    Result.Icon = QStringLiteral("kali");
  } else if (Lower.contains(QStringLiteral("\\blackarch_linux\\")) ||
             Lower.contains(QStringLiteral("\\blackarch\\"))) {
    Result.Name = QStringLiteral("BLACKARCH LINUX");
    Result.Icon = QStringLiteral("blackarch");
  } else {
    const QStringList Components = Relative.split('\\');
    QString Vendor = (Components.size() > 1) ? Components.at(1)
                                             : QStringLiteral("UEFI");
    Vendor = Vendor.toUpper();
    Result.Name = Vendor + QStringLiteral(" EFI SYSTEM");
    if (Lower.endsWith(QStringLiteral("grubx64.efi")) ||
        Lower.endsWith(QStringLiteral("shimx64.efi")) ||
        Lower.contains(QStringLiteral("systemd"))) {
      Result.Icon = QStringLiteral("linux");
      Result.Name = Vendor + QStringLiteral(" LINUX");
    }
  }
  return Result;
}

[[nodiscard]] QList<Candidate> ScanEsp(const QString& EspRoot) {
  QList<Candidate> Results;
  const QDir EfiDirectory(QDir(EspRoot).filePath(QStringLiteral("EFI")));
  if (!EfiDirectory.exists()) {
    return Results;
  }

  QDirIterator Iterator(
      EfiDirectory.absolutePath(),
      QDir::Files,
      QDirIterator::Subdirectories);
  while (Iterator.hasNext()) {
    const QString Path = Iterator.next();
    if (!Path.endsWith(QStringLiteral(".efi"), Qt::CaseInsensitive)) {
      continue;
    }
    const QString Lower = Path.toLower();
    if (Lower.contains(QStringLiteral("/efi/apex32/"))) {
      continue;
    }

    const Candidate CandidateEntry = RecognizeLoader(EspRoot, Path);
    const auto ExistingPath = std::find_if(
        Results.cbegin(),
        Results.cend(),
        [&CandidateEntry](const Candidate& Item) {
          return Item.LoaderPath.compare(
                     CandidateEntry.LoaderPath, Qt::CaseInsensitive) == 0;
        });
    if (ExistingPath != Results.cend()) {
      continue;
    }

    const auto ExistingSystem = std::find_if(
        Results.begin(),
        Results.end(),
        [&CandidateEntry](const Candidate& Item) {
          return Item.Name.compare(CandidateEntry.Name, Qt::CaseInsensitive) == 0;
        });
    if (ExistingSystem == Results.end()) {
      Results.push_back(CandidateEntry);
    } else if (CandidateEntry.LoaderPath.endsWith(
                   QStringLiteral("shimx64.efi"), Qt::CaseInsensitive) &&
               !ExistingSystem->LoaderPath.endsWith(
                   QStringLiteral("shimx64.efi"), Qt::CaseInsensitive)) {
      *ExistingSystem = CandidateEntry;
    }
  }

  std::sort(
      Results.begin(),
      Results.end(),
      [](const Candidate& Left, const Candidate& Right) {
        return Left.Name < Right.Name;
      });
  return Results;
}

[[nodiscard]] QString SafeConfigField(QString Text) {
  Text.replace('|', ' ');
  Text.replace('\n', ' ');
  Text.replace('\r', ' ');
  return Text.trimmed().toUpper();
}

class InstallerWindow final : public QWidget {
 public:
  InstallerWindow() {
    setWindowTitle(QStringLiteral("APEX32 Community Installer"));
    resize(900, 620);

    auto* Layout = new QVBoxLayout(this);
    auto* Brand = new QLabel(QStringLiteral("APEX32 SECURE GATEWAY"));
    QFont BrandFont = Brand->font();
    BrandFont.setPointSize(22);
    BrandFont.setBold(true);
    Brand->setFont(BrandFont);
    Brand->setAlignment(Qt::AlignCenter);
    Layout->addWidget(Brand);

    auto* Website = new QLabel(
        QStringLiteral("apex32-secure.com  //  GPL-3.0 Community Edition"));
    Website->setAlignment(Qt::AlignCenter);
    Layout->addWidget(Website);

    Tabs_ = new QTabWidget;
    Layout->addWidget(Tabs_);
    BuildSystemsTab();
    BuildInstallTab();
    BuildRecoveryTab();
    BuildAboutTab();

    EspRoot_ = DetectEspRoot();
    RefreshScan();
  }

 private:
  void BuildSystemsTab() {
    auto* Tab = new QWidget;
    auto* Layout = new QVBoxLayout(Tab);
    EspLabel_ = new QLabel;
    Layout->addWidget(EspLabel_);

    auto* ScanButton = new QPushButton(QStringLiteral("Scan Now"));
    ScanButton->setMinimumHeight(44);
    connect(ScanButton, &QPushButton::clicked, this, [this]() {
      if (EspRoot_.isEmpty()) {
        EspRoot_ = QFileDialog::getExistingDirectory(
            this,
            QStringLiteral("Select the EFI System Partition mount point"));
      }
      RefreshScan();
    });
    Layout->addWidget(ScanButton);

    Table_ = new QTableWidget;
    Table_->setColumnCount(4);
    Table_->setHorizontalHeaderLabels(
        {QStringLiteral("Use"),
         QStringLiteral("Operating System"),
         QStringLiteral("EFI Loader"),
         QStringLiteral("Icon")});
    Table_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    Table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    Table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    Table_->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    Layout->addWidget(Table_);
    Tabs_->addTab(Tab, QStringLiteral("Systems"));
  }

  void BuildInstallTab() {
    auto* Tab = new QWidget;
    auto* Layout = new QVBoxLayout(Tab);
    auto* Explanation = new QLabel(QStringLiteral(
        "APEX32 will copy the verified firmware application and generated "
        "system configuration to the selected EFI System Partition. "
        "A recovery backup is created before any existing APEX32 file is "
        "replaced."));
    Explanation->setWordWrap(true);
    Layout->addWidget(Explanation);

    auto* DefaultChoice = new QCheckBox(
        QStringLiteral("Make APEX32 Secure Gateway the default boot manager"));
    DefaultChoice->setChecked(true);
    DefaultChoice->setEnabled(false);
    Layout->addWidget(DefaultChoice);

    auto* InstallButton = new QPushButton(
        QStringLiteral("Install APEX32 and Make Default"));
    InstallButton->setMinimumHeight(52);
    connect(InstallButton, &QPushButton::clicked, this, [this]() {
      InstallSelected();
    });
    Layout->addWidget(InstallButton);
    InstallStatus_ = new QLabel(QStringLiteral("Ready for configuration."));
    InstallStatus_->setWordWrap(true);
    Layout->addWidget(InstallStatus_);
    Layout->addStretch();
    Tabs_->addTab(Tab, QStringLiteral("Install"));
  }

  void BuildRecoveryTab() {
    auto* Tab = new QWidget;
    auto* Layout = new QVBoxLayout(Tab);
    auto* Text = new QLabel(QStringLiteral(
        "The installer preserves the previous APEX32 firmware file before "
        "replacement. Recovery and uninstall actions will be enabled after "
        "the first hardware-tested Community package."));
    Text->setWordWrap(true);
    Layout->addWidget(Text);
    Layout->addStretch();
    Tabs_->addTab(Tab, QStringLiteral("Recovery"));
  }

  void BuildAboutTab() {
    auto* Tab = new QWidget;
    auto* Layout = new QVBoxLayout(Tab);
    auto* Text = new QLabel(QStringLiteral(
        "APEX32 Boot Manager Community Edition\n\n"
        "Copyright (C) 2026 Oussama / APEX32 Secure\n"
        "Licensed under GNU GPL version 3.\n\n"
        "Project and documentation: https://apex32-secure.com"));
    Text->setWordWrap(true);
    Layout->addWidget(Text);
    Layout->addStretch();
    Tabs_->addTab(Tab, QStringLiteral("About"));
  }

  void RefreshScan() {
    Candidates_.clear();
    Table_->setRowCount(0);
    if (EspRoot_.isEmpty()) {
      EspLabel_->setText(QStringLiteral(
          "EFI System Partition not detected. Select Scan Now to choose it."));
      return;
    }

    Candidates_ = ScanEsp(EspRoot_);
    EspLabel_->setText(
        QStringLiteral("EFI System Partition: %1  //  %2 loaders found")
            .arg(EspRoot_)
            .arg(Candidates_.size()));
    Table_->setRowCount(Candidates_.size());
    for (qsizetype Row = 0; Row < Candidates_.size(); ++Row) {
      auto* Enabled = new QCheckBox;
      Enabled->setChecked(Row < 8);
      Enabled->setStyleSheet(QStringLiteral("margin-left: 12px"));
      Table_->setCellWidget(static_cast<int>(Row), 0, Enabled);
      Table_->setItem(
          static_cast<int>(Row),
          1,
          new QTableWidgetItem(Candidates_.at(Row).Name));
      Table_->setItem(
          static_cast<int>(Row),
          2,
          new QTableWidgetItem(Candidates_.at(Row).LoaderPath));
      Table_->setItem(
          static_cast<int>(Row),
          3,
          new QTableWidgetItem(Candidates_.at(Row).Icon));
    }
  }

  [[nodiscard]] QByteArray BuildConfiguration() const {
    QByteArray Configuration("APEX32CFG|1\n");
    for (qsizetype Row = 0; Row < Candidates_.size(); ++Row) {
      const auto* Enabled = qobject_cast<QCheckBox*>(
          Table_->cellWidget(static_cast<int>(Row), 0));
      if ((Enabled == nullptr) || !Enabled->isChecked()) {
        continue;
      }
      const Candidate& Entry = Candidates_.at(Row);
      Configuration += "ENTRY|";
      Configuration += SafeConfigField(Entry.Name).toUtf8();
      Configuration += '|';
      Configuration += Entry.LoaderPath.toUtf8();
      Configuration += '|';
      Configuration += Entry.Icon.toUtf8();
      Configuration += '\n';
    }
    return Configuration;
  }

  void InstallSelected() {
    if (EspRoot_.isEmpty() || Candidates_.isEmpty()) {
      QMessageBox::warning(
          this,
          QStringLiteral("No systems found"),
          QStringLiteral("Select Systems, then choose Scan Now first."));
      return;
    }

    int SelectedCount = 0;
    for (int Row = 0; Row < Table_->rowCount(); ++Row) {
      const auto* Enabled = qobject_cast<QCheckBox*>(Table_->cellWidget(Row, 0));
      if ((Enabled != nullptr) && Enabled->isChecked()) {
        ++SelectedCount;
      }
    }
    if (SelectedCount > 8) {
      QMessageBox::warning(
          this,
          QStringLiteral("Too many systems selected"),
          QStringLiteral("Schema 1 supports up to eight systems. Uncheck at least one entry."));
      return;
    }

    QTemporaryFile ConfigurationFile(
        QDir::tempPath() + QStringLiteral("/apex32-config-XXXXXX.cfg"));
    ConfigurationFile.setAutoRemove(false);
    if (!ConfigurationFile.open() ||
        (ConfigurationFile.write(BuildConfiguration()) < 0) ||
        !ConfigurationFile.flush()) {
      QMessageBox::critical(
          this,
          QStringLiteral("Configuration error"),
          QStringLiteral("The temporary APEX32 configuration could not be created."));
      return;
    }
    const QString ConfigurationPath = ConfigurationFile.fileName();
    ConfigurationFile.close();

    const QString Helper = QStringLiteral(
        "/usr/libexec/apex32/apex32-installer-helper");
    const QString Firmware = QStringLiteral(
        "/usr/share/apex32/Apex32BootManager.efi");
    QProcess Process;
    Process.start(
        QStringLiteral("/usr/bin/pkexec"),
        {Helper,
         QStringLiteral("install"),
         EspRoot_,
         Firmware,
         ConfigurationPath});
    Process.waitForFinished(-1);
    QFile::remove(ConfigurationPath);

    if ((Process.exitStatus() == QProcess::NormalExit) &&
        (Process.exitCode() == 0)) {
      InstallStatus_->setText(QStringLiteral(
          "APEX32 is installed and first in the UEFI boot order. "
          "Restart when ready."));
      QMessageBox::information(
          this,
          QStringLiteral("APEX32 installed"),
          InstallStatus_->text());
    } else {
      const QString Error = QString::fromUtf8(Process.readAllStandardError());
      InstallStatus_->setText(
          QStringLiteral("Installation failed: %1").arg(Error));
      QMessageBox::critical(
          this, QStringLiteral("Installation failed"), InstallStatus_->text());
    }
  }

  QTabWidget* Tabs_ = nullptr;
  QLabel* EspLabel_ = nullptr;
  QLabel* InstallStatus_ = nullptr;
  QTableWidget* Table_ = nullptr;
  QString EspRoot_;
  QList<Candidate> Candidates_;
};

}  // namespace

int main(int argc, char** argv) {
  QApplication Application(argc, argv);
  Application.setApplicationName(QStringLiteral("APEX32 Community Installer"));
  Application.setOrganizationName(QStringLiteral("APEX32 Secure"));
  InstallerWindow Window;
  Window.show();
  return Application.exec();
}
