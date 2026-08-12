#pragma once

#include <stdint.h>

namespace app {

enum class AppPage : uint8_t { Boot, WifiRecovery, HostLauncher, Connecting, Terminal, Settings, Confirmation, HardError };
enum class TerminalMode : uint8_t { Remote, Local };
enum class SessionEvent : uint8_t {
    ConfigReady, WifiConnected, WifiFailed, HostSelected, HostVerified, Authenticated,
    HostKeyMismatch, ToggleTerminalMode, OpenSettings, CancelSettings, ActiveConnectionChanged,
    RequestEnd, CancelEnd, ConfirmEnd, RemoteClosed, WifiLost
};

class SessionFlow {
public:
    bool apply(SessionEvent event);
    AppPage page() const { return page_; }
    TerminalMode mode() const { return mode_; }
    bool wifiReady() const { return wifiReady_; }
    bool sessionLive() const { return sessionLive_; }
    bool sshWriteAllowed() const { return page_ == AppPage::Terminal && sessionLive_ && mode_ == TerminalMode::Remote; }

private:
    AppPage page_ = AppPage::Boot;
    AppPage previousPage_ = AppPage::Boot;
    TerminalMode mode_ = TerminalMode::Remote;
    TerminalMode previousMode_ = TerminalMode::Remote;
    bool wifiReady_ = false;
    bool sessionLive_ = false;
    bool hostVerified_ = false;
};

}  // namespace app
