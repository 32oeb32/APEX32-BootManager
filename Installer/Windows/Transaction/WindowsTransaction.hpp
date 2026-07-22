#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace Apex32::WindowsInstaller {

int TransactionSchemaVersion();

struct FailurePlan {
  bool AfterFileCommit = false;
  bool AfterFirmwareCommit = false;
  bool DuringRestore = false;
};

struct TransactionResult {
  bool Success = false;
  bool RolledBack = false;
  QString Message;
};

class FirmwareStore {
 public:
  virtual ~FirmwareStore() = default;

  virtual bool Snapshot(QJsonObject *State, QString *Error) = 0;
  virtual bool PromoteApex(const QString &LoaderPath, QString *Error) = 0;
  virtual bool Restore(const QJsonObject &State, QString *Error) = 0;
  virtual bool Matches(const QJsonObject &State, QString *Error) = 0;
  virtual bool ApexIsFirst(const QString &LoaderPath, QString *Error) = 0;
};

// This store is used only by the disposable Windows lifecycle test. It models
// firmware entries in one explicitly supplied JSON file and never calls a
// Windows boot-management API.
class FileFirmwareStore final : public FirmwareStore {
 public:
  explicit FileFirmwareStore(QString StorePath);

  void FailNextPromote();
  void FailNextRestore();

  bool Snapshot(QJsonObject *State, QString *Error) override;
  bool PromoteApex(const QString &LoaderPath, QString *Error) override;
  bool Restore(const QJsonObject &State, QString *Error) override;
  bool Matches(const QJsonObject &State, QString *Error) override;
  bool ApexIsFirst(const QString &LoaderPath, QString *Error) override;

 private:
  bool Load(QJsonObject *State, QString *Error) const;
  bool Save(const QJsonObject &State, QString *Error) const;

  QString StorePath_;
  bool FailPromote_ = false;
  bool FailRestore_ = false;
};

class TransactionEngine final {
 public:
  TransactionEngine(QString EspRoot, FirmwareStore *Store);

  TransactionResult Install(
      const QString &FirmwareSource,
      const QByteArray &Configuration,
      const FailurePlan &Failure = {});
  TransactionResult Restore(const FailurePlan &Failure = {});

  QString FirmwarePath() const;
  QString ConfigurationPath() const;
  QString RecoveryStatePath() const;

 private:
  QString EspRoot_;
  FirmwareStore *Store_ = nullptr;
};

}  // namespace Apex32::WindowsInstaller
