#include <QCoreApplication>
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

[[nodiscard]] bool Install(
    const QString& EspArgument,
    const QString& FirmwareArgument,
    const QString& ConfigArgument,
    QString* Error) {
  if (!RequireRoot(Error)) {
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

  QDir ApexDirectory(QDir(Esp).filePath(QStringLiteral("EFI/APEX32")));
  if (!ApexDirectory.exists() && !QDir().mkpath(ApexDirectory.absolutePath())) {
    *Error = QStringLiteral("cannot create EFI/APEX32");
    return false;
  }

  const QString Destination = ApexDirectory.filePath(
      QStringLiteral("Apex32BootManager.efi"));
  const QString Backup = ApexDirectory.filePath(
      QStringLiteral("Apex32BootManager.efi.before-community"));
  if (QFileInfo::exists(Destination) && !QFileInfo::exists(Backup) &&
      !QFile::copy(Destination, Backup)) {
    *Error = QStringLiteral("cannot create immutable firmware backup");
    return false;
  }

  if (!CopyAtomically(FirmwareInfo.canonicalFilePath(), Destination, Error) ||
      !CopyAtomically(
          ConfigInfo.canonicalFilePath(),
          ApexDirectory.filePath(QStringLiteral("apex32.cfg")),
          Error)) {
    return false;
  }

  int ExitCode = 0;
  const QString Source = RunCommand(
      QStringLiteral("/usr/bin/findmnt"),
      {QStringLiteral("-no"), QStringLiteral("SOURCE"), Esp},
      &ExitCode,
      Error).trimmed();
  if ((ExitCode != 0) || Source.isEmpty()) {
    return false;
  }
  const QString Parent = RunCommand(
      QStringLiteral("/usr/bin/lsblk"),
      {QStringLiteral("-no"), QStringLiteral("PKNAME"), Source},
      &ExitCode,
      Error).trimmed();
  if ((ExitCode != 0) || Parent.isEmpty()) {
    return false;
  }
  const QString Partition = RunCommand(
      QStringLiteral("/usr/bin/lsblk"),
      {QStringLiteral("-no"), QStringLiteral("PARTN"), Source},
      &ExitCode,
      Error).trimmed();
  if ((ExitCode != 0) || Partition.isEmpty()) {
    return false;
  }

  const QString Entries = RunCommand(
      QStringLiteral("/usr/bin/efibootmgr"), {}, &ExitCode, Error);
  if (ExitCode != 0) {
    return false;
  }
  QRegularExpression ExistingExpression(
      QStringLiteral("^Boot([0-9A-Fa-f]{4})\\*?\\s+APEX32 Secure Gateway$"),
      QRegularExpression::MultilineOption);
  QRegularExpressionMatch Existing = ExistingExpression.match(Entries);
  if (!Existing.hasMatch()) {
    (void)RunCommand(
        QStringLiteral("/usr/bin/efibootmgr"),
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
      return false;
    }
  }

  const QString UpdatedEntries = RunCommand(
      QStringLiteral("/usr/bin/efibootmgr"), {}, &ExitCode, Error);
  if (ExitCode != 0) {
    return false;
  }
  Existing = ExistingExpression.match(UpdatedEntries);
  if (!Existing.hasMatch()) {
    *Error = QStringLiteral("firmware did not expose the APEX32 boot entry");
    return false;
  }
  const QString ApexBootNumber = Existing.captured(1).toUpper();

  QRegularExpression OrderExpression(
      QStringLiteral("^BootOrder:\\s*([^\\r\\n]+)$"),
      QRegularExpression::MultilineOption);
  const QRegularExpressionMatch OrderMatch = OrderExpression.match(
      UpdatedEntries);
  if (!OrderMatch.hasMatch()) {
    *Error = QStringLiteral("cannot read firmware boot order");
    return false;
  }
  QStringList Order = OrderMatch.captured(1).split(',', Qt::SkipEmptyParts);
  for (QString& Item : Order) {
    Item = Item.trimmed().toUpper();
  }
  Order.removeAll(ApexBootNumber);
  Order.prepend(ApexBootNumber);
  (void)RunCommand(
      QStringLiteral("/usr/bin/efibootmgr"),
      {QStringLiteral("-o"), Order.join(',')},
      &ExitCode,
      Error);
  sync();
  return ExitCode == 0;
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
