# License status

No repository distribution license has been selected. This notice is not a license grant.

The GitHub origin was observed as public on 2026-08-15. Public visibility and GitHub's view/fork permissions do not supply a general repository distribution license; the repository owner still needs to decide the intended G1 distribution scope. See [GitHub's repository licensing guidance](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository).

Current implementation and validation may continue inside the already-authorized workspace. The implementation agent will not push, publish a release, send a public bundle, or change remote visibility without explicit authority.

External distribution remains blocked for repository content, imported architecture blueprints, fixtures, oracles, benchmarks, reports, and binaries. The frozen Legacy reference remains read-only, evidence-only, and excluded from any external candidate until ownership and license evidence are resolved.

Eigen, w64devkit, host toolchains, Python, PowerShell, CMake, and CI actions are executed outside the repository and are not bundled in the current project. Any future vendoring, runtime bundling, container, installer, or binary release requires a review of the exact delivered components and their upstream terms.

The current boundary and executable check are documented in [ADR-0008](docs/adr/0008-internal-default-license-and-provenance-gate.md) and the [license and provenance policy](docs/governance/license-and-provenance-policy.md).
