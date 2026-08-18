#pragma once

#include "gnc/model_sdk/static_descriptor.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gnc::compiler {

inline constexpr std::string_view kTypedStaticCompositionSourceVersion =
    "gnc.typed-static-composition-source/1";

struct SourceRef {
    std::string document_uri;
    std::string node_path;
};

// This IR slice records only the initial lifecycle fact required by the
// accepted YYZ qualification source. Topology and activation transactions
// remain outside the current Compiler surface.
enum class EntityLifecycle : std::uint8_t {
    Unspecified,
    ActiveAtInitialize,
};

[[nodiscard]] constexpr std::string_view to_string(
    EntityLifecycle lifecycle) noexcept {
    switch (lifecycle) {
    case EntityLifecycle::Unspecified:
        return "Unspecified";
    case EntityLifecycle::ActiveAtInitialize:
        return "active_at_initialize";
    }
    return "Unknown";
}

enum class ScopeKind : std::uint8_t {
    Unspecified,
    Vehicle,
};

[[nodiscard]] constexpr std::string_view to_string(
    ScopeKind kind) noexcept {
    switch (kind) {
    case ScopeKind::Unspecified:
        return "Unspecified";
    case ScopeKind::Vehicle:
        return "Vehicle";
    }
    return "Unknown";
}

struct ScopeKey {
    ScopeKind kind = ScopeKind::Unspecified;
    std::string subject_entity_id;
};

[[nodiscard]] inline bool operator==(const ScopeKey& lhs,
                                     const ScopeKey& rhs) {
    return lhs.kind == rhs.kind &&
           lhs.subject_entity_id == rhs.subject_entity_id;
}

[[nodiscard]] inline bool operator<(const ScopeKey& lhs,
                                    const ScopeKey& rhs) {
    if (lhs.kind != rhs.kind) {
        return static_cast<std::uint8_t>(lhs.kind) <
               static_cast<std::uint8_t>(rhs.kind);
    }
    return lhs.subject_entity_id < rhs.subject_entity_id;
}

enum class DiagnosticCode : std::uint8_t {
    InvalidCatalogDescriptor,
    DuplicateCatalogIdentity,
    InvalidStaticCompositionSource,
    InvalidEntity,
    DuplicateEntity,
    UnknownSubjectEntity,
    InvalidScope,
    DuplicateScope,
    UnknownScopeEntity,
    UnknownScope,
    PlacementMismatch,
    SubjectScopeMismatch,
    InvalidConfiguration,
    MissingAssetBinding,
    DuplicateAssetBinding,
    UnknownAssetRole,
    AssetSchemaMismatch,
    DuplicateOccurrence,
    UnknownDefinition,
    UnknownAlgorithm,
    UnknownEndpoint,
    PortDirectionMismatch,
    ContractMismatch,
    MissingRequiredBinding,
    MultipleRequiredBindings,
    NonCanonicalIr,
};

[[nodiscard]] constexpr std::string_view to_string(
    DiagnosticCode code) noexcept {
    switch (code) {
    case DiagnosticCode::InvalidCatalogDescriptor:
        return "GNC-CAT-INVALID-DESCRIPTOR";
    case DiagnosticCode::DuplicateCatalogIdentity:
        return "GNC-CAT-DUPLICATE-IDENTITY";
    case DiagnosticCode::InvalidStaticCompositionSource:
        return "GNC-SRC-INVALID-STATIC-COMPOSITION";
    case DiagnosticCode::InvalidEntity:
        return "GNC-IR-INVALID-ENTITY";
    case DiagnosticCode::DuplicateEntity:
        return "GNC-IR-DUPLICATE-ENTITY";
    case DiagnosticCode::UnknownSubjectEntity:
        return "GNC-IR-UNKNOWN-SUBJECT-ENTITY";
    case DiagnosticCode::InvalidScope:
        return "GNC-IR-INVALID-SCOPE";
    case DiagnosticCode::DuplicateScope:
        return "GNC-IR-DUPLICATE-SCOPE";
    case DiagnosticCode::UnknownScopeEntity:
        return "GNC-IR-UNKNOWN-SCOPE-ENTITY";
    case DiagnosticCode::UnknownScope:
        return "GNC-IR-UNKNOWN-SCOPE";
    case DiagnosticCode::PlacementMismatch:
        return "GNC-IR-PLACEMENT-MISMATCH";
    case DiagnosticCode::SubjectScopeMismatch:
        return "GNC-IR-SUBJECT-SCOPE-MISMATCH";
    case DiagnosticCode::InvalidConfiguration:
        return "GNC-IR-INVALID-CONFIGURATION";
    case DiagnosticCode::MissingAssetBinding:
        return "GNC-IR-MISSING-ASSET-BINDING";
    case DiagnosticCode::DuplicateAssetBinding:
        return "GNC-IR-DUPLICATE-ASSET-BINDING";
    case DiagnosticCode::UnknownAssetRole:
        return "GNC-IR-UNKNOWN-ASSET-ROLE";
    case DiagnosticCode::AssetSchemaMismatch:
        return "GNC-IR-ASSET-SCHEMA-MISMATCH";
    case DiagnosticCode::DuplicateOccurrence:
        return "GNC-IR-DUPLICATE-OCCURRENCE";
    case DiagnosticCode::UnknownDefinition:
        return "GNC-CAT-UNKNOWN-DEFINITION";
    case DiagnosticCode::UnknownAlgorithm:
        return "GNC-CAT-UNKNOWN-ALGORITHM";
    case DiagnosticCode::UnknownEndpoint:
        return "GNC-BIND-UNKNOWN-ENDPOINT";
    case DiagnosticCode::PortDirectionMismatch:
        return "GNC-BIND-PORT-DIRECTION";
    case DiagnosticCode::ContractMismatch:
        return "GNC-BIND-CONTRACT-MISMATCH";
    case DiagnosticCode::MissingRequiredBinding:
        return "GNC-BIND-MISSING-REQUIRED";
    case DiagnosticCode::MultipleRequiredBindings:
        return "GNC-BIND-MULTIPLE-REQUIRED";
    case DiagnosticCode::NonCanonicalIr:
        return "GNC-IR-NONCANONICAL";
    }
    return "GNC-COMPILER-UNKNOWN";
}

struct Diagnostic {
    DiagnosticCode code =
        DiagnosticCode::InvalidStaticCompositionSource;
    SourceRef source;
    std::string subject;
    std::string detail;
};

template <typename Value>
struct CompileOutcome {
    std::optional<Value> value;
    std::vector<Diagnostic> diagnostics;

    [[nodiscard]] bool succeeded() const noexcept {
        return value.has_value() && diagnostics.empty();
    }
};

struct PackageLock {
    std::string package_id;
    std::string package_version;
};

struct CatalogModelRecord {
    PackageLock package;
    gnc::model_sdk::StaticModelDescriptor descriptor;
};

struct CatalogAlgorithmRecord {
    PackageLock package;
    gnc::model_sdk::StaticAlgorithmDescriptor descriptor;
};

namespace detail {

[[nodiscard]] inline std::string exact_key(std::string_view identity,
                                           std::string_view version) {
    std::string key(identity);
    key.push_back('\n');
    key.append(version);
    return key;
}

[[nodiscard]] inline SourceRef catalog_source(std::string_view package_id,
                                              std::string_view subject) {
    return {"catalog://" + std::string(package_id), std::string(subject)};
}

inline void validate_ports(
    const std::vector<gnc::model_sdk::StaticPortDescriptor>& ports,
    std::string_view package_id, std::string_view owner,
    gnc::model_sdk::StaticPortDirection supported_direction,
    std::vector<Diagnostic>& diagnostics) {
    if (ports.empty()) {
        diagnostics.push_back(
            {DiagnosticCode::InvalidCatalogDescriptor,
             catalog_source(package_id, owner), std::string(owner),
             "current static composition requires at least one port"});
    }
    std::set<std::string> port_ids;
    for (const auto& port : ports) {
        if (port.port_id.empty() || port.contract_id.empty()) {
            diagnostics.push_back(
                {DiagnosticCode::InvalidCatalogDescriptor,
                 catalog_source(package_id, owner), std::string(owner),
                 "port identity and contract identity are required"});
        }
        if (!port_ids.insert(port.port_id).second) {
            diagnostics.push_back(
                {DiagnosticCode::InvalidCatalogDescriptor,
                 catalog_source(package_id, owner), port.port_id,
                 "duplicate port identity within descriptor"});
        }
        if (port.direction != supported_direction) {
            diagnostics.push_back(
                {DiagnosticCode::InvalidCatalogDescriptor,
                 catalog_source(package_id, owner), port.port_id,
                 "current static composition supports model Output ports "
                 "and algorithm Input ports only"});
        }
    }
}

template <typename Port>
[[nodiscard]] inline const Port* find_port(
    const std::vector<Port>& ports, std::string_view port_id) {
    const auto found = std::find_if(
        ports.begin(), ports.end(),
        [port_id](const Port& port) {
            return port.port_id == port_id;
        });
    return found == ports.end() ? nullptr : &*found;
}

[[nodiscard]] inline std::string endpoint_key(
    std::string_view occurrence_id, std::string_view port_id) {
    std::string key(occurrence_id);
    key.push_back('\n');
    key.append(port_id);
    return key;
}

} // namespace detail

// An immutable, process-local view assembled from package-owned static
// descriptors. It performs exact lookup only and owns no runtime instances.
class Catalog {
  public:
    [[nodiscard]] static CompileOutcome<Catalog> build(
        std::vector<gnc::model_sdk::StaticPackageDescriptor> packages) {
        CompileOutcome<Catalog> outcome;
        std::vector<CatalogModelRecord> models;
        std::vector<CatalogAlgorithmRecord> algorithms;
        std::set<std::string> package_keys;

        for (auto& package : packages) {
            const auto package_source = detail::catalog_source(
                package.package_id, "package");
            if (package.package_id.empty() || package.package_version.empty()) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::InvalidCatalogDescriptor, package_source,
                     package.package_id,
                     "package identity and version are required"});
            }
            if (!package_keys
                     .insert(detail::exact_key(package.package_id,
                                               package.package_version))
                     .second) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::DuplicateCatalogIdentity, package_source,
                     package.package_id,
                     "duplicate exact package contribution"});
            }

            PackageLock lock{package.package_id, package.package_version};
            for (auto& model : package.models) {
                const auto& definition = model.definition;
                if (definition.model_id.empty() ||
                    definition.model_version.empty() ||
                    !gnc::model_sdk::valid_model_execution_form(
                        definition.execution_form) ||
                    model.preparation_algorithm_id.empty() ||
                    model.preparation_algorithm_version.empty()) {
                    outcome.diagnostics.push_back(
                        {DiagnosticCode::InvalidCatalogDescriptor,
                         detail::catalog_source(package.package_id,
                                                definition.model_id),
                         definition.model_id,
                         "model identity, execution form, and preparation "
                         "identity are required"});
                }
                if (!gnc::model_sdk::valid_model_placement(
                        model.placement)) {
                    outcome.diagnostics.push_back(
                        {DiagnosticCode::InvalidCatalogDescriptor,
                         detail::catalog_source(package.package_id,
                                                definition.model_id),
                         definition.model_id,
                         "a supported package-owned placement policy is "
                         "required"});
                }
                const auto& configuration = model.configuration;
                if (configuration.schema_id.empty() ||
                    configuration.schema_version == 0U ||
                    configuration.fields.empty()) {
                    outcome.diagnostics.push_back(
                        {DiagnosticCode::InvalidCatalogDescriptor,
                         detail::catalog_source(package.package_id,
                                                definition.model_id),
                         definition.model_id,
                         "an exact nonempty configuration schema is "
                         "required"});
                }
                std::string previous_config_field;
                for (const auto& field : configuration.fields) {
                    if (field.field_id.empty() ||
                        (!previous_config_field.empty() &&
                         field.field_id <= previous_config_field)) {
                        outcome.diagnostics.push_back(
                            {DiagnosticCode::InvalidCatalogDescriptor,
                             detail::catalog_source(package.package_id,
                                                    definition.model_id),
                             field.field_id,
                             "configuration fields must have unique "
                             "canonical order"});
                    }
                    previous_config_field = field.field_id;
                }
                std::string previous_asset_role;
                for (const auto& slot : model.asset_slots) {
                    if (slot.role.empty() ||
                        slot.asset_schema_id.empty() ||
                        (!previous_asset_role.empty() &&
                         slot.role <= previous_asset_role)) {
                        outcome.diagnostics.push_back(
                            {DiagnosticCode::InvalidCatalogDescriptor,
                             detail::catalog_source(package.package_id,
                                                    definition.model_id),
                             slot.role,
                             "asset slots must have identities and unique "
                             "canonical order"});
                    }
                    previous_asset_role = slot.role;
                }
                detail::validate_ports(
                    model.ports, package.package_id, definition.model_id,
                    gnc::model_sdk::StaticPortDirection::Output,
                    outcome.diagnostics);
                models.push_back({lock, std::move(model)});
            }

            for (auto& algorithm : package.algorithms) {
                if (algorithm.algorithm_id.empty() ||
                    algorithm.algorithm_version.empty()) {
                    outcome.diagnostics.push_back(
                        {DiagnosticCode::InvalidCatalogDescriptor,
                         detail::catalog_source(package.package_id,
                                                algorithm.algorithm_id),
                         algorithm.algorithm_id,
                         "algorithm identity and version are required"});
                }
                detail::validate_ports(
                    algorithm.ports, package.package_id,
                    algorithm.algorithm_id,
                    gnc::model_sdk::StaticPortDirection::Input,
                    outcome.diagnostics);
                algorithms.push_back({lock, std::move(algorithm)});
            }
        }

        const auto model_less = [](const CatalogModelRecord& lhs,
                                   const CatalogModelRecord& rhs) {
            return detail::exact_key(lhs.descriptor.definition.model_id,
                                     lhs.descriptor.definition.model_version) <
                   detail::exact_key(rhs.descriptor.definition.model_id,
                                     rhs.descriptor.definition.model_version);
        };
        std::sort(models.begin(), models.end(), model_less);
        for (std::size_t index = 1U; index < models.size(); ++index) {
            if (!model_less(models[index - 1U], models[index]) &&
                !model_less(models[index], models[index - 1U])) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::DuplicateCatalogIdentity,
                     detail::catalog_source(
                         models[index].package.package_id,
                         models[index].descriptor.definition.model_id),
                     models[index].descriptor.definition.model_id,
                     "duplicate exact model definition"});
            }
        }

        const auto algorithm_less = [](const CatalogAlgorithmRecord& lhs,
                                       const CatalogAlgorithmRecord& rhs) {
            return detail::exact_key(lhs.descriptor.algorithm_id,
                                     lhs.descriptor.algorithm_version) <
                   detail::exact_key(rhs.descriptor.algorithm_id,
                                     rhs.descriptor.algorithm_version);
        };
        std::sort(algorithms.begin(), algorithms.end(), algorithm_less);
        for (std::size_t index = 1U; index < algorithms.size(); ++index) {
            if (!algorithm_less(algorithms[index - 1U], algorithms[index]) &&
                !algorithm_less(algorithms[index], algorithms[index - 1U])) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::DuplicateCatalogIdentity,
                     detail::catalog_source(
                         algorithms[index].package.package_id,
                         algorithms[index].descriptor.algorithm_id),
                     algorithms[index].descriptor.algorithm_id,
                     "duplicate exact algorithm descriptor"});
            }
        }

        if (outcome.diagnostics.empty()) {
            outcome.value = Catalog(std::move(models),
                                    std::move(algorithms));
        }
        return outcome;
    }

    [[nodiscard]] const CatalogModelRecord* find_model(
        std::string_view model_id, std::string_view version) const {
        const auto key = detail::exact_key(model_id, version);
        const auto found = std::lower_bound(
            models_.begin(), models_.end(), key,
            [](const CatalogModelRecord& record, const std::string& value) {
                return detail::exact_key(
                           record.descriptor.definition.model_id,
                           record.descriptor.definition.model_version) < value;
            });
        if (found == models_.end() ||
            detail::exact_key(found->descriptor.definition.model_id,
                              found->descriptor.definition.model_version) !=
                key) {
            return nullptr;
        }
        return &*found;
    }

    [[nodiscard]] const CatalogAlgorithmRecord* find_algorithm(
        std::string_view algorithm_id,
        std::string_view version) const {
        const auto key = detail::exact_key(algorithm_id, version);
        const auto found = std::lower_bound(
            algorithms_.begin(), algorithms_.end(), key,
            [](const CatalogAlgorithmRecord& record,
               const std::string& value) {
                return detail::exact_key(record.descriptor.algorithm_id,
                                         record.descriptor.algorithm_version) <
                       value;
            });
        if (found == algorithms_.end() ||
            detail::exact_key(found->descriptor.algorithm_id,
                              found->descriptor.algorithm_version) != key) {
            return nullptr;
        }
        return &*found;
    }

  private:
    Catalog(std::vector<CatalogModelRecord> models,
            std::vector<CatalogAlgorithmRecord> algorithms) noexcept
        : models_(std::move(models)), algorithms_(std::move(algorithms)) {}

    std::vector<CatalogModelRecord> models_;
    std::vector<CatalogAlgorithmRecord> algorithms_;
};

struct SourceEntity {
    std::string entity_id;
    EntityLifecycle lifecycle = EntityLifecycle::Unspecified;
    SourceRef identity_source;
    SourceRef lifecycle_source;
};

struct SourceScope {
    ScopeKey key;
    SourceRef source;
};

struct SourceConfigFieldProvenance {
    std::string field_id;
    SourceRef source;
};

struct SourceAssetBinding {
    std::string role;
    std::string asset_schema_id;
    std::string asset_id;
    SourceRef source;
};

struct SourceModelOccurrence {
    std::string occurrence_id;
    std::string model_id;
    std::string model_version;
    SourceRef source;
    std::string subject_entity_id;
    SourceRef subject_source;
    std::optional<ScopeKey> scope;
    SourceRef scope_source;
    gnc::model_sdk::ModelPlacement placement =
        gnc::model_sdk::ModelPlacement::Unspecified;
    SourceRef placement_source;
    gnc::model_sdk::CanonicalConfigBlock configuration;
    SourceRef configuration_source;
    std::vector<SourceConfigFieldProvenance>
        configuration_field_sources;
    std::vector<SourceAssetBinding> asset_bindings;

    SourceModelOccurrence() = default;

    SourceModelOccurrence(std::string occurrence_identity,
                          std::string model_identity,
                          std::string version,
                          SourceRef occurrence_source,
                          std::string subject_identity,
                          SourceRef subject_provenance)
        : occurrence_id(std::move(occurrence_identity)),
          model_id(std::move(model_identity)),
          model_version(std::move(version)),
          source(std::move(occurrence_source)),
          subject_entity_id(std::move(subject_identity)),
          subject_source(std::move(subject_provenance)) {}
};

// A stateless kernel used as a binding consumer in the current conformance
// slice. It is not a ModelDefinition-backed model occurrence.
struct SourceAlgorithmConsumer {
    std::string consumer_id;
    std::string algorithm_id;
    std::string algorithm_version;
    SourceRef source;
};

struct SourceBinding {
    std::string binding_id;
    std::string provider_occurrence_id;
    std::string provider_port_id;
    std::string consumer_id;
    std::string consumer_port_id;
    SourceRef source;
};

// Programmatic entity/subject/identity/binding input for the current R2
// slices. This is not the syntax-neutral SourceTree defined by the target
// Source Frontend.
struct TypedStaticCompositionSource {
    std::string source_version;
    std::string mission_id;
    SourceRef mission_source;
    std::string plan_id;
    std::vector<SourceEntity> entities;
    std::vector<SourceScope> scopes;
    std::vector<SourceModelOccurrence> model_occurrences;
    std::vector<SourceAlgorithmConsumer> algorithm_consumers;
    std::vector<SourceBinding> binding_intents;
};

struct CanonicalPort {
    std::string port_id;
    std::string contract_id;
};

struct CanonicalEntity {
    std::string entity_id;
    EntityLifecycle lifecycle = EntityLifecycle::Unspecified;
    SourceRef identity_source;
    SourceRef lifecycle_source;
};

struct CanonicalScope {
    ScopeKey key;
    SourceRef source;
};

struct CanonicalConfigFieldProvenance {
    std::string field_id;
    SourceRef source;
};

struct CanonicalAssetBinding {
    std::string role;
    std::string asset_schema_id;
    std::string asset_id;
    SourceRef source;
};

struct CanonicalModelOccurrence {
    std::string occurrence_id;
    PackageLock package;
    std::string model_id;
    std::string model_version;
    gnc::model_sdk::ModelExecutionForm execution_form =
        gnc::model_sdk::ModelExecutionForm::Unspecified;
    std::string preparation_algorithm_id;
    std::string preparation_algorithm_version;
    std::vector<CanonicalPort> output_ports;
    SourceRef source;
    std::string subject_entity_id;
    SourceRef subject_source;
    std::optional<ScopeKey> scope;
    SourceRef scope_source;
    gnc::model_sdk::ModelPlacement placement =
        gnc::model_sdk::ModelPlacement::Unspecified;
    SourceRef placement_source;
    gnc::model_sdk::CanonicalConfigBlock configuration;
    SourceRef configuration_source;
    std::vector<CanonicalConfigFieldProvenance>
        configuration_field_sources;
    std::vector<CanonicalAssetBinding> asset_bindings;
};

struct CanonicalAlgorithmConsumer {
    std::string consumer_id;
    PackageLock package;
    std::string algorithm_id;
    std::string algorithm_version;
    std::vector<CanonicalPort> input_ports;
    SourceRef source;
};

struct CanonicalBindingIntent {
    std::string binding_id;
    std::string provider_occurrence_id;
    std::string provider_port_id;
    std::string consumer_id;
    std::string consumer_port_id;
    SourceRef source;
};

// The executable R2 canonical graph. Source locations remain as provenance,
// while semantic encoding excludes representation-specific locations and
// plan identity. Topology, activation and runtime instances remain outside.
struct CanonicalMissionIr {
    std::uint32_t revision = 1U;
    std::string mission_id;
    SourceRef mission_source;
    std::vector<CanonicalEntity> entities;
    std::vector<CanonicalScope> scopes;
    std::vector<CanonicalModelOccurrence> model_occurrences;
    std::vector<CanonicalAlgorithmConsumer> algorithm_consumers;
    std::vector<CanonicalBindingIntent> binding_intents;
};

enum class BindingProofResult : std::uint8_t {
    Proven,
};

struct BindingProof {
    std::string proof_id;
    std::string assertion_code;
    std::string binding_id;
    std::string contract_id;
    std::vector<SourceRef> source_refs;
    BindingProofResult result = BindingProofResult::Proven;
};

struct ModelPreparationIdentityPlan {
    std::string occurrence_id;
    PackageLock package;
    std::string model_id;
    std::string model_version;
    gnc::model_sdk::ModelExecutionForm execution_form =
        gnc::model_sdk::ModelExecutionForm::Unspecified;
    std::string preparation_algorithm_id;
    std::string preparation_algorithm_version;
    SourceRef source;
};

struct AlgorithmConsumerPlan {
    std::string consumer_id;
    PackageLock package;
    std::string algorithm_id;
    std::string algorithm_version;
    SourceRef source;
};

struct BindingPlanEntry {
    std::string binding_id;
    std::string provider_occurrence_id;
    std::string provider_port_id;
    std::string consumer_id;
    std::string consumer_port_id;
    std::string contract_id;
};

enum class CompiledObligationKind : std::uint8_t {
    PureQueryEvaluation,
    ClosureEvaluation,
};

[[nodiscard]] constexpr std::string_view to_string(
    CompiledObligationKind kind) noexcept {
    switch (kind) {
    case CompiledObligationKind::PureQueryEvaluation:
        return "PureQueryEvaluation";
    case CompiledObligationKind::ClosureEvaluation:
        return "ClosureEvaluation";
    }
    return "Unknown";
}

struct CompiledObligation {
    std::string obligation_id;
    std::size_t ordinal = 0U;
    CompiledObligationKind kind =
        CompiledObligationKind::PureQueryEvaluation;
    std::string provider_occurrence_id;
    std::string consumer_id;
    std::string binding_id;
};

// This narrow, in-process descriptor freezes only exact identities, one-way
// bindings, and query/closure obligations. It has no canonical model config,
// asset binding, runtime instance, function address, Session identity, or
// mutable state, so it cannot reconstruct a complete PreparedModel.
struct ExecutionPlanDescriptor {
    std::uint32_t revision = 1U;
    std::string plan_id;
    std::string mission_id;
    std::vector<PackageLock> dependency_lock;
    std::vector<ModelPreparationIdentityPlan>
        model_preparation_identities;
    std::vector<AlgorithmConsumerPlan> algorithms;
    std::vector<BindingPlanEntry> bindings;
    std::vector<BindingProof> binding_proofs;
    std::vector<CompiledObligation> obligations;
};

struct StaticCompilation {
    CanonicalMissionIr ir;
    ExecutionPlanDescriptor plan;
};

namespace detail {

[[nodiscard]] inline std::vector<CanonicalPort> canonical_ports(
    const std::vector<gnc::model_sdk::StaticPortDescriptor>& ports) {
    std::vector<CanonicalPort> result;
    result.reserve(ports.size());
    for (const auto& port : ports) {
        result.push_back({port.port_id, port.contract_id});
    }
    std::sort(result.begin(), result.end(),
              [](const CanonicalPort& lhs, const CanonicalPort& rhs) {
                  return lhs.port_id < rhs.port_id;
              });
    return result;
}

[[nodiscard]] inline bool valid_source_ref(
    const SourceRef& source) noexcept {
    return !source.document_uri.empty() && !source.node_path.empty();
}

[[nodiscard]] inline bool valid_canonical_config_value(
    const gnc::model_sdk::CanonicalConfigValue& value) noexcept {
    if (const auto* text = std::get_if<std::string>(&value)) {
        return !text->empty();
    }
    if (const auto* token =
            std::get_if<gnc::model_sdk::CanonicalEnumValue>(&value)) {
        return !token->token.empty();
    }
    if (const auto* number = std::get_if<double>(&value)) {
        return std::isfinite(*number) &&
               !(*number == 0.0 && std::signbit(*number));
    }
    return true;
}

inline bool canonicalize_configuration(
    const SourceModelOccurrence& source,
    const gnc::model_sdk::StaticModelDescriptor& descriptor,
    gnc::model_sdk::CanonicalConfigBlock& configuration,
    std::vector<CanonicalConfigFieldProvenance>& provenance,
    std::vector<Diagnostic>& diagnostics) {
    const auto fail = [&](const SourceRef& location,
                          std::string detail) {
        diagnostics.push_back(
            {DiagnosticCode::InvalidConfiguration, location,
             source.occurrence_id, std::move(detail)});
    };
    configuration = source.configuration;
    if (configuration.schema_id !=
            descriptor.configuration.schema_id ||
        configuration.schema_version !=
            descriptor.configuration.schema_version ||
        !valid_source_ref(source.configuration_source)) {
        fail(source.configuration_source,
             "configuration schema identity/version or provenance differs "
             "from the package descriptor");
        return false;
    }
    std::sort(configuration.fields.begin(), configuration.fields.end(),
              [](const gnc::model_sdk::CanonicalConfigField& lhs,
                 const gnc::model_sdk::CanonicalConfigField& rhs) {
                  return lhs.field_id < rhs.field_id;
              });
    if (configuration.fields.size() !=
        descriptor.configuration.fields.size()) {
        fail(source.configuration_source,
             "configuration field set differs from the exact schema");
        return false;
    }
    for (std::size_t index = 0U;
         index < configuration.fields.size(); ++index) {
        const auto& actual = configuration.fields[index];
        const auto& expected = descriptor.configuration.fields[index];
        if (actual.field_id != expected.field_id ||
            gnc::model_sdk::canonical_config_value_kind(actual.value) !=
                expected.value_kind ||
            !valid_canonical_config_value(actual.value)) {
            fail(source.configuration_source,
                 "configuration field identity, type, or canonical value "
                 "differs from the exact schema");
            return false;
        }
    }

    auto field_sources = source.configuration_field_sources;
    std::sort(field_sources.begin(), field_sources.end(),
              [](const SourceConfigFieldProvenance& lhs,
                 const SourceConfigFieldProvenance& rhs) {
                  return lhs.field_id < rhs.field_id;
              });
    if (field_sources.size() != configuration.fields.size()) {
        fail(source.configuration_source,
             "every canonical configuration field requires one provenance "
             "reference");
        return false;
    }
    provenance.clear();
    provenance.reserve(field_sources.size());
    for (std::size_t index = 0U; index < field_sources.size(); ++index) {
        if (field_sources[index].field_id !=
                configuration.fields[index].field_id ||
            !valid_source_ref(field_sources[index].source)) {
            fail(field_sources[index].source,
                 "configuration field provenance is missing, duplicated, "
                 "or attached to another field");
            return false;
        }
        provenance.push_back(
            {field_sources[index].field_id,
             field_sources[index].source});
    }
    return true;
}

inline bool canonicalize_assets(
    const SourceModelOccurrence& source,
    const gnc::model_sdk::StaticModelDescriptor& descriptor,
    std::vector<CanonicalAssetBinding>& assets,
    std::vector<Diagnostic>& diagnostics) {
    auto bindings = source.asset_bindings;
    std::sort(bindings.begin(), bindings.end(),
              [](const SourceAssetBinding& lhs,
                 const SourceAssetBinding& rhs) {
                  return lhs.role < rhs.role;
              });
    for (std::size_t index = 1U; index < bindings.size(); ++index) {
        if (bindings[index - 1U].role == bindings[index].role) {
            diagnostics.push_back(
                {DiagnosticCode::DuplicateAssetBinding,
                 bindings[index].source, source.occurrence_id,
                 "an asset role is bound more than once"});
            return false;
        }
    }
    if (bindings.size() < descriptor.asset_slots.size()) {
        diagnostics.push_back(
            {DiagnosticCode::MissingAssetBinding, source.source,
             source.occurrence_id,
             "a required package asset role is unbound"});
        return false;
    }
    if (bindings.size() > descriptor.asset_slots.size()) {
        diagnostics.push_back(
            {DiagnosticCode::UnknownAssetRole,
             bindings[descriptor.asset_slots.size()].source,
             source.occurrence_id,
             "the source binds an asset role absent from the package "
             "descriptor"});
        return false;
    }
    assets.clear();
    assets.reserve(bindings.size());
    for (std::size_t index = 0U; index < bindings.size(); ++index) {
        const auto& binding = bindings[index];
        const auto& slot = descriptor.asset_slots[index];
        if (binding.role != slot.role) {
            diagnostics.push_back(
                {DiagnosticCode::UnknownAssetRole, binding.source,
                 source.occurrence_id,
                 "the source asset role differs from the package slot"});
            return false;
        }
        if (binding.asset_schema_id != slot.asset_schema_id) {
            diagnostics.push_back(
                {DiagnosticCode::AssetSchemaMismatch, binding.source,
                 source.occurrence_id,
                 "the asset schema differs from the package slot"});
            return false;
        }
        if (binding.asset_id.empty() ||
            !valid_source_ref(binding.source)) {
            diagnostics.push_back(
                {DiagnosticCode::AssetSchemaMismatch, binding.source,
                 source.occurrence_id,
                 "asset identity and provenance are required"});
            return false;
        }
        assets.push_back({binding.role, binding.asset_schema_id,
                          binding.asset_id, binding.source});
    }
    return true;
}

} // namespace detail

[[nodiscard]] inline CompileOutcome<CanonicalMissionIr>
build_canonical_mission_ir(const TypedStaticCompositionSource& source,
                           const Catalog& catalog) {
    CompileOutcome<CanonicalMissionIr> outcome;
    if (source.source_version != kTypedStaticCompositionSourceVersion ||
        source.mission_id.empty()) {
        outcome.diagnostics.push_back(
            {DiagnosticCode::InvalidStaticCompositionSource,
             {"typed://mission", "/"}, source.mission_id,
             "static composition source version and mission identity are "
             "required"});
        return outcome;
    }
    if (source.model_occurrences.empty() &&
        source.algorithm_consumers.empty() &&
        source.binding_intents.empty()) {
        outcome.diagnostics.push_back(
            {DiagnosticCode::InvalidStaticCompositionSource,
             {"typed://mission", "/"}, source.mission_id,
             "static composition must contain at least one model, "
             "algorithm, or binding"});
        return outcome;
    }

    CanonicalMissionIr ir;
    ir.mission_id = source.mission_id;
    ir.mission_source = source.mission_source;

    std::set<std::string> entity_ids;
    auto entity_sources = source.entities;
    std::stable_sort(entity_sources.begin(), entity_sources.end(),
                     [](const SourceEntity& lhs,
                        const SourceEntity& rhs) {
                         return lhs.entity_id < rhs.entity_id;
                     });
    for (const auto& entity : entity_sources) {
        if (entity.entity_id.empty()) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::InvalidEntity, entity.identity_source,
                 entity.entity_id, "entity identity is required"});
            continue;
        }
        if (!entity_ids.insert(entity.entity_id).second) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::DuplicateEntity, entity.identity_source,
                 entity.entity_id, "entity identity is duplicated"});
            continue;
        }
        if (entity.lifecycle != EntityLifecycle::ActiveAtInitialize) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::InvalidEntity, entity.lifecycle_source,
                 entity.entity_id,
                 "current canonical IR requires active_at_initialize"});
            continue;
        }
        ir.entities.push_back(
            {entity.entity_id, entity.lifecycle, entity.identity_source,
             entity.lifecycle_source});
    }

    std::set<ScopeKey> scope_keys;
    auto scope_sources = source.scopes;
    std::sort(scope_sources.begin(), scope_sources.end(),
              [](const SourceScope& lhs, const SourceScope& rhs) {
                  return lhs.key < rhs.key;
              });
    for (const auto& scope : scope_sources) {
        if (scope.key.kind != ScopeKind::Vehicle ||
            scope.key.subject_entity_id.empty()) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::InvalidScope, scope.source,
                 scope.key.subject_entity_id,
                 "current canonical IR supports a typed Vehicle scope "
                 "anchored by subject entity identity"});
            continue;
        }
        if (!scope_keys.insert(scope.key).second) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::DuplicateScope, scope.source,
                 scope.key.subject_entity_id,
                 "the exact Vehicle scope is declared more than once"});
            continue;
        }
        if (entity_ids.find(scope.key.subject_entity_id) ==
            entity_ids.end()) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownScopeEntity, scope.source,
                 scope.key.subject_entity_id,
                 "Vehicle scope subject entity is absent from the IR"});
            continue;
        }
        ir.scopes.push_back({scope.key, scope.source});
    }

    std::set<std::string> composition_node_ids;

    auto model_sources = source.model_occurrences;
    std::sort(model_sources.begin(), model_sources.end(),
              [](const SourceModelOccurrence& lhs,
                 const SourceModelOccurrence& rhs) {
                  return lhs.occurrence_id < rhs.occurrence_id;
              });
    for (const auto& model : model_sources) {
        if (model.occurrence_id.empty() ||
            !composition_node_ids.insert(model.occurrence_id).second) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::DuplicateOccurrence, model.source,
                 model.occurrence_id,
                 "occurrence identity is empty or duplicated"});
            continue;
        }
        if (!model.subject_entity_id.empty() &&
            entity_ids.find(model.subject_entity_id) == entity_ids.end()) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownSubjectEntity,
                 model.subject_source, model.occurrence_id,
                 "model occurrence subject entity is absent from the IR"});
            continue;
        }
        if (model.scope.has_value() &&
            scope_keys.find(*model.scope) == scope_keys.end()) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownScope, model.scope_source,
                 model.occurrence_id,
                 "model occurrence references an undeclared typed scope"});
            continue;
        }
        if (model.scope.has_value() &&
            (model.scope->kind != ScopeKind::Vehicle ||
             model.subject_entity_id !=
                 model.scope->subject_entity_id)) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::SubjectScopeMismatch,
                 model.scope_source, model.occurrence_id,
                 "Vehicle scope and model subject entity differ"});
            continue;
        }
        const auto* catalog_model =
            catalog.find_model(model.model_id, model.model_version);
        if (catalog_model == nullptr) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownDefinition, model.source,
                 model.occurrence_id,
                 "exact model definition is absent from the Catalog"});
            continue;
        }
        const auto& descriptor = catalog_model->descriptor;
        if (model.placement !=
                gnc::model_sdk::ModelPlacement::Unspecified &&
            model.placement != descriptor.placement) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::PlacementMismatch,
                 model.placement_source, model.occurrence_id,
                 "source placement differs from the package-owned model "
                 "placement policy"});
            continue;
        }
        gnc::model_sdk::CanonicalConfigBlock configuration;
        std::vector<CanonicalConfigFieldProvenance>
            configuration_provenance;
        if (!detail::canonicalize_configuration(
                model, descriptor, configuration,
                configuration_provenance, outcome.diagnostics)) {
            continue;
        }
        std::vector<CanonicalAssetBinding> assets;
        if (!detail::canonicalize_assets(
                model, descriptor, assets, outcome.diagnostics)) {
            continue;
        }

        CanonicalModelOccurrence canonical;
        canonical.occurrence_id = model.occurrence_id;
        canonical.package = catalog_model->package;
        canonical.model_id = descriptor.definition.model_id;
        canonical.model_version = descriptor.definition.model_version;
        canonical.execution_form =
            descriptor.definition.execution_form;
        canonical.preparation_algorithm_id =
            descriptor.preparation_algorithm_id;
        canonical.preparation_algorithm_version =
            descriptor.preparation_algorithm_version;
        canonical.output_ports =
            detail::canonical_ports(descriptor.ports);
        canonical.source = model.source;
        canonical.subject_entity_id = model.subject_entity_id;
        canonical.subject_source = model.subject_source;
        canonical.scope = model.scope;
        canonical.scope_source = model.scope_source;
        canonical.placement = descriptor.placement;
        canonical.placement_source =
            model.placement ==
                    gnc::model_sdk::ModelPlacement::Unspecified
                ? detail::catalog_source(
                      catalog_model->package.package_id,
                      descriptor.definition.model_id)
                : model.placement_source;
        canonical.configuration = std::move(configuration);
        canonical.configuration_source = model.configuration_source;
        canonical.configuration_field_sources =
            std::move(configuration_provenance);
        canonical.asset_bindings = std::move(assets);
        ir.model_occurrences.push_back(std::move(canonical));
    }

    auto algorithm_sources = source.algorithm_consumers;
    std::sort(algorithm_sources.begin(), algorithm_sources.end(),
              [](const SourceAlgorithmConsumer& lhs,
                 const SourceAlgorithmConsumer& rhs) {
                  return lhs.consumer_id < rhs.consumer_id;
              });
    for (const auto& algorithm : algorithm_sources) {
        if (algorithm.consumer_id.empty() ||
            !composition_node_ids.insert(algorithm.consumer_id).second) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::DuplicateOccurrence, algorithm.source,
                 algorithm.consumer_id,
                 "composition node identity is empty or duplicated"});
            continue;
        }
        const auto* catalog_algorithm = catalog.find_algorithm(
            algorithm.algorithm_id, algorithm.algorithm_version);
        if (catalog_algorithm == nullptr) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownAlgorithm, algorithm.source,
                 algorithm.consumer_id,
                 "exact algorithm descriptor is absent from the Catalog"});
            continue;
        }
        const auto& descriptor = catalog_algorithm->descriptor;
        ir.algorithm_consumers.push_back(
            {algorithm.consumer_id,
             catalog_algorithm->package,
             descriptor.algorithm_id,
             descriptor.algorithm_version,
             detail::canonical_ports(descriptor.ports),
             algorithm.source});
    }

    auto binding_sources = source.binding_intents;
    std::sort(binding_sources.begin(), binding_sources.end(),
              [](const SourceBinding& lhs, const SourceBinding& rhs) {
                  return lhs.binding_id < rhs.binding_id;
              });
    std::set<std::string> binding_ids;
    for (const auto& binding : binding_sources) {
        if (binding.binding_id.empty() ||
            !binding_ids.insert(binding.binding_id).second) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::InvalidStaticCompositionSource,
                 binding.source,
                 binding.binding_id,
                 "binding identity is empty or duplicated"});
        }
        ir.binding_intents.push_back(
            {binding.binding_id,
             binding.provider_occurrence_id,
             binding.provider_port_id,
             binding.consumer_id,
             binding.consumer_port_id,
             binding.source});
    }

    if (outcome.diagnostics.empty()) {
        outcome.value.emplace(std::move(ir));
    }
    return outcome;
}

// Human-readable canonical semantic view for direct IR regression tests and
// dry-run inspection. This is not a persistence or wire serialization format.
[[nodiscard]] inline std::string explain_canonical_mission_ir(
    const CanonicalMissionIr& ir) {
    std::ostringstream stream;
    stream << "mission-ir " << ir.revision << " mission " << ir.mission_id
           << '\n';
    for (const auto& entity : ir.entities) {
        stream << "entity " << entity.entity_id << ' '
               << to_string(entity.lifecycle) << '\n';
    }
    for (const auto& scope : ir.scopes) {
        stream << "scope " << to_string(scope.key.kind) << " subject "
               << scope.key.subject_entity_id << '\n';
    }
    for (const auto& model : ir.model_occurrences) {
        stream << "model " << model.occurrence_id << ' '
               << model.package.package_id << '@'
               << model.package.package_version << ' ' << model.model_id
               << '@' << model.model_version << ' '
               << gnc::model_sdk::to_string(model.execution_form)
               << " preparation " << model.preparation_algorithm_id << '@'
               << model.preparation_algorithm_version << " placement "
               << gnc::model_sdk::to_string(model.placement);
        if (!model.subject_entity_id.empty()) {
            stream << " subject " << model.subject_entity_id;
        }
        if (model.scope.has_value()) {
            stream << " scope " << to_string(model.scope->kind) << ':'
                   << model.scope->subject_entity_id;
        }
        stream << '\n';
        stream << "config " << model.occurrence_id << ' '
               << model.configuration.schema_id << '@'
               << model.configuration.schema_version << '\n';
        for (const auto& field : model.configuration.fields) {
            stream << "config-field " << model.occurrence_id << '.'
                   << field.field_id << ' '
                   << gnc::model_sdk::to_string(
                          gnc::model_sdk::canonical_config_value_kind(
                              field.value))
                   << ' ';
            if (const auto* text =
                    std::get_if<std::string>(&field.value)) {
                stream << *text;
            } else if (const auto* integer =
                           std::get_if<std::int64_t>(&field.value)) {
                stream << *integer;
            } else if (const auto* token =
                           std::get_if<
                               gnc::model_sdk::CanonicalEnumValue>(
                               &field.value)) {
                stream << token->token;
            } else {
                stream << std::setprecision(17)
                       << std::get<double>(field.value);
            }
            stream << '\n';
        }
        for (const auto& asset : model.asset_bindings) {
            stream << "asset " << model.occurrence_id << '.'
                   << asset.role << ' ' << asset.asset_schema_id << ' '
                   << asset.asset_id << '\n';
        }
        for (const auto& port : model.output_ports) {
            stream << "output " << model.occurrence_id << '.'
                   << port.port_id << ' ' << port.contract_id << '\n';
        }
    }
    for (const auto& algorithm : ir.algorithm_consumers) {
        stream << "algorithm-consumer " << algorithm.consumer_id << ' '
               << algorithm.package.package_id << '@'
               << algorithm.package.package_version << ' '
               << algorithm.algorithm_id << '@'
               << algorithm.algorithm_version << '\n';
        for (const auto& port : algorithm.input_ports) {
            stream << "input " << algorithm.consumer_id << '.'
                   << port.port_id << ' ' << port.contract_id << '\n';
        }
    }
    for (const auto& binding : ir.binding_intents) {
        stream << "intent " << binding.binding_id << ' '
               << binding.provider_occurrence_id << '.'
               << binding.provider_port_id << " -> "
               << binding.consumer_id << '.'
               << binding.consumer_port_id << '\n';
    }
    return stream.str();
}

[[nodiscard]] inline CompileOutcome<StaticCompilation> compile_static_plan(
    const TypedStaticCompositionSource& source, const Catalog& catalog) {
    CompileOutcome<StaticCompilation> outcome;
    auto ir_outcome = build_canonical_mission_ir(source, catalog);
    if (!ir_outcome.succeeded()) {
        outcome.diagnostics = std::move(ir_outcome.diagnostics);
        return outcome;
    }
    auto ir = std::move(*ir_outcome.value);
    if (source.plan_id.empty()) {
        outcome.diagnostics.push_back(
            {DiagnosticCode::InvalidStaticCompositionSource,
             {"typed://mission", "/plan_id"}, source.mission_id,
             "plan identity is required for static plan lowering"});
        return outcome;
    }

    std::map<std::string, const CanonicalModelOccurrence*> model_by_id;
    for (const auto& model : ir.model_occurrences) {
        model_by_id.emplace(model.occurrence_id, &model);
    }
    std::map<std::string, const CanonicalAlgorithmConsumer*>
        algorithm_by_id;
    for (const auto& algorithm : ir.algorithm_consumers) {
        algorithm_by_id.emplace(algorithm.consumer_id, &algorithm);
    }

    std::vector<BindingPlanEntry> resolved_bindings;
    std::vector<BindingProof> proofs;
    std::map<std::string, std::size_t> provider_counts;
    std::map<std::string, std::size_t> consumer_counts;

    for (const auto& intent : ir.binding_intents) {
        const auto& binding = intent;
        const auto model_found =
            model_by_id.find(binding.provider_occurrence_id);
        const auto algorithm_found =
            algorithm_by_id.find(binding.consumer_id);
        if (model_found == model_by_id.end() ||
            algorithm_found == algorithm_by_id.end()) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownEndpoint, binding.source,
                 binding.binding_id,
                 "binding must connect a model provider to an algorithm "
                 "consumer"});
            continue;
        }

        const auto* provider_port = detail::find_port(
            model_found->second->output_ports,
            binding.provider_port_id);
        const auto* consumer_port = detail::find_port(
            algorithm_found->second->input_ports,
            binding.consumer_port_id);
        if (provider_port == nullptr || consumer_port == nullptr) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownEndpoint, binding.source,
                 binding.binding_id,
                 "binding references an unknown provider or consumer port"});
            continue;
        }
        if (provider_port->contract_id != consumer_port->contract_id) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::ContractMismatch, binding.source,
                 binding.binding_id,
                 "provider and consumer contract identities differ"});
            continue;
        }

        const auto provider_key = detail::endpoint_key(
            binding.provider_occurrence_id, binding.provider_port_id);
        const auto consumer_key = detail::endpoint_key(
            binding.consumer_id, binding.consumer_port_id);
        ++provider_counts[provider_key];
        ++consumer_counts[consumer_key];
        resolved_bindings.push_back(
            {binding.binding_id, binding.provider_occurrence_id,
             binding.provider_port_id, binding.consumer_id,
             binding.consumer_port_id, provider_port->contract_id});
        proofs.push_back(
            {"proof.binding." + binding.binding_id,
             "GNC.PLAN.BINDING.CONTRACT.EXACT", binding.binding_id,
             provider_port->contract_id,
             {model_found->second->source, binding.source,
              algorithm_found->second->source},
             BindingProofResult::Proven});
    }

    for (const auto& model : ir.model_occurrences) {
        for (const auto& port : model.output_ports) {
            if (provider_counts[detail::endpoint_key(
                    model.occurrence_id, port.port_id)] == 0U) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::MissingRequiredBinding, model.source,
                     model.occurrence_id + "." + port.port_id,
                     "required model output has no consumer"});
            }
        }
    }
    for (const auto& algorithm : ir.algorithm_consumers) {
        for (const auto& port : algorithm.input_ports) {
            const auto count = consumer_counts[detail::endpoint_key(
                algorithm.consumer_id, port.port_id)];
            if (count == 0U) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::MissingRequiredBinding, algorithm.source,
                     algorithm.consumer_id + "." + port.port_id,
                     "required algorithm input has no provider"});
            } else if (count > 1U) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::MultipleRequiredBindings,
                     algorithm.source,
                     algorithm.consumer_id + "." + port.port_id,
                     "required algorithm input has multiple providers"});
            }
        }
    }

    if (!outcome.diagnostics.empty()) {
        return outcome;
    }

    ExecutionPlanDescriptor plan;
    plan.plan_id = source.plan_id;
    plan.mission_id = source.mission_id;

    std::map<std::string, PackageLock> dependency_locks;
    for (const auto& model : ir.model_occurrences) {
        dependency_locks.emplace(
            detail::exact_key(model.package.package_id,
                              model.package.package_version),
            model.package);
        plan.model_preparation_identities.push_back(
            {model.occurrence_id, model.package,
             model.model_id,
             model.model_version,
             model.execution_form,
             model.preparation_algorithm_id,
             model.preparation_algorithm_version, model.source});
    }
    for (const auto& algorithm : ir.algorithm_consumers) {
        dependency_locks.emplace(
            detail::exact_key(algorithm.package.package_id,
                              algorithm.package.package_version),
            algorithm.package);
        plan.algorithms.push_back(
            {algorithm.consumer_id, algorithm.package,
             algorithm.algorithm_id,
             algorithm.algorithm_version,
             algorithm.source});
    }
    for (const auto& dependency : dependency_locks) {
        plan.dependency_lock.push_back(dependency.second);
    }

    plan.bindings = std::move(resolved_bindings);
    plan.binding_proofs = std::move(proofs);
    for (std::size_t index = 0U; index < plan.bindings.size(); ++index) {
        const auto& binding = plan.bindings[index];
        const auto provider = model_by_id.at(binding.provider_occurrence_id);
        const auto form = provider->execution_form;
        const auto kind = form == gnc::model_sdk::ModelExecutionForm::PureQuery
                              ? CompiledObligationKind::PureQueryEvaluation
                              : CompiledObligationKind::ClosureEvaluation;
        plan.obligations.push_back(
            {"obligation." + binding.binding_id, index, kind,
             binding.provider_occurrence_id,
             binding.consumer_id, binding.binding_id});
    }

    outcome.value.emplace(
        StaticCompilation{std::move(ir), std::move(plan)});
    return outcome;
}

[[nodiscard]] inline std::string explain_static_plan(
    const ExecutionPlanDescriptor& plan) {
    std::ostringstream stream;
    stream << "plan " << plan.plan_id << " mission " << plan.mission_id
           << '\n';
    for (const auto& dependency : plan.dependency_lock) {
        stream << "lock " << dependency.package_id << '@'
               << dependency.package_version << '\n';
    }
    for (const auto& model : plan.model_preparation_identities) {
        stream << "model " << model.occurrence_id << ' '
               << model.model_id << '@' << model.model_version << ' '
               << gnc::model_sdk::to_string(model.execution_form)
               << " preparation "
               << model.preparation_algorithm_id << '@'
               << model.preparation_algorithm_version << '\n';
    }
    for (const auto& algorithm : plan.algorithms) {
        stream << "consumer " << algorithm.consumer_id << ' '
               << algorithm.algorithm_id << '@'
               << algorithm.algorithm_version << '\n';
    }
    for (const auto& binding : plan.bindings) {
        stream << "bind " << binding.binding_id << ' '
               << binding.provider_occurrence_id << '.'
               << binding.provider_port_id << " -> "
               << binding.consumer_id << '.'
               << binding.consumer_port_id << ' '
               << binding.contract_id << '\n';
    }
    for (const auto& proof : plan.binding_proofs) {
        stream << "prove " << proof.proof_id << ' '
               << proof.assertion_code << ' ' << proof.contract_id << '\n';
    }
    for (const auto& obligation : plan.obligations) {
        stream << "obligation " << obligation.ordinal << ' '
               << obligation.obligation_id << ' '
               << to_string(obligation.kind) << ' '
               << obligation.provider_occurrence_id << " -> "
               << obligation.consumer_id << '\n';
    }
    return stream.str();
}

} // namespace gnc::compiler
