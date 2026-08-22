# Architecture Decision Records

Showduino OS ADRs record **why** a decision was made, not every detail of how.

From **Architecture Frozen** onward, new ADRs are rare. Prefer fitting the platform (see `os2/Foundation.h` Ten Laws). If you need an ADR that invents a new architectural rule, stop — redesign the feature (Law 9).

| ADR | Title |
|-----|--------|
| [0001](0001-platform-boundary.md) | Platform vs Stage Runtime |
| [0002](0002-command-service.md) | CommandService (intent) |
| [0003](0003-event-bus.md) | Event Bus |
| [0004](0004-production-model.md) | Production as core object |
| [0005](0005-app-registry.md) | App Registry |

Format: Context → Decision → Consequences.