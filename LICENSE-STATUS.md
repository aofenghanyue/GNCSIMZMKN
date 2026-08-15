# License status

No repository distribution license has been selected. This notice is not a license grant.

On 2026-08-15, the repository owner accepted an internal-development-only G1 scope with no new external distribution. The GitHub origin and an existing fork, `zbyandmoon/GNCSIMZMKN`, were observed as public on the same date. Public visibility and GitHub's view/fork permissions do not supply a general repository distribution license. The public origin requires a separately authorized transition to private. GitHub documents that the existing public fork will remain public in a detached network after that transition. See [GitHub's repository licensing guidance](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository) and [repository visibility effects](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/managing-repository-settings/setting-repository-visibility).

Current implementation and validation may continue inside the already-authorized workspace. The implementation agent will not push to a public remote, publish a release, send a public bundle, or change remote visibility without explicit authority.

External distribution remains blocked for repository content, imported architecture blueprints, fixtures, oracles, benchmarks, reports, and binaries. Making the origin private will stop future public distribution controlled by the repository owner; it cannot recall the existing public fork or other copies. The frozen Legacy reference remains read-only, evidence-only, and excluded from any external candidate until ownership and license evidence are resolved.

Eigen, w64devkit, host toolchains, Python, PowerShell, CMake, and CI actions are executed outside the repository and are not bundled in the current project. Any future vendoring, runtime bundling, container, installer, or binary release requires a review of the exact delivered components and their upstream terms.

The current boundary and executable check are documented in [ADR-0008](docs/adr/0008-internal-default-license-and-provenance-gate.md) and the [license and provenance policy](docs/governance/license-and-provenance-policy.md).
