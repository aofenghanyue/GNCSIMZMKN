# ADR-0004: Internal-only licensing and provenance gate

- Status: Accepted
- Date: 2026-08-09
- Owners: Product Owner
- Required reviewers: Architecture Lead, Scientific Authority
- Reviewed by: `codex-r0-architecture`, `codex-r0-science`
- Related tasks: R0-GOV-002
- Architecture references: 03 §4、05 §3.2、08 §12、08 §21

## Context

The repository has no selected distribution license. Its first-party skeleton, read-only legacy archive, scientific assets, external tools, future dependencies, and generated evidence have different rights and provenance risks. A repository-wide license label cannot safely grant rights over material whose ownership or redistribution terms have not been reviewed.

Official guidance supports precise, item-level handling: GitHub documents that a repository with no license provides no general reuse grant; SPDX provides stable license identifiers and expressions when an exact license is known; the REUSE specification describes file-level licensing records; Creative Commons warns that data/database rights and third-party permissions require separate consideration.

References:

- [GitHub repository licensing](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository)
- [SPDX License List](https://spdx.org/licenses/)
- [REUSE Specification 3.3](https://reuse.software/spec/)
- [Creative Commons data FAQ](https://creativecommons.org/faq/#frequently-asked-questions-about-data-and-cc-licenses)

## Decision

Until a later accepted ADR selects an external distribution license and confirms ownership, all repository content remains internal research material with no external license grant.

Every imported dependency, tool, formula, dataset, asset, template, reference archive, and externally shared generated artifact requires an item-level provenance record. The R0 record is governance metadata only. It uses a closed subject-type vocabulary plus an explicit scientific-context flag, and records identity, origin, integrity, rights status or permission basis, allowed and prohibited uses, reviewer, and downstream propagation requirements. Scientific records also require an independently confirmed reference/oracle and the basis for that independence. It cannot become a runtime, package, Artifact, Compiler, or Session schema.

The policy uses SPDX expressions only when the reviewed license text matches an SPDX entry. Unknown, ambiguous, missing, or conflicting rights produce a blocked external-sharing decision. `NOASSERTION`, a blank value, or an inferred license cannot serve as clearance.

Internal build, test, evidence extraction, and research analysis are allowed only within the authorized workspace and subject to source-specific restrictions. External publication, redistribution, sublicensing, public repository upload, and transfer of the legacy archive remain prohibited until the required review closes.

Restricted and unreviewed material always propagates its restrictions. Generated artifacts identify upstream provenance records and lineage, inherit the most restrictive applicable classification and sharing constraint, and carry every upstream propagation requirement. A derivative result receives its own identity and lineage; generation does not erase upstream obligations.

## Consequences

- `LICENSE-STATUS.md` remains the visible repository notice and links to this decision after acceptance.
- A versioned R0 governance record and validation script provide executable positive and negative checks.
- New third-party dependencies still require their own ADR under repository policy.
- Public release requires a later license-selection ADR, rights-holder confirmation, notice/attribution bundle, and legal review where ownership or jurisdiction is unclear.
- Scientific provenance and legal permission remain separate checklist sections and both must close for externally shared scientific material.

## Verification plan

- Validate a complete internal-use provenance record.
- Reject unknown subject types and records missing identity, origin, integrity, rights basis, restrictions, reviewer, or required scientific facts.
- Reject scientific records without a confirmed independent reference/oracle and independence basis.
- Reject restricted records with propagation disabled and generated records with missing upstream/lineage refs or weaker classification.
- Confirm the legacy archive is explicitly classified as internal evidence only.
- Run the policy conformance checks from `tools/bootstrap.ps1`.
- Obtain independent Architecture Lead and Scientific Authority review before changing this ADR to Accepted.
