extern "C" {
    #include "third_party/vice-3.3/src/rs232drv/rs232bmc.h"
}

#include <string.h>

#include <circle/logger.h>
#include <circle/net/dnsclient.h>
#include <circle/net/error.h>
#include <circle/net/in.h>
#include <circle/net/netsubsystem.h>
#include <circle/net/socket.h>
#include <circle/netdevice.h>
#include <circle/sched/mutex.h>
#include <circle/sched/scheduler.h>
#include <circle/sched/task.h>
#include <circle/timer.h>




extern "C" unsigned long bmc_tempo_us(void);





CNetSubSystem *ViceNetworkGetSubsystem(void);

namespace {

const unsigned kQueueSize = 16384;
const unsigned kCommandSize = 256;
const unsigned kSocketSendSize = 256;
// Keep diagnostic history bounded so serial tracing cannot consume modem RAM.
const unsigned kTraceSize = 128;
// C64 OS reports an SSID through ATW; retain a modem-side copy for ATI3.
const unsigned kSsidSize = 33;
const u64 kEscapeGuardMicroseconds = 1000000;
const u64 kTelnetCandidateMicroseconds = 100000;
const uint8_t kTelnetIac = 255;
const uint8_t kTelnetWill = 251;
const uint8_t kTelnetWont = 252;
const uint8_t kTelnetDo = 253;
const uint8_t kTelnetDont = 254;
const uint8_t kTelnetSubnegotiation = 250;
const uint8_t kTelnetSubnegotiationEnd = 240;
const uint8_t kTelnetBinary = 0;
const char FromBmcModem[] = "bmc-modem";
/* Modem behaviour as in modem.cc of the 1541 Ultimate-II+ firmware (Gideon Zweijtzer and the 1541ultimate authors), GNU GPL v3 license. */

const u16 kListenPort = 3000;
const unsigned kMaxRings = 10;
const u64 kRingMicroseconds = 3000000;

class BmcModem;

class NetworkTask : public CTask {
public:
  explicit NetworkTask(BmcModem *modem)
      : CTask(12 * 1024), modem_(modem) {
    SetName("bmc-modem-net");
  }

  void Run() override;

private:
  BmcModem *modem_;
};

class ListenTask : public CTask {
public:
  explicit ListenTask(BmcModem *modem)
      : CTask(8 * 1024), modem_(modem) {
    SetName("bmc-modem-ascolto");
  }

  void Run() override;

private:
  BmcModem *modem_;
};

class BmcModem {
public:
  BmcModem()
      : socket_(0), read_(0), write_(0), transmitLength_(0),
        commandLength_(0), traceWrite_(0), traceLength_(0),
        aciaTraceWrite_(0), aciaTraceLength_(0),
        open_(false),
        dataMode_(false), echo_(true),
        quiet_(false), numericResponses_(false), crlf_(false),
        dtrEnabled_(false),
        commandInputActive_(false), receiveEnabled_(true),
        remoteDisconnectPending_(false), plusCount_(0), worker_(0),
        listener_(0), incoming_(0), rings_(0), nextRing_(0),
        answerPending_(false),
        telnetState_(kTelnetData), telnetCommand_(0),
        telnetDetected_(false), telnetEnabled_(true), lastTransmitTime_(0),
        escapeDeadline_(0),
        telnetCandidateDeadline_(0) {}

  ~BmcModem() { Disconnect(); }

  void Initialize() {
    CMutexGuard guard(lock_);
    if (wifiSsid_[0] == '\0') {
      strcpy(wifiSsid_, "BMC64");
    }
    if (worker_ == 0) {
      worker_ = new NetworkTask(this);
    }
    Reset();
  }

  int Open(int device) {
    CMutexGuard guard(lock_);
    Reset();
    open_ = true;
    if (listener_ == 0) {
      listener_ = new ListenTask(this);
    }
    CLogger::Get()->Write(FromBmcModem, LogNotice,
                          "opened RS232 device %d", device + 1);
    return 0;
  }

  void Close(int) {
    CMutexGuard guard(lock_);
    Disconnect();
    open_ = false;
  }

  void Reset() {
    CMutexGuard guard(lock_);
    Disconnect();
    read_ = 0;
    write_ = 0;
    transmitLength_ = 0;
    commandLength_ = 0;
    dataMode_ = false;
    echo_ = true;
    quiet_ = false;
    numericResponses_ = false;

    crlf_ = false;
    dtrEnabled_ = false;
    commandInputActive_ = false;
    receiveEnabled_ = true;
    plusCount_ = 0;
    telnetState_ = kTelnetData;
    telnetCommand_ = 0;
    telnetDetected_ = false;
    telnetEnabled_ = true;
    lastTransmitTime_ = 0;
    escapeDeadline_ = 0;
    telnetCandidateDeadline_ = 0;
    answerPending_ = false;
    ResetRegisters();
  }




  void Incoming(CSocket *caller) {
    CMutexGuard guard(lock_);
    if (socket_ != 0 || incoming_ != 0) {
      SendText(caller, "The modem you are connecting to is currently busy.\n");
      delete caller;
      return;
    }
    if (!open_ || !dtrEnabled_) {
      SendText(caller, "Modem Software is currently not running...\n");
      delete caller;
      return;
    }
    SendText(caller, "Welcome to the Modem Emulation Layer of BMC64-NG!\n");
    incoming_ = caller;
    rings_ = 0;
    nextRing_ = (u64) bmc_tempo_us();
    CLogger::Get()->Write(FromBmcModem, LogNotice, "incoming call");
  }

  int Put(uint8_t byte) {
    CMutexGuard guard(lock_);
    if (!open_) {
      return -1;
    }

    TraceByte(byte);
    Pump();
    if (dataMode_) {
      PutData(byte);
    } else {
      PutCommand(byte);
    }
    Pump();
    return 0;
  }

  void NoteAciaTransmit(uint8_t byte) {
    CMutexGuard guard(lock_);
    // This precedes the normal serial-backend trace and identifies ACIA writes.
    aciaTrace_[aciaTraceWrite_] = byte;
    aciaTraceWrite_ = (aciaTraceWrite_ + 1) % kTraceSize;
    if (aciaTraceLength_ < kTraceSize) {
      ++aciaTraceLength_;
    }
  }

  unsigned ReadAciaTrace(uint8_t *bytes, unsigned maximum) {
    CMutexGuard guard(lock_);
    unsigned length = aciaTraceLength_ < maximum ? aciaTraceLength_ : maximum;
    unsigned index = (aciaTraceWrite_ + kTraceSize - length) % kTraceSize;
    for (unsigned position = 0; position < length; ++position) {
      bytes[position] = aciaTrace_[index];
      index = (index + 1) % kTraceSize;
    }
    return length;
  }

  void ClearAciaTrace() {
    CMutexGuard guard(lock_);
    aciaTraceWrite_ = 0;
    aciaTraceLength_ = 0;
  }

  int Get(uint8_t *byte) {
    CMutexGuard guard(lock_);
    if (!open_ || byte == 0) {
      return -1;
    }

    Pump();
    if (read_ == write_) {
      return 0;
    }

    *byte = queue_[read_];
    read_ = (read_ + 1) % kQueueSize;
    return 1;
  }

  bool HasCarrier() {
    CMutexGuard guard(lock_);
    return socket_ != 0 || remoteDisconnectPending_;
  }

  void SetStatus(int status) {
    CMutexGuard guard(lock_);
    bool dtrEnabled = (status & 0x02) != 0;
    // Treat a DTR drop as a hardware hangup, matching a physical modem.
    if (dtrEnabled_ && !dtrEnabled && socket_ != 0) {
      Disconnect();
    }
    dtrEnabled_ = dtrEnabled;
    receiveEnabled_ = (status & 0x01) != 0;
  }

  // Serial timing is handled by the emulated ACIA; retain the backend hook.
  void SetBps(unsigned int) {}

  void ServiceNetwork() {
    CMutexGuard guard(lock_);
    Pump();
  }

private:
  class CMutexGuard {
  public:
    explicit CMutexGuard(CMutex &lock) : lock_(lock) { lock_.Acquire(); }
    ~CMutexGuard() { lock_.Release(); }

  private:
    CMutex &lock_;
  };

  void SendText(CSocket *target, const char *text) {
    target->Send(text, strlen(text), MSG_DONTWAIT);
  }

  void ResetRegisters() {
    static const uint8_t defaults[16] = {0, 0, 43, 13, 10, 8, 2, 50,
                                         2, 6, 14, 95, 50, 0, 0, 0};
    memcpy(regs_, defaults, sizeof regs_);
  }

  void Answer() {
    Disconnect();
    socket_ = incoming_;
    incoming_ = 0;
    answerPending_ = false;
    telnetEnabled_ = false;
    dataMode_ = true;
    lastTransmitTime_ = (u64) bmc_tempo_us();
    CLogger::Get()->Write(FromBmcModem, LogNotice, "incoming call answered");
    Result("CONNECT");
  }

  void ServiceIncoming() {
    if (incoming_ == 0) {
      return;
    }
    if (answerPending_ || (regs_[0] != 0 && rings_ >= regs_[0])) {
      Answer();
      return;
    }
    u64 now = (u64) bmc_tempo_us();
    if (now < nextRing_) {
      return;
    }
    if (rings_ >= kMaxRings) {
      delete incoming_;
      incoming_ = 0;
      CLogger::Get()->Write(FromBmcModem, LogNotice, "incoming call not answered");
      return;
    }
    Result("RING");
    ++rings_;
    regs_[1] = static_cast<uint8_t>(rings_);
    nextRing_ = now + kRingMicroseconds;
  }

  void Disconnect() {
    delete socket_;
    socket_ = 0;
    remoteDisconnectPending_ = false;
    dataMode_ = false;
    plusCount_ = 0;
    escapeDeadline_ = 0;
    transmitLength_ = 0;
    telnetState_ = kTelnetData;
    telnetCommand_ = 0;
    telnetDetected_ = false;
    telnetCandidateDeadline_ = 0;
  }

  void BeginRemoteDisconnect() {
    if (telnetState_ == kTelnetCandidate) {
      QueueByte(kTelnetIac);
    }
    unsigned queued = Available();
    Disconnect();
    if (queued != 0) {
      remoteDisconnectPending_ = true;
      dataMode_ = true;
      CLogger::Get()->Write(FromBmcModem, LogNotice,
                            "delaying carrier loss for %u queued bytes",
                            queued);
    } else {
      Result("NO CARRIER");
    }
  }

  void FinishRemoteDisconnect() {
    if (!remoteDisconnectPending_ || Available() != 0) {
      return;
    }
    remoteDisconnectPending_ = false;
    dataMode_ = false;
    CLogger::Get()->Write(FromBmcModem, LogNotice,
                          "queued receive data drained; dropping carrier");
    Result("NO CARRIER");
  }

  unsigned Available() const {
    return (write_ + kQueueSize - read_) % kQueueSize;
  }

  unsigned Free() const { return kQueueSize - Available() - 1; }

  void QueueByte(uint8_t byte) {
    if (Free() == 0) {
      return;
    }
    queue_[write_] = byte;
    write_ = (write_ + 1) % kQueueSize;
  }

  void QueueText(const char *text) {
    while (*text != '\0') {
      QueueByte(static_cast<uint8_t>(*text++));
    }
  }

  void TraceByte(uint8_t byte) {
    trace_[traceWrite_] = byte;
    traceWrite_ = (traceWrite_ + 1) % kTraceSize;
    if (traceLength_ < kTraceSize) {
      ++traceLength_;
    }
  }

  void QueueTrace() {
    // AT+TRACE returns the bounded serial-backend history as hexadecimal text.
    const char hex[] = "0123456789ABCDEF";
    unsigned index = (traceWrite_ + kTraceSize - traceLength_) % kTraceSize;
    for (unsigned count = 0; count < traceLength_; ++count) {
      uint8_t byte = trace_[index];
      QueueByte(hex[byte >> 4]);
      QueueByte(hex[byte & 0x0f]);
      QueueByte(' ');
      index = (index + 1) % kTraceSize;
    }
  }

  void QueueAciaTrace() {
    // AT+ACIATRACE separates ACIA register writes from backend serial writes.
    const char hex[] = "0123456789ABCDEF";
    uint8_t bytes[kTraceSize];
    unsigned length = bmcmodem_acia_trace_read(bytes, sizeof bytes);
    for (unsigned index = 0; index < length; ++index) {
      QueueByte(hex[bytes[index] >> 4]);
      QueueByte(hex[bytes[index] & 0x0f]);
      QueueByte(' ');
    }
  }

  void SetWifiSsid(const char *target) {
    // ATW accepts a quoted SSID and optional comma-delimited Wi-Fi parameters.
    if (*target == '"') {
      ++target;
    }
    unsigned length = 0;
    while (*target != '\0' && *target != '"' && *target != ',' &&
           length + 1 < kSsidSize) {
      wifiSsid_[length++] = *target++;
    }
    wifiSsid_[length] = '\0';
  }

  void WifiStatus() {
    // C64 OS probes ATI3 for the active Wi-Fi name before opening CNP.
    QueueText(wifiSsid_);
    QueueText(crlf_ ? "\r\n" : "\r");
    Result("OK");
  }

  void Result(const char *text) {
    CLogger::Get()->Write(FromBmcModem, LogNotice, "queued result: %s", text);
    if (quiet_) {
      return;
    }
    QueueText(crlf_ ? "\r\n" : "\r");
    if (numericResponses_) {
      // C64 OS requests V0 and expects standard Hayes numeric result codes.
      if (strcmp(text, "OK") == 0) {
        QueueText("0");
      } else if (strcmp(text, "CONNECT") == 0) {
        QueueText("1");
      } else if (strcmp(text, "RING") == 0) {
        QueueText("2");
      } else if (strcmp(text, "NO CARRIER") == 0) {
        QueueText("3");
      } else if (strcmp(text, "ERROR") == 0) {
        QueueText("4");
      } else {
        QueueText(text);
      }
    } else {
      QueueText(text);
    }
    QueueText(crlf_ ? "\r\n" : "\r");
  }

  // Hayes command/data handling: bytes from the emulated ACIA.
  void PutCommand(uint8_t byte) {
    if (echo_) {
      QueueByte(byte);
    }

    if (byte == '\r' || byte == '\n') {
      if (commandLength_ != 0) {
        command_[commandLength_] = '\0';
        CLogger::Get()->Write(FromBmcModem, LogNotice,
                              "received modem command: %s", command_);
        ExecuteCommand();
      }
      commandLength_ = 0;
      commandInputActive_ = false;
      return;
    }

    if (byte == '\b' || byte == 0x14) {
      if (commandLength_ != 0) {
        --commandLength_;
      }
      return;
    }

    if (commandLength_ + 1 < kCommandSize) {
      if (!commandInputActive_) {
        CLogger::Get()->Write(FromBmcModem, LogNotice,
                              "received modem command data");
        commandInputActive_ = true;
      }
      command_[commandLength_++] = static_cast<char>(byte);
    }
  }

  void PutData(uint8_t byte) {
    u64 now = (u64) bmc_tempo_us();
    if (byte == '+' && plusCount_ < 3 &&
        (plusCount_ != 0 || now - lastTransmitTime_ >=
                                kEscapeGuardMicroseconds)) {
      if (plusCount_ < 3) {
        ++plusCount_;
      }
      escapeDeadline_ = now + kEscapeGuardMicroseconds;
      return;
    }

    FlushPendingPluses();
    QueueTransmit(byte);
    lastTransmitTime_ = now;
  }

  void FlushPendingPluses() {
    while (plusCount_ != 0) {
      QueueTransmit('+');
      --plusCount_;
    }
    escapeDeadline_ = 0;
  }

  void ResolveEscapeSequence() {
    if (plusCount_ == 0 || (u64) bmc_tempo_us() < escapeDeadline_) {
      return;
    }

    if (plusCount_ == 3) {
      plusCount_ = 0;
      escapeDeadline_ = 0;
      dataMode_ = false;
      Result("OK");
    } else {
      FlushPendingPluses();
      lastTransmitTime_ = (u64) bmc_tempo_us();
    }
  }

  void QueueTransmit(uint8_t byte) {
    unsigned required = telnetDetected_ && byte == kTelnetIac ? 2 : 1;
    if (transmitLength_ + required > sizeof transmit_) {
      return;
    }
    transmit_[transmitLength_++] = byte;
    if (required == 2) {
      transmit_[transmitLength_++] = byte;
    }
  }

  void QueueTelnetReply(uint8_t command, uint8_t option) {
    if (transmitLength_ + 3 > sizeof transmit_) {
      return;
    }
    transmit_[transmitLength_++] = kTelnetIac;
    transmit_[transmitLength_++] = command;
    transmit_[transmitLength_++] = option;
  }

  // Telnet filtering: control sequences stay on TCP, payload reaches the C64.
  void HandleTelnetOption(uint8_t option) {
    if (telnetCommand_ == kTelnetWill) {
      QueueTelnetReply(option == kTelnetBinary ? kTelnetDo : kTelnetDont,
                       option);
    } else if (telnetCommand_ == kTelnetDo) {
      QueueTelnetReply(option == kTelnetBinary ? kTelnetWill : kTelnetWont,
                       option);
    }
  }

  void HandleTelnetByte(uint8_t byte) {
    // CNP on port 6400 is a binary protocol; every byte, including 0xff,
    // belongs to its payload rather than Telnet negotiation.
    if (!telnetEnabled_) {
      QueueByte(byte);
      return;
    }

    // TCP transport: the scheduler task calls this independently of ACIA polls.
    switch (telnetState_) {
      case kTelnetData:
        if (byte == kTelnetIac) {
          telnetState_ = telnetDetected_ ? kTelnetCommand : kTelnetCandidate;
          if (!telnetDetected_) {
            telnetCandidateDeadline_ =
                (u64) bmc_tempo_us() + kTelnetCandidateMicroseconds;
          }
        } else {
          QueueByte(byte);
        }
        break;

      case kTelnetCandidate:
        telnetCandidateDeadline_ = 0;
        if (byte == kTelnetIac) {
          QueueByte(kTelnetIac);
          QueueByte(kTelnetIac);
          telnetState_ = kTelnetData;
        } else if (byte == kTelnetWill || byte == kTelnetWont ||
                   byte == kTelnetDo || byte == kTelnetDont) {
          telnetCommand_ = byte;
          telnetState_ = kTelnetCandidateOption;
        } else {
          QueueByte(kTelnetIac);
          QueueByte(byte);
          telnetState_ = kTelnetData;
        }
        break;

      case kTelnetCandidateOption:
        telnetDetected_ = true;
        HandleTelnetOption(byte);
        telnetState_ = kTelnetData;
        break;

      case kTelnetCommand:
        if (byte == kTelnetIac) {
          QueueByte(byte);
          telnetState_ = kTelnetData;
        } else if (byte == kTelnetWill || byte == kTelnetWont ||
                   byte == kTelnetDo || byte == kTelnetDont) {
          telnetCommand_ = byte;
          telnetState_ = kTelnetOption;
        } else if (byte == kTelnetSubnegotiation) {
          telnetState_ = kTelnetSubnegotiation;
        } else {
          telnetState_ = kTelnetData;
        }
        break;

      case kTelnetOption:
        HandleTelnetOption(byte);
        telnetState_ = kTelnetData;
        break;

      case kTelnetSubnegotiation:
        if (byte == kTelnetIac) {
          telnetState_ = kTelnetSubnegotiationIac;
        }
        break;

      case kTelnetSubnegotiationIac:
        if (byte == kTelnetSubnegotiationEnd) {
          telnetState_ = kTelnetData;
        } else {
          telnetState_ = kTelnetSubnegotiation;
        }
        break;
    }
  }

  void ResolveTelnetCandidate() {
    if (telnetState_ == kTelnetCandidate &&
        (u64) bmc_tempo_us() >= telnetCandidateDeadline_) {
      QueueByte(kTelnetIac);
      telnetState_ = kTelnetData;
      telnetCandidateDeadline_ = 0;
    }
  }

  static char Upper(char value) {
    if (value >= 'a' && value <= 'z') {
      return static_cast<char>(value - 'a' + 'A');
    }
    return value;
  }

  void ExecuteCommand() {
    if (commandLength_ < 2 || Upper(command_[0]) != 'A' ||
        Upper(command_[1]) != 'T') {
      Result("ERROR");
      return;
    }

    if (commandLength_ == 2) {
      Result("OK");
      return;
    }

    unsigned index = 2;
    // Parse a compact Hayes command stream: C64 OS sends several settings in
    // one initialization command before its quoted ATD dial string.
    while (index < commandLength_) {
      while (index < commandLength_ && command_[index] == ' ') {
        ++index;
      }
      if (index == commandLength_) {
        break;
      }
      char operation = Upper(command_[index++]);
      if (operation == 'D') {
        if (index == commandLength_) {
          if (socket_ == 0) {
            Result("ERROR");
          } else {
            dataMode_ = true;
            lastTransmitTime_ = (u64) bmc_tempo_us();
            Result("CONNECT");
          }
        } else {
          Connect(command_ + index, true);
        }
        return;
      }
      if (operation == 'C') {
        // ATC connects without data mode; C64 OS uses it for its network test.
        if (index == commandLength_) {
          CNetSubSystem *net = ViceNetworkGetSubsystem();
          Result(net != 0 && net->IsRunning() ? "OK" : "ERROR");
        } else {
          Connect(command_ + index, false);
        }
        return;
      }
      if (operation == 'W') {
        CLogger::Get()->Write(FromBmcModem, LogNotice,
                              "received C64 OS WiFi command");
        // BMC64's host owns Wi-Fi configuration; retain only its reported SSID.
        if (index < commandLength_) {
          SetWifiSsid(command_ + index);
        }
        CNetSubSystem *net = ViceNetworkGetSubsystem();
        Result(net != 0 && net->IsRunning() ? "OK" : "ERROR");
        return;
      }
      if (operation == '+') {
        const char traceCommand[] = "TRACE";
        unsigned traceIndex = 0;
        while (traceCommand[traceIndex] != '\0' &&
               index + traceIndex < commandLength_ &&
               Upper(command_[index + traceIndex]) == traceCommand[traceIndex]) {
          ++traceIndex;
        }
        if (traceCommand[traceIndex] == '\0' &&
            index + traceIndex == commandLength_) {
          QueueTrace();
          Result("OK");
          return;
        }
        const char clearCommand[] = "TRACECLEAR";
        traceIndex = 0;
        while (clearCommand[traceIndex] != '\0' &&
               index + traceIndex < commandLength_ &&
               Upper(command_[index + traceIndex]) == clearCommand[traceIndex]) {
          ++traceIndex;
        }
        if (clearCommand[traceIndex] == '\0' &&
            index + traceIndex == commandLength_) {
          traceWrite_ = 0;
          traceLength_ = 0;
          Result("OK");
          return;
        }
        const char aciaTraceCommand[] = "ACIATRACE";
        traceIndex = 0;
        while (aciaTraceCommand[traceIndex] != '\0' &&
               index + traceIndex < commandLength_ &&
               Upper(command_[index + traceIndex]) ==
                   aciaTraceCommand[traceIndex]) {
          ++traceIndex;
        }
        if (aciaTraceCommand[traceIndex] == '\0' &&
            index + traceIndex == commandLength_) {
          QueueAciaTrace();
          Result("OK");
          return;
        }
        const char aciaClearCommand[] = "ACIATRACECLEAR";
        traceIndex = 0;
        while (aciaClearCommand[traceIndex] != '\0' &&
               index + traceIndex < commandLength_ &&
               Upper(command_[index + traceIndex]) ==
                   aciaClearCommand[traceIndex]) {
          ++traceIndex;
        }
        if (aciaClearCommand[traceIndex] == '\0' &&
            index + traceIndex == commandLength_) {
          bmcmodem_acia_trace_clear();
          Result("OK");
          return;
        }
        Result("ERROR");
        return;
      }

      if (operation == '&') {
        if (index >= commandLength_) {
          Result("ERROR");
          return;
        }
        operation = Upper(command_[index++]);
        if (operation == 'F' || operation == 'K' || operation == 'L' ||
          operation == 'P' || operation == 'W') {
          // These ZiModem-compatible persistence commands have no BMC64
          // equivalent, but accepting their numeric argument keeps init alive.
          while (index < commandLength_ && command_[index] >= '0' &&
                 command_[index] <= '9') {
            ++index;
          }
          continue;
        }
        Result("ERROR");
        return;
      }

      if (operation == 'Z') {
        if (index != commandLength_) {
          Result("ERROR");
          return;
        }
        Reset();
        Result("OK");
        return;
      }
      if (operation == 'A') {


        if (incoming_ != 0) {
          Answer();
        } else {
          answerPending_ = true;
        }
        return;
      }
      if (operation == 'H') {
        // ATH may include an ignored numeric modifier (for example ATH0).
        Disconnect();
        if (incoming_ != 0) {
          delete incoming_;
          incoming_ = 0;
        }
        while (index < commandLength_ && command_[index] >= '0' &&
               command_[index] <= '9') {
          ++index;
        }
        continue;
      }
      if (operation == 'I') {
        unsigned info = 0;
        if (index < commandLength_ && command_[index] >= '0' &&
            command_[index] <= '9') {
          while (index < commandLength_ && command_[index] >= '0' &&
                 command_[index] <= '9') {
            info = info * 10 + static_cast<unsigned>(command_[index++] - '0');
          }
        }
        if (index != commandLength_) {
          Result("ERROR");
          return;
        }
        if (info == 3) {
          WifiStatus();
          return;
        }
        Result("BMC64 ZIMODEM COMPATIBLE");
        continue;
      }
      if (operation == 'O') {
        if (socket_ == 0 || index != commandLength_) {
          Result("ERROR");
          return;
        }
        dataMode_ = true;
        lastTransmitTime_ = (u64) bmc_tempo_us();
        Result("CONNECT");
        return;
      }

      if (operation == 'S') {
        unsigned registerNumber = 0;
        unsigned registerValue = 0;
        if (index >= commandLength_ || command_[index] < '0' ||
            command_[index] > '9') {
          Result("ERROR");
          return;
        }
        while (index < commandLength_ && command_[index] >= '0' &&
               command_[index] <= '9') {
          registerNumber = registerNumber * 10 +
                           static_cast<unsigned>(command_[index++] - '0');
        }

        if (index < commandLength_ && command_[index] == '?') {
          char text[8];
          unsigned value = registerNumber < 16 ? regs_[registerNumber] : 0;
          unsigned length = 0;
          char digits[4];
          do {
            digits[length++] = static_cast<char>('0' + value % 10);
            value /= 10;
          } while (value != 0 && length < 3);
          unsigned position = 0;
          while (length != 0) {
            text[position++] = digits[--length];
          }
          text[position++] = '\r';
          text[position] = '\0';
          QueueText(text);
          return;
        }
        if (index >= commandLength_ || command_[index++] != '=') {
          Result("ERROR");
          return;
        }
        while (index < commandLength_ && command_[index] >= '0' &&
               command_[index] <= '9') {
          registerValue = registerValue * 10 +
                          static_cast<unsigned>(command_[index++] - '0');
        }
        if (registerNumber < 16) {
          regs_[registerNumber] = static_cast<uint8_t>(registerValue);
        }
        continue;
      }

      if (index >= commandLength_ || command_[index] < '0' ||
          command_[index] > '9') {
        Result("ERROR");
        return;
      }
      unsigned value = 0;
      while (index < commandLength_ && command_[index] >= '0' &&
             command_[index] <= '9') {
        value = value * 10 + static_cast<unsigned>(command_[index++] - '0');
      }

      switch (operation) {
        case 'E':
          echo_ = value != 0;
          break;
        case 'F':
          if (value > 3) {
            Result("ERROR");
            return;
          }
          break;
        case 'Q':
          quiet_ = value != 0;
          break;
        case 'R':
          if (value > 1) {
            Result("ERROR");
            return;
          }
          crlf_ = value != 0;
          break;
        case 'V':
          if (value > 1) {
            Result("ERROR");
            return;
          }
          numericResponses_ = value == 0;
          break;
        case 'X':
        case 'B':
          // Dial-result level and requested baud are modeled by the ACIA/TCP
          // backend, so preserve compatibility without changing host settings.
          break;
        default:
          Result("ERROR");
          return;
      }
    }
    Result("OK");
  }

  void Connect(char *target, bool enterDataMode) {



    if (*target != '\0') {
      ++target;
    }
    while (*target == ' ') {
      ++target;
    }
    if (*target == '\0') {
      Result("ERROR");
      return;
    }

    if (*target == '"') {
      ++target;
      char *end = target;
      while (*end != '\0') {
        ++end;
      }
      if (end == target || *--end != '"') {
        Result("ERROR");
        return;
      }
      *end = '\0';
    }

    unsigned port = 80;
    char *portDelimiter = 0;
    for (char *cursor = target; *cursor != '\0'; ++cursor) {
      if (*cursor == ':') {
        portDelimiter = cursor;
        break;
      }
    }
    if (portDelimiter != 0) {
      *portDelimiter++ = '\0';
      unsigned letta = 0;
      while (*portDelimiter >= '0' && *portDelimiter <= '9') {
        letta = letta * 10 + static_cast<unsigned>(*portDelimiter++ - '0');
      }
      if (letta != 0 && letta <= 65535) {
        port = letta;
      }
    }

    CNetSubSystem *net = ViceNetworkGetSubsystem();
    if (net == 0 || !net->IsRunning()) {
      CLogger::Get()->Write(FromBmcModem, LogNotice,
                            "dial failed: network unavailable");
      Result("NO CARRIER");
      return;
    }

    CLogger::Get()->Write(FromBmcModem, LogNotice, "dialing %s:%u", target,
                          port);
    CIPAddress address;
    CDNSClient dns(net);
    if (!dns.Resolve(target, &address)) {
      CLogger::Get()->Write(FromBmcModem, LogNotice,
                            "dial failed: DNS lookup failed");
      Result("NO CARRIER");
      return;
    }

    Disconnect();
    // Preserve binary CNP data verbatim. Other ports retain ZiModem-style
    // Telnet negotiation for traditional BBS connections.
    telnetEnabled_ = port != 6400;
    socket_ = new CSocket(net, IPPROTO_TCP);
    if (socket_ == 0 || socket_->Connect(address, static_cast<u16>(port)) < 0) {
      Disconnect();
      CLogger::Get()->Write(FromBmcModem, LogNotice,
                            "dial failed: TCP connection refused");
      Result("NO CARRIER");
      return;
    }

    // ATC leaves command mode active; ATD immediately forwards CNP payload.
    dataMode_ = enterDataMode;
    if (enterDataMode) {
      lastTransmitTime_ = (u64) bmc_tempo_us();
    }
    CLogger::Get()->Write(FromBmcModem, LogNotice, "TCP connection established");
    Result("CONNECT");
  }

  void Pump() {
    ServiceIncoming();
    if (socket_ == 0) {
      FinishRemoteDisconnect();
      return;
    }

    ResolveEscapeSequence();
    FlushTransmit();



    if (socket_ == 0) {
      FinishRemoteDisconnect();
      return;
    }
    ReceiveData();
    if (socket_ == 0) {
      return;
    }
    ResolveTelnetCandidate();
  }

  void FlushTransmit() {
    if (transmitLength_ == 0) {
      return;
    }

    unsigned sendLength = transmitLength_;
    if (sendLength > kSocketSendSize) {
      sendLength = kSocketSendSize;
    }
    int sent = socket_->Send(transmit_, sendLength, MSG_DONTWAIT);
    if (sent > 0) {
      unsigned count = static_cast<unsigned>(sent);
      if (count < transmitLength_) {
        memmove(transmit_, transmit_ + count, transmitLength_ - count);
      }
      transmitLength_ -= count;
    } else if (sent < 0 && sent != -NET_ERROR_WOULD_BLOCK) {
      CLogger::Get()->Write(FromBmcModem, LogNotice,
                            "socket send failed: %d", sent);
      BeginRemoteDisconnect();
    }
  }

  void ReceiveData() {
    if (!receiveEnabled_ || socket_ == 0) {
      return;
    }

    unsigned free = Free();
    if (free < FRAME_BUFFER_SIZE) {
      return;
    }

    uint8_t received[FRAME_BUFFER_SIZE];
    int count = socket_->Receive(received, sizeof received, MSG_DONTWAIT);
    if (count < 0 && count != -NET_ERROR_WOULD_BLOCK) {
      CLogger::Get()->Write(FromBmcModem, LogNotice,
                            "socket receive failed: %d", count);
      BeginRemoteDisconnect();
      return;
    }
    for (int index = 0; index < count; ++index) {
      HandleTelnetByte(received[index]);
    }
  }

private:
  // Protocol parsing state.
  enum TelnetState {
    kTelnetData,
    kTelnetCandidate,
    kTelnetCandidateOption,
    kTelnetCommand,
    kTelnetOption,
    kTelnetSubnegotiation,
    kTelnetSubnegotiationIac
  };

  CMutex lock_;
  CSocket *socket_;

  // Serial data visible to VICE and TCP data waiting to be transmitted.
  uint8_t queue_[kQueueSize];
  uint8_t transmit_[kQueueSize];
  uint8_t trace_[kTraceSize];
  uint8_t aciaTrace_[kTraceSize];
  char wifiSsid_[kSsidSize];
  unsigned read_;
  unsigned write_;
  unsigned transmitLength_;
  char command_[kCommandSize];
  unsigned commandLength_;
  unsigned traceWrite_;
  unsigned traceLength_;
  unsigned aciaTraceWrite_;
  unsigned aciaTraceLength_;
  bool open_;
  bool dataMode_;
  bool echo_;
  bool quiet_;
  bool numericResponses_;
  bool crlf_;
  bool dtrEnabled_;
  bool commandInputActive_;
  bool receiveEnabled_;
  bool remoteDisconnectPending_;
  unsigned plusCount_;
  NetworkTask *worker_;
  ListenTask *listener_;
  CSocket *incoming_;
  unsigned rings_;
  u64 nextRing_;
  bool answerPending_;
  uint8_t regs_[16];
  TelnetState telnetState_;
  uint8_t telnetCommand_;
  bool telnetDetected_;
  bool telnetEnabled_;
  u64 lastTransmitTime_;
  u64 escapeDeadline_;
  u64 telnetCandidateDeadline_;
};

void ListenTask::Run() {
  CNetSubSystem *net = 0;
  while ((net = ViceNetworkGetSubsystem()) == 0 || !net->IsRunning()) {
    CScheduler::Get()->MsSleep(1000);
  }
  CSocket listener(net, IPPROTO_TCP);
  if (listener.Bind(kListenPort) < 0 || listener.Listen() < 0) {
    CLogger::Get()->Write(FromBmcModem, LogWarning,
                          "cannot listen on port %u", kListenPort);
    return;
  }
  CLogger::Get()->Write(FromBmcModem, LogNotice, "listening on port %u",
                        kListenPort);
  for (;;) {
    CIPAddress address;
    u16 port = 0;
    CSocket *caller = listener.Accept(&address, &port);
    if (caller == 0) {
      CScheduler::Get()->MsSleep(100);
      continue;
    }
    modem_->Incoming(caller);
  }
}

void NetworkTask::Run() {
  for (;;) {
    modem_->ServiceNetwork();
    CScheduler::Get()->MsSleep(1);
  }
}

BmcModem modem;

}  // namespace

extern "C" int bmcmodem_open(int device) { return modem.Open(device); }

extern "C" void bmcmodem_close(int device) { modem.Close(device); }

extern "C" int bmcmodem_putc(int device, uint8_t byte) {
  (void)device;
  return modem.Put(byte);
}

extern "C" int bmcmodem_getc(int device, uint8_t *byte) {
  (void)device;
  return modem.Get(byte);
}

extern "C" int bmcmodem_has_carrier(void) {
  return modem.HasCarrier() ? 1 : 0;
}

extern "C" void bmcmodem_set_status(int status) { modem.SetStatus(status); }

extern "C" void bmcmodem_set_bps(unsigned int bps) { modem.SetBps(bps); }

extern "C" void bmcmodem_note_acia_tx(uint8_t byte) {
  modem.NoteAciaTransmit(byte);
}

extern "C" unsigned int bmcmodem_acia_trace_read(uint8_t *bytes,
                                                    unsigned int maximum) {
  return modem.ReadAciaTrace(bytes, maximum);
}

extern "C" void bmcmodem_acia_trace_clear(void) { modem.ClearAciaTrace(); }

extern "C" void bmcmodem_init(void) {
  modem.Initialize();
}

extern "C" void bmcmodem_reset(void) { modem.Reset(); }
