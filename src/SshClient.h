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
    // Methods the server offers (SSH_AUTH_METHOD_* bits); 0 on error. Call
    // after the host key is verified.
    int authMethods(String& error);
    bool authenticatePassword(const char* password, String& error);
    // On a rejected key the session stays open (and `rejected` is set) so the
    // caller can fall back to a password.
    bool authenticatePublicKey(const char* privateKey, bool& rejected, String& error);

    // OpenSSH-format private key text for a new Ed25519 key.
    static bool generateEd25519Key(char* privateKey, size_t capacity, String& error);
    // "ssh-ed25519 AAAA... comment" and its SHA256 fingerprint.
    static bool describePublicKey(const char* privateKey, const char* comment,
                                  String& publicKeyLine, String& fingerprint);

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
    bool openShell(String& error);

    ssh_session session_ = nullptr;
    ssh_channel channel_ = nullptr;
    String fingerprint_;
    uint8_t fingerprintHash_[app::kHostKeyHashSize]{};
    size_t fingerprintHashLength_ = 0;
    bool verified_ = false;
};
