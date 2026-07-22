#include "WindowsFirmwareStore.hpp"

#include <QJsonValue>
#include <QSet>
#include <QVector>

#include <windows.h>

#include <limits>

namespace Apex32::WindowsInstaller {
namespace {

constexpr wchar_t kGlobalVariableGuid[] =
    L"{8BE4DF61-93CA-11d2-AA0D-00E098032B8C}";
constexpr quint32 kBootVariableAttributes = 0x00000007U;
constexpr qsizetype kMaximumVariableBytes = 128 * 1024;
constexpr auto kApexLoaderPath = "\\EFI\\APEX32\\Apex32BootManager.efi";
constexpr quint8 kMediaDevicePath = 0x04;
constexpr quint8 kHardDriveDevicePath = 0x01;
constexpr quint8 kFilePathDevicePath = 0x04;

bool SetError(QString *Error, const QString &Message) {
  if (Error != nullptr) {
    *Error = Message;
  }
  return false;
}

QString WindowsError(const QString &Operation, DWORD Code) {
  wchar_t *Buffer = nullptr;
  const DWORD Length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER |
          FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      nullptr,
      Code,
      0,
      reinterpret_cast<wchar_t *>(&Buffer),
      0,
      nullptr);
  QString Detail;
  if (Length != 0 && Buffer != nullptr) {
    Detail = QString::fromWCharArray(Buffer, static_cast<qsizetype>(Length))
                 .trimmed();
  }
  if (Buffer != nullptr) {
    LocalFree(Buffer);
  }
  if (Detail.isEmpty()) {
    Detail = QStringLiteral("Windows error %1").arg(Code);
  }
  return QStringLiteral("%1: %2").arg(Operation, Detail);
}

quint16 ReadLe16(const QByteArray &Data, qsizetype Offset) {
  return static_cast<quint16>(
      static_cast<quint8>(Data.at(Offset)) |
      (static_cast<quint16>(static_cast<quint8>(Data.at(Offset + 1))) << 8));
}

quint32 ReadLe32(const QByteArray &Data, qsizetype Offset) {
  return static_cast<quint32>(
      static_cast<quint8>(Data.at(Offset)) |
      (static_cast<quint32>(static_cast<quint8>(Data.at(Offset + 1))) << 8) |
      (static_cast<quint32>(static_cast<quint8>(Data.at(Offset + 2))) << 16) |
      (static_cast<quint32>(static_cast<quint8>(Data.at(Offset + 3))) << 24));
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

QString NormalizeLoaderPath(QString Path) {
  Path.replace(QLatin1Char('/'), QLatin1Char('\\'));
  while (Path.contains(QStringLiteral("\\\\"))) {
    Path.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
  }
  if (!Path.startsWith(QLatin1Char('\\'))) {
    Path.prepend(QLatin1Char('\\'));
  }
  return Path;
}

struct LoadOptionLayout {
  qsizetype DevicePathStart = 0;
  qsizetype DevicePathEnd = 0;
  qsizetype FilePathNodeStart = 0;
  qsizetype FilePathNodeLength = 0;
};

bool ParseLoadOptionLayout(
    const QByteArray &Option,
    LoadOptionLayout *Layout,
    QString *Error) {
  if (Option.size() < 12) {
    return SetError(Error, QStringLiteral("The firmware load option is truncated."));
  }
  const quint16 DevicePathLength = ReadLe16(Option, 4);
  qsizetype Cursor = 6;
  bool DescriptionEnded = false;
  while (Cursor + 1 < Option.size()) {
    if (ReadLe16(Option, Cursor) == 0) {
      Cursor += 2;
      DescriptionEnded = true;
      break;
    }
    Cursor += 2;
  }
  if (!DescriptionEnded || DevicePathLength < 4 ||
      Cursor > Option.size() - DevicePathLength) {
    return SetError(
        Error, QStringLiteral("The firmware load option has invalid bounds."));
  }

  Layout->DevicePathStart = Cursor;
  Layout->DevicePathEnd = Cursor + DevicePathLength;
  bool HardDriveSeen = false;
  while (Cursor < Layout->DevicePathEnd) {
    if (Cursor + 4 > Layout->DevicePathEnd) {
      return SetError(Error, QStringLiteral("A device-path node is truncated."));
    }
    const quint8 Type = static_cast<quint8>(Option.at(Cursor));
    const quint8 SubType = static_cast<quint8>(Option.at(Cursor + 1));
    const quint16 Length = ReadLe16(Option, Cursor + 2);
    if (Length < 4 || Cursor > Layout->DevicePathEnd - Length) {
      return SetError(Error, QStringLiteral("A device-path node has invalid bounds."));
    }
    if (Type == kMediaDevicePath && SubType == kHardDriveDevicePath) {
      HardDriveSeen = true;
    }
    if (Type == kMediaDevicePath && SubType == kFilePathDevicePath) {
      if (!HardDriveSeen || Length < 6 || ((Length - 4) % 2) != 0) {
        return SetError(
            Error, QStringLiteral("The EFI file-path node is invalid."));
      }
      Layout->FilePathNodeStart = Cursor;
      Layout->FilePathNodeLength = Length;
      return true;
    }
    Cursor += Length;
  }
  return SetError(
      Error, QStringLiteral("The load option has no ESP file-path node."));
}

bool LoadOptionPath(
    const QByteArray &Option,
    QString *Path,
    QString *Error) {
  LoadOptionLayout Layout;
  if (!ParseLoadOptionLayout(Option, &Layout, Error)) {
    return false;
  }
  const qsizetype Begin = Layout.FilePathNodeStart + 4;
  const qsizetype End =
      Layout.FilePathNodeStart + Layout.FilePathNodeLength;
  QString Result;
  for (qsizetype Cursor = Begin; Cursor + 1 < End; Cursor += 2) {
    const quint16 Character = ReadLe16(Option, Cursor);
    if (Character == 0) {
      *Path = NormalizeLoaderPath(Result);
      return true;
    }
    Result.append(QChar(Character));
  }
  return SetError(
      Error, QStringLiteral("The EFI file-path node is not terminated."));
}

bool BuildApexLoadOption(
    const QByteArray &Template,
    const QString &LoaderPath,
    QByteArray *Result,
    QString *Error) {
  LoadOptionLayout Layout;
  if (!ParseLoadOptionLayout(Template, &Layout, Error)) {
    return false;
  }

  const QString Normalized = NormalizeLoaderPath(LoaderPath);
  QByteArray FilePathNode;
  FilePathNode.append(static_cast<char>(kMediaDevicePath));
  FilePathNode.append(static_cast<char>(kFilePathDevicePath));
  const qsizetype NodeLength = 4 + ((Normalized.size() + 1) * 2);
  if (NodeLength > std::numeric_limits<quint16>::max()) {
    return SetError(Error, QStringLiteral("The APEX32 loader path is too long."));
  }
  AppendLe16(&FilePathNode, static_cast<quint16>(NodeLength));
  AppendUtf16(&FilePathNode, Normalized);

  const QByteArray Prefix = Template.mid(
      Layout.DevicePathStart,
      Layout.FilePathNodeStart - Layout.DevicePathStart);
  const qsizetype OldFilePathEnd =
      Layout.FilePathNodeStart + Layout.FilePathNodeLength;
  const QByteArray Suffix = Template.mid(
      OldFilePathEnd, Layout.DevicePathEnd - OldFilePathEnd);
  const qsizetype NewDevicePathLength =
      Prefix.size() + FilePathNode.size() + Suffix.size();
  if (NewDevicePathLength > std::numeric_limits<quint16>::max()) {
    return SetError(Error, QStringLiteral("The APEX32 device path is too long."));
  }

  QByteArray Option;
  Option.reserve(
      6 + (32 * 2) + Prefix.size() + FilePathNode.size() + Suffix.size());
  AppendLe32(&Option, ReadLe32(Template, 0) | 1U);
  AppendLe16(&Option, static_cast<quint16>(NewDevicePathLength));
  AppendUtf16(&Option, QStringLiteral("APEX32 Secure Gateway"));
  Option.append(Prefix);
  Option.append(FilePathNode);
  Option.append(Suffix);
  if (Option.size() > kMaximumVariableBytes) {
    return SetError(Error, QStringLiteral("The generated boot option is too large."));
  }
  *Result = Option;
  return true;
}

QString BootVariableName(int Number) {
  const QString HexNumber = QString::number(Number, 16)
                                .rightJustified(4, QLatin1Char('0'))
                                .toUpper();
  return QStringLiteral("Boot%1").arg(HexNumber);
}

bool DecodeBootOrder(
    const FirmwareVariableValue &Variable,
    QVector<quint16> *Order,
    QString *Error) {
  if (!Variable.Exists || Variable.Data.isEmpty() ||
      (Variable.Data.size() % 2) != 0 ||
      Variable.Data.size() > kMaximumVariableBytes) {
    return SetError(Error, QStringLiteral("UEFI BootOrder is unavailable or invalid."));
  }
  Order->clear();
  Order->reserve(Variable.Data.size() / 2);
  for (qsizetype Offset = 0; Offset < Variable.Data.size(); Offset += 2) {
    Order->push_back(ReadLe16(Variable.Data, Offset));
  }
  return true;
}

QByteArray EncodeBootOrder(const QVector<quint16> &Order) {
  QByteArray Data;
  Data.reserve(Order.size() * 2);
  for (const quint16 Number : Order) {
    AppendLe16(&Data, Number);
  }
  return Data;
}

bool DecodeBase64(
    const QJsonObject &State,
    const QString &Key,
    QByteArray *Data,
    QString *Error) {
  const QString Encoded = State.value(Key).toString();
  const auto Decoded = QByteArray::fromBase64Encoding(
      Encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
  if (!Decoded) {
    return SetError(
        Error, QStringLiteral("Recovery field %1 is not valid base64.").arg(Key));
  }
  *Data = Decoded.decoded;
  return true;
}

struct NativeSnapshot {
  QByteArray BootOrder;
  quint32 BootOrderAttributes = 0;
  int CandidateNumber = -1;
  bool HadApex = false;
  QByteArray ApexOption;
  quint32 ApexAttributes = 0;
};

bool DecodeSnapshot(
    const QJsonObject &State,
    NativeSnapshot *Snapshot,
    QString *Error) {
  if (State.value(QStringLiteral("schema")).toInt() != 1 ||
      !State.value(QStringLiteral("hadApex")).isBool()) {
    return SetError(
        Error, QStringLiteral("The native firmware recovery schema is invalid."));
  }
  Snapshot->CandidateNumber =
      State.value(QStringLiteral("candidateNumber")).toInt(-1);
  Snapshot->HadApex = State.value(QStringLiteral("hadApex")).toBool();
  const qint64 OrderAttributes =
      State.value(QStringLiteral("bootOrderAttributes")).toInteger(-1);
  const qint64 ApexAttributes =
      State.value(QStringLiteral("apexAttributes")).toInteger(0);
  if (Snapshot->CandidateNumber < 0 ||
      Snapshot->CandidateNumber > std::numeric_limits<quint16>::max() ||
      OrderAttributes <= 0 ||
      OrderAttributes > std::numeric_limits<quint32>::max() ||
      ApexAttributes < 0 ||
      ApexAttributes > std::numeric_limits<quint32>::max() ||
      !DecodeBase64(
          State, QStringLiteral("bootOrder"), &Snapshot->BootOrder, Error)) {
    if (Error != nullptr && !Error->isEmpty()) {
      return false;
    }
    return SetError(
        Error, QStringLiteral("The native firmware recovery state is invalid."));
  }
  Snapshot->BootOrderAttributes = static_cast<quint32>(OrderAttributes);
  Snapshot->ApexAttributes = static_cast<quint32>(ApexAttributes);
  FirmwareVariableValue OrderVariable;
  OrderVariable.Exists = true;
  OrderVariable.Data = Snapshot->BootOrder;
  QVector<quint16> ParsedOrder;
  if (!DecodeBootOrder(OrderVariable, &ParsedOrder, Error)) {
    return false;
  }
  if (Snapshot->HadApex &&
      (!DecodeBase64(
           State, QStringLiteral("apexOption"), &Snapshot->ApexOption, Error) ||
       Snapshot->ApexOption.isEmpty() || Snapshot->ApexAttributes == 0)) {
    return false;
  }
  return true;
}

bool IsNotFound(DWORD Error) {
  return Error == ERROR_ENVVAR_NOT_FOUND || Error == ERROR_FILE_NOT_FOUND;
}

}  // namespace

bool WindowsFirmwareVariableAccess::Prepare(QString *Error) {
  if (Prepared_) {
    return true;
  }
  FIRMWARE_TYPE Type = FirmwareTypeUnknown;
  if (GetFirmwareType(&Type) == FALSE || Type != FirmwareTypeUefi) {
    return SetError(
        Error,
        QStringLiteral("Windows is not running in supported UEFI firmware mode."));
  }

  HANDLE Token = nullptr;
  if (OpenProcessToken(
          GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &Token) ==
      FALSE) {
    return SetError(
        Error, WindowsError(QStringLiteral("Cannot open the process token"), GetLastError()));
  }
  LUID Identifier{};
  if (LookupPrivilegeValueW(
          nullptr, SE_SYSTEM_ENVIRONMENT_NAME, &Identifier) == FALSE) {
    const DWORD Code = GetLastError();
    CloseHandle(Token);
    return SetError(
        Error, WindowsError(QStringLiteral("Cannot locate the firmware privilege"), Code));
  }
  TOKEN_PRIVILEGES Privileges{};
  Privileges.PrivilegeCount = 1;
  Privileges.Privileges[0].Luid = Identifier;
  Privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
  SetLastError(ERROR_SUCCESS);
  const BOOL Adjusted = AdjustTokenPrivileges(
      Token, FALSE, &Privileges, sizeof(Privileges), nullptr, nullptr);
  const DWORD AdjustError = GetLastError();
  CloseHandle(Token);
  if (Adjusted == FALSE || AdjustError != ERROR_SUCCESS) {
    return SetError(
        Error,
        WindowsError(
            QStringLiteral("Cannot enable the firmware environment privilege"),
            AdjustError));
  }
  Prepared_ = true;
  return true;
}

bool WindowsFirmwareVariableAccess::Read(
    const QString &Name,
    FirmwareVariableValue *Value,
    QString *Error) {
  if (Value == nullptr || Name.isEmpty() || !Prepare(Error)) {
    return false;
  }
  DWORD Capacity = 4096;
  while (Capacity <= static_cast<DWORD>(kMaximumVariableBytes)) {
    QByteArray Buffer(static_cast<qsizetype>(Capacity), Qt::Uninitialized);
    DWORD Attributes = 0;
    SetLastError(ERROR_SUCCESS);
    const DWORD Size = GetFirmwareEnvironmentVariableExW(
        reinterpret_cast<LPCWSTR>(Name.utf16()),
        kGlobalVariableGuid,
        Buffer.data(),
        Capacity,
        &Attributes);
    if (Size != 0) {
      Buffer.resize(static_cast<qsizetype>(Size));
      Value->Exists = true;
      Value->Data = Buffer;
      Value->Attributes = Attributes;
      return true;
    }
    const DWORD Code = GetLastError();
    if (IsNotFound(Code)) {
      *Value = {};
      return true;
    }
    if (Code != ERROR_INSUFFICIENT_BUFFER && Code != ERROR_MORE_DATA) {
      return SetError(
          Error,
          WindowsError(
              QStringLiteral("Cannot read firmware variable %1").arg(Name), Code));
    }
    Capacity *= 2;
  }
  return SetError(
      Error, QStringLiteral("Firmware variable %1 exceeds the safety limit.").arg(Name));
}

bool WindowsFirmwareVariableAccess::Write(
    const QString &Name,
    const QByteArray &Data,
    quint32 Attributes,
    QString *Error) {
  if (Name.isEmpty() || Data.isEmpty() || Data.size() > kMaximumVariableBytes ||
      !Prepare(Error)) {
    return Data.isEmpty()
               ? SetError(Error, QStringLiteral("Refusing to write an empty firmware variable."))
               : false;
  }
  if (SetFirmwareEnvironmentVariableExW(
          reinterpret_cast<LPCWSTR>(Name.utf16()),
          kGlobalVariableGuid,
          const_cast<char *>(Data.constData()),
          static_cast<DWORD>(Data.size()),
          Attributes) == FALSE) {
    return SetError(
        Error,
        WindowsError(
            QStringLiteral("Cannot write firmware variable %1").arg(Name),
            GetLastError()));
  }
  return true;
}

bool WindowsFirmwareVariableAccess::Remove(
    const QString &Name,
    QString *Error) {
  if (Name.isEmpty() || !Prepare(Error)) {
    return false;
  }
  if (SetFirmwareEnvironmentVariableExW(
          reinterpret_cast<LPCWSTR>(Name.utf16()),
          kGlobalVariableGuid,
          nullptr,
          0,
          0) == FALSE) {
    const DWORD Code = GetLastError();
    if (!IsNotFound(Code)) {
      return SetError(
          Error,
          WindowsError(
              QStringLiteral("Cannot remove firmware variable %1").arg(Name),
              Code));
    }
  }
  return true;
}

NativeFirmwareStore::NativeFirmwareStore(FirmwareVariableAccess *Variables)
    : Variables_(Variables) {}

bool NativeFirmwareStore::Snapshot(QJsonObject *State, QString *Error) {
  if (Variables_ == nullptr || State == nullptr) {
    return SetError(Error, QStringLiteral("The native firmware store is unavailable."));
  }
  FirmwareVariableValue OrderVariable;
  if (!Variables_->Read(QStringLiteral("BootOrder"), &OrderVariable, Error)) {
    return false;
  }
  QVector<quint16> Order;
  if (!DecodeBootOrder(OrderVariable, &Order, Error)) {
    return false;
  }

  int ApexNumber = -1;
  int ApexCount = 0;
  FirmwareVariableValue ApexVariable;
  QByteArray Template;
  QByteArray PreferredTemplate;
  QSet<quint16> OrderedNumbers;
  for (const quint16 Number : Order) {
    OrderedNumbers.insert(Number);
    FirmwareVariableValue Variable;
    if (!Variables_->Read(BootVariableName(Number), &Variable, Error)) {
      return false;
    }
    if (!Variable.Exists) {
      return SetError(
          Error,
          QStringLiteral("BootOrder references a missing Boot%1 variable.")
              .arg(Number, 4, 16, QLatin1Char('0')));
    }
    QString Path;
    QString ParseError;
    if (!LoadOptionPath(Variable.Data, &Path, &ParseError)) {
      continue;
    }
    if (Template.isEmpty()) {
      Template = Variable.Data;
    }
    if (Path.compare(
            QStringLiteral("\\EFI\\Microsoft\\Boot\\bootmgfw.efi"),
            Qt::CaseInsensitive) == 0) {
      PreferredTemplate = Variable.Data;
    }
    if (Path.compare(QString::fromLatin1(kApexLoaderPath), Qt::CaseInsensitive) ==
        0) {
      ++ApexCount;
      ApexNumber = Number;
      ApexVariable = Variable;
    }
  }
  if (ApexCount > 1) {
    return SetError(
        Error, QStringLiteral("Multiple active APEX32 firmware entries were found."));
  }
  if (OrderVariable.Attributes == 0 ||
      (ApexCount == 1 && ApexVariable.Attributes == 0)) {
    return SetError(
        Error,
        QStringLiteral("A required UEFI variable has invalid attributes."));
  }

  if (ApexNumber < 0) {
    for (int Number = 1; Number <= std::numeric_limits<quint16>::max(); ++Number) {
      if (OrderedNumbers.contains(static_cast<quint16>(Number))) {
        continue;
      }
      FirmwareVariableValue Candidate;
      if (!Variables_->Read(BootVariableName(Number), &Candidate, Error)) {
        return false;
      }
      if (!Candidate.Exists) {
        ApexNumber = Number;
        break;
      }
    }
  }
  if (ApexNumber < 0 ||
      (ApexCount == 0 && Template.isEmpty() && PreferredTemplate.isEmpty())) {
    return SetError(
        Error, QStringLiteral("No safe firmware boot entry or template is available."));
  }

  PendingBootNumber_ = ApexNumber;
  PendingHadApex_ = ApexCount == 1;
  PendingTemplate_ =
      !PreferredTemplate.isEmpty() ? PreferredTemplate : Template;
  State->insert(QStringLiteral("schema"), 1);
  State->insert(
      QStringLiteral("bootOrder"),
      QString::fromLatin1(OrderVariable.Data.toBase64()));
  State->insert(
      QStringLiteral("bootOrderAttributes"),
      static_cast<qint64>(OrderVariable.Attributes));
  State->insert(QStringLiteral("candidateNumber"), ApexNumber);
  State->insert(QStringLiteral("hadApex"), PendingHadApex_);
  State->insert(
      QStringLiteral("apexOption"),
      QString::fromLatin1(ApexVariable.Data.toBase64()));
  State->insert(
      QStringLiteral("apexAttributes"),
      static_cast<qint64>(ApexVariable.Attributes));
  return true;
}

bool NativeFirmwareStore::PromoteApex(
    const QString &LoaderPath,
    QString *Error) {
  if (Variables_ == nullptr || PendingBootNumber_ < 0 ||
      NormalizeLoaderPath(LoaderPath).compare(
          QString::fromLatin1(kApexLoaderPath), Qt::CaseInsensitive) != 0) {
    return SetError(
        Error, QStringLiteral("The native firmware promotion was not prepared."));
  }

  const QString VariableName = BootVariableName(PendingBootNumber_);
  FirmwareVariableValue CurrentApex;
  if (!Variables_->Read(VariableName, &CurrentApex, Error)) {
    return false;
  }
  if (!PendingHadApex_) {
    if (CurrentApex.Exists) {
      return SetError(
          Error, QStringLiteral("The reserved APEX32 boot number is no longer free."));
    }
    QByteArray LoadOption;
    if (!BuildApexLoadOption(PendingTemplate_, LoaderPath, &LoadOption, Error) ||
        !Variables_->Write(
            VariableName, LoadOption, kBootVariableAttributes, Error)) {
      return false;
    }
  } else {
    if (!CurrentApex.Exists || CurrentApex.Data.size() < 4) {
      return SetError(
          Error, QStringLiteral("The existing APEX32 boot option is invalid."));
    }
    QByteArray ActiveOption = CurrentApex.Data;
    const quint32 ActiveAttributes = ReadLe32(ActiveOption, 0) | 1U;
    ActiveOption[0] = static_cast<char>(ActiveAttributes & 0xffU);
    ActiveOption[1] = static_cast<char>((ActiveAttributes >> 8) & 0xffU);
    ActiveOption[2] = static_cast<char>((ActiveAttributes >> 16) & 0xffU);
    ActiveOption[3] = static_cast<char>((ActiveAttributes >> 24) & 0xffU);
    if (ActiveOption != CurrentApex.Data &&
        !Variables_->Write(
            VariableName,
            ActiveOption,
            CurrentApex.Attributes,
            Error)) {
      return false;
    }
  }

  FirmwareVariableValue OrderVariable;
  QVector<quint16> Order;
  if (!Variables_->Read(QStringLiteral("BootOrder"), &OrderVariable, Error) ||
      !DecodeBootOrder(OrderVariable, &Order, Error)) {
    return false;
  }
  QVector<quint16> NewOrder;
  NewOrder.reserve(Order.size() + 1);
  NewOrder.push_back(static_cast<quint16>(PendingBootNumber_));
  for (const quint16 Number : Order) {
    if (Number != static_cast<quint16>(PendingBootNumber_)) {
      NewOrder.push_back(Number);
    }
  }
  const quint32 Attributes =
      OrderVariable.Attributes == 0 ? kBootVariableAttributes
                                    : OrderVariable.Attributes;
  return Variables_->Write(
      QStringLiteral("BootOrder"), EncodeBootOrder(NewOrder), Attributes, Error);
}

bool NativeFirmwareStore::Restore(
    const QJsonObject &State,
    QString *Error) {
  if (Variables_ == nullptr) {
    return SetError(Error, QStringLiteral("The native firmware store is unavailable."));
  }
  NativeSnapshot Snapshot;
  if (!DecodeSnapshot(State, &Snapshot, Error)) {
    return false;
  }
  const QString VariableName = BootVariableName(Snapshot.CandidateNumber);
  if ((Snapshot.HadApex &&
       !Variables_->Write(
           VariableName,
           Snapshot.ApexOption,
           Snapshot.ApexAttributes,
           Error)) ||
      (!Snapshot.HadApex && !Variables_->Remove(VariableName, Error)) ||
      !Variables_->Write(
          QStringLiteral("BootOrder"),
          Snapshot.BootOrder,
          Snapshot.BootOrderAttributes,
          Error)) {
    return false;
  }
  PendingBootNumber_ = -1;
  PendingHadApex_ = false;
  PendingTemplate_.clear();
  return Matches(State, Error);
}

bool NativeFirmwareStore::Matches(
    const QJsonObject &State,
    QString *Error) {
  if (Variables_ == nullptr) {
    return SetError(Error, QStringLiteral("The native firmware store is unavailable."));
  }
  NativeSnapshot Snapshot;
  if (!DecodeSnapshot(State, &Snapshot, Error)) {
    return false;
  }
  FirmwareVariableValue Order;
  FirmwareVariableValue Apex;
  if (!Variables_->Read(QStringLiteral("BootOrder"), &Order, Error) ||
      !Variables_->Read(BootVariableName(Snapshot.CandidateNumber), &Apex, Error)) {
    return false;
  }
  if (!Order.Exists || Order.Data != Snapshot.BootOrder ||
      Order.Attributes != Snapshot.BootOrderAttributes ||
      (Snapshot.HadApex &&
       (!Apex.Exists || Apex.Data != Snapshot.ApexOption ||
        Apex.Attributes != Snapshot.ApexAttributes)) ||
      (!Snapshot.HadApex && Apex.Exists)) {
    return SetError(
        Error, QStringLiteral("The original UEFI firmware state was not restored."));
  }
  return true;
}

bool NativeFirmwareStore::ApexIsFirst(
    const QString &LoaderPath,
    QString *Error) {
  if (Variables_ == nullptr) {
    return SetError(Error, QStringLiteral("The native firmware store is unavailable."));
  }
  FirmwareVariableValue OrderVariable;
  QVector<quint16> Order;
  if (!Variables_->Read(QStringLiteral("BootOrder"), &OrderVariable, Error) ||
      !DecodeBootOrder(OrderVariable, &Order, Error) || Order.isEmpty()) {
    return false;
  }
  FirmwareVariableValue First;
  if (!Variables_->Read(BootVariableName(Order.first()), &First, Error) ||
      !First.Exists || First.Data.size() < 4) {
    return SetError(Error, QStringLiteral("The first UEFI boot entry is unavailable."));
  }
  QString Path;
  if ((ReadLe32(First.Data, 0) & 1U) == 0 ||
      !LoadOptionPath(First.Data, &Path, Error) ||
      Path.compare(NormalizeLoaderPath(LoaderPath), Qt::CaseInsensitive) != 0) {
    return SetError(Error, QStringLiteral("APEX32 is not first in UEFI BootOrder."));
  }
  return true;
}

bool ReadSecureBootState(
    FirmwareVariableAccess *Variables,
    bool *Enabled,
    QString *Error) {
  if (Variables == nullptr || Enabled == nullptr) {
    return SetError(Error, QStringLiteral("The Secure Boot query is unavailable."));
  }
  FirmwareVariableValue Variable;
  if (!Variables->Read(QStringLiteral("SecureBoot"), &Variable, Error)) {
    return false;
  }
  if (!Variable.Exists) {
    *Enabled = false;
    return true;
  }
  if (Variable.Data.size() != 1) {
    return SetError(Error, QStringLiteral("The Secure Boot variable is malformed."));
  }
  *Enabled = Variable.Data.at(0) != 0;
  return true;
}

}  // namespace Apex32::WindowsInstaller
