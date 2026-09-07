const ROLE_PHRASE = {
  P4_INTERNAL_AUDIO: 'P4 Internal Audio',
  DIGITAL_INPUTS: 'Digital Inputs',
  DIGITAL_OUTPUTS: 'Digital Outputs',
  DIGITAL_IO: 'Digital I/O',
  PWM_OUTPUTS: 'PWM Outputs',
  SERVO_OUTPUTS: 'Servo Outputs',
  I2C_MULTIPLEXER: 'I2C Multiplexer',
  NONE: 'Unconfigured'
};

export function isP4DataOffline(data) {
  return !data || data.p4Online === false || data.stageLink === 'offline' || data.error === 'p4_offline';
}

export function linkWord(online, seen) {
  if (online) return 'ONLINE';
  if (seen === false) return 'SEARCHING';
  return 'OFFLINE';
}

export function showDisplayState(sys) {
  if (isP4DataOffline(sys)) return 'OFFLINE';
  const raw = String(sys.showState || '').toUpperCase();
  if (sys.emergencyActive || raw === 'EMERGENCY_STOP' || raw === 'EMERGENCY') return 'EMERGENCY';
  if (raw === 'RUNNING' || sys.showRunning) return 'RUNNING';
  if (raw === 'PAUSED' || sys.showPaused) return 'PAUSED';
  if (raw === 'ERROR') return 'FAULT';
  if (raw === 'BOOTING') return 'SYNCHRONISING';
  return 'STOPPED';
}

export function systemHealth(comms, sys) {
  if (!comms) return 'FAULT';
  if (sys && !isP4DataOffline(sys) && (sys.emergencyActive || sys.showState === 'EMERGENCY_STOP')) {
    return 'EMERGENCY';
  }
  if (!comms.p4Online || isP4DataOffline(sys)) return 'DEGRADED';
  return 'READY';
}

export function chipClass(word) {
  switch (String(word || '').toUpperCase()) {
    case 'ONLINE':
    case 'READY':
    case 'RUNNING':
    case 'CLEAR':
      return 'ok';
    case 'OFFLINE':
    case 'EMERGENCY':
    case 'FAULT':
      return 'bad';
    case 'DEGRADED':
    case 'SEARCHING':
    case 'SYNCHRONISING':
    case 'PAUSED':
    case 'UNCONFIGURED':
    case 'PLANNED':
    case 'PENDING':
      return 'warn';
    case 'STOPPED':
    default:
      return 'unknown';
  }
}

export function productionLabel(sys) {
  if (isP4DataOffline(sys)) return 'NO PRODUCTION';
  const name = (sys && (sys.productionName || sys.showName)) || '';
  return name || 'NO PRODUCTION';
}

export function pluginRolePhrase(role) {
  if (!role) return 'Unconfigured';
  return ROLE_PHRASE[role] || String(role).replace(/_/g, ' ');
}

export function pluginDisplayName(device) {
  const name = (device && (device.name || device.friendlyName)) || '';
  if (name && name !== 'Unknown I2C Device' && name !== 'I2C plugin') return name;
  const chip = (device && (device.chip || device.board)) || '';
  const role = pluginRolePhrase(device && device.role);
  if (chip && role) return `${chip} - ${role}`;
  return name || 'Unknown I2C Device';
}

export function pluginConfigWord(device) {
  if (!device) return 'UNCONFIGURED';
  if (device.configured === false || device.role === 'NONE' || device.classification === 'unconfigured') {
    return 'UNCONFIGURED';
  }
  return device.configured ? 'READY' : 'UNCONFIGURED';
}

export function presenceWord(online) {
  return online ? 'ONLINE' : 'OFFLINE';
}

export function emergencyWord(sys) {
  if (isP4DataOffline(sys)) return 'OFFLINE';
  if (sys && sys.emergencyActive) return 'EMERGENCY';
  return 'CLEAR';
}

export function loopWord(sys) {
  if (isP4DataOffline(sys)) return '—';
  if (!sys) return '—';
  return sys.emergencyButtonPressed ? 'OPEN' : 'HEALTHY';
}
