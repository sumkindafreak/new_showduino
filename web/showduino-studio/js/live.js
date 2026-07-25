import { getRuntimeState, startRuntimeStore, subscribeRuntime } from './state/runtimeStore.js';

export function connectLive() {
  startRuntimeStore();
}

export function getLiveSnapshot() {
  const state = getRuntimeState();
  return {
    devices: state.nodeCollection.nodes,
    network: state.networkState,
    commands: {
      running: state.runtimeStatus.executingActions,
      queue: [],
      history: state.logs
    },
    time: state.clock,
    lifecycle: state.connectionStatus.lifecycle
  };
}

export function subscribeLive(listener) {
  return subscribeRuntime((state) => {
    listener({
      devices: state.nodeCollection.nodes,
      network: state.networkState,
      commands: {
        running: state.runtimeStatus.executingActions,
        queue: [],
        history: state.logs
      },
      time: state.clock,
      lifecycle: state.connectionStatus.lifecycle
    });
  });
}