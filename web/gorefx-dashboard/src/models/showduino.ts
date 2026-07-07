export type CommandTransport = 'serial' | 'wifi' | 'mock';

export type ShowduinoCommandType =
  | 'SHOW_LOAD'
  | 'SHOW_START'
  | 'SHOW_STOP'
  | 'RELAY_SET'
  | 'RELAY_PULSE'
  | 'AUDIO_PLAY'
  | 'AUDIO_STOP'
  | 'DMX_SET'
  | 'PIXEL_EFFECT'
  | 'STATUS_REQUEST'
  | 'EMERGENCY_STOP'
  | 'EMERGENCY_CLEAR';

export interface ShowduinoCommand {
  id: string;
  type: ShowduinoCommandType;
  label: string;
  raw: string;
  transport: CommandTransport;
  createdAt: string;
}

export type ShowStepType = 'relay' | 'audio' | 'dmx' | 'pixel' | 'delay' | 'sensor' | 'command';

export interface ShowStep {
  id: string;
  type: ShowStepType;
  name: string;
  timeMs: number;
  durationMs?: number;
  command: string;
  notes?: string;
}

export interface ShowduinoProject {
  id: string;
  showName: string;
  version: string;
  description: string;
  createdAt: string;
  updatedAt: string;
  steps: ShowStep[];
}

export const makeCommand = (
  type: ShowduinoCommandType,
  label: string,
  raw: string,
  transport: CommandTransport = 'mock'
): ShowduinoCommand => ({
  id: `${Date.now()}-${Math.random().toString(16).slice(2)}`,
  type,
  label,
  raw,
  transport,
  createdAt: new Date().toISOString()
});

export const demoProject: ShowduinoProject = {
  id: 'demo-zombie-burst',
  showName: 'Zombie Burst',
  version: '0.1.0',
  description: 'Starter timed scare sequence for testing relays, audio, pixels and fog.',
  createdAt: new Date().toISOString(),
  updatedAt: new Date().toISOString(),
  steps: [
    {
      id: 'step-audio-thunder',
      type: 'audio',
      name: 'Play thunder',
      timeMs: 0,
      durationMs: 4000,
      command: 'AUDIO:1:PLAY:014',
      notes: 'Player 1 starts the scare bed.'
    },
    {
      id: 'step-relay-1-on',
      type: 'relay',
      name: 'Relay 1 ON',
      timeMs: 500,
      durationMs: 2500,
      command: 'RELAY:1:ON',
      notes: 'Useful for air cannon, light or prop trigger.'
    },
    {
      id: 'step-pixel-hellfire',
      type: 'pixel',
      name: 'Hellfire pixels',
      timeMs: 2000,
      durationMs: 5000,
      command: 'PIXEL:HELLFIRE',
      notes: 'NeoPixel effect cue.'
    },
    {
      id: 'step-fog-on',
      type: 'relay',
      name: 'Fog ON',
      timeMs: 4000,
      durationMs: 2000,
      command: 'RELAY:4:ON',
      notes: 'Relay 4 reserved for fog in this demo.'
    },
    {
      id: 'step-relay-all-safe',
      type: 'command',
      name: 'Safe stop relays',
      timeMs: 6500,
      durationMs: 0,
      command: 'SHOW:STOP',
      notes: 'Early safety placeholder command for stopping the demo show.'
    }
  ]
};

export const serializeProject = (project: ShowduinoProject): string => {
  return JSON.stringify(project, null, 2);
};
