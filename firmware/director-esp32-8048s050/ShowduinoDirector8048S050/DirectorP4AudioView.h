#ifndef DIRECTOR_P4_AUDIO_VIEW_H
#define DIRECTOR_P4_AUDIO_VIEW_H

/**
 * DirectorP4AudioView.h
 *
 * LVGL Audio page builder for the Director.
 * Presents the P4 / IAN Stage Engine audio state.
 *
 * This page is a monitoring and control surface for the P4.
 * It does NOT imply that the Director plays audio locally.
 * All data is either mirrored from P4 or labelled as unavailable.
 */

#include <lvgl.h>
#include "ShowduinoOsUi.h"
#include "DirectorP4AudioModel.h"

namespace DirectorP4AudioView {

/* ---- Page handle struct ---- */
struct PageHandles {
    /* Summary bar */
    lv_obj_t *summaryEngineLabel    = nullptr;  /* "P4 AUDIO ENGINE: state" */
    lv_obj_t *summaryLinkLabel      = nullptr;  /* P4 link state            */

    /* Primary panel */
    lv_obj_t *scrollBox             = nullptr;

    /* Engine status card */
    lv_obj_t *engineStateLabel      = nullptr;
    lv_obj_t *engineStateChip       = nullptr;

    /* Track card */
    lv_obj_t *trackNameLabel        = nullptr;
    lv_obj_t *trackSourceLabel      = nullptr;
    lv_obj_t *trackStateLabel       = nullptr;
    lv_obj_t *trackProgressBar      = nullptr;

    /* Volume / mute */
    lv_obj_t *volumeLabel           = nullptr;
    lv_obj_t *muteLabel             = nullptr;

    /* Controls row */
    lv_obj_t *pauseBtn              = nullptr;
    lv_obj_t *resumeBtn             = nullptr;
    lv_obj_t *stopBtn               = nullptr;
    lv_obj_t *muteBtn               = nullptr;
    lv_obj_t *unmuteBtn             = nullptr;
    lv_obj_t *volDownBtn            = nullptr;
    lv_obj_t *volUpBtn              = nullptr;
    lv_obj_t *statusRequestBtn      = nullptr;

    /* Pending command display */
    lv_obj_t *pendingCmdLabel       = nullptr;
    lv_obj_t *pendingResponseLabel  = nullptr;

    /* Fault / warning */
    lv_obj_t *faultPanel            = nullptr;
    lv_obj_t *faultLabel            = nullptr;

    /* Emergency audio banner */
    lv_obj_t *emergencyPanel        = nullptr;
};

/* ---- Build the static page chrome ---- */
inline PageHandles buildChrome(lv_obj_t *screen, ShowduinoOsTheme &os,
                               lv_event_cb_t cb, void *user) {
    PageHandles h;

    /* Title bar + summary */
    lv_obj_t *sum = os.makePageChrome(screen, "P4 STAGE AUDIO");

    /* Summary row 1: engine state */
    h.summaryEngineLabel = os.makeLabel(sum, "P4 AUDIO ENGINE: Awaiting status", 10, 6);
    lv_obj_add_style(h.summaryEngineLabel, &os.body, 0);
    lv_obj_set_width(h.summaryEngineLabel, 500);
    lv_label_set_long_mode(h.summaryEngineLabel, LV_LABEL_LONG_WRAP);

    /* Summary row 2: link state */
    h.summaryLinkLabel = os.makeLabel(sum, "P4 link: Unknown", 10, 30);
    lv_obj_add_style(h.summaryLinkLabel, &os.caption, 0);
    lv_obj_set_width(h.summaryLinkLabel, 400);

    /* Request status button in summary */
    const int statusBtnW = 138, statusBtnH = 38;
    h.statusRequestBtn = os.makeButton(sum, "Request Status",
                                       OS_CONTENT_FULL_W - statusBtnW - OS_PAD - 14,
                                       (OS_SUMMARY_H - statusBtnH) / 2,
                                       statusBtnW, statusBtnH,
                                       cb, user, "AUDIO:LOCAL:STATUS");

    /* Primary scrollable content */
    h.scrollBox = os.makePrimaryPanel(screen);
    const int pW = OS_CONTENT_FULL_W - 24;

    int y = OS_PAD;

    /* ---- Engine status card ---- */
    lv_obj_t *engineCard = lv_obj_create(h.scrollBox);
    lv_obj_remove_style_all(engineCard);
    lv_obj_add_style(engineCard, &os.panel, 0);
    lv_obj_set_pos(engineCard, 0, y);
    lv_obj_set_size(engineCard, pW, 66);
    lv_obj_clear_flag(engineCard, LV_OBJ_FLAG_SCROLLABLE);

    os.makeHeading(engineCard, "P4 / IAN AUDIO ENGINE", OS_PAD, 4);

    /* State chip (colored rectangle) */
    h.engineStateChip = lv_obj_create(engineCard);
    lv_obj_remove_style_all(h.engineStateChip);
    lv_obj_set_pos(h.engineStateChip, OS_PAD, 24);
    lv_obj_set_size(h.engineStateChip, 16, 16);
    lv_obj_set_style_radius(h.engineStateChip, 8, 0);
    lv_obj_set_style_bg_color(h.engineStateChip, lv_color_hex(0x75A0AD), 0);
    lv_obj_set_style_bg_opa(h.engineStateChip, LV_OPA_COVER, 0);

    h.engineStateLabel = os.makeLabel(engineCard, "Awaiting P4 status", OS_PAD + 22, 24);
    lv_obj_add_style(h.engineStateLabel, &os.body, 0);
    lv_obj_set_width(h.engineStateLabel, pW - OS_PAD * 2 - 22);
    lv_label_set_long_mode(h.engineStateLabel, LV_LABEL_LONG_WRAP);

    y += 74;

    /* ---- Current track card ---- */
    lv_obj_t *trackCard = lv_obj_create(h.scrollBox);
    lv_obj_remove_style_all(trackCard);
    lv_obj_add_style(trackCard, &os.panel, 0);
    lv_obj_set_pos(trackCard, 0, y);
    lv_obj_set_size(trackCard, pW, 80);
    lv_obj_clear_flag(trackCard, LV_OBJ_FLAG_SCROLLABLE);

    os.makeHeading(trackCard, "CURRENT TRACK", OS_PAD, 4);

    h.trackNameLabel = os.makeLabel(trackCard, "No track \xe2\x80\x94 not reported by P4", OS_PAD, 24);
    lv_obj_add_style(h.trackNameLabel, &os.body, 0);
    lv_obj_set_width(h.trackNameLabel, pW - OS_PAD * 2);
    lv_label_set_long_mode(h.trackNameLabel, LV_LABEL_LONG_WRAP);

    h.trackStateLabel = os.makeLabel(trackCard, "State: Awaiting P4", OS_PAD, 46);
    lv_obj_add_style(h.trackStateLabel, &os.caption, 0);
    lv_obj_set_width(h.trackStateLabel, pW - OS_PAD * 2);

    h.trackSourceLabel = os.makeLabel(trackCard, "", pW / 2, 46);
    lv_obj_add_style(h.trackSourceLabel, &os.caption, 0);
    lv_obj_set_width(h.trackSourceLabel, pW / 2 - OS_PAD);

    y += 88;

    /* ---- Volume / mute card ---- */
    lv_obj_t *volCard = lv_obj_create(h.scrollBox);
    lv_obj_remove_style_all(volCard);
    lv_obj_add_style(volCard, &os.panel, 0);
    lv_obj_set_pos(volCard, 0, y);
    lv_obj_set_size(volCard, pW, 58);
    lv_obj_clear_flag(volCard, LV_OBJ_FLAG_SCROLLABLE);

    os.makeHeading(volCard, "VOLUME / MUTE", OS_PAD, 4);

    h.volumeLabel = os.makeLabel(volCard, "Volume: Not reported", OS_PAD, 24);
    lv_obj_add_style(h.volumeLabel, &os.caption, 0);
    lv_obj_set_width(h.volumeLabel, 300);

    h.muteLabel = os.makeLabel(volCard, "Mute: Not reported", 320, 24);
    lv_obj_add_style(h.muteLabel, &os.caption, 0);
    lv_obj_set_width(h.muteLabel, 200);

    y += 66;

    /* ---- Control row ---- */
    lv_obj_t *ctrlCard = lv_obj_create(h.scrollBox);
    lv_obj_remove_style_all(ctrlCard);
    lv_obj_add_style(ctrlCard, &os.panel, 0);
    lv_obj_set_pos(ctrlCard, 0, y);
    lv_obj_set_size(ctrlCard, pW, 100);
    lv_obj_clear_flag(ctrlCard, LV_OBJ_FLAG_SCROLLABLE);

    os.makeHeading(ctrlCard, "P4 AUDIO CONTROLS", OS_PAD, 4);
    os.makeCaption(ctrlCard, "Commands sent to P4 \xe2\x80\x94 not confirmed until P4 responds", OS_PAD, 24);

    /* Row 1: playback — use commands confirmed to exist in Stage protocol */
    const int btnH = 36, btnY1 = 44;
    h.pauseBtn  = os.makeButton(ctrlCard, "Pause",    OS_PAD,       btnY1, 80, btnH, cb, user, "AUDIO:LOCAL:PAUSE");
    h.resumeBtn = os.makeButton(ctrlCard, "Play",     OS_PAD + 88,  btnY1, 80, btnH, cb, user, "AUDIO:LOCAL:PLAY");
    h.stopBtn   = os.makeButton(ctrlCard, "Stop",     OS_PAD + 176, btnY1, 80, btnH, cb, user, "AUDIO:LOCAL:STOP", true);

    /* Row 2: mute / volume */
    const int btnY2 = 88;
    h.muteBtn    = os.makeButton(ctrlCard, "Mute",    OS_PAD,       btnY2, 80, btnH, cb, user, "AUDIO:LOCAL:MUTE");
    h.unmuteBtn  = os.makeButton(ctrlCard, "Play",    OS_PAD + 88,  btnY2, 80, btnH, cb, user, "AUDIO:LOCAL:PLAY");
    h.volDownBtn = os.makeButton(ctrlCard, "Vol \xe2\x96\xbd", OS_PAD + 176, btnY2, 60, btnH, cb, user, "AUDIO:LOCAL:VOLUME:-");
    h.volUpBtn   = os.makeButton(ctrlCard, "Vol \xe2\x96\xb2", OS_PAD + 244, btnY2, 60, btnH, cb, user, "AUDIO:LOCAL:VOLUME:+");

    y += 108;

    /* ---- Pending command status card ---- */
    lv_obj_t *pendCard = lv_obj_create(h.scrollBox);
    lv_obj_remove_style_all(pendCard);
    lv_obj_add_style(pendCard, &os.panel, 0);
    lv_obj_set_pos(pendCard, 0, y);
    lv_obj_set_size(pendCard, pW, 62);
    lv_obj_clear_flag(pendCard, LV_OBJ_FLAG_SCROLLABLE);

    os.makeHeading(pendCard, "LAST COMMAND STATUS", OS_PAD, 4);

    h.pendingCmdLabel = os.makeLabel(pendCard, "No command sent", OS_PAD, 24);
    lv_obj_add_style(h.pendingCmdLabel, &os.body, 0);
    lv_obj_set_width(h.pendingCmdLabel, pW - OS_PAD * 2);
    lv_label_set_long_mode(h.pendingCmdLabel, LV_LABEL_LONG_WRAP);

    h.pendingResponseLabel = os.makeLabel(pendCard, "", OS_PAD, 44);
    lv_obj_add_style(h.pendingResponseLabel, &os.caption, 0);
    lv_obj_set_width(h.pendingResponseLabel, pW - OS_PAD * 2);
    lv_label_set_long_mode(h.pendingResponseLabel, LV_LABEL_LONG_WRAP);

    y += 70;

    /* ---- Emergency audio banner (hidden until active) ---- */
    h.emergencyPanel = lv_obj_create(h.scrollBox);
    lv_obj_remove_style_all(h.emergencyPanel);
    lv_obj_set_pos(h.emergencyPanel, 0, y);
    lv_obj_set_size(h.emergencyPanel, pW, 50);
    lv_obj_set_style_bg_color(h.emergencyPanel, lv_color_hex(0x7F1D1D), 0);
    lv_obj_set_style_bg_opa(h.emergencyPanel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(h.emergencyPanel, lv_color_hex(0xFF4D5E), 0);
    lv_obj_set_style_border_width(h.emergencyPanel, 2, 0);
    lv_obj_set_style_radius(h.emergencyPanel, OS_BTN_RADIUS, 0);
    lv_obj_clear_flag(h.emergencyPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(h.emergencyPanel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *emLab = lv_label_create(h.emergencyPanel);
    lv_label_set_text(emLab, "\xe2\x9a\xa0  EMERGENCY AUDIO ACTIVE \xe2\x80\x94 P4 emergency audio has priority  \xe2\x9a\xa0");
    lv_obj_set_style_text_color(emLab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(emLab);

    /* ---- P4 ownership notice ---- */
    y += 58;
    lv_obj_t *noticeLabel = os.makeLabel(h.scrollBox,
        "All audio is owned and executed by the P4 Stage Engine (IAN).\n"
        "This Director has no local audio engine, no I2S output, and no audio files.\n"
        "Commands are forwarded to P4 and confirmed only when P4 responds.",
        0, y);
    lv_obj_add_style(noticeLabel, &os.caption, 0);
    lv_obj_set_width(noticeLabel, pW);
    lv_label_set_long_mode(noticeLabel, LV_LABEL_LONG_WRAP);

    return h;
}

/* ---- Update all labels from the model ---- */
inline void updateState(PageHandles &h, ShowduinoOsTheme &os,
                         const DirectorP4AudioModel &m) {
    /* Summary engine line */
    if (h.summaryEngineLabel) {
        char buf[80];
        snprintf(buf, sizeof(buf), "P4 AUDIO ENGINE: %s", m.engineStateText());
        ShowduinoOsTheme::setTextIfChanged(h.summaryEngineLabel, buf);
        lv_obj_set_style_text_color(h.summaryEngineLabel,
                                    lv_color_hex(m.engineStateColor()), 0);
    }

    /* Summary link line */
    if (h.summaryLinkLabel) {
        const char *lnk;
        if (m.emergencyAudio)    lnk = "P4 link: Online \xe2\x80\x94 EMERGENCY AUDIO";
        else if (!m.p4LinkOnline) lnk = "P4 link: Offline";
        else if (!m.dataFresh)    lnk = "P4 link: Online \xe2\x80\x94 awaiting status";
        else if (m.isStale())     lnk = "P4 link: Online \xe2\x80\x94 data stale";
        else                      lnk = "P4 link: Online";
        ShowduinoOsTheme::setTextIfChanged(h.summaryLinkLabel, lnk);
        uint32_t lnkColor = m.p4LinkOnline ? (m.isStale() ? 0xFFD166u : 0x3CFFB0u) : 0xFF5A6Au;
        lv_obj_set_style_text_color(h.summaryLinkLabel, lv_color_hex(lnkColor), 0);
    }

    /* Engine state chip + label */
    if (h.engineStateChip) {
        lv_obj_set_style_bg_color(h.engineStateChip,
                                   lv_color_hex(m.engineStateColor()), 0);
    }
    if (h.engineStateLabel) {
        ShowduinoOsTheme::setTextIfChanged(h.engineStateLabel, m.engineStateText());
        lv_obj_set_style_text_color(h.engineStateLabel, lv_color_hex(m.engineStateColor()), 0);
    }

    /* Track name */
    if (h.trackNameLabel) {
        const char *tn = m.trackKnown && m.trackName[0]
                         ? m.trackName
                         : "No track \xe2\x80\x94 not reported by P4";
        ShowduinoOsTheme::setTextIfChanged(h.trackNameLabel, tn);
    }

    /* Track state */
    if (h.trackStateLabel) {
        char tsBuf[64];
        snprintf(tsBuf, sizeof(tsBuf), "State: %s", m.engineStateText());
        ShowduinoOsTheme::setTextIfChanged(h.trackStateLabel, tsBuf);
    }

    /* Track source */
    if (h.trackSourceLabel) {
        char tsBuf[48];
        if (m.trackKnown && m.trackSource[0]) {
            snprintf(tsBuf, sizeof(tsBuf), "Show: %s", m.trackSource);
        } else {
            tsBuf[0] = '\0';
        }
        ShowduinoOsTheme::setTextIfChanged(h.trackSourceLabel, tsBuf);
    }

    /* Volume */
    if (h.volumeLabel) {
        char buf[48];
        if (m.volumeKnown) snprintf(buf, sizeof(buf), "Volume: %u", (unsigned)m.volume);
        else strncpy(buf, "Volume: Not reported", sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        ShowduinoOsTheme::setTextIfChanged(h.volumeLabel, buf);
    }

    /* Mute */
    if (h.muteLabel) {
        const char *muteStr = m.muteKnown ? (m.muted ? "Mute: ON" : "Mute: OFF") : "Mute: Not reported";
        ShowduinoOsTheme::setTextIfChanged(h.muteLabel, muteStr);
        uint32_t muteColor = m.muteKnown ? (m.muted ? 0xFFD166u : 0x3CFFB0u) : 0x75A0ADu;
        lv_obj_set_style_text_color(h.muteLabel, lv_color_hex(muteColor), 0);
    }

    /* Control button states based on engine state and emergency */
    bool emergency = m.emergencyAudio;
    bool p4ok = m.p4LinkOnline;
    bool pending = m.pending.isPending();

    auto setEnabled = [](lv_obj_t *btn, bool en) {
        if (!btn) return;
        if (en) {
            lv_obj_clear_state(btn, LV_STATE_DISABLED);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        } else {
            lv_obj_add_state(btn, LV_STATE_DISABLED);
            lv_obj_set_style_bg_opa(btn, LV_OPA_40, 0);
        }
    };

    bool canPlay  = p4ok && !emergency && !pending;
    bool canMute  = p4ok && !emergency;
    bool canVol   = p4ok && !emergency;

    setEnabled(h.pauseBtn,   canPlay);
    setEnabled(h.resumeBtn,  canPlay);
    setEnabled(h.stopBtn,    canPlay);
    setEnabled(h.muteBtn,    canMute && m.muteKnown && !m.muted);
    setEnabled(h.unmuteBtn,  canMute && m.muteKnown && m.muted);
    setEnabled(h.volDownBtn, canVol);
    setEnabled(h.volUpBtn,   canVol);
    setEnabled(h.statusRequestBtn, p4ok);

    /* Pending command display */
    if (h.pendingCmdLabel) {
        char buf[96];
        if (m.pending.active || m.pending.state != P4AudioCmdState::None) {
            snprintf(buf, sizeof(buf), "%.48s \xe2\x80\x94 %s",
                     m.pending.command[0] ? m.pending.command : "(unknown)",
                     m.pending.stateText());
        } else {
            snprintf(buf, sizeof(buf), "No recent command");
        }
        ShowduinoOsTheme::setTextIfChanged(h.pendingCmdLabel, buf);
        lv_obj_set_style_text_color(h.pendingCmdLabel,
                                     lv_color_hex(m.pending.stateColor()), 0);
    }

    if (h.pendingResponseLabel) {
        ShowduinoOsTheme::setTextIfChanged(h.pendingResponseLabel,
            m.lastP4Response[0] ? m.lastP4Response : "");
    }

    /* Emergency banner */
    if (h.emergencyPanel) {
        if (m.emergencyAudio) {
            lv_obj_clear_flag(h.emergencyPanel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(h.emergencyPanel, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

} /* namespace DirectorP4AudioView */

#endif /* DIRECTOR_P4_AUDIO_VIEW_H */
