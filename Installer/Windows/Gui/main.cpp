#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSet>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVector>
#include <QVBoxLayout>
#include <QWidget>

#include "Transaction/WindowsFirmwareStore.hpp"
#include "Transaction/WindowsTransaction.hpp"

#include <shellapi.h>
#include <windows.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#ifndef APEX32_ENABLE_HARDWARE_INSTALL
#define APEX32_ENABLE_HARDWARE_INSTALL 0
#endif

#ifndef APEX32_PACKAGED_FIRMWARE_SHA256
#define APEX32_PACKAGED_FIRMWARE_SHA256 ""
#endif

namespace {

constexpr int kMaximumLoaders = 64;
constexpr int kMaximumSelectedLoaders = 32;
constexpr bool kHardwareInstallEnabled =
    APEX32_ENABLE_HARDWARE_INSTALL != 0;

struct Loader {
  QString Name;
  QString Path;
  QString Icon;
  bool DefaultSelected = true;
};

bool IsAdministrator() {
  BOOL IsMember = FALSE;
  PSID AdministratorsGroup = nullptr;
  SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
  if (AllocateAndInitializeSid(
          &NtAuthority,
          2,
          SECURITY_BUILTIN_DOMAIN_RID,
          DOMAIN_ALIAS_RID_ADMINS,
          0,
          0,
          0,
          0,
          0,
          0,
          &AdministratorsGroup) == FALSE) {
    return false;
  }
  const BOOL Checked = CheckTokenMembership(
      nullptr, AdministratorsGroup, &IsMember);
  FreeSid(AdministratorsGroup);
  return Checked != FALSE && IsMember != FALSE;
}

QString WindowsTool(const QString &Name) {
  const QString WindowsDirectory = qEnvironmentVariable("SystemRoot");
  return QDir::toNativeSeparators(
      QDir(WindowsDirectory).filePath(QStringLiteral("System32/") + Name));
}

bool RunTool(
    const QString &Program,
    const QStringList &Arguments,
    QString *Error) {
  QProcess Process;
  Process.start(Program, Arguments);
  if (!Process.waitForStarted(10000) || !Process.waitForFinished(30000)) {
    if (Error != nullptr) {
      *Error = QStringLiteral("The Windows system tool did not finish.");
    }
    Process.kill();
    Process.waitForFinished();
    return false;
  }
  if (Process.exitStatus() != QProcess::NormalExit || Process.exitCode() != 0) {
    if (Error != nullptr) {
      *Error = QString::fromLocal8Bit(Process.readAllStandardError()).trimmed();
      if (Error->isEmpty()) {
        *Error =
            QString::fromLocal8Bit(Process.readAllStandardOutput()).trimmed();
      }
      if (Error->isEmpty()) {
        *Error = QStringLiteral("The Windows system tool rejected the request.");
      }
    }
    return false;
  }
  return true;
}

QString FreeDriveLetter() {
  for (wchar_t Letter = L'Z'; Letter >= L'P'; --Letter) {
    wchar_t Root[] = {Letter, L':', L'\\', L'\0'};
    if (GetDriveTypeW(Root) == DRIVE_NO_ROOT_DIR) {
      return QString(QChar(Letter)) + QStringLiteral(":");
    }
  }
  return {};
}

class EspMount final {
 public:
  EspMount() = default;
  EspMount(const EspMount &) = delete;
  EspMount &operator=(const EspMount &) = delete;

  ~EspMount() {
    Unmount();
  }

  bool Mount(QString *Error) {
    Drive_ = FreeDriveLetter();
    if (Drive_.isEmpty()) {
      if (Error != nullptr) {
        *Error = QStringLiteral("No temporary drive letter is available.");
      }
      return false;
    }
    if (!RunTool(
            WindowsTool(QStringLiteral("mountvol.exe")),
            {Drive_, QStringLiteral("/S")},
            Error)) {
      Drive_.clear();
      return false;
    }
    Mounted_ = true;
    if (!QFileInfo(EfiDirectory()).isDir()) {
      if (Error != nullptr) {
        *Error = QStringLiteral("The mounted system partition has no EFI directory.");
      }
      Unmount();
      return false;
    }
    return true;
  }

  QString RootDirectory() const {
    return Drive_ + QStringLiteral("\\");
  }

  QString EfiDirectory() const {
    return Drive_ + QStringLiteral("\\EFI");
  }

 private:
  void Unmount() {
    if (!Mounted_) {
      return;
    }
    QString Ignored;
    RunTool(
        WindowsTool(QStringLiteral("mountvol.exe")),
        {Drive_, QStringLiteral("/D")},
        &Ignored);
    Mounted_ = false;
    Drive_.clear();
  }

  QString Drive_;
  bool Mounted_ = false;
};

QString FriendlyName(const QString &RelativePath) {
  const QString Lower = RelativePath.toLower();
  if (Lower.contains(QStringLiteral("\\microsoft\\"))) {
    return QStringLiteral("WINDOWS BOOT MANAGER");
  }
  if (Lower.contains(QStringLiteral("\\kali\\"))) {
    return QStringLiteral("KALI LINUX");
  }
  if (Lower.contains(QStringLiteral("blackarch"))) {
    return QStringLiteral("BLACKARCH LINUX");
  }
  if (Lower.contains(QStringLiteral("\\ubuntu\\"))) {
    return QStringLiteral("UBUNTU");
  }
  if (Lower.contains(QStringLiteral("\\fedora\\"))) {
    return QStringLiteral("FEDORA");
  }
  if (Lower.contains(QStringLiteral("\\debian\\"))) {
    return QStringLiteral("DEBIAN");
  }
  if (Lower.contains(QStringLiteral("\\linuxmint\\")) ||
      Lower.contains(QStringLiteral("\\mint\\"))) {
    return QStringLiteral("LINUX MINT");
  }
  if (Lower.contains(QStringLiteral("\\opensuse\\")) ||
      Lower.contains(QStringLiteral("\\suse\\"))) {
    return QStringLiteral("OPENSUSE");
  }
  if (Lower.contains(QStringLiteral("\\pop_os\\")) ||
      Lower.contains(QStringLiteral("\\pop-os\\"))) {
    return QStringLiteral("POP!_OS");
  }
  if (Lower.contains(QStringLiteral("\\opencore\\"))) {
    return QStringLiteral("OPENCORE");
  }
  if (Lower.contains(QStringLiteral("\\arch\\"))) {
    return QStringLiteral("ARCH LINUX");
  }
  if (Lower.endsWith(QStringLiteral("shimx64.efi")) ||
      Lower.endsWith(QStringLiteral("grubx64.efi"))) {
    return QStringLiteral("LINUX");
  }
  if (Lower.endsWith(QStringLiteral("bootx64.efi"))) {
    return QStringLiteral("UEFI FALLBACK (RECOVERY)");
  }
  return QStringLiteral("UEFI APPLICATION");
}

QString IconName(const QString &Name) {
  const QString Lower = Name.toLower();
  if (Lower.contains(QStringLiteral("windows"))) return QStringLiteral("windows");
  if (Lower.contains(QStringLiteral("kali"))) return QStringLiteral("kali");
  if (Lower.contains(QStringLiteral("blackarch"))) return QStringLiteral("blackarch");
  if (Lower.contains(QStringLiteral("ubuntu"))) return QStringLiteral("ubuntu");
  if (Lower.contains(QStringLiteral("fedora"))) return QStringLiteral("fedora");
  if (Lower.contains(QStringLiteral("debian"))) return QStringLiteral("debian");
  if (Lower.contains(QStringLiteral("mint"))) return QStringLiteral("mint");
  if (Lower.contains(QStringLiteral("opensuse"))) return QStringLiteral("opensuse");
  if (Lower.contains(QStringLiteral("pop!"))) return QStringLiteral("popos");
  if (Lower.contains(QStringLiteral("opencore"))) return QStringLiteral("opencore");
  if (Lower.contains(QStringLiteral("arch"))) return QStringLiteral("arch");
  if (Lower.contains(QStringLiteral("recovery"))) return QStringLiteral("recovery");
  if (Lower.contains(QStringLiteral("linux"))) return QStringLiteral("linux");
  return QStringLiteral("generic");
}

bool SafeConfigurationField(const QString &Value, int Maximum) {
  if (Value.isEmpty() || Value.size() > Maximum || Value.contains(QLatin1Char('|'))) {
    return false;
  }
  for (const QChar Character : Value) {
    const ushort Code = Character.unicode();
    if (Code < 0x20 || Code > 0x7e) {
      return false;
    }
  }
  return true;
}

QString PackagedFirmwarePath() {
  return QDir::cleanPath(
      QDir(QCoreApplication::applicationDirPath())
          .filePath(QStringLiteral("../share/apex32/Apex32BootManager.efi")));
}

bool VerifyPackagedFirmware(QString *Path, QString *Error) {
  if (!kHardwareInstallEnabled) {
    if (Error != nullptr) {
      *Error = QStringLiteral("This build is intentionally scan-only.");
    }
    return false;
  }
  const QString Candidate = PackagedFirmwarePath();
  QFile File(Candidate);
  if (!File.open(QIODevice::ReadOnly) || File.size() < 4096 ||
      File.size() > (64 * 1024 * 1024)) {
    if (Error != nullptr) {
      *Error = QStringLiteral("The packaged APEX32 firmware is missing or invalid.");
    }
    return false;
  }
  const QByteArray Contents = File.readAll();
  if (Contents.size() < 2 || Contents.at(0) != 'M' || Contents.at(1) != 'Z') {
    if (Error != nullptr) {
      *Error = QStringLiteral("The packaged firmware is not a PE/COFF image.");
    }
    return false;
  }
  const QByteArray Expected(APEX32_PACKAGED_FIRMWARE_SHA256);
  const QByteArray Actual =
      QCryptographicHash::hash(Contents, QCryptographicHash::Sha256).toHex();
  if (Expected.size() != 64 || Actual.compare(Expected, Qt::CaseInsensitive) != 0) {
    if (Error != nullptr) {
      *Error = QStringLiteral("The packaged firmware checksum is not trusted.");
    }
    return false;
  }
  *Path = Candidate;
  return true;
}

std::string Capabilities() {
  std::string Result =
      "APEX32CAPS|1\n"
      "SCAN|1\n"
      "TRANSACTION|1\n";
  Result += kHardwareInstallEnabled ? "INSTALL|1\nRESTORE|1\n"
                                    : "INSTALL|0\nRESTORE|0\n";
  Result += "UAC|1\n";
  return Result;
}

class InstallerWindow final : public QMainWindow {
 public:
  InstallerWindow() {
    setWindowTitle(QStringLiteral("APEX32 Community Installer"));
    resize(1020, 700);
    FirmwareReady_ = VerifyPackagedFirmware(&FirmwarePath_, &FirmwareError_);

    auto *Central = new QWidget(this);
    auto *Layout = new QVBoxLayout(Central);
    auto *Title = new QLabel(QStringLiteral("APEX32 SECURE GATEWAY"), Central);
    Title->setObjectName(QStringLiteral("title"));
    Title->setAlignment(Qt::AlignCenter);
    auto *Brand = new QLabel(
        QStringLiteral("apex32-secure.com  //  GPL-3.0 Community Edition"),
        Central);
    Brand->setObjectName(QStringLiteral("brand"));
    Brand->setAlignment(Qt::AlignCenter);
    Status_ = new QLabel(Central);
    Status_->setWordWrap(true);
    Status_->setText(
        FirmwareReady_
            ? QStringLiteral(
                  "Select Scan Systems. Windows will request administrator "
                  "approval once, then installation and recovery stay graphical.")
            : QStringLiteral("Scan remains available. %1").arg(FirmwareError_));
    Scan_ = new QPushButton(QStringLiteral("Scan Systems"), Central);
    Table_ = new QTableWidget(0, 3, Central);
    Table_->setHorizontalHeaderLabels(
        {QStringLiteral("Use"),
         QStringLiteral("Operating System"),
         QStringLiteral("EFI Loader")});
    Table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    Table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    Table_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    Table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    Table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    Install_ = new QPushButton(
        kHardwareInstallEnabled
            ? QStringLiteral("Install APEX32 and Make Default")
            : QStringLiteral("Hardware installation disabled in this source build"),
        Central);
    Install_->setEnabled(false);
    Restore_ = new QPushButton(
        kHardwareInstallEnabled
            ? QStringLiteral("Restore Previous Boot State")
            : QStringLiteral("Hardware restore disabled in this source build"),
        Central);
    Restore_->setEnabled(false);

    Layout->addWidget(Title);
    Layout->addWidget(Brand);
    Layout->addSpacing(12);
    Layout->addWidget(Status_);
    Layout->addWidget(Scan_);
    Layout->addWidget(Table_, 1);
    Layout->addWidget(Install_);
    Layout->addWidget(Restore_);
    setCentralWidget(Central);

    setStyleSheet(QStringLiteral(
        "QWidget { background: #070a12; color: #d8e6ef; font-size: 15px; }"
        "QLabel#title { color: #74f6ff; font-size: 30px; font-weight: 700; letter-spacing: 4px; }"
        "QLabel#brand { color: #ff724f; font-size: 16px; }"
        "QPushButton { background: #101927; border: 1px solid #36dce8; padding: 12px; }"
        "QPushButton:hover { background: #163044; }"
        "QPushButton:disabled { color: #66717e; border-color: #303744; }"
        "QTableWidget { background: #090d16; gridline-color: #203342; }"
        "QHeaderView::section { background: #101927; color: #74f6ff; padding: 8px; }"));

    connect(Scan_, &QPushButton::clicked, this, [this]() { StartScan(); });
    connect(Install_, &QPushButton::clicked, this, [this]() { InstallSelected(); });
    connect(Restore_, &QPushButton::clicked, this, [this]() { RestorePrevious(); });
  }

  void StartScan() {
    if (!IsAdministrator()) {
      const std::wstring Program =
          QDir::toNativeSeparators(QCoreApplication::applicationFilePath())
              .toStdWString();
      const HINSTANCE Result = ShellExecuteW(
          nullptr,
          L"runas",
          Program.c_str(),
          L"--elevated-scan",
          nullptr,
          SW_SHOWNORMAL);
      if (reinterpret_cast<INT_PTR>(Result) <= 32) {
        QMessageBox::warning(
            this,
            QStringLiteral("Scan cancelled"),
            QStringLiteral(
                "Windows administrator approval is required to read the EFI "
                "System Partition and prepare installation."));
        return;
      }
      QCoreApplication::quit();
      return;
    }

    Scan_->setEnabled(false);
    Install_->setEnabled(false);
    Restore_->setEnabled(false);
    Status_->setText(QStringLiteral("Scanning the EFI System Partition..."));
    Table_->setRowCount(0);
    Loaders_.clear();
    QApplication::processEvents();

    QString Error;
    EspMount Mount;
    if (!Mount.Mount(&Error)) {
      Status_->setText(QStringLiteral("Scan failed."));
      QMessageBox::critical(this, QStringLiteral("Scan failed"), Error);
      Scan_->setEnabled(true);
      return;
    }

    QSet<QString> Seen;
    QDirIterator Iterator(
        Mount.EfiDirectory(),
        {QStringLiteral("*.efi")},
        QDir::Files,
        QDirIterator::Subdirectories);
    const QDir EfiRoot(Mount.EfiDirectory());
    while (Iterator.hasNext() && Loaders_.size() < kMaximumLoaders) {
      const QString Absolute = Iterator.next();
      QString Relative = EfiRoot.relativeFilePath(Absolute);
      Relative.replace(QLatin1Char('/'), QLatin1Char('\\'));
      const QString DisplayPath = QStringLiteral("\\EFI\\") + Relative;
      const QString Lower = DisplayPath.toLower();
      if (Lower.startsWith(QStringLiteral("\\efi\\apex32\\")) ||
          Seen.contains(Lower) || !SafeConfigurationField(DisplayPath, 159)) {
        continue;
      }
      Seen.insert(Lower);
      const QString Name = FriendlyName(DisplayPath);
      Loaders_.push_back(
          {Name,
           DisplayPath,
           IconName(Name),
           !Name.contains(QStringLiteral("RECOVERY"))});
    }

    std::sort(
        Loaders_.begin(),
        Loaders_.end(),
        [](const Loader &Left, const Loader &Right) {
          if (Left.Name == Right.Name) {
            return Left.Path < Right.Path;
          }
          return Left.Name < Right.Name;
        });

    for (const Loader &Entry : Loaders_) {
      const int Row = Table_->rowCount();
      Table_->insertRow(Row);
      auto *Use = new QCheckBox(Table_);
      Use->setChecked(Entry.DefaultSelected);
      Use->setStyleSheet(QStringLiteral("margin-left: 12px"));
      Table_->setCellWidget(Row, 0, Use);
      Table_->setItem(Row, 1, new QTableWidgetItem(Entry.Name));
      Table_->setItem(Row, 2, new QTableWidgetItem(Entry.Path));
    }

    RecoveryAvailable_ = QFileInfo(
        QDir(Mount.RootDirectory())
            .filePath(QStringLiteral("EFI/APEX32/recovery-state.json")))
                             .isFile();
    Install_->setEnabled(
        kHardwareInstallEnabled && FirmwareReady_ && !Loaders_.isEmpty());
    Restore_->setEnabled(kHardwareInstallEnabled && RecoveryAvailable_);
    Status_->setText(
        QStringLiteral(
            "%1 EFI loaders found. Select the systems to display, then "
            "install APEX32 as the first UEFI boot entry.")
            .arg(Loaders_.size()));
    Scan_->setEnabled(true);
  }

 private:
  QByteArray BuildConfiguration(QString *Error) const {
    QByteArray Configuration("APEX32CFG|1\n");
    int Selected = 0;
    for (int Row = 0; Row < Table_->rowCount(); ++Row) {
      const auto *Use = qobject_cast<QCheckBox *>(Table_->cellWidget(Row, 0));
      if (Use == nullptr || !Use->isChecked()) {
        continue;
      }
      if (++Selected > kMaximumSelectedLoaders) {
        if (Error != nullptr) {
          *Error = QStringLiteral("Select at most 32 EFI loaders.");
        }
        return {};
      }
      const Loader &Entry = Loaders_.at(Row);
      if (!SafeConfigurationField(Entry.Name, 39) ||
          !SafeConfigurationField(Entry.Path, 159) ||
          !SafeConfigurationField(Entry.Icon, 15)) {
        if (Error != nullptr) {
          *Error = QStringLiteral("A selected EFI loader has unsafe metadata.");
        }
        return {};
      }
      Configuration += "ENTRY|";
      Configuration += Entry.Name.toLatin1();
      Configuration += '|';
      Configuration += Entry.Path.toLatin1();
      Configuration += '|';
      Configuration += Entry.Icon.toLatin1();
      Configuration += '\n';
    }
    if (Selected == 0) {
      if (Error != nullptr) {
        *Error = QStringLiteral("Select at least one operating system.");
      }
      return {};
    }
    return Configuration;
  }

  bool PrepareNativeStore(
      Apex32::WindowsInstaller::WindowsFirmwareVariableAccess *Variables,
      QString *Error) {
    if (!Variables->Prepare(Error)) {
      return false;
    }
    bool SecureBoot = false;
    if (!Apex32::WindowsInstaller::ReadSecureBootState(
            Variables, &SecureBoot, Error)) {
      return false;
    }
    if (SecureBoot) {
      if (Error != nullptr) {
        *Error = QStringLiteral(
            "Secure Boot is enabled. This unsigned Community beta refuses to "
            "replace the default boot path. Use Restore if APEX32 was already "
            "installed, or wait for a signed release.");
      }
      return false;
    }
    return true;
  }

  void InstallSelected() {
    if (!kHardwareInstallEnabled || !FirmwareReady_) {
      QMessageBox::critical(
          this,
          QStringLiteral("Installation unavailable"),
          FirmwareError_.isEmpty()
              ? QStringLiteral("This build cannot perform hardware installation.")
              : FirmwareError_);
      return;
    }
    QString Error;
    const QByteArray Configuration = BuildConfiguration(&Error);
    if (Configuration.isEmpty()) {
      QMessageBox::warning(this, QStringLiteral("Selection required"), Error);
      return;
    }
    if (QMessageBox::question(
            this,
            QStringLiteral("Install APEX32"),
            QStringLiteral(
                "APEX32 will copy its verified firmware to the EFI System "
                "Partition and become first in UEFI BootOrder. The previous "
                "files and exact boot order will be saved for graphical "
                "Restore. Continue?")) != QMessageBox::Yes) {
      return;
    }

    Install_->setEnabled(false);
    Status_->setText(QStringLiteral("Installing APEX32 transactionally..."));
    QApplication::processEvents();
    EspMount Mount;
    Apex32::WindowsInstaller::WindowsFirmwareVariableAccess Variables;
    if (!Mount.Mount(&Error) || !PrepareNativeStore(&Variables, &Error)) {
      Status_->setText(QStringLiteral("Installation was stopped safely."));
      QMessageBox::critical(this, QStringLiteral("Installation stopped"), Error);
      Install_->setEnabled(true);
      return;
    }
    Apex32::WindowsInstaller::NativeFirmwareStore Store(&Variables);
    Apex32::WindowsInstaller::TransactionEngine Engine(
        Mount.RootDirectory(), &Store);
    const Apex32::WindowsInstaller::TransactionResult Result =
        Engine.Install(FirmwarePath_, Configuration);
    if (!Result.Success) {
      Status_->setText(QStringLiteral("Installation failed and was rolled back."));
      QMessageBox::critical(
          this, QStringLiteral("Installation failed"), Result.Message);
      Install_->setEnabled(true);
      return;
    }
    RecoveryAvailable_ = true;
    Restore_->setEnabled(true);
    Install_->setEnabled(true);
    Status_->setText(
        QStringLiteral("APEX32 is installed and first in UEFI BootOrder."));
    QMessageBox::information(
        this,
        QStringLiteral("APEX32 installed"),
        QStringLiteral(
            "Installation and verification completed. Restart Windows when "
            "ready to enter APEX32 Secure Gateway."));
  }

  void RestorePrevious() {
    if (!kHardwareInstallEnabled || !RecoveryAvailable_) {
      QMessageBox::information(
          this,
          QStringLiteral("No recovery state"),
          QStringLiteral("No verified APEX32 recovery transaction is available."));
      return;
    }
    if (QMessageBox::question(
            this,
            QStringLiteral("Restore previous boot state"),
            QStringLiteral(
                "Restore the exact pre-APEX32 BootOrder and recover or remove "
                "the files created by the installer?")) != QMessageBox::Yes) {
      return;
    }

    Restore_->setEnabled(false);
    Status_->setText(QStringLiteral("Restoring the previous UEFI state..."));
    QApplication::processEvents();
    QString Error;
    EspMount Mount;
    Apex32::WindowsInstaller::WindowsFirmwareVariableAccess Variables;
    if (!Mount.Mount(&Error) || !Variables.Prepare(&Error)) {
      Status_->setText(QStringLiteral("Restore was stopped safely."));
      QMessageBox::critical(this, QStringLiteral("Restore stopped"), Error);
      Restore_->setEnabled(true);
      return;
    }
    Apex32::WindowsInstaller::NativeFirmwareStore Store(&Variables);
    Apex32::WindowsInstaller::TransactionEngine Engine(
        Mount.RootDirectory(), &Store);
    const Apex32::WindowsInstaller::TransactionResult Result = Engine.Restore();
    if (!Result.Success) {
      Status_->setText(QStringLiteral("Restore failed and retained recoverable state."));
      QMessageBox::critical(this, QStringLiteral("Restore failed"), Result.Message);
      Restore_->setEnabled(true);
      return;
    }
    RecoveryAvailable_ = false;
    Status_->setText(QStringLiteral("The previous EFI files and BootOrder were restored."));
    QMessageBox::information(
        this,
        QStringLiteral("Previous boot state restored"),
        QStringLiteral(
            "APEX32 installation state was removed and the exact previous "
            "UEFI boot order was restored."));
  }

  QLabel *Status_ = nullptr;
  QPushButton *Scan_ = nullptr;
  QTableWidget *Table_ = nullptr;
  QPushButton *Install_ = nullptr;
  QPushButton *Restore_ = nullptr;
  QVector<Loader> Loaders_;
  QString FirmwarePath_;
  QString FirmwareError_;
  bool FirmwareReady_ = false;
  bool RecoveryAvailable_ = false;
};

}  // namespace

int main(int argc, char **argv) {
  const std::string CapabilityText = Capabilities();
  int CapabilityFileIndex = -1;
  bool PrintCapabilities = false;
  for (int Index = 1; Index < argc; ++Index) {
    const std::string_view Argument(argv[Index]);
    if (Argument == "--capabilities-file") {
      CapabilityFileIndex = Index;
    } else if (Argument == "--capabilities") {
      PrintCapabilities = true;
    }
  }
  if (CapabilityFileIndex >= 0 && CapabilityFileIndex + 1 < argc) {
    std::ofstream File(
        argv[CapabilityFileIndex + 1], std::ios::binary | std::ios::trunc);
    if (!File) {
      return 2;
    }
    File << CapabilityText;
    return 0;
  }
  if (PrintCapabilities) {
    std::cout << CapabilityText;
    return 0;
  }

  QApplication Application(argc, argv);
  const QStringList Arguments = QCoreApplication::arguments();
  InstallerWindow Window;
  Window.show();
  if (Arguments.contains(QStringLiteral("--elevated-scan"))) {
    QTimer::singleShot(0, &Window, [&Window]() { Window.StartScan(); });
  }
  return Application.exec();
}
