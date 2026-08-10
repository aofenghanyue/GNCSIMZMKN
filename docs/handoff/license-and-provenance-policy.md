# License and provenance policy

## Authority and current release posture

`ADR-0004` is the authority for R0 licensing and provenance. The current repository grants no external distribution license. All content remains internal research material until a later accepted ADR identifies the rights holder, selects an external license, closes every applicable provenance record, and records the required approvals.

This policy is a project gate and does not provide legal advice. Ownership, jurisdiction, contract, export, privacy, patent, or database-right uncertainty requires review by the repository owner and qualified counsel before external transfer.

## Scope matrix

| Subject | Allowed internal use | External sharing gate | Required record |
| --- | --- | --- | --- |
| First-party source and documentation | Authorized build, test, review, and research | Rights-holder confirmation, accepted distribution-license ADR, notices, and release approval | Repository identity, commit, rights decision, reviewer |
| Third-party dependency or vendored code | Evaluation only after license/provenance review; production adoption also requires a dependency ADR | Exact license expression, compatibility assessment, obligations, notices, source/version/hash, and approval | One record per resolved dependency/version |
| Dataset, formula, parameter, media, template, or other asset | Authorized research within recorded restrictions | Permission or license covering the proposed use, scientific provenance, integrity, attribution, restrictions, and approval | One record per independently versioned asset |
| Legacy source archive | Evidence extraction, test reproduction, semantic comparison | Explicitly prohibited while rights remain unreviewed | Archive commit, SHA-256, scope, rights status, evidence-only restriction |
| External executable or service | Use under its installed license and tool policy; no copying into this repository | Output rights, executable redistribution rights, service terms, and export approval | Tool identity/version, license mode, input/output restrictions |
| Generated artifact or evidence bundle | Internal use when every input and producer is authorized | All upstream records cleared; classification, notices, attribution, lineage, redaction, and reviewer approval | Output identity/hash plus upstream record and lineage refs |

## Mandatory provenance record

Every in-scope record closes these sections:

1. **Identity** — stable record id, a subject type from the closed governance vocabulary, explicit `scientific_context_required`, subject identity, and repository path or URI.
2. **Origin** — first-party, third-party, legacy, generated, or human-import origin; source; exact version, date, commit, or artifact ref.
3. **Integrity** — hash algorithm and value for the reviewed bytes or an immutable version identifier for a repository snapshot.
4. **Rights** — `cleared`, `restricted`, or `unreviewed`; exact SPDX expression when applicable; permission or policy basis; allowed uses; prohibited uses; notice and attribution obligations.
5. **Scientific context** — required for a formula, parameter, dataset, model, model asset, table, coefficient set, or scientific fixture: applicable domain, units, frames, time convention, independent oracle/reference, explicit confirmation and basis of independence, and new target identity.
6. **Propagation** — downstream classification, notices, attribution, source-offer or other obligations. Restricted material sets `inherit_restrictions=true`. Generated outputs also identify upstream record ids and lineage refs, preserve the strongest upstream classification, and include every upstream propagation requirement.
7. **Review** — decision, named reviewer, date, conditions, and expiry or re-review trigger when applicable.

An SPDX expression is recorded only after the reviewed text matches the corresponding SPDX entry. A blank value, inferred license, or `NOASSERTION` cannot authorize external sharing.

## Decision rules

- Missing identity, origin, integrity, rights basis, restrictions, propagation, or reviewer blocks the record.
- Unknown subject types and inconsistent `scientific_context_required` flags block the record.
- Scientific material with missing domain, unit/frame/time facts, confirmed independent reference, independence basis, or new model identity remains blocked for migration and external use.
- `restricted` and `unreviewed` records can approve specified internal uses and always prohibit external sharing.
- `approved-external` requires `rights.status=cleared`, a reviewed license expression or permission reference, and complete obligations.
- Conflicting records use the most restrictive decision until the conflict is resolved and reviewed.
- A generated artifact must resolve all upstream records in the governance register, retain all upstream propagation requirements, cite lineage, and inherit the highest upstream restriction. Transformation alone cannot broaden its sharing decision.
- Manual downloads, copied snippets, screenshots, model coefficients, generated tables, and tool outputs follow the same gate as versioned dependencies and assets.

## Third-party intake workflow

1. Open the dependency or asset ADR when repository policy requires one.
2. Capture the original source, version, immutable hash, author/provider, and acquisition date.
3. Preserve the license text, permission record, notices, and attribution requirements outside transient caches.
4. Record intended internal and external uses, linking/static/runtime form, modification, redistribution, and generated-output implications.
5. Complete technical, architecture, scientific, and rights review as applicable.
6. Add the material only after the decision allows the intended use. Keep a blocked record when the result is useful evidence for future review.

## External release checklist

- [ ] Repository owner and rights holders are identified.
- [ ] A distribution-license ADR is Accepted for first-party material.
- [ ] Every shipped dependency, asset, archive excerpt, template, and generated artifact has a closed provenance record.
- [ ] License expressions, permission scope, notices, attribution, source-offer, and modification markings are packaged.
- [ ] Scientific provenance, applicable domain, oracle, and target identity are complete for scientific content.
- [ ] Secrets, personal data, restricted paths, credentials, and internal-only metadata are removed or approved.
- [ ] The evidence graph propagates classification and obligations to every exported artifact.
- [ ] Product Owner and Architecture Lead approve; Scientific Authority also approves scientific content.
- [ ] Qualified legal review closes any ownership, contract, patent, database-right, jurisdiction, or export uncertainty.

## Machine-checkable R0 evidence

`docs/quality/provenance-register.json` is the R0 governance register. `tools/verify-provenance-record.ps1` validates mandatory structure and decision consistency. This format is confined to governance evidence; runtime packages, Compiler, Session, Artifact schemas, and external APIs cannot consume it as a stable product contract.

The closed R0 subject types are `source-code`, `documentation`, `dependency`, `tool`, `formula`, `parameter`, `dataset`, `asset`, `model`, `model-asset`, `table`, `coefficient-set`, `fixture`, `scientific-fixture`, `reference-archive`, `template`, `generated-artifact`, and `evidence-bundle`. Formula, parameter, dataset, model, model-asset, table, coefficient-set, and scientific-fixture always require scientific context. Ambiguous carrier types such as asset, fixture, template, generated-artifact, and evidence-bundle use the explicit flag.

Official references:

- [GitHub repository licensing](https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/customizing-your-repository/licensing-a-repository)
- [SPDX License List](https://spdx.org/licenses/)
- [REUSE Specification 3.3](https://reuse.software/spec/)
- [Creative Commons data FAQ](https://creativecommons.org/faq/#frequently-asked-questions-about-data-and-cc-licenses)
