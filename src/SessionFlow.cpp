#include "SessionFlow.h"

namespace app {

bool SessionFlow::apply(SessionEvent event) {
    switch (event) {
        case SessionEvent::ConfigReady:
            return page_ == AppPage::Boot;
        case SessionEvent::WifiConnected:
            wifiReady_ = true; sessionLive_ = false; hostVerified_ = false; page_ = AppPage::HostLauncher; return true;
        case SessionEvent::WifiFailed:
            wifiReady_ = false; sessionLive_ = false; page_ = AppPage::WifiRecovery; return true;
        case SessionEvent::HostSelected:
            if (!wifiReady_ || page_ != AppPage::HostLauncher) return false;
            hostVerified_ = false; page_ = AppPage::Connecting; return true;
        case SessionEvent::HostVerified:
            if (page_ != AppPage::Connecting) return false;
            hostVerified_ = true; return true;
        case SessionEvent::Authenticated:
            if (page_ != AppPage::Connecting || !hostVerified_) return false;
            sessionLive_ = true; mode_ = TerminalMode::Remote; page_ = AppPage::Terminal; return true;
        case SessionEvent::HostKeyMismatch:
            sessionLive_ = false; hostVerified_ = false; page_ = AppPage::HardError; return true;
        case SessionEvent::ToggleTerminalMode:
            if (page_ != AppPage::Terminal || !sessionLive_) return false;
            mode_ = mode_ == TerminalMode::Remote ? TerminalMode::Local : TerminalMode::Remote; return true;
        case SessionEvent::OpenSettings:
            previousPage_ = page_; previousMode_ = mode_; page_ = AppPage::Settings; return true;
        case SessionEvent::CancelSettings:
            if (page_ != AppPage::Settings) return false;
            page_ = previousPage_; mode_ = previousMode_; return true;
        case SessionEvent::ActiveConnectionChanged:
        case SessionEvent::RemoteClosed:
            sessionLive_ = false; hostVerified_ = false; mode_ = TerminalMode::Remote;
            page_ = wifiReady_ ? AppPage::HostLauncher : AppPage::WifiRecovery; return true;
        case SessionEvent::RequestEnd:
            if (!sessionLive_) return false;
            previousMode_ = mode_; page_ = AppPage::Confirmation; return true;
        case SessionEvent::CancelEnd:
            if (page_ != AppPage::Confirmation) return false;
            page_ = AppPage::Terminal; mode_ = previousMode_; return true;
        case SessionEvent::ConfirmEnd:
            if (page_ != AppPage::Confirmation) return false;
            sessionLive_ = false; hostVerified_ = false; mode_ = TerminalMode::Remote; page_ = AppPage::HostLauncher; return true;
        case SessionEvent::WifiLost:
            wifiReady_ = false; sessionLive_ = false; hostVerified_ = false; page_ = AppPage::WifiRecovery; return true;
    }
    return false;
}

}  // namespace app
