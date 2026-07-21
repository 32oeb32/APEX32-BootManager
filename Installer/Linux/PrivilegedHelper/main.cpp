#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStringList>
#include <QTextStream>

#include <unistd.h>

#include <algorithm>

#ifndef APEX32_ENABLE_HARDWARE_INSTALL
#define APEX32_ENABLE_HARDWARE_INSTALL 0
#endif

namespace {

[[nodiscard]] bool RequireRoot(QString* Error) {
  if (geteuid() == 0) {
    return true;
  }
  *Error = QStringLiteral(
      "helper must run through the graphical authorization prompt");
  return false;
}

[[nodiscard]] bool Scan(
    const QString& EspArgument,
    QByteArray* Protocol,
    QString* Error) {
  if (!RequireRoot(Error)) {
    return false;
  }

  const QFileInfo EspInfo(EspArgument);
  const QString Esp = EspInfo.canonicalFilePath();
  const QDir EfiDirectory(QDir(Esp).filePath(QStringLiteral("EFI")));
  if (Esp.isEmpty() || !EspInfo.isDir() || !EfiDirectory.exists()) {
    *Error = QStringLiteral("invalid EFI System Partition mount point");
    return false;
  }

  QStringList LoaderPaths;
  QDirIterator Iterator(
      EfiDirectory.absolutePath(),
      QDir::Files,
      QDirIterator::Subdirectories);
  while (Iterator.hasNext() && LoaderPaths.size() < 256) {
    const QString AbsolutePath = Iterator.next();
    if (!AbsolutePath.endsWith(QStringLiteral(".efi"), Qt::CaseInsensitive)) {
      continue;
    }

    QString Relative = QDir(Esp).relativeFilePath(AbsolutePath);
    Relative.replace('/', '\\');
    const QString LoaderPath = QStringLiteral("\\") + Relative;
    const QString Lower = LoaderPath.toLower();
    if (Lower.contains(QStringLiteral("\\efi\\apex32\\")) ||
        LoaderPath.contains('|') || LoaderPath.contains('\n') ||
        LoaderPath.contains('\r')) {
      continue;
    }
    LoaderPaths.push_back(LoaderPath);
  }

  std::sort(
      LoaderPaths.begin(),
      LoaderPaths.end(),
      [](const QString& Left, const QString& Right) {
        return Left.compare(Right, Qt::CaseInsensitive) < 0;
      });

  QByteArray Result("APEX32SCAN|1\n");
  for (const QString& LoaderPath : LoaderPaths) {
    Result += "LOADER|";
    Result += LoaderPath.toUtf8();
    Result += '\n';
  }
  *Protocol = Result;
  return true;
}

#if APEX32_ENABLE_HARDWARE_INSTALL
[[nodiscard]] bool RequireMutationAccess(
    const QString& EspArgument,
    QString* Error) {
#if defined(APEX32_TRANSACTION_TEST)
  if (qEnvironmentVariable("APEX32_TRANSACTION_TEST") !=
      QStringLiteral("1")) {
    *Error = QStringLiteral("transaction test mode was not explicitly enabled");
    return false;
  }
  const QString AllowedEsp = QFileInfo(
      qEnvironmentVariable("APEX32_TRANSACTION_TEST_ESP"))
                                 .canonicalFilePath();
  const QString RequestedEsp = QFileInfo(EspArgument).canonicalFilePath();
  if (AllowedEsp.isEmpty() || RequestedEsp.isEmpty() ||
      RequestedEsp != AllowedEsp ||
      !RequestedEsp.startsWith(QDir::tempPath() + QDir::separator())) {
    *Error = QStringLiteral(
        "transaction test is restricted to its declared temporary ESP");
    return false;
  }
  return true;
#else
  return RequireRoot(Error);
#endif
}

[[nodiscard]] bool CopyAtomically(
    const QString& Source,
    const QString& Destination,
    QString* Error) {
  QFile Input(Source);
  if (!Input.open(QIODevice::ReadOnly)) {
    *Error = QStringLiteral("cannot read %1").arg(Source);
    return false;
  }

  QSaveFile Output(Destination);
  Output.setDirectWriteFallback(false);
  if (!Output.open(QIODevice::WriteOnly) ||
      (Output.write(Input.readAll()) < 0) || !Output.commit()) {
    *Error = QStringLiteral("cannot write %1").arg(Destination);
    return false;
  }
  return true;
}

[[nodiscard]] bool FilesMatch(
    const QString& LeftPath,
    const QString& RightPath,
    QString* Error) {
  QFile Left(LeftPath);
  QFile Right(RightPath);
  if (!Left.open(QIODevice::ReadOnly) || !Right.open(QIODevice::ReadOnly)) {
    *Error = QStringLiteral("cannot verify staged installer files");
    return false;
  }
  const QByteArray LeftHash = QCryptographicHash::hash(
      Left.readAll(), QCryptographicHash::Sha256);
  const QByteArray RightHash = QCryptographicHash::hash(
      Right.readAll(), QCryptographicHash::Sha256);
  if (LeftHash != RightHash) {
    *Error = QStringLiteral("staged installer file verification failed");
    return false;
  }
  return true;
}

[[nodiscard]] QString ToolPath(const QString& Name) {
#if defined(APEX32_TRANSACTION_TEST)
  return QDir(qEnvironmentVariable("APEX32_TRANSACTION_TEST_TOOLS"))
      .filePath(Name);
#else
  return QStringLiteral("/usr/bin/") + Name;
#endif
}

[[nodiscard]] QString RunCommand(
    const QString& Program,
    const QStringList& Arguments,
    int* ExitCode,
    QString* Error) {
  QProcess Process;
  Process.start(Program, Arguments);
  if (!Process.waitForFinished(30000)) {
    *Error = QStringLiteral("%1 timed out").arg(Program);
    *ExitCode = -1;
    return {};
  }
  *ExitCode = Process.exitCode();
  if (*ExitCode != 0) {
    *Error = QString::fromUtf8(Process.readAllStandardError()).trimmed();
  }
  return QString::fromUtf8(Process.readAllStandardOutput());
}

[[nodiscard]] QStringList ApexEntryIds(const QString& Entries) {
  QStringList Result;
  const QRegularExpression Expression(
      QStringLiteral("^Boot([0-9A-Fa-f]{4})\\*?\\s+APEX32 Secure Gateway(?:\\t|$)"),
      QRegularExpression::MultilineOption);
  QRegularExpressionMatchIterator Matches = Expression.globalMatch(Entries);
  while (Matches.hasNext()) {
    Result.push_back(Matches.next().captured(1).toUpper());
  }
  return Result;
}

[[nodiscard]] QString BootOrder(const QString& Entries) {
  const QRegularExpression Expression(
      QStringLiteral("^BootOrder:\\s*([^\\r\\n]+)$"),
      QRegularExpression::MultilineOption);
  const QRegularExpressionMatch Match = Expression.match(Entries);
  if (!Match.hasMatch()) {
    return {};
  }
  QStringList Items = Match.captured(1).split(',', Qt::SkipEmptyParts);
  for (QString& Item : Items) {
    Item = Item.trimmed().toUpper();
  }
  return Items.join(',');
}

void RemoveTransactionFiles(const QStringList& Paths) {
  for (const QString& Path : Paths) {
    (void)QFile::remove(Path);
  }
}

[[nodiscard]] bool RestoreFile(
    const QString& Destination,
    const QString& Snapshot,
    const bool Existed,
    QString* Error) {
  if (Existed) {
    return CopyAtomically(Snapshot, Destination, Error);
  }
  if (QFileInfo::exists(Destination) && !QFile::remove(Destination)) {
    *Error = QStringLiteral("cannot remove new file during rollback: %1")
                 .arg(Destination);
    return false;
  }
  return true;
}

void RollBack(
    const QString& Destination,
    const QString& ConfigDestination,
    const QString& FirmwareSnapshot,
    const QString& ConfigSnapshot,
    const bool FirmwareExisted,
    const bool ConfigExisted,
    const QString& OriginalOrder,
    const bool EntryExisted,
    QString* Error) {
  QStringList RollbackErrors;
  QString RollbackError;
  if (!RestoreFile(
          Destination,
          FirmwareSnapshot,
          FirmwareExisted,
          &RollbackError)) {
    RollbackErrors.push_back(RollbackError);
  }
  RollbackError.clear();
  if (!RestoreFile(
          ConfigDestination,
          ConfigSnapshot,
          ConfigExisted,
          &RollbackError)) {
    RollbackErrors.push_back(RollbackError);
  }

  int ExitCode = 0;
  if (!EntryExisted) {
    RollbackError.clear();
    const QString Entries = RunCommand(
        ToolPath(QStringLiteral("efibootmgr")), {}, &ExitCode, &RollbackError);
    if (ExitCode == 0) {
      for (const QString& Id : ApexEntryIds(Entries)) {
        RollbackError.clear();
        (void)RunCommand(
            ToolPath(QStringLiteral("efibootmgr")),
            {QStringLiteral("-b"), Id, QStringLiteral("-B")},
            &ExitCode,
            &RollbackError);
        if (ExitCode != 0) {
          RollbackErrors.push_back(RollbackError);
        }
      }
    } else {
      RollbackErrors.push_back(RollbackError);
    }
  }

  if (!OriginalOrder.isEmpty()) {
    RollbackError.clear();
    (void)RunCommand(
        ToolPath(QStringLiteral("efibootmgr")),
        {QStringLiteral("-o"), OriginalOrder},
        &ExitCode,
        &RollbackError);
    if (ExitCode != 0) {
      RollbackErrors.push_back(RollbackError);
    }
  }

  if (!RollbackErrors.isEmpty()) {
    *Error += QStringLiteral("; rollback warning: ") +
              RollbackErrors.join(QStringLiteral("; "));
  }
}

[[nodiscard]] bool Install(
    const QString& EspArgument,
    const QString& FirmwareArgument,
    const QString& ConfigArgument,
    QString* Error) {
  if (!RequireMutationAccess(EspArgument, Error)) {
    return false;
  }

  const QFileInfo EspInfo(EspArgument);
  const QFileInfo FirmwareInfo(FirmwareArgument);
  const QFileInfo ConfigInfo(ConfigArgument);
  const QString Esp = EspInfo.canonicalFilePath();
  if (Esp.isEmpty() || !EspInfo.isDir() ||
      !QDir(Esp).exists(QStringLiteral("EFI"))) {
    *Error = QStringLiteral("invalid EFI System Partition mount point");
    return false;
  }
  if (!FirmwareInfo.isFile() || !ConfigInfo.isFile()) {
    *Error = QStringLiteral("firmware or configuration source is missing");
    return false;
  }
  if (FirmwareInfo.size() < 2 || FirmwareInfo.size() > (64 * 1024 * 1024) ||
      ConfigInfo.size() < 11 || ConfigInfo.size() > (64 * 1024)) {
    *Error = QStringLiteral("firmware or configuration size is invalid");
    return false;
  }
  QFile FirmwareFile(FirmwareInfo.canonicalFilePath());
  QFile ConfigFile(ConfigInfo.canonicalFilePath());
  if (!FirmwareFile.open(QIODevice::ReadOnly) ||
      FirmwareFile.read(2) != QByteArray("MZ", 2) ||
      !ConfigFile.open(QIODevice::ReadOnly) ||
      !ConfigFile.readLine(32).startsWith("APEX32CFG|1")) {
    *Error = QStringLiteral("firmware or configuration format is invalid");
    return false;
  }

  QDir ApexDirectory(QDir(Esp).filePath(QStringLiteral("EFI/APEX32")));
  if (!ApexDirectory.exists() && !QDir().mkpath(ApexDirectory.absolutePath())) {
    *Error = QStringLiteral("cannot create EFI/APEX32");
    return false;
  }

  const QString Destination = ApexDirectory.filePath(
      QStringLiteral("Apex32BootManager.efi"));
  const QString ConfigDestination = ApexDirectory.filePath(
      QStringLiteral("apex32.cfg"));
  const QString FirmwareBackup = ApexDirectory.filePath(
      QStringLiteral("Apex32BootManager.efi.before-community"));
  const QString ConfigBackup = ApexDirectory.filePath(
      QStringLiteral("apex32.cfg.before-community"));
  const bool FirmwareBackupExisted = QFileInfo::exists(FirmwareBackup);
  const bool ConfigBackupExisted = QFileInfo::exists(ConfigBackup);
  const QString FirmwarePending = ApexDirectory.filePath(
      QStringLiteral(".Apex32BootManager.efi.pending"));
  const QString ConfigPending = ApexDirectory.filePath(
      QStringLiteral(".apex32.cfg.pending"));
  const QString FirmwareSnapshot = ApexDirectory.filePath(
      QStringLiteral(".Apex32BootManager.efi.rollback"));
  const QString ConfigSnapshot = ApexDirectory.filePath(
      QStringLiteral(".apex32.cfg.rollback"));
  const QStringList TransactionFiles = {
      FirmwarePending, ConfigPending, FirmwareSnapshot, ConfigSnapshot};
  for (const QString& Path : TransactionFiles) {
    if (QFileInfo::exists(Path)) {
      *Error = QStringLiteral(
          "unfinished installer transaction detected; recovery is required");
      return false;
    }
  }

  int ExitCode = 0;
  const QString OriginalEntries = RunCommand(
      ToolPath(QStringLiteral("efibootmgr")), {}, &ExitCode, Error);
  if (ExitCode != 0) {
    return false;
  }
  const QString OriginalOrder = BootOrder(OriginalEntries);
  const QStringList OriginalEntryIds = ApexEntryIds(OriginalEntries);
  if (OriginalOrder.isEmpty()) {
    *Error = QStringLiteral("cannot read firmware boot order");
    return false;
  }
  if (OriginalEntryIds.size() > 1) {
    *Error = QStringLiteral("multiple APEX32 firmware entries detected");
    return false;
  }
  const bool EntryExisted = OriginalEntryIds.size() == 1;
  const bool FirmwareExisted = QFileInfo::exists(Destination);
  const bool ConfigExisted = QFileInfo::exists(ConfigDestination);

  if (!CopyAtomically(
          FirmwareInfo.canonicalFilePath(), FirmwarePending, Error) ||
      !CopyAtomically(
          ConfigInfo.canonicalFilePath(), ConfigPending, Error) ||
      !FilesMatch(
          FirmwareInfo.canonicalFilePath(), FirmwarePending, Error) ||
      !FilesMatch(ConfigInfo.canonicalFilePath(), ConfigPending, Error)) {
    RemoveTransactionFiles(TransactionFiles);
    return false;
  }

  if ((FirmwareExisted &&
       !CopyAtomically(Destination, FirmwareSnapshot, Error)) ||
      (ConfigExisted &&
       !CopyAtomically(ConfigDestination, ConfigSnapshot, Error))) {
    RemoveTransactionFiles(TransactionFiles);
    return false;
  }

  auto FailCommittedTransaction = [&](const QString& Failure) {
    *Error = Failure;
    RollBack(
        Destination,
        ConfigDestination,
        FirmwareSnapshot,
        ConfigSnapshot,
        FirmwareExisted,
        ConfigExisted,
        OriginalOrder,
        EntryExisted,
        Error);
    RemoveTransactionFiles(TransactionFiles);
    if (!FirmwareBackupExisted) {
      (void)QFile::remove(FirmwareBackup);
    }
    if (!ConfigBackupExisted) {
      (void)QFile::remove(ConfigBackup);
    }
    sync();
    return false;
  };

  if (!CopyAtomically(FirmwarePending, Destination, Error) ||
      !CopyAtomically(ConfigPending, ConfigDestination, Error) ||
      !FilesMatch(FirmwarePending, Destination, Error) ||
      !FilesMatch(ConfigPending, ConfigDestination, Error)) {
    return FailCommittedTransaction(*Error);
  }

  const QString Source = RunCommand(
      ToolPath(QStringLiteral("findmnt")),
      {QStringLiteral("-no"), QStringLiteral("SOURCE"), Esp},
      &ExitCode,
      Error).trimmed();
  if ((ExitCode != 0) || Source.isEmpty()) {
    return FailCommittedTransaction(*Error);
  }
  const QString Parent = RunCommand(
      ToolPath(QStringLiteral("lsblk")),
      {QStringLiteral("-no"), QStringLiteral("PKNAME"), Source},
      &ExitCode,
      Error).trimmed();
  if ((ExitCode != 0) || Parent.isEmpty()) {
    return FailCommittedTransaction(*Error);
  }
  const QString Partition = RunCommand(
      ToolPath(QStringLiteral("lsblk")),
      {QStringLiteral("-no"), QStringLiteral("PARTN"), Source},
      &ExitCode,
      Error).trimmed();
  if ((ExitCode != 0) || Partition.isEmpty()) {
    return FailCommittedTransaction(*Error);
  }

  QString ApexBootNumber = EntryExisted ? OriginalEntryIds.first() : QString();
  if (ApexBootNumber.isEmpty()) {
    (void)RunCommand(
        ToolPath(QStringLiteral("efibootmgr")),
        {QStringLiteral("-c"),
         QStringLiteral("-d"),
         QStringLiteral("/dev/") + Parent,
         QStringLiteral("-p"),
         Partition,
         QStringLiteral("-L"),
         QStringLiteral("APEX32 Secure Gateway"),
         QStringLiteral("-l"),
         QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi")},
        &ExitCode,
        Error);
    if (ExitCode != 0) {
      return FailCommittedTransaction(*Error);
    }
  }

  const QString UpdatedEntries = RunCommand(
      ToolPath(QStringLiteral("efibootmgr")), {}, &ExitCode, Error);
  if (ExitCode != 0) {
    return FailCommittedTransaction(*Error);
  }
  const QStringList UpdatedEntryIds = ApexEntryIds(UpdatedEntries);
  if (UpdatedEntryIds.size() != 1) {
    return FailCommittedTransaction(
        QStringLiteral("firmware did not expose exactly one APEX32 boot entry"));
  }
  ApexBootNumber = UpdatedEntryIds.first();

  const QString UpdatedOrder = BootOrder(UpdatedEntries);
  if (UpdatedOrder.isEmpty()) {
    return FailCommittedTransaction(
        QStringLiteral("cannot read updated firmware boot order"));
  }
  QStringList Order = UpdatedOrder.split(',', Qt::SkipEmptyParts);
  Order.removeAll(ApexBootNumber);
  Order.prepend(ApexBootNumber);
  (void)RunCommand(
      ToolPath(QStringLiteral("efibootmgr")),
      {QStringLiteral("-o"), Order.join(',')},
      &ExitCode,
      Error);
  if (ExitCode != 0) {
    return FailCommittedTransaction(*Error);
  }

  const QString VerifiedEntries = RunCommand(
      ToolPath(QStringLiteral("efibootmgr")), {}, &ExitCode, Error);
  if (ExitCode != 0 || ApexEntryIds(VerifiedEntries).size() != 1 ||
      BootOrder(VerifiedEntries).section(',', 0, 0) != ApexBootNumber ||
      !FilesMatch(
          FirmwareInfo.canonicalFilePath(), Destination, Error) ||
      !FilesMatch(
          ConfigInfo.canonicalFilePath(), ConfigDestination, Error)) {
    const QString VerificationError = Error->isEmpty()
                                          ? QStringLiteral("post-install verification failed")
                                          : *Error;
    return FailCommittedTransaction(VerificationError);
  }

  if (FirmwareExisted && !FirmwareBackupExisted &&
      !CopyAtomically(FirmwareSnapshot, FirmwareBackup, Error)) {
    return FailCommittedTransaction(*Error);
  }
  if (ConfigExisted && !ConfigBackupExisted &&
      !CopyAtomically(ConfigSnapshot, ConfigBackup, Error)) {
    return FailCommittedTransaction(*Error);
  }

  RemoveTransactionFiles(TransactionFiles);
  sync();
  return true;
}
#endif

}  // namespace

int main(int argc, char** argv) {
  QCoreApplication Application(argc, argv);
  QTextStream ErrorStream(stderr);
  const QStringList Arguments = Application.arguments();
  if ((Arguments.size() == 3) &&
      (Arguments.at(1) == QStringLiteral("scan"))) {
    QString Error;
    QByteArray Protocol;
    if (!Scan(Arguments.at(2), &Protocol, &Error)) {
      ErrorStream << Error << '\n';
      return 1;
    }
    QTextStream OutputStream(stdout);
    OutputStream << QString::fromUtf8(Protocol);
    return 0;
  }

  if ((Arguments.size() != 5) ||
      (Arguments.at(1) != QStringLiteral("install"))) {
    ErrorStream << "invalid helper request\n";
    return 2;
  }

#if !APEX32_ENABLE_HARDWARE_INSTALL
  ErrorStream
      << "hardware installation is disabled in this scan-only build\n";
  return 3;
#else
  QString Error;
  if (!Install(Arguments.at(2), Arguments.at(3), Arguments.at(4), &Error)) {
    ErrorStream << Error << '\n';
    return 1;
  }
  return 0;
#endif
}
