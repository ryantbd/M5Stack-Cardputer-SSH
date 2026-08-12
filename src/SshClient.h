#pragma once

#include <Arduino.h>
#include <libssh/libssh.h>

#include "AppConfig.h"
#include "SshReadResult.h"

class SshClient {
public:
    SshClient() = default;
    ~SshClient();

    SshClient(const SshClient&) = delete;
    SshClient& operator=(const SshClient&) = delete;

    bool start(const char* host, uint16_t port, const char* user, String& error);
    bool authenticatePassword(const char* password, String& error);

    bool verifyServerKey(const uint8_t* expectedHash, size_t length);
    void trustCurrentServerKey();
    bool copyServerKeyHash(uint8_t* destination, size_t length) const;

    void disconnect();

    bool connected() const;
    bool sessionConnected() const;
    app::SshReadResult read(char* buffer, size_t length);
    bool write(const char* data, size_t length);
    bool write(const String& data);

    const String& serverFingerprint() const;
    bool hostKeyVerified() const;

private:
    bool readServerFingerprint(String& error);

    ssh_session session_ = nullptr;
    ssh_channel channel_ = nullptr;
    String fingerprint_;
    uint8_t fingerprintHash_[app::kHostKeyHashSize]{};
    size_t fingerprintHashLength_ = 0;
    bool verified_ = false;
};
