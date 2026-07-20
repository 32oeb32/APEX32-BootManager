#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
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
#include <QTextStream>
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
  constexpr const char* kPrimaryCandidates[] = {"/boot/efi", "/efi"};
  for (const char* CandidatePath : kPrimaryCandidates) {
    const QDir Directory(QString::fromUtf8(CandidatePath));
    if (Directory.exists()) {
      return Directory.absolutePath();
    }
  }

  constexpr const char* kFallbackCandidates[] = {"/boot"};
  for (const char* CandidatePath : kFallbackCandidates) {
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

void AppendCandidate(
    QList<Candidate>* Results,
    const Candidate& CandidateEntry) {
  const auto ExistingPath = std::find_if(
      Results->cbegin(),
      Results->cend(),
      [&CandidateEntry](const Candidate& Item) {
        return Item.LoaderPath.compare(
                   CandidateEntry.LoaderPath, Qt::CaseInsensitive) == 0;
      });
  if (ExistingPath != Results->cend()) {
    return;
  }

  const auto ExistingSystem = std::find_if(
      Results->begin(),
      Results->end(),
      [&CandidateEntry](const Candidate& Item) {
        return Item.Name.compare(
                   CandidateEntry.Name, Qt::CaseInsensitive) == 0;
      });
  if (ExistingSystem == Results->end()) {
    Results->push_back(CandidateEntry);
  } else if (CandidateEntry.LoaderPath.endsWith(
                 QStringLiteral("shimx64.efi"), Qt::CaseInsensitive) &&
             !ExistingSystem->LoaderPath.endsWith(
                 QStringLiteral("shimx64.efi"), Qt::CaseInsensitive)) {
    *ExistingSystem = CandidateEntry;
  }
}

void SortCandidates(QList<Candidate>* Results) {
  std::sort(
      Results->begin(),
      Results->end(),
      [](const Candidate& Left, const Candidate& Right) {
        return Left.Name < Right.Name;
      });
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

    AppendCandidate(&Results, RecognizeLoader(EspRoot, Path));
  }

  SortCandidates(&Results);
  return Results;
}

[[nodiscard]] QString ResolveHelperPath() {
  const QString Installed = QStringLiteral(
      "/usr/libexec/apex32/apex32-installer-helper");
  if (QFileInfo(Installed).isExecutable()) {
    return Installed;
  }

  const QString Development = QDir(
      QCoreApplication::applicationDirPath())
                                  .filePath(
                                      QStringLiteral(
                                          "apex32-installer-helper"));
  if (QFileInfo(Development).isExecutable()) {
    return Development;
  }
  return {};
}

[[nodiscard]] QList<Candidate> ParseScanProtocol(
    const QString& EspRoot,
    const QByteArray& Protocol,
    QString* Error) {
  const QList<QByteArray> Lines = Protocol.split('\n');
  if (Lines.isEmpty() || Lines.first().trimmed() != "APEX32SCAN|1") {
    *Error = QStringLiteral("authorized scanner returned an invalid response");
    return {};
  }

  QList<Candidate> Results;
  for (qsizetype Index = 1; Index < Lines.size(); ++Index) {
    const QByteArray Line = Lines.at(Index).trimmed();
    if (Line.isEmpty()) {
      continue;
    }
    if (!Line.startsWith("LOADER|")) {
      *Error = QStringLiteral("authorized scanner returned an unknown record");
      return {};
    }

    const QString LoaderPath = QString::fromUtf8(Line.mid(7));
    if (!LoaderPath.startsWith(QStringLiteral("\\EFI\\"),
                               Qt::CaseInsensitive) ||
        !LoaderPath.endsWith(QStringLiteral(".efi"),
                             Qt::CaseInsensitive) ||
        LoaderPath.size() > 512 || LoaderPath.contains('|') ||
        LoaderPath.contains(QStringLiteral(".."))) {
      *Error = QStringLiteral("authorized scanner returned an unsafe loader path");
      return {};
    }

    QString Relative = LoaderPath.mid(1);
    Relative.replace('\\', '/');
    const QString Absolute = QDir(EspRoot).absoluteFilePath(Relative);
    AppendCandidate(&Results, RecognizeLoader(EspRoot, Absolute));
  }
  SortCandidates(&Results);
  return Results;
}

[[nodiscard]] QList<Candidate> ScanEspAuthorized(
    const QString& EspRoot,
    QString* Error) {
  const QString Helper = ResolveHelperPath();
  if (Helper.isEmpty()) {
    *Error = QStringLiteral(
        "installer helper not found; rebuild or install the complete package");
    return {};
  }
  if (!QFileInfo(QStringLiteral("/usr/bin/pkexec")).isExecutable()) {
    *Error = QStringLiteral("graphical authorization service is unavailable");
    return {};
  }

  QProcess Process;
  Process.start(
      QStringLiteral("/usr/bin/pkexec"),
      {Helper, QStringLiteral("scan"), EspRoot});
  if (!Process.waitForStarted(5000)) {
    *Error = QStringLiteral("could not start the graphical authorization prompt");
    return {};
  }
  if (!Process.waitForFinished(120000)) {
    Process.kill();
    Process.waitForFinished();
    *Error = QStringLiteral("authorized scan timed out");
    return {};
  }
  if ((Process.exitStatus() != QProcess::NormalExit) ||
      (Process.exitCode() != 0)) {
    *Error = QString::fromUtf8(Process.readAllStandardError()).trimmed();
    if (Error->isEmpty()) {
      *Error = QStringLiteral("authorization was cancelled or scanning failed");
    }
    return {};
  }
  return ParseScanProtocol(EspRoot, Process.readAllStandardOutput(), Error);
}

[[nodiscard]] QString SafeConfigField(QString Text) {
  Text.replace('|', ' ');
  Text.replace('\n', ' ');
  Text.replace('\r', ' ');
  return Text.trimmed().toUpper();
}

[[nodiscard]] QByteArray BuildConfigurationFor(
    const QList<Candidate>& Candidates) {
  QByteArray Configuration("APEX32CFG|1\n");
  for (const Candidate& Entry : Candidates) {
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

[[nodiscard]] bool HasCandidate(
    const QList<Candidate>& Candidates,
    const QString& Name,
    const QString& LoaderSuffix,
    const QString& Icon) {
  return std::any_of(
      Candidates.cbegin(),
      Candidates.cend(),
      [&Name, &LoaderSuffix, &Icon](const Candidate& Entry) {
        return Entry.Name == Name && Entry.Icon == Icon &&
               Entry.LoaderPath.endsWith(
                   LoaderSuffix, Qt::CaseInsensitive);
      });
}

[[nodiscard]] int RunInstallerSelfTest(const QString& EspArgument) {
  QTextStream Output(stdout);
  QTextStream Error(stderr);
  const QString EspRoot = QFileInfo(EspArgument).canonicalFilePath();
  if (EspRoot.isEmpty() || !QDir(EspRoot).exists(QStringLiteral("EFI"))) {
    Error << "SELF-TEST FAIL: invalid mock ESP\n";
    return 2;
  }

  const QList<Candidate> Candidates = ScanEsp(EspRoot);
  const bool ExpectedCandidates =
      Candidates.size() == 4 &&
      HasCandidate(
          Candidates,
          QStringLiteral("KALI LINUX"),
          QStringLiteral("\\EFI\\kali\\grubx64.efi"),
          QStringLiteral("kali")) &&
      HasCandidate(
          Candidates,
          QStringLiteral("UBUNTU LINUX"),
          QStringLiteral("\\EFI\\ubuntu\\shimx64.efi"),
          QStringLiteral("linux")) &&
      HasCandidate(
          Candidates,
          QStringLiteral("WINDOWS BOOT MANAGER"),
          QStringLiteral("\\EFI\\Microsoft\\Boot\\bootmgfw.efi"),
          QStringLiteral("windows")) &&
      HasCandidate(
          Candidates,
          QStringLiteral("TOOLS EFI SYSTEM"),
          QStringLiteral("\\EFI\\tools\\shellx64.efi"),
          QStringLiteral("generic"));
  if (!ExpectedCandidates) {
    Error << "SELF-TEST FAIL: discovery or loader recognition mismatch\n";
    for (const Candidate& Entry : Candidates) {
      Error << Entry.Name << '|' << Entry.LoaderPath << '|' << Entry.Icon
            << '\n';
    }
    return 3;
  }

  const QByteArray Configuration = BuildConfigurationFor(Candidates);
  if (!Configuration.startsWith("APEX32CFG|1\n") ||
      Configuration.contains("APEX32\\Apex32BootManager.efi") ||
      Configuration.count('\n') != Candidates.size() + 1) {
    Error << "SELF-TEST FAIL: generated configuration is invalid\n";
    return 4;
  }

  const QByteArray AuthorizedProtocol(
      "APEX32SCAN|1\n"
      "LOADER|\\EFI\\Microsoft\\Boot\\bootmgfw.efi\n"
      "LOADER|\\EFI\\kali\\grubx64.efi\n"
      "LOADER|\\EFI\\ubuntu\\grubx64.efi\n"
      "LOADER|\\EFI\\ubuntu\\shimx64.efi\n"
      "LOADER|\\EFI\\tools\\shellx64.efi\n");
  QString ProtocolError;
  const QList<Candidate> AuthorizedCandidates = ParseScanProtocol(
      EspRoot, AuthorizedProtocol, &ProtocolError);
  if (!ProtocolError.isEmpty() || AuthorizedCandidates.size() != 4 ||
      !HasCandidate(
          AuthorizedCandidates,
          QStringLiteral("UBUNTU LINUX"),
          QStringLiteral("\\EFI\\ubuntu\\shimx64.efi"),
          QStringLiteral("linux"))) {
    Error << "SELF-TEST FAIL: authorized scan protocol parsing mismatch\n";
    return 5;
  }

  Output << "PASS: regular-user discovery found " << Candidates.size()
         << " systems, preferred shim, excluded APEX32, parsed authorized scan, "
            "and generated schema 1\n";
  return 0;
}

class InstallerWindow final : public QWidget {
 public:
  InstallerWindow(QString EspOverride, const bool SafeTestMode)
      : SafeTestMode_(SafeTestMode) {
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

    if (SafeTestMode_) {
      auto* TestBanner = new QLabel(QStringLiteral(
          "SAFE REGULAR-USER TEST // REAL INSTALLATION IS DISABLED"));
      TestBanner->setAlignment(Qt::AlignCenter);
      TestBanner->setStyleSheet(QStringLiteral(
          "padding: 9px; color: #35f2e6; background: #10282b; "
          "border: 1px solid #35f2e6; font-weight: 700;"));
      Layout->addWidget(TestBanner);
    }

    Tabs_ = new QTabWidget;
    Layout->addWidget(Tabs_);
    BuildSystemsTab();
    BuildInstallTab();
    BuildRecoveryTab();
    BuildAboutTab();

    EspRoot_ = EspOverride.isEmpty()
                   ? DetectEspRoot()
                   : QFileInfo(EspOverride).canonicalFilePath();
    RefreshScan(false);
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
      if (EspRoot_.isEmpty() && !SafeTestMode_) {
        EspRoot_ = QFileDialog::getExistingDirectory(
            this,
            QStringLiteral("Select the EFI System Partition mount point"));
      }
      RefreshScan(true);
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

    InstallButton_ = new QPushButton(
        QStringLiteral("Install APEX32 and Make Default"));
    InstallButton_->setMinimumHeight(52);
    InstallButton_->setEnabled(!SafeTestMode_);
    connect(InstallButton_, &QPushButton::clicked, this, [this]() {
      InstallSelected();
    });
    Layout->addWidget(InstallButton_);
    InstallStatus_ = new QLabel(
        SafeTestMode_
            ? QStringLiteral(
                  "Safe test mode is active. No ESP files or firmware variables can be changed.")
            : QStringLiteral("Ready for configuration."));
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

  void RefreshScan(const bool AllowAuthorization) {
    Candidates_.clear();
    Table_->setRowCount(0);
    if (EspRoot_.isEmpty()) {
      EspLabel_->setText(QStringLiteral(
          "EFI System Partition not detected. Select Scan Now to choose it."));
      return;
    }

    Candidates_ = ScanEsp(EspRoot_);
    if (Candidates_.isEmpty() && AllowAuthorization && !SafeTestMode_) {
      EspLabel_->setText(QStringLiteral(
          "Waiting for authorization to scan %1 read-only...").arg(EspRoot_));
      QApplication::processEvents();
      QString Error;
      Candidates_ = ScanEspAuthorized(EspRoot_, &Error);
      if (!Error.isEmpty()) {
        EspLabel_->setText(
            QStringLiteral("EFI System Partition: %1  //  scan failed")
                .arg(EspRoot_));
        QMessageBox::warning(
            this,
            QStringLiteral("EFI scan failed"),
            Error);
        return;
      }
    }

    if (Candidates_.isEmpty() && !AllowAuthorization && !SafeTestMode_) {
      EspLabel_->setText(QStringLiteral(
          "EFI System Partition: %1  //  Select Scan Now for read-only authorized discovery")
                            .arg(EspRoot_));
      return;
    }
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
    QList<Candidate> Selected;
    for (qsizetype Row = 0; Row < Candidates_.size(); ++Row) {
      const auto* Enabled = qobject_cast<QCheckBox*>(
          Table_->cellWidget(static_cast<int>(Row), 0));
      if ((Enabled == nullptr) || !Enabled->isChecked()) {
        continue;
      }
      Selected.push_back(Candidates_.at(Row));
    }
    return BuildConfigurationFor(Selected);
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
    if (SelectedCount == 0) {
      QMessageBox::warning(
          this,
          QStringLiteral("No systems selected"),
          QStringLiteral("Select at least one operating system before installation."));
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
  QPushButton* InstallButton_ = nullptr;
  QTableWidget* Table_ = nullptr;
  QString EspRoot_;
  QList<Candidate> Candidates_;
  bool SafeTestMode_ = false;
};

}  // namespace

int main(int argc, char** argv) {
  if ((argc == 3) &&
      (QString::fromLocal8Bit(argv[1]) == QStringLiteral("--self-test"))) {
    QCoreApplication Application(argc, argv);
    return RunInstallerSelfTest(QString::fromLocal8Bit(argv[2]));
  }

  QApplication Application(argc, argv);
  Application.setApplicationName(QStringLiteral("APEX32 Community Installer"));
  Application.setOrganizationName(QStringLiteral("APEX32 Secure"));
  const QString TestEsp = qEnvironmentVariable("APEX32_TEST_ESP");
  InstallerWindow Window(TestEsp, !TestEsp.isEmpty());
  Window.show();
  return Application.exec();
}
