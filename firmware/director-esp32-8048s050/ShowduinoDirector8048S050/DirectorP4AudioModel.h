#ifndef DIRECTOR_P4_AUDIO_MODEL_H
#define DIRECTOR_P4_AUDIO_MODEL_H

/**
 * DirectorP4AudioModel.h
 *
 * Mirrored P4 / IAN Stage Engine audio state for Director UI presentation.
 *
 * OWNERSHIP:
 *   - The P4 ESP32 Stage Engine (IAN) is the sole authority for all audio:
 *       playback, assets, I2S output, volume, mute, timing, emergency audio.
 *   - The Director has NO local audio engine, NO I2S pins, NO audio files.
 *   - This model only represents what the P4 has reported (or what we know
 *     has been rejected/unsupported).
 *   - Nothing in this model is confirmed until the P4 sends a matching response.
 *
 * PROTOCOL LIMITATIONS (current):
 *   - P4 does not send STATE:AUDIO:* wire messages.
 *   - P4 rejects all AUDIO:LOCAL:* commands (SHOWDUINO_P4_LOCAL_AUDIO_ENABLED=0).
 *   - P4 routes AUDIO:NODE:* commands to the C3 bridge which returns NODE_UNAVAILABLE.
 *   - No duration, elapsed, or detailed fault codes are reported by P4.
 *   - P4 online/offline is inferred from the shared Stage link state, not audio-specific.
 *
 * The UI must present these limitations honestly.
 */

#include <Arduino.h>
#include <string.h>

/* ---- Engine states (P4-reported or inferred) ---- */
enum class P4AudioEngineState : uint8_t {
    Unknown    = 0,  /* No state reported */
    Offline,         /* P4 link is down */
    Disabled,        /* Hardware confirmed disabled (REJECTED:AUDIO:LOCAL:DISABLED) */
    Idle,            /* Engine ready, nothing playing */
    Loading,         /* Show assets loading */
    Ready,           /* Show assets ready */
    Playing,         /* Playback in progress */
    Paused,          /* Playback paused */
    Muted,           /* Engine muted */
    Fault,           /* Engine fault */
    EmergencyAudio   /* Emergency audio has priority */
};

/* ---- Pending command states ---- */
enum class P4AudioCmdState : uint8_t {
    None       = 0,
    Sent,            /* Command forwarded to Stage, awaiting response */
    Acknowledged,    /* Stage sent ACK: */
    Rejected,        /* Stage sent REJECTED: */
    Unsupported,     /* Stage sent UNSUPPORTED: or NOT_IMPLEMENTED: */
    Unavailable,     /* NODE_UNAVAILABLE response */
    TimedOut         /* No response within timeout */
};

#define P4_AUDIO_CMD_TIMEOUT_MS  8000UL   /* 8 s timeout per command */
#define P4_AUDIO_STALE_MS        15000UL  /* Data older than this is labelled stale */

/* ---- Most-recent pending command ---- */
struct P4AudioPendingCmd {
    char          command[48]  = {};
    char          response[64] = {};
    P4AudioCmdState state      = P4AudioCmdState::None;
    unsigned long sentMs       = 0;
    bool          active       = false;

    void send(const char *cmd) {
        if (!cmd) return;
        strncpy(command, cmd, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
        memset(response, 0, sizeof(response));
        state   = P4AudioCmdState::Sent;
        sentMs  = millis();
        active  = true;
    }

    void acknowledge(const char *resp) {
        state = P4AudioCmdState::Acknowledged;
        if (resp) { strncpy(response, resp, sizeof(response) - 1); response[sizeof(response) - 1] = '\0'; }
        active = false;
    }

    void reject(const char *resp) {
        state = P4AudioCmdState::Rejected;
        if (resp) { strncpy(response, resp, sizeof(response) - 1); response[sizeof(response) - 1] = '\0'; }
        active = false;
    }

    void markUnsupported(const char *resp) {
        state = P4AudioCmdState::Unsupported;
        if (resp) { strncpy(response, resp, sizeof(response) - 1); response[sizeof(response) - 1] = '\0'; }
        active = false;
    }

    void markUnavailable(const char *resp) {
        state = P4AudioCmdState::Unavailable;
        if (resp) { strncpy(response, resp, sizeof(response) - 1); response[sizeof(response) - 1] = '\0'; }
        active = false;
    }

    void checkTimeout() {
        if (!active) return;
        if ((millis() - sentMs) >= P4_AUDIO_CMD_TIMEOUT_MS) {
            state  = P4AudioCmdState::TimedOut;
            active = false;
        }
    }

    bool isPending() const { return active; }

    const char *stateText() const {
        switch (state) {
            case P4AudioCmdState::Sent:         return "Sent \xe2\x80\x94 awaiting P4";
            case P4AudioCmdState::Acknowledged: return "Acknowledged by P4";
            case P4AudioCmdState::Rejected:     return "Rejected by P4";
            case P4AudioCmdState::Unsupported:  return "Not supported by P4";
            case P4AudioCmdState::Unavailable:  return "Audio node unavailable";
            case P4AudioCmdState::TimedOut:     return "No response from P4";
            default:                            return "No command pending";
        }
    }

    uint32_t stateColor() const {
        switch (state) {
            case P4AudioCmdState::Acknowledged: return 0x3CFFB0u;  /* Ok    */
            case P4AudioCmdState::Rejected:     return 0xFF5A6Au;  /* Fault */
            case P4AudioCmdState::Unsupported:  return 0xFFD166u;  /* Warn  */
            case P4AudioCmdState::Unavailable:  return 0xFFD166u;  /* Warn  */
            case P4AudioCmdState::TimedOut:     return 0xFF5A6Au;  /* Fault */
            case P4AudioCmdState::Sent:         return 0x39E7FFu;  /* Accent*/
            default:                            return 0x75A0ADu;  /* Muted */
        }
    }
};

/* ---- P4 mirrored audio state ---- */
struct DirectorP4AudioModel {
    /* Engine state (P4-reported or inferred from protocol responses) */
    P4AudioEngineState engineState     = P4AudioEngineState::Unknown;
    bool               p4LinkOnline    = false;
    bool               hardwareEnabled = false; /* false = P4 has audio disabled */
    bool               dataFresh       = false;  /* true if we have any state from P4 */
    unsigned long      lastUpdateMs    = 0;

    /* Current track — P4-reported (NOT read from SD by Director) */
    char  trackName[64]    = {};  /* filename or title as reported */
    char  trackSource[48]  = {};  /* show source if reported */
    bool  trackKnown       = false;
    uint8_t volume         = 0;   /* 0–100 scale; NOT confirmed until P4 reports */
    bool  volumeKnown      = false;
    bool  muted            = false;
    bool  muteKnown        = false;

    /* Elapsed / duration — future capability; P4 does not report yet */
    uint32_t elapsedMs     = 0;
    uint32_t durationMs    = 0;
    bool     timingKnown   = false;

    /* Fault / warning from P4 */
    char  lastFault[80]    = {};
    bool  hasFault         = false;
    bool  emergencyAudio   = false;

    /* Last P4 response line (for display) */
    char  lastP4Response[80] = {};

    /* Pending command tracker */
    P4AudioPendingCmd pending;

    /* ---- Helpers ---- */

    bool isStale() const {
        return dataFresh && lastUpdateMs > 0 &&
               (millis() - lastUpdateMs) >= P4_AUDIO_STALE_MS;
    }

    const char *engineStateText() const {
        switch (engineState) {
            case P4AudioEngineState::Offline:        return "P4 OFFLINE";
            case P4AudioEngineState::Disabled:       return "Audio hardware disabled on P4";
            case P4AudioEngineState::Idle:           return "Idle";
            case P4AudioEngineState::Loading:        return "Loading assets";
            case P4AudioEngineState::Ready:          return "Ready";
            case P4AudioEngineState::Playing:        return "Playing";
            case P4AudioEngineState::Paused:         return "Paused";
            case P4AudioEngineState::Muted:          return "Muted";
            case P4AudioEngineState::Fault:          return "FAULT";
            case P4AudioEngineState::EmergencyAudio: return "EMERGENCY AUDIO ACTIVE";
            default:                                 return "Unknown \xe2\x80\x94 awaiting P4";
        }
    }

    uint32_t engineStateColor() const {
        switch (engineState) {
            case P4AudioEngineState::Playing:        return 0x3CFFB0u;  /* Ok     */
            case P4AudioEngineState::Ready:          return 0x3CFFB0u;  /* Ok     */
            case P4AudioEngineState::Paused:         return 0xFFD166u;  /* Warn   */
            case P4AudioEngineState::Muted:          return 0xFFD166u;  /* Warn   */
            case P4AudioEngineState::Fault:          return 0xFF5A6Au;  /* Fault  */
            case P4AudioEngineState::Offline:        return 0xFF5A6Au;  /* Fault  */
            case P4AudioEngineState::EmergencyAudio: return 0xFF5A6Au;  /* Fault  */
            case P4AudioEngineState::Disabled:       return 0xFFD166u;  /* Warn   */
            default:                                 return 0x75A0ADu;  /* Muted  */
        }
    }

    /* Update engine state from P4 link state */
    void setLinkState(bool online) {
        p4LinkOnline = online;
        if (!online) {
            engineState = P4AudioEngineState::Offline;
            dataFresh   = false;
        } else if (engineState == P4AudioEngineState::Offline) {
            engineState = P4AudioEngineState::Unknown;
        }
    }

    /* Apply a REJECTED:AUDIO:LOCAL:DISABLED response */
    void applyLocalDisabled() {
        hardwareEnabled = false;
        if (engineState == P4AudioEngineState::Unknown ||
            engineState == P4AudioEngineState::Offline) {
            engineState = P4AudioEngineState::Disabled;
        }
        dataFresh    = true;
        lastUpdateMs = millis();
    }

    /* Apply an ACK:AUDIO:* response */
    void applyAck(const char *resp) {
        pending.acknowledge(resp);
        if (resp) {
            strncpy(lastP4Response, resp, sizeof(lastP4Response) - 1);
            lastP4Response[sizeof(lastP4Response) - 1] = '\0';
        }
        lastUpdateMs = millis();
        dataFresh    = true;
    }

    /* Apply an UNSUPPORTED:AUDIO:* or NOT_IMPLEMENTED:AUDIO:* response */
    void applyUnsupported(const char *resp) {
        pending.markUnsupported(resp);
        if (resp) {
            strncpy(lastP4Response, resp, sizeof(lastP4Response) - 1);
            lastP4Response[sizeof(lastP4Response) - 1] = '\0';
        }
    }

    /* Apply a NODE_UNAVAILABLE:AUDIO response */
    void applyNodeUnavailable(const char *resp) {
        pending.markUnavailable(resp);
        if (resp) {
            strncpy(lastP4Response, resp, sizeof(lastP4Response) - 1);
            lastP4Response[sizeof(lastP4Response) - 1] = '\0';
        }
    }

    /* Track an outgoing audio command for the pending state machine */
    void trackCommand(const char *cmd) {
        pending.send(cmd);
        if (cmd) {
            strncpy(lastP4Response, "Sent \xe2\x80\x94 awaiting P4", sizeof(lastP4Response) - 1);
            lastP4Response[sizeof(lastP4Response) - 1] = '\0';
        }
    }

    /* Tick: check for timeout */
    void tick() {
        pending.checkTimeout();
    }

    /* Set emergency audio active (from Stage emergency state) */
    void setEmergencyAudio(bool active) {
        emergencyAudio = active;
        if (active) engineState = P4AudioEngineState::EmergencyAudio;
        else if (engineState == P4AudioEngineState::EmergencyAudio)
            engineState = p4LinkOnline ? P4AudioEngineState::Unknown : P4AudioEngineState::Offline;
    }

    void resetToUnknown() {
        engineState     = p4LinkOnline ? P4AudioEngineState::Unknown : P4AudioEngineState::Offline;
        hardwareEnabled = false;
        dataFresh       = false;
        trackKnown      = false;
        volumeKnown     = false;
        muteKnown       = false;
        timingKnown     = false;
        hasFault        = false;
        emergencyAudio  = false;
        memset(trackName,      0, sizeof(trackName));
        memset(trackSource,    0, sizeof(trackSource));
        memset(lastFault,      0, sizeof(lastFault));
        memset(lastP4Response, 0, sizeof(lastP4Response));
    }
};

#endif /* DIRECTOR_P4_AUDIO_MODEL_H */
