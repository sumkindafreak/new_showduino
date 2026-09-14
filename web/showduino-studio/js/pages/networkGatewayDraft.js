/** Transient Network-page Wi-Fi draft. Not persisted. Password never goes in status. */

export function createGatewayDraft() {
  return {
    ssid: '',
    password: '',
    ssidDirty: false,
    passwordDirty: false
  };
}

export function applySsidInput(draft, value) {
  draft.ssid = value == null ? '' : String(value);
  draft.ssidDirty = true;
}

export function applyPasswordInput(draft, value) {
  draft.password = value == null ? '' : String(value);
  draft.passwordDirty = true;
}

export function applyScanSelection(draft, ssid) {
  if (ssid == null || ssid === '') return false;
  draft.ssid = String(ssid);
  draft.ssidDirty = true;
  return true;
}

export function ssidForField(draft, serverSsid) {
  if (draft.ssidDirty) return draft.ssid;
  if (serverSsid != null && String(serverSsid) !== '') return String(serverSsid);
  return draft.ssid || '';
}

export function passwordForField(draft) {
  return draft.password || '';
}

/** Capture credentials for connectGateway. Call BEFORE any paint() that may rebuild controls. */
export function captureConnectCredentials(draft) {
  const ssid = String(draft.ssid || '').trim();
  const password = draft.password == null ? '' : String(draft.password);
  if (!ssid) return { ok: false, error: 'missing_ssid' };
  return { ok: true, ssid, password };
}

export function afterConnectSuccess(draft) {
  draft.password = '';
  draft.passwordDirty = false;
  if (draft.ssid) draft.ssidDirty = true;
}

export function afterForget(draft) {
  draft.ssid = '';
  draft.password = '';
  draft.ssidDirty = false;
  draft.passwordDirty = false;
}

export function statusSafeDraft(draft) {
  return {
    ssid: draft.ssid,
    ssidDirty: !!draft.ssidDirty,
    passwordDirty: !!draft.passwordDirty
  };
}

export function fieldsAfterRepaint(draft, serverSsid) {
  return {
    ssid: ssidForField(draft, serverSsid),
    password: passwordForField(draft)
  };
}
