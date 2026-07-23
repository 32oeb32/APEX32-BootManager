#include "Installer/Windows/Transaction/WindowsFirmwareStore.hpp"

#include <QCoreApplication>
#include <QHash>
#include <QJsonObject>
#include <QString>

#include <cstdlib>
#include <initializer_list>
#include <iostream>

namespace {

using Apex32::WindowsInstaller::FirmwareVariableAccess;
using Apex32::WindowsInstaller::FirmwareVariableValue;
using Apex32::WindowsInstaller::NativeFirmwareStore;
using Apex32::WindowsInstaller::ReadSecureBootState;

[[noreturn]] void Fail(const QString &Message) {
  std::cerr << "FAIL: " << Message.toStdString() << '\n';
  std::exit(1);
}

void Require(bool Condition, const QString &Message) {
  if (!Condition) {
    Fail(Message);
  }
}

void AppendLe16(QByteArray *Data, quint16 Value) {
  Data->append(static_cast<char>(Value & 0xffU));
  Data->append(static_cast<char>((Value >> 8) & 0xffU));
}

void AppendLe32(QByteArray *Data, quint32 Value) {
  Data->append(static_cast<char>(Value & 0xffU));
  Data->append(static_cast<char>((Value >> 8) & 0xffU));
  Data->append(static_cast<char>((Value >> 16) & 0xffU));
  Data->append(static_cast<char>((Value >> 24) & 0xffU));
}

void AppendUtf16(QByteArray *Data, const QString &Text) {
  for (const QChar Character : Text) {
    AppendLe16(Data, Character.unicode());
  }
  AppendLe16(Data, 0);
}

QByteArray BootOrder(std::initializer_list<quint16> Numbers) {
  QByteArray Data;
  for (const quint16 Number : Numbers) {
    AppendLe16(&Data, Number);
  }
  return Data;
}

QByteArray LoadOption(
    const QString &Description,
    const QString &Path,
    quint32 Attributes = 1,
    quint8 PartitionIdentity = 1) {
  QByteArray DevicePath;
  DevicePath.append(static_cast<char>(0x04));
  DevicePath.append(static_cast<char>(0x01));
  AppendLe16(&DevicePath, 42);
  QByteArray HardDrivePayload(38, '\0');
  // The GPT signature begins 20 bytes into the hard-drive node payload.
  // A distinct byte gives the fixture a distinct ESP identity.
  HardDrivePayload[20] = static_cast<char>(PartitionIdentity);
  DevicePath.append(HardDrivePayload);

  QByteArray FilePath;
  FilePath.append(static_cast<char>(0x04));
  FilePath.append(static_cast<char>(0x04));
  const int FilePathLength = 4 + ((Path.size() + 1) * 2);
  AppendLe16(&FilePath, static_cast<quint16>(FilePathLength));
  AppendUtf16(&FilePath, Path);
  DevicePath.append(FilePath);
  DevicePath.append(QByteArray::fromHex("7fff0400"));

  QByteArray Option;
  AppendLe32(&Option, Attributes);
  AppendLe16(&Option, static_cast<quint16>(DevicePath.size()));
  AppendUtf16(&Option, Description);
  Option.append(DevicePath);
  return Option;
}

class FakeVariables final : public FirmwareVariableAccess {
 public:
  bool Read(
      const QString &Name,
      FirmwareVariableValue *Value,
      QString *) override {
    if (Values.contains(Name)) {
      *Value = Values.value(Name);
    } else {
      *Value = {};
    }
    return true;
  }

  bool Write(
      const QString &Name,
      const QByteArray &Data,
      quint32 Attributes,
      QString *Error) override {
    if (Name == FailWriteName) {
      FailWriteName.clear();
      if (Error != nullptr) {
        *Error = QStringLiteral("Injected variable write failure.");
      }
      return false;
    }
    Values.insert(Name, FirmwareVariableValue{true, Data, Attributes});
    return true;
  }

  bool Remove(const QString &Name, QString *) override {
    Values.remove(Name);
    return true;
  }

  void Set(const QString &Name, const QByteArray &Data, quint32 Attributes = 7) {
    Values.insert(Name, FirmwareVariableValue{true, Data, Attributes});
  }

  QHash<QString, FirmwareVariableValue> Values;
  QString FailWriteName;
};

FakeVariables InitialVariables() {
  FakeVariables Variables;
  Variables.Set(QStringLiteral("BootOrder"), BootOrder({1, 2}));
  Variables.Set(
      QStringLiteral("Boot0001"),
      LoadOption(
          QStringLiteral("Windows Boot Manager"),
          QStringLiteral("\\EFI\\Microsoft\\Boot\\bootmgfw.efi")));
  Variables.Set(
      QStringLiteral("Boot0002"),
      LoadOption(
          QStringLiteral("Linux"),
          QStringLiteral("\\EFI\\linux\\shimx64.efi"),
          1,
          2));
  return Variables;
}

}  // namespace

int main(int argc, char **argv) {
  QCoreApplication Application(argc, argv);
  QString Error;

  FakeVariables Variables = InitialVariables();
  const QByteArray OriginalOrder = Variables.Values.value(
      QStringLiteral("BootOrder")).Data;
  NativeFirmwareStore Store(&Variables);
  QJsonObject Snapshot;
  Require(Store.Snapshot(&Snapshot, &Error), Error);
  Require(
      Snapshot.value(QStringLiteral("candidateNumber")).toInt() == 3,
      QStringLiteral("the first unused boot number was not reserved"));
  Require(
      Store.PromoteApex(
          QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"), &Error),
      Error);
  Require(
      Variables.Values.value(QStringLiteral("BootOrder")).Data ==
          BootOrder({3, 1, 2}),
      QStringLiteral("APEX32 was not promoted without dropping entries"));
  Require(
      Variables.Values.contains(QStringLiteral("Boot0003")),
      QStringLiteral("the APEX32 boot option was not created"));
  Require(
      Store.ApexIsFirst(
          QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"), &Error),
      Error);
  Require(Store.Restore(Snapshot, &Error), Error);
  Require(
      Variables.Values.value(QStringLiteral("BootOrder")).Data == OriginalOrder &&
          !Variables.Values.contains(QStringLiteral("Boot0003")),
      QStringLiteral("restore did not recover the exact original variables"));

  Variables = InitialVariables();
  const QByteArray ExistingApex = LoadOption(
      QStringLiteral("Existing APEX32"),
      QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"));
  Variables.Set(QStringLiteral("Boot0003"), ExistingApex);
  Variables.Set(QStringLiteral("BootOrder"), BootOrder({1, 3, 2}));
  NativeFirmwareStore ExistingStore(&Variables);
  QJsonObject ExistingSnapshot;
  Require(ExistingStore.Snapshot(&ExistingSnapshot, &Error), Error);
  Require(
      ExistingSnapshot.value(QStringLiteral("hadApex")).toBool(),
      QStringLiteral("an existing APEX32 entry was not recognized"));
  Require(
      ExistingStore.PromoteApex(
          QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"), &Error),
      Error);
  Require(
      Variables.Values.value(QStringLiteral("BootOrder")).Data ==
          BootOrder({3, 1, 2}),
      QStringLiteral("the existing APEX32 entry was not promoted"));
  Require(ExistingStore.Restore(ExistingSnapshot, &Error), Error);
  Require(
      Variables.Values.value(QStringLiteral("Boot0003")).Data == ExistingApex &&
          Variables.Values.value(QStringLiteral("BootOrder")).Data ==
              BootOrder({1, 3, 2}),
      QStringLiteral("an existing APEX32 entry was not restored exactly"));

  Variables = InitialVariables();
  const QByteArray OtherEspApex = LoadOption(
      QStringLiteral("APEX32 on Linux ESP"),
      QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"),
      1,
      2);
  Variables.Set(QStringLiteral("Boot0003"), OtherEspApex);
  Variables.Set(QStringLiteral("BootOrder"), BootOrder({3, 1, 2}));
  NativeFirmwareStore CrossEspStore(&Variables);
  QJsonObject CrossEspSnapshot;
  Require(CrossEspStore.Snapshot(&CrossEspSnapshot, &Error), Error);
  Require(
      !CrossEspSnapshot.value(QStringLiteral("hadApex")).toBool() &&
          CrossEspSnapshot.value(QStringLiteral("candidateNumber")).toInt() == 4,
      QStringLiteral("an APEX32 entry from another ESP was reused"));
  Require(
      CrossEspStore.PromoteApex(
          QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"), &Error),
      Error);
  const QByteArray ExpectedWindowsEspApex = LoadOption(
      QStringLiteral("APEX32 Secure Gateway"),
      QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"),
      1,
      1);
  Require(
      Variables.Values.value(QStringLiteral("BootOrder")).Data ==
              BootOrder({4, 3, 1, 2}) &&
          Variables.Values.value(QStringLiteral("Boot0004")).Data ==
              ExpectedWindowsEspApex &&
          Variables.Values.value(QStringLiteral("Boot0003")).Data == OtherEspApex,
      QStringLiteral(
          "the Windows-ESP entry was not isolated from the other-ESP entry"));
  Require(
      CrossEspStore.ApexIsFirst(
          QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"), &Error),
      Error);
  Require(CrossEspStore.Restore(CrossEspSnapshot, &Error), Error);
  Require(
      Variables.Values.value(QStringLiteral("BootOrder")).Data ==
              BootOrder({3, 1, 2}) &&
          !Variables.Values.contains(QStringLiteral("Boot0004")) &&
          Variables.Values.value(QStringLiteral("Boot0003")).Data == OtherEspApex,
      QStringLiteral(
          "cross-ESP restore did not preserve the unrelated APEX32 entry"));

  Variables = InitialVariables();
  const QByteArray InactiveApex = LoadOption(
      QStringLiteral("Inactive APEX32"),
      QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"),
      0);
  Variables.Set(QStringLiteral("Boot0003"), InactiveApex, 11);
  Variables.Set(QStringLiteral("BootOrder"), BootOrder({3, 1, 2}), 13);
  NativeFirmwareStore InactiveStore(&Variables);
  QJsonObject InactiveSnapshot;
  Require(InactiveStore.Snapshot(&InactiveSnapshot, &Error), Error);
  Require(
      InactiveStore.PromoteApex(
          QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"), &Error),
      Error);
  Require(
      (static_cast<quint8>(Variables.Values.value(
           QStringLiteral("Boot0003")).Data.at(0)) & 1U) != 0,
      QStringLiteral("an existing inactive APEX32 entry was not activated"));
  Require(InactiveStore.Restore(InactiveSnapshot, &Error), Error);
  Require(
      Variables.Values.value(QStringLiteral("Boot0003")).Data == InactiveApex &&
          Variables.Values.value(QStringLiteral("Boot0003")).Attributes == 11 &&
          Variables.Values.value(QStringLiteral("BootOrder")).Attributes == 13,
      QStringLiteral("restore did not preserve exact variable attributes"));

  Variables = InitialVariables();
  NativeFirmwareStore FailureStore(&Variables);
  QJsonObject FailureSnapshot;
  Require(FailureStore.Snapshot(&FailureSnapshot, &Error), Error);
  Variables.FailWriteName = QStringLiteral("BootOrder");
  Require(
      !FailureStore.PromoteApex(
          QStringLiteral("\\EFI\\APEX32\\Apex32BootManager.efi"), &Error),
      QStringLiteral("the injected BootOrder failure was ignored"));
  Error.clear();
  Require(FailureStore.Restore(FailureSnapshot, &Error), Error);
  Require(
      Variables.Values.value(QStringLiteral("BootOrder")).Data == OriginalOrder &&
          !Variables.Values.contains(QStringLiteral("Boot0003")),
      QStringLiteral("the failed promotion did not roll back cleanly"));

  Variables = InitialVariables();
  Variables.Set(
      QStringLiteral("Boot0001"),
      LoadOption(
          QStringLiteral("Generic firmware entry"),
          QStringLiteral("\\EFI\\vendor\\loader.efi")));
  NativeFirmwareStore MissingAnchorStore(&Variables);
  QJsonObject MissingAnchorSnapshot;
  Error.clear();
  Require(
      !MissingAnchorStore.Snapshot(&MissingAnchorSnapshot, &Error) &&
          Error.contains(QStringLiteral("Windows system ESP")),
      QStringLiteral("a missing Windows-ESP identity did not fail closed"));

  Variables = InitialVariables();
  Variables.Set(
      QStringLiteral("Boot0003"),
      LoadOption(
          QStringLiteral("Other Windows Boot Manager"),
          QStringLiteral("\\EFI\\Microsoft\\Boot\\bootmgfw.efi"),
          1,
          3));
  Variables.Set(QStringLiteral("BootOrder"), BootOrder({1, 3, 2}));
  NativeFirmwareStore AmbiguousAnchorStore(&Variables);
  QJsonObject AmbiguousAnchorSnapshot;
  Error.clear();
  Require(
      !AmbiguousAnchorStore.Snapshot(&AmbiguousAnchorSnapshot, &Error) &&
          Error.contains(QStringLiteral("Multiple Windows Boot Manager")),
      QStringLiteral("ambiguous Windows-ESP identities did not fail closed"));

  Variables.Set(QStringLiteral("SecureBoot"), QByteArray(1, '\1'));
  bool SecureBoot = false;
  Require(ReadSecureBootState(&Variables, &SecureBoot, &Error), Error);
  Require(SecureBoot, QStringLiteral("Secure Boot state was not detected"));

  std::cout
      << "PASS: native Windows firmware store created, promoted, reused, "
         "isolated multiple ESPs, rolled back, and restored Boot variables\n";
  return 0;
}
