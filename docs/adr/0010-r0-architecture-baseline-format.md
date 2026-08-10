# ADR-0010: R0 architecture baseline document format

- Status: Accepted
- Date: 2026-08-09
- Owners: Architecture Lead
- Reviewed by: Product Owner (approved 2026-08-09)
- Related tasks: R0-ARCH-001, R0-ARCH-002
- Architecture references: reference-glossary, 01 section 16, 02 sections 1.2, 1.3, 13, and 15, ADR-0003, ADR-0011

## Context

R0-ARCH-001 requires three machine-readable governance artifacts: a terminology baseline, a module dependency map, and a legacy-to-target ownership map. The acceptance rule requires every shared term, enum, key, and owner declaration to resolve to one registry authority. A repeatable validator also needs an unambiguous edge direction and an ownership record shape.

These artifacts describe the R0 architecture review surface. They do not define Mission, plan, runtime, evidence, or other production payload schemas. No R1-R8 production module may consume them as a runtime API during R0.

## Decision

Use three UTF-8 JSON documents under `specs/architecture/r0/`, all identified by `schema_version: gnczmkn.r0-architecture-baseline/1` and a distinct `document_kind`:

1. `terminology-baseline.json` records canonical terms, retired aliases, complete enums selected by the R0 sources, shared key compositions, and the four AuthorityDomain owners.
2. `module-dependency-map.json` records direct module dependencies with edges directed from consumer to dependency. It also records forbidden reachability constraints.
3. `legacy-to-target-ownership-map.json` decomposes every registered legacy concept into target responsibilities. Every responsibility has one `authority_domain`, one `owner_role`, one target module, and one or more canonical target terms.

The terminology registry itself has one logical authority record governed by the Architecture Lead. Its imported vocabulary comes from `reference-glossary.md`; ADR-0011 provides a narrow overlay for scientific terms already shared by the architecture booklets and R0 scientific decisions but absent from that glossary. While the overlay exists, `authority_path` points to ADR-0011 and `imported_registry_path` points to the glossary. A term may preserve one or more detailed architecture references in `detail_authorities`; those references provide semantic detail and do not create additional terminology registries. Every term, enum, key, AuthorityDomain record, and legacy target responsibility carries one scalar `authority_ref` or `authority_domain` plus one scalar owner where ownership applies.

The committed JSON files are review baselines. A task-local PowerShell validator derives the expected terminology rows from the referenced Markdown, checks the committed documents for exact conformance, compares the declared module edges with the root CMake graph, proves acyclicity and forbidden reachability, and checks complete legacy ownership coverage. The validator has in-memory negative self-tests and does not mutate the repository during ordinary verification.

The format is an accepted R0 governance contract. Any production schema derived from it requires the owning stage task, valid and invalid fixtures, consumers, and a separate stability decision when applicable.

## Consequences

- Positive: canonical spelling, alias retirement, dependency direction, and split legacy ownership are queryable and reviewable.
- Positive: Markdown remains the architecture authority while drift is detected automatically.
- Costs: the committed baseline must be regenerated and reviewed whenever an authoritative source changes.
- Risks: automatic Markdown extraction depends on the current registry table structure; the validator fails closed when the structure or source hash changes.
- Modules kept unchanged: foundation, contracts, model_sdk, compiler, kernel, evidence, workflow, application, adapters, packages, and all R1-R8 production paths.

## Alternatives considered

- Markdown-only baseline: rejected because uniqueness, coverage, graph reachability, and owner cardinality would remain manual checks.
- One monolithic JSON document: rejected because terminology, build dependency, and migration ownership have different consumers and review cadence.
- A production JSON Schema during R0-ARCH-001: deferred because this task establishes governance data only; production schema stability and fixture policy belong to the owning contract tasks.
- Multiple terminology authorities per term: rejected because it cannot satisfy the task acceptance rule. Detailed references remain a non-authoritative list.

## Verification

- `tools/verify-r0-arch-001.ps1` passes on the three committed documents.
- `tools/verify-r0-arch-001.ps1 -SelfTest` proves duplicate identity, multiple authority, forbidden dependency, source drift, and incomplete ownership are rejected.
- The terminology conformance report records source hashes, counts, validation results, and the decision status.
- `tools/bootstrap.ps1` continues to pass without any production target consuming the R0 artifacts.

## Supersession rule

Reopen this decision when a production consumer needs these artifacts, the glossary table shape can no longer represent the registry, a fifth AuthorityDomain is accepted, or a superseding ADR changes module dependency direction or ownership semantics.
