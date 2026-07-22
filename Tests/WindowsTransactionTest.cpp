#include "Installer/Windows/Transaction/WindowsTransaction.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>
#include <cstdlib>

namespace {

using Apex32::WindowsInstaller::FailurePlan;
using Apex32::WindowsInstaller::FileFirmwareStore;
using Apex32::WindowsInstaller::TransactionEngine;
using Apex32::WindowsInstaller::TransactionResult;

[[noreturn]] void Fail(const QString &Message) {
  std::cerr << "FAIL: " << Message.toStdString() << '\n';
  std::exit(1);
}

void Require(bool Condition, const QString &Message) {
  if (!Condition) {
    Fail(Message);
  }
}

void Write(const QString &Path, const QByteArray &Contents) {
  Require(
      QDir().mkpath(QFileInfo(Path).absolutePath()),
      QStringLiteral("could not create test directory"));
  QFile File(Path);
  Require(
      File.open(QIODevice::WriteOnly | QIODevice::Truncate),
      QStringLiteral("could not open test file %1").arg(Path));
  Require(
      File.write(Contents) == Contents.size(),
      QStringLiteral("could not write test file %1").arg(Path));
}

QByteArray Read(const QString &Path) {
  QFile File(Path);
  Require(
      File.open(QIODevice::ReadOnly),
      QStringLiteral("could not read test file %1").arg(Path));
  return File.readAll();
}

QJsonObject InitialFirmwareState() {
  return QJsonObject{
      {QStringLiteral("entries"),
       QJsonArray{
           QJsonObject{
               {QStringLiteral("id"), QStringLiteral("{windows}")},
               {QStringLiteral("description"),
                QStringLiteral("Windows Boot Manager")},
               {QStringLiteral("path"),
                QStringLiteral("\\EFI\\Microsoft\\Boot\\bootmgfw.efi")}},
           QJsonObject{
               {QStringLiteral("id"), QStringLiteral("{linux}")},
               {QStringLiteral("description"), QStringLiteral("Linux")},
               {QStringLiteral("path"),
                QStringLiteral("\\EFI\\linux\\shimx64.efi")}}}},
      {QStringLiteral("order"),
       QJsonArray{QStringLiteral("{windows}"), QStringLiteral("{linux}")}}};
}

void WriteFirmwareState(const QString &Path, const QJsonObject &State) {
  Write(Path, QJsonDocument(State).toJson(QJsonDocument::Indented));
}

QJsonObject ReadFirmwareState(const QString &Path) {
  const QJsonDocument Document = QJsonDocument::fromJson(Read(Path));
  Require(Document.isObject(), QStringLiteral("firmware test state is invalid"));
  return Document.object();
}

void RequireSuccess(const TransactionResult &Result, const QString &Operation) {
  Require(
      Result.Success,
      QStringLiteral("%1 failed: %2").arg(Operation, Result.Message));
}

void RequireRollback(
    const TransactionResult &Result,
    const QString &Operation) {
  Require(!Result.Success, QStringLiteral("%1 unexpectedly succeeded").arg(Operation));
  Require(
      Result.RolledBack,
      QStringLiteral("%1 did not roll back: %2").arg(Operation, Result.Message));
}

int ApexEntryCount(const QJsonObject &State) {
  int Count = 0;
  for (const QJsonValue &Value : State.value(QStringLiteral("entries")).toArray()) {
    if (Value.toObject().value(QStringLiteral("path")).toString().compare(
            QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"),
            Qt::CaseInsensitive) == 0) {
      ++Count;
    }
  }
  return Count;
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication Application(argc, argv);
  QTemporaryDir Sandbox(
      QDir(QDir::tempPath())
          .filePath(QStringLiteral("apex32-windows-transaction-XXXXXX")));
  Require(Sandbox.isValid(), QStringLiteral("could not create transaction sandbox"));

  const QString Esp = QDir(Sandbox.path()).filePath(QStringLiteral("esp"));
  const QString StorePath =
      QDir(Sandbox.path()).filePath(QStringLiteral("firmware-store.json"));
  const QString Source =
      QDir(Sandbox.path()).filePath(QStringLiteral("Apex32BootManager.efi"));
  const QString Sentinel =
      QDir(Sandbox.path()).filePath(QStringLiteral("outside-sentinel"));
  Require(QDir().mkpath(Esp), QStringLiteral("could not create mock ESP"));
  WriteFirmwareState(StorePath, InitialFirmwareState());
  Write(Source, QByteArrayLiteral("MZ-APEX32-V1"));
  Write(Sentinel, QByteArrayLiteral("UNCHANGED"));

  const QString OriginalFirmwarePath =
      QDir(Esp).filePath(QStringLiteral("EFI/APEX32/Apex32BootManager.efi"));
  const QString OriginalConfigurationPath =
      QDir(Esp).filePath(QStringLiteral("EFI/APEX32/apex32.cfg"));
  const QByteArray OriginalFirmware = QByteArrayLiteral("MZ-ORIGINAL-APEX32");
  const QByteArray OriginalConfiguration = QByteArrayLiteral("original=1\n");
  Write(OriginalFirmwarePath, OriginalFirmware);
  Write(OriginalConfigurationPath, OriginalConfiguration);

  FileFirmwareStore Store(StorePath);
  TransactionEngine Engine(Esp, &Store);
  const QByteArray ConfigurationV1 =
      QByteArrayLiteral("schema=1\ndefault=APEX32\n");
  RequireSuccess(
      Engine.Install(Source, ConfigurationV1), QStringLiteral("initial install"));
  Require(
      Read(Engine.FirmwarePath()) == QByteArrayLiteral("MZ-APEX32-V1"),
      QStringLiteral("installed firmware did not match source"));
  Require(
      Read(Engine.ConfigurationPath()) == ConfigurationV1,
      QStringLiteral("installed configuration did not match"));
  Require(
      QFileInfo::exists(Engine.RecoveryStatePath()),
      QStringLiteral("recovery state was not persisted"));
  const QString FirmwareBackup = QDir(Esp).filePath(
      QStringLiteral("EFI/APEX32/recovery/firmware.before-community"));
  const QString ConfigurationBackup = QDir(Esp).filePath(
      QStringLiteral("EFI/APEX32/recovery/config.before-community"));
  Require(
      Read(FirmwareBackup) == OriginalFirmware &&
          Read(ConfigurationBackup) == OriginalConfiguration,
      QStringLiteral("immutable recovery backups were not created"));
  Require(
      ReadFirmwareState(StorePath)
              .value(QStringLiteral("order"))
              .toArray()
              .first()
              .toString() == QStringLiteral("{apex32-disposable-test}"),
      QStringLiteral("APEX32 was not promoted to first"));

  const QByteArray RecoveryStateBeforeReinstall = Read(Engine.RecoveryStatePath());
  Write(Source, QByteArrayLiteral("MZ-APEX32-V2"));
  const QByteArray ConfigurationV2 =
      QByteArrayLiteral("schema=1\ndefault=WINDOWS\n");
  RequireSuccess(
      Engine.Install(Source, ConfigurationV2), QStringLiteral("idempotent reinstall"));
  Require(
      Read(FirmwareBackup) == OriginalFirmware &&
          Read(ConfigurationBackup) == OriginalConfiguration &&
          Read(Engine.RecoveryStatePath()) == RecoveryStateBeforeReinstall,
      QStringLiteral("reinstall replaced immutable recovery state"));
  Require(
      ApexEntryCount(ReadFirmwareState(StorePath)) == 1,
      QStringLiteral("reinstall created a duplicate firmware entry"));

  const QByteArray InstalledFirmware = Read(Engine.FirmwarePath());
  const QByteArray InstalledConfiguration = Read(Engine.ConfigurationPath());
  const QByteArray InstalledState = Read(Engine.RecoveryStatePath());
  const QJsonObject InstalledFirmwareState = ReadFirmwareState(StorePath);
  Write(Source, QByteArrayLiteral("MZ-APEX32-V3"));

  FailurePlan AfterFiles;
  AfterFiles.AfterFileCommit = true;
  RequireRollback(
      Engine.Install(
          Source,
          QByteArrayLiteral("schema=1\ndefault=FAIL-AFTER-FILES\n"),
          AfterFiles),
      QStringLiteral("post-file-commit failure"));
  Require(
      Read(Engine.FirmwarePath()) == InstalledFirmware &&
          Read(Engine.ConfigurationPath()) == InstalledConfiguration &&
          ReadFirmwareState(StorePath) == InstalledFirmwareState,
      QStringLiteral("post-file-commit failure did not restore all state"));

  Store.FailNextPromote();
  RequireRollback(
      Engine.Install(Source, QByteArrayLiteral("schema=1\ndefault=FAIL\n")),
      QStringLiteral("firmware promotion failure"));
  Require(
      Read(Engine.FirmwarePath()) == InstalledFirmware &&
          Read(Engine.ConfigurationPath()) == InstalledConfiguration &&
          Read(Engine.RecoveryStatePath()) == InstalledState &&
          ReadFirmwareState(StorePath) == InstalledFirmwareState,
      QStringLiteral("promotion failure did not restore installed state"));

  FailurePlan AfterFirmware;
  AfterFirmware.AfterFirmwareCommit = true;
  RequireRollback(
      Engine.Install(
          Source,
          QByteArrayLiteral("schema=1\ndefault=FAIL-AFTER-FIRMWARE\n"),
          AfterFirmware),
      QStringLiteral("post-promotion failure"));
  Require(
      Read(Engine.FirmwarePath()) == InstalledFirmware &&
          ReadFirmwareState(StorePath) == InstalledFirmwareState,
      QStringLiteral("post-promotion failure did not restore all state"));

  Store.FailNextRestore();
  RequireRollback(
      Engine.Restore(), QStringLiteral("injected firmware restore failure"));
  Require(
      Read(Engine.FirmwarePath()) == InstalledFirmware &&
          ReadFirmwareState(StorePath) == InstalledFirmwareState,
      QStringLiteral("failed restore did not retain the installed state"));

  FailurePlan DuringRestore;
  DuringRestore.DuringRestore = true;
  RequireRollback(
      Engine.Restore(DuringRestore), QStringLiteral("mid-restore failure"));
  Require(
      Read(Engine.FirmwarePath()) == InstalledFirmware &&
          Read(Engine.RecoveryStatePath()) == InstalledState &&
          ReadFirmwareState(StorePath) == InstalledFirmwareState,
      QStringLiteral("mid-restore rollback was incomplete"));

  RequireSuccess(Engine.Restore(), QStringLiteral("full restore"));
  Require(
      Read(OriginalFirmwarePath) == OriginalFirmware &&
          Read(OriginalConfigurationPath) == OriginalConfiguration,
      QStringLiteral("full restore did not recover original files"));
  Require(
      ReadFirmwareState(StorePath) == InitialFirmwareState(),
      QStringLiteral("full restore did not recover original boot order"));
  Require(
      !QFileInfo::exists(Engine.RecoveryStatePath()) &&
          !QFileInfo::exists(FirmwareBackup) &&
          !QFileInfo::exists(ConfigurationBackup),
      QStringLiteral("full restore left recovery metadata behind"));

  const QString EmptyEsp =
      QDir(Sandbox.path()).filePath(QStringLiteral("empty-esp"));
  const QString EmptyStorePath =
      QDir(Sandbox.path()).filePath(QStringLiteral("empty-firmware-store.json"));
  Require(QDir().mkpath(EmptyEsp), QStringLiteral("could not create empty mock ESP"));
  WriteFirmwareState(EmptyStorePath, InitialFirmwareState());
  FileFirmwareStore EmptyStore(EmptyStorePath);
  TransactionEngine EmptyEngine(EmptyEsp, &EmptyStore);
  RequireSuccess(
      EmptyEngine.Install(Source, ConfigurationV1),
      QStringLiteral("clean install"));
  RequireSuccess(EmptyEngine.Restore(), QStringLiteral("clean install restore"));
  Require(
      !QFileInfo::exists(EmptyEngine.FirmwarePath()) &&
          !QFileInfo::exists(EmptyEngine.ConfigurationPath()),
      QStringLiteral("restore retained files that did not exist originally"));

  const QByteArray StoreBeforeInvalid = Read(EmptyStorePath);
  Write(Source, QByteArrayLiteral("NOT-A-PE-IMAGE"));
  const TransactionResult Invalid = EmptyEngine.Install(Source, ConfigurationV1);
  Require(!Invalid.Success, QStringLiteral("invalid firmware was accepted"));
  Require(
      Read(EmptyStorePath) == StoreBeforeInvalid &&
          !QFileInfo::exists(EmptyEngine.RecoveryStatePath()),
      QStringLiteral("invalid firmware changed transaction state"));
  Require(
      Read(Sentinel) == QByteArrayLiteral("UNCHANGED"),
      QStringLiteral("transaction escaped the declared temporary ESP"));

  std::cout
      << "PASS: Windows transaction install, reinstall, rollback, restore, "
         "and temporary-ESP confinement\n";
  return 0;
}
