// Native host tests for the allocation-free performance and read-only card model.
#include <cassert>
#include <cstdint>
#include <iostream>
#include "../components/DirectorV11Performance.h"
#include "../components/DirectorV11CardModel.h"

int main() {
  using namespace ShowduinoV11;
  TimingWindow<10> timing;
  auto empty = timing.snapshot();
  assert(!empty.valid && empty.window_samples == 0 && empty.total_samples == 0);

  for (uint32_t i = 1; i <= 10; ++i) timing.observe(i * 100);
  auto first = timing.snapshot();
  assert(first.valid);
  assert(first.window_samples == 10 && first.total_samples == 10);
  assert(first.minimum_us == 100 && first.maximum_us == 1000);
  assert(first.mean_us == 550 && first.p95_us == 1000);

  timing.observe(2000); // oldest value rolls out of the fixed window
  auto rolled = timing.snapshot();
  assert(rolled.window_samples == 10 && rolled.total_samples == 11);
  assert(rolled.minimum_us == 200 && rolled.maximum_us == 2000);
  assert(rolled.mean_us == 740 && rolled.p95_us == 2000);
  timing.clear();
  assert(!timing.snapshot().valid);

  PerformanceMonitor monitor;
  monitor.observeUiLoop(850);
  monitor.observeLvglHandler(4400);
  const auto combined = monitor.snapshot();
  assert(combined.ui_loop.mean_us == 850);
  assert(combined.lvgl_handler.maximum_us == 4400);

  NodeEvidence node;
  assert(classifyNode(node) == CardStatus::LinkOffline);
  node.commsOnline = true;
  assert(classifyNode(node) == CardStatus::AwaitingState);
  node.stateReceived = true;
  node.nodeReportsOnline = true;
  assert(classifyNode(node) == CardStatus::AwaitingState); // ACK is insufficient
  node.p4Authorised = true;
  assert(classifyNode(node) == CardStatus::Ready);
  node.stateAgeMs = 6001;
  assert(classifyNode(node) == CardStatus::Stale);
  node.nodeReportsFault = true;
  assert(classifyNode(node) == CardStatus::Fault);
  node.commsOnline = false;
  assert(classifyNode(node) == CardStatus::LinkOffline);
  node.emergencyActive = true;
  assert(classifyNode(node) == CardStatus::Emergency);
  assert(statusColorRgb(CardStatus::Emergency) == 0xF04A48);
  assert(statusLabel(CardStatus::Ready)[0] == 'R');

  std::cout << "Director V1.1 native model tests PASSED\n";
  return 0;
}
