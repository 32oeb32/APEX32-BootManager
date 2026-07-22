#include <QApplication>
#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
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

#include "Transaction/WindowsTransaction.hpp"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {

constexpr int kMaximumLoaders = 64;

struct Loader {
  QString Name;
  QString Path;
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
      nullptr,
      AdministratorsGroup,
      &IsMember);
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
        *Error = QString::fromLocal8Bit(Process.readAllStandardOutput()).trimmed();
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

QString FriendlyName(const QString &RelativePath) {
  const QString Lower = RelativePath.toLower();
  if (Lower.contains(QStringLiteral("\\microsoft\\"))) {
    return QStringLiteral("WINDOWS");
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
    return QStringLiteral("UEFI FALLBACK");
  }
  return QStringLiteral("UEFI APPLICATION");
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
    const QString EfiDirectory = Drive_ + QStringLiteral("\\EFI");
    if (!QFileInfo(EfiDirectory).isDir()) {
      if (Error != nullptr) {
        *Error = QStringLiteral("The mounted system partition has no EFI directory.");
      }
      Unmount();
      return false;
    }
    return true;
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

class InstallerWindow final : public QMainWindow {
 public:
  InstallerWindow() {
    setWindowTitle(QStringLiteral("APEX32 Community Installer"));
    resize(980, 650);

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
    Status_ = new QLabel(
        QStringLiteral(
            "Select Scan Systems. Transaction recovery schema %1 is CI-qualified; "
            "hardware writes remain locked.")
            .arg(Apex32::WindowsInstaller::TransactionSchemaVersion()),
        Central);
    Status_->setWordWrap(true);
    Scan_ = new QPushButton(QStringLiteral("Scan Systems"), Central);
    Table_ = new QTableWidget(0, 2, Central);
    Table_->setHorizontalHeaderLabels(
        {QStringLiteral("Operating System"), QStringLiteral("EFI Loader")});
    Table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    Table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    Table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    Table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    Install_ = new QPushButton(
        QStringLiteral("Install and Make Default — hardware backend not yet qualified"),
        Central);
    Install_->setEnabled(false);
    Restore_ = new QPushButton(
        QStringLiteral("Restore Previous Boot State — hardware backend not yet qualified"),
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
            QStringLiteral("Windows administrator approval is required to read the EFI System Partition."));
        return;
      }
      QCoreApplication::quit();
      return;
    }

    Scan_->setEnabled(false);
    Status_->setText(QStringLiteral("Scanning the EFI System Partition..."));
    Table_->setRowCount(0);
    QApplication::processEvents();

    QString Error;
    EspMount Mount;
    if (!Mount.Mount(&Error)) {
      Status_->setText(QStringLiteral("Scan failed."));
      QMessageBox::critical(this, QStringLiteral("Scan failed"), Error);
      Scan_->setEnabled(true);
      return;
    }

    QVector<Loader> Loaders;
    QSet<QString> Seen;
    QDirIterator Iterator(
        Mount.EfiDirectory(),
        {QStringLiteral("*.efi")},
        QDir::Files,
        QDirIterator::Subdirectories);
    const QDir EfiRoot(Mount.EfiDirectory());
    while (Iterator.hasNext() && Loaders.size() < kMaximumLoaders) {
      const QString Absolute = Iterator.next();
      QString Relative = EfiRoot.relativeFilePath(Absolute);
      Relative.replace(QLatin1Char('/'), QLatin1Char('\\'));
      const QString DisplayPath = QStringLiteral("\\EFI\\") + Relative;
      const QString Lower = DisplayPath.toLower();
      if (Lower.startsWith(QStringLiteral("\\efi\\apex32\\")) ||
          Seen.contains(Lower)) {
        continue;
      }
      Seen.insert(Lower);
      Loaders.push_back({FriendlyName(DisplayPath), DisplayPath});
    }

    std::sort(
        Loaders.begin(),
        Loaders.end(),
        [](const Loader &Left, const Loader &Right) {
          if (Left.Name == Right.Name) {
            return Left.Path < Right.Path;
          }
          return Left.Name < Right.Name;
        });

    for (const Loader &Entry : Loaders) {
      const int Row = Table_->rowCount();
      Table_->insertRow(Row);
      Table_->setItem(Row, 0, new QTableWidgetItem(Entry.Name));
      Table_->setItem(Row, 1, new QTableWidgetItem(Entry.Path));
    }
    Status_->setText(
        QStringLiteral(
            "%1 EFI loaders found. Transactional rollback and restore pass in "
            "a disposable ESP; the real Windows firmware backend remains locked.")
            .arg(Loaders.size()));
    Scan_->setEnabled(true);
  }

 private:
  QLabel *Status_ = nullptr;
  QPushButton *Scan_ = nullptr;
  QTableWidget *Table_ = nullptr;
  QPushButton *Install_ = nullptr;
  QPushButton *Restore_ = nullptr;
};

}  // namespace

int main(int argc, char **argv) {
  constexpr std::string_view Capabilities =
      "APEX32CAPS|1\n"
      "SCAN|1\n"
      "TRANSACTION|1\n"
      "INSTALL|0\n"
      "RESTORE|0\n"
      "UAC|1\n";
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
    std::ofstream File(argv[CapabilityFileIndex + 1],
                       std::ios::binary | std::ios::trunc);
    if (!File) {
      return 2;
    }
    File << Capabilities;
    return 0;
  }
  if (PrintCapabilities) {
    std::cout << Capabilities;
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
