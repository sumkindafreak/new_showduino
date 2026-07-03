export type CueType = "audio" | "pixel" | "wait" | "note" | "relay_future" | "dmx_future";

export type PixelEffect =
  | "OFF"
  | "SOLID"
  | "PULSE"
  | "FIRE"
  | "STROBE"
  | "LIGHTNING"
  | "PORTAL_GLOW"
  | "BLACKOUT";

export interface RgbColour {
  r: number;
  g: number;
  b: number;
}

export interface BaseCue {
  id: string;
  name: string;
  type: CueType;
  trackId: string;
  startMs: number;
  durationMs: number;
  enabled: boolean;
  notes: string;
}

export interface AudioCue extends BaseCue {
  type: "audio";
  file: string;
  volume: number;
  action: "PLAY" | "STOP" | "VOLUME";
}

export interface PixelCue extends BaseCue {
  type: "pixel";
  line: 1 | 2 | 3 | 4 | "ALL";
  effect: PixelEffect;
  colour: RgbColour;
  brightness: number;
  speed: number;
}

export interface WaitCue extends BaseCue {
  type: "wait";
}

export interface NoteCue extends BaseCue {
  type: "note";
}

export interface FutureRelayCue extends BaseCue {
  type: "relay_future";
  relay: number;
  action: "ON" | "OFF" | "PULSE";
}

export interface FutureDmxCue extends BaseCue {
  type: "dmx_future";
  channel: number;
  value: number;
}

export type SceneCue = AudioCue | PixelCue | WaitCue | NoteCue | FutureRelayCue | FutureDmxCue;

export interface SceneTrack {
  id: string;
  name: string;
  type: CueType | "mixed";
  colour: string;
  locked: boolean;
  muted: boolean;
}

export interface ShowduinoScene {
  format: "showduino-scene";
  version: 1;
  id: string;
  name: string;
  description: string;
  author: string;
  durationMs: number;
  createdAt: string;
  updatedAt: string;
  tracks: SceneTrack[];
  cues: SceneCue[];
}

export interface CompiledCommand {
  cueId: string;
  timeMs: number;
  command: string;
  label: string;
}
