#include "HostDraftTransaction.h"

namespace app {

HostDraftTransaction::HostDraftTransaction(const SshHostProfile& profile, int savedIndex)
    : profile_(profile), savedIndex_(savedIndex) {}

HostDraftTransaction::~HostDraftTransaction() {
    secureClear(&profile_, sizeof(profile_));
}

bool HostDraftTransaction::beginAttempt() {
    if (stage_ != HostDraftStage::Editing && stage_ != HostDraftStage::Recovery) return false;
    stage_ = HostDraftStage::Connecting;
    return true;
}

bool HostDraftTransaction::attemptFailed() {
    if (stage_ != HostDraftStage::Connecting) return false;
    stage_ = HostDraftStage::Recovery;
    return true;
}

bool HostDraftTransaction::authenticationSucceeded() {
    if (stage_ != HostDraftStage::Connecting) return false;
    stage_ = HostDraftStage::Authenticated;
    return true;
}

bool HostDraftTransaction::persistenceSucceeded() {
    if (stage_ != HostDraftStage::Authenticated) return false;
    stage_ = HostDraftStage::Persisted;
    return true;
}

bool HostDraftTransaction::persistenceFailed() {
    if (stage_ != HostDraftStage::Authenticated) return false;
    stage_ = HostDraftStage::Recovery;
    return true;
}

bool HostDraftTransaction::retry() {
    if (stage_ != HostDraftStage::Recovery) return false;
    stage_ = HostDraftStage::Connecting;
    return true;
}

bool HostDraftTransaction::edit() {
    if (stage_ != HostDraftStage::Recovery) return false;
    stage_ = HostDraftStage::Editing;
    return true;
}

bool HostDraftTransaction::discard() {
    if (stage_ != HostDraftStage::Editing && stage_ != HostDraftStage::Recovery) return false;
    secureClear(&profile_, sizeof(profile_));
    stage_ = HostDraftStage::Discarded;
    return true;
}

bool HostDraftTransaction::canPersist() const {
    return stage_ == HostDraftStage::Authenticated;
}

}  // namespace app
