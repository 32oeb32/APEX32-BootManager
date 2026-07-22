#pragma once

#include "WindowsTransaction.hpp"

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace Apex32::WindowsInstaller {

struct FirmwareVariableValue {
  bool Exists = false;
  QByteArray Data;
  quint32 Attributes = 0;
};

// The native store depends on this narrow interface so its load-option and
// BootOrder logic can be tested without accessing the runner or host firmware.
class FirmwareVariableAccess {
 public:
  virtual ~FirmwareVariableAccess() = default;

  virtual bool Read(
      const QString &Name,
      FirmwareVariableValue *Value,
      QString *Error) = 0;
  virtual bool Write(
      const QString &Name,
      const QByteArray &Data,
      quint32 Attributes,
      QString *Error) = 0;
  virtual bool Remove(const QString &Name, QString *Error) = 0;
};

class WindowsFirmwareVariableAccess final : public FirmwareVariableAccess {
 public:
  bool Prepare(QString *Error);
  bool Read(
      const QString &Name,
      FirmwareVariableValue *Value,
      QString *Error) override;
  bool Write(
      const QString &Name,
      const QByteArray &Data,
      quint32 Attributes,
      QString *Error) override;
  bool Remove(const QString &Name, QString *Error) override;

 private:
  bool Prepared_ = false;
};

class NativeFirmwareStore final : public FirmwareStore {
 public:
  explicit NativeFirmwareStore(FirmwareVariableAccess *Variables);

  bool Snapshot(QJsonObject *State, QString *Error) override;
  bool PromoteApex(const QString &LoaderPath, QString *Error) override;
  bool Restore(const QJsonObject &State, QString *Error) override;
  bool Matches(const QJsonObject &State, QString *Error) override;
  bool ApexIsFirst(const QString &LoaderPath, QString *Error) override;

 private:
  FirmwareVariableAccess *Variables_ = nullptr;
  int PendingBootNumber_ = -1;
  bool PendingHadApex_ = false;
  QByteArray PendingTemplate_;
};

bool ReadSecureBootState(
    FirmwareVariableAccess *Variables,
    bool *Enabled,
    QString *Error);

}  // namespace Apex32::WindowsInstaller
