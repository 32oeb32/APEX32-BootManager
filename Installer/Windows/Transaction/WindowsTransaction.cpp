#include "WindowsTransaction.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStringList>

#include <array>
#include <utility>

namespace Apex32::WindowsInstaller {
namespace {

constexpr auto kApexRelative = "EFI/APEX32";
constexpr auto kFirmwareRelative = "EFI/APEX32/Apex32BootManager.efi";
constexpr auto kConfigurationRelative = "EFI/APEX32/apex32.cfg";
constexpr auto kStateRelative = "EFI/APEX32/recovery-state.json";
constexpr auto kFirmwareBackupRelative =
    "EFI/APEX32/recovery/firmware.before-community";
constexpr auto kConfigurationBackupRelative =
    "EFI/APEX32/recovery/config.before-community";
constexpr auto kLoaderPath = "\\EFI\\APEX32\\Apex32BootManager.efi";
constexpr qint64 kMaximumTransactionFileBytes = 64LL * 1024LL * 1024LL;
constexpr qsizetype kMaximumConfigurationBytes = 1024 * 1024;

struct FileCapture {
  QString Path;
  bool Existed = false;
  QByteArray Contents;
};

QString Digest(const QByteArray &Data) {
  return QString::fromLatin1(
      QCryptographicHash::hash(Data, QCryptographicHash::Sha256).toHex());
}

bool SetError(QString *Error, const QString &Message) {
  if (Error != nullptr) {
    *Error = Message;
  }
  return false;
}

bool ConfinedPath(
    const QString &Root,
    const QString &Relative,
    QString *Result,
    QString *Error) {
  const QString CleanRoot = QDir::fromNativeSeparators(
      QDir::cleanPath(QFileInfo(Root).absoluteFilePath()));
  if (CleanRoot.isEmpty() || !QFileInfo(CleanRoot).isAbsolute() ||
      Relative.isEmpty() || QDir::isAbsolutePath(Relative)) {
    return SetError(Error, QStringLiteral("Invalid transaction path."));
  }
  const QString Candidate = QDir::fromNativeSeparators(
      QDir::cleanPath(QDir(CleanRoot).absoluteFilePath(Relative)));
  QString Prefix = CleanRoot;
  if (!Prefix.endsWith(QLatin1Char('/'))) {
    Prefix += QLatin1Char('/');
  }
  if (!Candidate.startsWith(Prefix, Qt::CaseInsensitive)) {
    return SetError(
        Error,
        QStringLiteral("A transaction path escaped the declared ESP root."));
  }
  *Result = Candidate;
  return true;
}

bool ReadFile(const QString &Path, QByteArray *Contents, QString *Error) {
  QFile File(Path);
  if (!File.open(QIODevice::ReadOnly)) {
    return SetError(
        Error,
        QStringLiteral("Unable to read %1: %2").arg(Path, File.errorString()));
  }
  if (File.size() < 0 || File.size() > kMaximumTransactionFileBytes) {
    return SetError(
        Error,
        QStringLiteral("Transaction input %1 exceeds the safety limit.")
            .arg(Path));
  }
  *Contents = File.readAll();
  if (File.error() != QFileDevice::NoError) {
    return SetError(
        Error,
        QStringLiteral("Unable to finish reading %1: %2")
            .arg(Path, File.errorString()));
  }
  return true;
}

bool AtomicWrite(const QString &Path, const QByteArray &Contents, QString *Error) {
  const QFileInfo Info(Path);
  if (!QDir().mkpath(Info.absolutePath())) {
    return SetError(
        Error,
        QStringLiteral("Unable to create %1.").arg(Info.absolutePath()));
  }
  QSaveFile File(Path);
  File.setDirectWriteFallback(false);
  if (!File.open(QIODevice::WriteOnly) || File.write(Contents) != Contents.size()) {
    return SetError(
        Error,
        QStringLiteral("Unable to stage %1: %2").arg(Path, File.errorString()));
  }
  if (!File.commit()) {
    return SetError(
        Error,
        QStringLiteral("Unable to commit %1: %2").arg(Path, File.errorString()));
  }
  return true;
}

bool Capture(const QString &Path, FileCapture *Result, QString *Error) {
  Result->Path = Path;
  Result->Existed = QFileInfo::exists(Path);
  Result->Contents.clear();
  return !Result->Existed || ReadFile(Path, &Result->Contents, Error);
}

bool RestoreCapture(const FileCapture &Capture, QString *Error) {
  if (Capture.Existed) {
    return AtomicWrite(Capture.Path, Capture.Contents, Error);
  }
  if (QFileInfo::exists(Capture.Path) && !QFile::remove(Capture.Path)) {
    return SetError(
        Error,
        QStringLiteral("Unable to remove rolled-back file %1.").arg(Capture.Path));
  }
  return true;
}

bool ParseState(
    const QByteArray &Contents,
    QJsonObject *State,
    QString *Error) {
  QJsonParseError ParseError;
  const QJsonDocument Document = QJsonDocument::fromJson(Contents, &ParseError);
  if (ParseError.error != QJsonParseError::NoError || !Document.isObject()) {
    return SetError(
        Error,
        QStringLiteral("The APEX32 recovery state is invalid: %1")
            .arg(ParseError.errorString()));
  }
  *State = Document.object();
  if (State->value(QStringLiteral("schema")).toInt() != 1 ||
      !State->value(QStringLiteral("firmwareStore")).isObject() ||
      !State->value(QStringLiteral("hadFirmware")).isBool() ||
      !State->value(QStringLiteral("hadConfiguration")).isBool()) {
    return SetError(
        Error,
        QStringLiteral("The APEX32 recovery state has an unsupported schema."));
  }
  return true;
}

bool VerifyFile(
    const QString &Path,
    const QByteArray &Expected,
    QString *Error) {
  QByteArray Actual;
  if (!ReadFile(Path, &Actual, Error)) {
    return false;
  }
  if (Digest(Actual) != Digest(Expected)) {
    return SetError(
        Error,
        QStringLiteral("Verification failed for %1.").arg(Path));
  }
  return true;
}

void CleanKnownDirectories(const QString &EspRoot) {
  QDir Apex(QDir(EspRoot).filePath(QString::fromLatin1(kApexRelative)));
  Apex.rmdir(QStringLiteral("recovery"));
  QDir Efi(QDir(EspRoot).filePath(QStringLiteral("EFI")));
  Efi.rmdir(QStringLiteral("APEX32"));
}

bool RestoreAll(
    const std::array<FileCapture, 5> &Files,
    FirmwareStore *Store,
    const QJsonObject &FirmwareState,
    const QString &EspRoot,
    QString *Error) {
  QString FirstError;
  QString CurrentError;
  if (!Store->Restore(FirmwareState, &CurrentError)) {
    FirstError = CurrentError;
  }
  for (const FileCapture &File : Files) {
    CurrentError.clear();
    if (!RestoreCapture(File, &CurrentError) && FirstError.isEmpty()) {
      FirstError = CurrentError;
    }
  }
  CleanKnownDirectories(EspRoot);
  if (!FirstError.isEmpty()) {
    return SetError(Error, FirstError);
  }
  return true;
}

}  // namespace

int TransactionSchemaVersion() {
  return 1;
}

FileFirmwareStore::FileFirmwareStore(QString StorePath)
    : StorePath_(std::move(StorePath)) {}

void FileFirmwareStore::FailNextPromote() {
  FailPromote_ = true;
}

void FileFirmwareStore::FailNextRestore() {
  FailRestore_ = true;
}

bool FileFirmwareStore::Load(QJsonObject *State, QString *Error) const {
  QByteArray Contents;
  if (!ReadFile(StorePath_, &Contents, Error)) {
    return false;
  }
  QJsonParseError ParseError;
  const QJsonDocument Document = QJsonDocument::fromJson(Contents, &ParseError);
  if (ParseError.error != QJsonParseError::NoError || !Document.isObject()) {
    return SetError(
        Error,
        QStringLiteral("The disposable firmware store is invalid: %1")
            .arg(ParseError.errorString()));
  }
  *State = Document.object();
  if (!State->value(QStringLiteral("entries")).isArray() ||
      !State->value(QStringLiteral("order")).isArray()) {
    return SetError(
        Error,
        QStringLiteral("The disposable firmware store has no entry order."));
  }
  return true;
}

bool FileFirmwareStore::Save(const QJsonObject &State, QString *Error) const {
  return AtomicWrite(
      StorePath_, QJsonDocument(State).toJson(QJsonDocument::Indented), Error);
}

bool FileFirmwareStore::Snapshot(QJsonObject *State, QString *Error) {
  return Load(State, Error);
}

bool FileFirmwareStore::PromoteApex(
    const QString &LoaderPath,
    QString *Error) {
  if (FailPromote_) {
    FailPromote_ = false;
    return SetError(Error, QStringLiteral("Injected firmware promotion failure."));
  }
  QJsonObject State;
  if (!Load(&State, Error)) {
    return false;
  }
  QJsonArray Entries = State.value(QStringLiteral("entries")).toArray();
  QString ApexIdentifier;
  for (const QJsonValue &Value : Entries) {
    const QJsonObject Entry = Value.toObject();
    if (Entry.value(QStringLiteral("path")).toString().compare(
            LoaderPath, Qt::CaseInsensitive) == 0) {
      ApexIdentifier = Entry.value(QStringLiteral("id")).toString();
      break;
    }
  }
  if (ApexIdentifier.isEmpty()) {
    ApexIdentifier = QStringLiteral("{apex32-disposable-test}");
    Entries.append(QJsonObject{
        {QStringLiteral("id"), ApexIdentifier},
        {QStringLiteral("description"), QStringLiteral("APEX32 Secure Gateway")},
        {QStringLiteral("path"), LoaderPath}});
  }

  QJsonArray NewOrder;
  NewOrder.append(ApexIdentifier);
  for (const QJsonValue &Value : State.value(QStringLiteral("order")).toArray()) {
    const QString Identifier = Value.toString();
    if (Identifier.compare(ApexIdentifier, Qt::CaseInsensitive) != 0) {
      NewOrder.append(Identifier);
    }
  }
  State.insert(QStringLiteral("entries"), Entries);
  State.insert(QStringLiteral("order"), NewOrder);
  return Save(State, Error);
}

bool FileFirmwareStore::Restore(
    const QJsonObject &State,
    QString *Error) {
  if (FailRestore_) {
    FailRestore_ = false;
    return SetError(Error, QStringLiteral("Injected firmware restore failure."));
  }
  return Save(State, Error);
}

bool FileFirmwareStore::Matches(
    const QJsonObject &State,
    QString *Error) {
  QJsonObject Current;
  return Load(&Current, Error) && Current == State;
}

bool FileFirmwareStore::ApexIsFirst(
    const QString &LoaderPath,
    QString *Error) {
  QJsonObject State;
  if (!Load(&State, Error)) {
    return false;
  }
  const QJsonArray Order = State.value(QStringLiteral("order")).toArray();
  if (Order.isEmpty()) {
    return SetError(Error, QStringLiteral("The disposable boot order is empty."));
  }
  const QString First = Order.first().toString();
  for (const QJsonValue &Value : State.value(QStringLiteral("entries")).toArray()) {
    const QJsonObject Entry = Value.toObject();
    if (Entry.value(QStringLiteral("id")).toString() == First &&
        Entry.value(QStringLiteral("path")).toString().compare(
            LoaderPath, Qt::CaseInsensitive) == 0) {
      return true;
    }
  }
  return SetError(Error, QStringLiteral("APEX32 is not first in boot order."));
}

TransactionEngine::TransactionEngine(QString EspRoot, FirmwareStore *Store)
    : EspRoot_(QDir::cleanPath(std::move(EspRoot))), Store_(Store) {}

QString TransactionEngine::FirmwarePath() const {
  return QDir(EspRoot_).filePath(QString::fromLatin1(kFirmwareRelative));
}

QString TransactionEngine::ConfigurationPath() const {
  return QDir(EspRoot_).filePath(QString::fromLatin1(kConfigurationRelative));
}

QString TransactionEngine::RecoveryStatePath() const {
  return QDir(EspRoot_).filePath(QString::fromLatin1(kStateRelative));
}

TransactionResult TransactionEngine::Install(
    const QString &FirmwareSource,
    const QByteArray &Configuration,
    const FailurePlan &Failure) {
  TransactionResult Result;
  QString Error;
  if (Store_ == nullptr || !QFileInfo(EspRoot_).isDir()) {
    Result.Message = QStringLiteral("The declared ESP root or firmware store is invalid.");
    return Result;
  }

  QString Firmware;
  QString ConfigurationPath;
  QString StatePath;
  QString FirmwareBackup;
  QString ConfigurationBackup;
  if (!ConfinedPath(EspRoot_, QString::fromLatin1(kFirmwareRelative), &Firmware, &Error) ||
      !ConfinedPath(EspRoot_, QString::fromLatin1(kConfigurationRelative), &ConfigurationPath, &Error) ||
      !ConfinedPath(EspRoot_, QString::fromLatin1(kStateRelative), &StatePath, &Error) ||
      !ConfinedPath(EspRoot_, QString::fromLatin1(kFirmwareBackupRelative), &FirmwareBackup, &Error) ||
      !ConfinedPath(EspRoot_, QString::fromLatin1(kConfigurationBackupRelative), &ConfigurationBackup, &Error)) {
    Result.Message = Error;
    return Result;
  }

  QByteArray FirmwareContents;
  if (!ReadFile(FirmwareSource, &FirmwareContents, &Error) ||
      FirmwareContents.size() < 2 || FirmwareContents.at(0) != 'M' ||
      FirmwareContents.at(1) != 'Z') {
    Result.Message = Error.isEmpty()
                         ? QStringLiteral("The firmware payload is not a PE/COFF image.")
                         : Error;
    return Result;
  }
  if (Configuration.isEmpty() ||
      Configuration.size() > kMaximumConfigurationBytes) {
    Result.Message = QStringLiteral(
        "The generated APEX32 configuration is empty or exceeds the safety limit.");
    return Result;
  }

  std::array<FileCapture, 5> BeforeFiles{{
      {Firmware},
      {ConfigurationPath},
      {StatePath},
      {FirmwareBackup},
      {ConfigurationBackup},
  }};
  for (FileCapture &File : BeforeFiles) {
    if (!Capture(File.Path, &File, &Error)) {
      Result.Message = Error;
      return Result;
    }
  }
  QJsonObject BeforeFirmware;
  if (!Store_->Snapshot(&BeforeFirmware, &Error)) {
    Result.Message = Error;
    return Result;
  }

  auto RollBack = [&](const QString &Cause) {
    QString RollbackError;
    Result.RolledBack = RestoreAll(
        BeforeFiles, Store_, BeforeFirmware, EspRoot_, &RollbackError);
    Result.Message = Cause;
    if (!Result.RolledBack) {
      Result.Message += QStringLiteral(" Rollback also failed: ") + RollbackError;
    }
  };

  QJsonObject RecoveryState;
  if (BeforeFiles[2].Existed) {
    if (!ParseState(BeforeFiles[2].Contents, &RecoveryState, &Error)) {
      Result.Message = Error;
      return Result;
    }
  } else {
    RecoveryState.insert(QStringLiteral("schema"), 1);
    RecoveryState.insert(QStringLiteral("hadFirmware"), BeforeFiles[0].Existed);
    RecoveryState.insert(
        QStringLiteral("hadConfiguration"), BeforeFiles[1].Existed);
    RecoveryState.insert(QStringLiteral("firmwareStore"), BeforeFirmware);
    RecoveryState.insert(
        QStringLiteral("firmwareSha256"),
        BeforeFiles[0].Existed ? Digest(BeforeFiles[0].Contents) : QString());
    RecoveryState.insert(
        QStringLiteral("configurationSha256"),
        BeforeFiles[1].Existed ? Digest(BeforeFiles[1].Contents) : QString());
    if (BeforeFiles[0].Existed &&
        !AtomicWrite(FirmwareBackup, BeforeFiles[0].Contents, &Error)) {
      RollBack(Error);
      return Result;
    }
    if (BeforeFiles[1].Existed &&
        !AtomicWrite(ConfigurationBackup, BeforeFiles[1].Contents, &Error)) {
      RollBack(Error);
      return Result;
    }
    if (!AtomicWrite(
            StatePath,
            QJsonDocument(RecoveryState).toJson(QJsonDocument::Indented),
            &Error)) {
      RollBack(Error);
      return Result;
    }
  }

  if (!AtomicWrite(Firmware, FirmwareContents, &Error) ||
      !AtomicWrite(ConfigurationPath, Configuration, &Error) ||
      !VerifyFile(Firmware, FirmwareContents, &Error) ||
      !VerifyFile(ConfigurationPath, Configuration, &Error)) {
    RollBack(Error);
    return Result;
  }
  if (Failure.AfterFileCommit) {
    RollBack(QStringLiteral("Injected failure after the file commit."));
    return Result;
  }
  if (!Store_->PromoteApex(QString::fromLatin1(kLoaderPath), &Error)) {
    RollBack(Error);
    return Result;
  }
  if (Failure.AfterFirmwareCommit) {
    RollBack(QStringLiteral("Injected failure after firmware promotion."));
    return Result;
  }
  if (!Store_->ApexIsFirst(QString::fromLatin1(kLoaderPath), &Error)) {
    RollBack(Error);
    return Result;
  }

  Result.Success = true;
  Result.Message = BeforeFiles[2].Existed
                       ? QStringLiteral("APEX32 was reinstalled transactionally.")
                       : QStringLiteral("APEX32 was installed transactionally.");
  return Result;
}

TransactionResult TransactionEngine::Restore(const FailurePlan &Failure) {
  TransactionResult Result;
  QString Error;
  if (Store_ == nullptr || !QFileInfo(EspRoot_).isDir()) {
    Result.Message = QStringLiteral("The declared ESP root or firmware store is invalid.");
    return Result;
  }

  QString Firmware;
  QString ConfigurationPath;
  QString StatePath;
  QString FirmwareBackup;
  QString ConfigurationBackup;
  if (!ConfinedPath(EspRoot_, QString::fromLatin1(kFirmwareRelative), &Firmware, &Error) ||
      !ConfinedPath(EspRoot_, QString::fromLatin1(kConfigurationRelative), &ConfigurationPath, &Error) ||
      !ConfinedPath(EspRoot_, QString::fromLatin1(kStateRelative), &StatePath, &Error) ||
      !ConfinedPath(EspRoot_, QString::fromLatin1(kFirmwareBackupRelative), &FirmwareBackup, &Error) ||
      !ConfinedPath(EspRoot_, QString::fromLatin1(kConfigurationBackupRelative), &ConfigurationBackup, &Error)) {
    Result.Message = Error;
    return Result;
  }

  QByteArray StateContents;
  QJsonObject RecoveryState;
  if (!ReadFile(StatePath, &StateContents, &Error) ||
      !ParseState(StateContents, &RecoveryState, &Error)) {
    Result.Message = Error;
    return Result;
  }
  const bool HadFirmware =
      RecoveryState.value(QStringLiteral("hadFirmware")).toBool();
  const bool HadConfiguration =
      RecoveryState.value(QStringLiteral("hadConfiguration")).toBool();
  QByteArray OriginalFirmware;
  QByteArray OriginalConfiguration;
  if (HadFirmware &&
      (!ReadFile(FirmwareBackup, &OriginalFirmware, &Error) ||
       Digest(OriginalFirmware) !=
           RecoveryState.value(QStringLiteral("firmwareSha256")).toString())) {
    Result.Message = Error.isEmpty()
                         ? QStringLiteral("The firmware recovery backup is invalid.")
                         : Error;
    return Result;
  }
  if (HadConfiguration &&
      (!ReadFile(ConfigurationBackup, &OriginalConfiguration, &Error) ||
       Digest(OriginalConfiguration) !=
           RecoveryState.value(QStringLiteral("configurationSha256")).toString())) {
    Result.Message = Error.isEmpty()
                         ? QStringLiteral("The configuration recovery backup is invalid.")
                         : Error;
    return Result;
  }

  std::array<FileCapture, 5> BeforeFiles{{
      {Firmware},
      {ConfigurationPath},
      {StatePath},
      {FirmwareBackup},
      {ConfigurationBackup},
  }};
  for (FileCapture &File : BeforeFiles) {
    if (!Capture(File.Path, &File, &Error)) {
      Result.Message = Error;
      return Result;
    }
  }
  QJsonObject BeforeFirmware;
  if (!Store_->Snapshot(&BeforeFirmware, &Error)) {
    Result.Message = Error;
    return Result;
  }
  const QJsonObject OriginalFirmwareState =
      RecoveryState.value(QStringLiteral("firmwareStore")).toObject();

  auto RollBack = [&](const QString &Cause) {
    QString RollbackError;
    Result.RolledBack = RestoreAll(
        BeforeFiles, Store_, BeforeFirmware, EspRoot_, &RollbackError);
    Result.Message = Cause;
    if (!Result.RolledBack) {
      Result.Message += QStringLiteral(" Rollback also failed: ") + RollbackError;
    }
  };

  if (!Store_->Restore(OriginalFirmwareState, &Error)) {
    RollBack(Error);
    return Result;
  }
  if (Failure.DuringRestore) {
    RollBack(QStringLiteral("Injected failure during restore."));
    return Result;
  }
  if ((HadFirmware && !AtomicWrite(Firmware, OriginalFirmware, &Error)) ||
      (!HadFirmware && QFileInfo::exists(Firmware) && !QFile::remove(Firmware)) ||
      (HadConfiguration &&
       !AtomicWrite(ConfigurationPath, OriginalConfiguration, &Error)) ||
      (!HadConfiguration && QFileInfo::exists(ConfigurationPath) &&
       !QFile::remove(ConfigurationPath))) {
    if (Error.isEmpty()) {
      Error = QStringLiteral("Unable to remove an APEX32 transaction file.");
    }
    RollBack(Error);
    return Result;
  }
  if (!Store_->Matches(OriginalFirmwareState, &Error) ||
      (HadFirmware && !VerifyFile(Firmware, OriginalFirmware, &Error)) ||
      (HadConfiguration &&
       !VerifyFile(ConfigurationPath, OriginalConfiguration, &Error))) {
    RollBack(Error);
    return Result;
  }
  for (const QString &Path :
       {StatePath, FirmwareBackup, ConfigurationBackup}) {
    if (QFileInfo::exists(Path) && !QFile::remove(Path)) {
      RollBack(QStringLiteral("Unable to remove recovery metadata %1.").arg(Path));
      return Result;
    }
  }
  CleanKnownDirectories(EspRoot_);

  Result.Success = true;
  Result.Message = QStringLiteral("The original Windows firmware state was restored.");
  return Result;
}

}  // namespace Apex32::WindowsInstaller
