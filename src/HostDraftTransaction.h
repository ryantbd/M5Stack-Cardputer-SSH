#pragma once

#include <stdint.h>

#include "AppConfig.h"

namespace app {

enum class HostDraftStage : uint8_t {
    Editing,
    Connecting,
    Recovery,
    Authenticated,
    Persisted,
    Discarded,
};

class HostDraftTransaction {
public:
    HostDraftTransaction(const SshHostProfile& profile, int savedIndex);
    ~HostDraftTransaction();

    HostDraftTransaction(const HostDraftTransaction&) = delete;
    HostDraftTransaction& operator=(const HostDraftTransaction&) = delete;

    SshHostProfile& profile() { return profile_; }
    const SshHostProfile& profile() const { return profile_; }
    int savedIndex() const { return savedIndex_; }
    HostDraftStage stage() const { return stage_; }

    bool beginAttempt();
    bool attemptFailed();
    bool authenticationSucceeded();
    bool persistenceSucceeded();
    bool persistenceFailed();
    bool retry();
    bool edit();
    bool discard();
    bool canPersist() const;

private:
    SshHostProfile profile_{};
    int savedIndex_ = -1;
    HostDraftStage stage_ = HostDraftStage::Editing;
};

}  // namespace app
