#pragma once

#include "gnc/model_sdk/static_descriptor.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace gnc::compiler {

inline constexpr std::string_view kTypedSourceTreeVersion =
    "gnc.typed-source-tree/1";

struct SourceRef {
    std::string document_uri;
    std::string node_path;
};

enum class DiagnosticCode : std::uint8_t {
    InvalidCatalogDescriptor,
    DuplicateCatalogIdentity,
    InvalidSourceTree,
    DuplicateOccurrence,
    UnknownDefinition,
    UnknownAlgorithm,
    UnknownEndpoint,
    PortDirectionMismatch,
    ContractMismatch,
    MissingRequiredBinding,
    MultipleRequiredBindings,
};

[[nodiscard]] constexpr std::string_view to_string(
    DiagnosticCode code) noexcept {
    switch (code) {
    case DiagnosticCode::InvalidCatalogDescriptor:
        return "GNC-CAT-INVALID-DESCRIPTOR";
    case DiagnosticCode::DuplicateCatalogIdentity:
        return "GNC-CAT-DUPLICATE-IDENTITY";
    case DiagnosticCode::InvalidSourceTree:
        return "GNC-SRC-INVALID-TYPED-TREE";
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
    }
    return "GNC-COMPILER-UNKNOWN";
}

struct Diagnostic {
    DiagnosticCode code = DiagnosticCode::InvalidSourceTree;
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
    std::vector<Diagnostic>& diagnostics) {
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
    }
}

template <typename Descriptor>
[[nodiscard]] inline const gnc::model_sdk::StaticPortDescriptor* find_port(
    const Descriptor& descriptor, std::string_view port_id) {
    const auto found = std::find_if(
        descriptor.ports.begin(), descriptor.ports.end(),
        [port_id](const gnc::model_sdk::StaticPortDescriptor& port) {
            return port.port_id == port_id;
        });
    return found == descriptor.ports.end() ? nullptr : &*found;
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
                detail::validate_ports(model.ports, package.package_id,
                                       definition.model_id,
                                       outcome.diagnostics);
                models.push_back({lock, std::move(model)});
            }

            for (auto& algorithm : package.algorithms) {
                if (algorithm.algorithm_id.empty() ||
                    algorithm.algorithm_version.empty() ||
                    algorithm.composition_model_id.empty()) {
                    outcome.diagnostics.push_back(
                        {DiagnosticCode::InvalidCatalogDescriptor,
                         detail::catalog_source(package.package_id,
                                                algorithm.algorithm_id),
                         algorithm.algorithm_id,
                         "algorithm and composition model identities are "
                         "required"});
                }
                detail::validate_ports(algorithm.ports, package.package_id,
                                       algorithm.algorithm_id,
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

struct SourceModelOccurrence {
    std::string occurrence_id;
    std::string model_id;
    std::string model_version;
    SourceRef source;
};

struct SourceAlgorithmOccurrence {
    std::string occurrence_id;
    std::string algorithm_id;
    std::string algorithm_version;
    SourceRef source;
};

struct SourceBinding {
    std::string binding_id;
    std::string provider_occurrence_id;
    std::string provider_port_id;
    std::string consumer_occurrence_id;
    std::string consumer_port_id;
    SourceRef source;
};

// Programmatic typed entry for the first R2 slice. No syntax frontend or
// format-specific node is visible to the Compiler core.
struct SourceTree {
    std::string source_tree_version;
    std::string mission_id;
    std::string plan_id;
    std::vector<SourceModelOccurrence> models;
    std::vector<SourceAlgorithmOccurrence> algorithms;
    std::vector<SourceBinding> bindings;
};

struct ResolvedModelOccurrence {
    std::string occurrence_id;
    PackageLock package;
    gnc::model_sdk::StaticModelDescriptor descriptor;
    SourceRef source;
};

struct ResolvedAlgorithmOccurrence {
    std::string occurrence_id;
    PackageLock package;
    gnc::model_sdk::StaticAlgorithmDescriptor descriptor;
    SourceRef source;
};

struct BindingIntent {
    SourceBinding source_binding;
};

struct MissionIr {
    std::uint32_t revision = 1U;
    std::string mission_id;
    std::vector<ResolvedModelOccurrence> models;
    std::vector<ResolvedAlgorithmOccurrence> algorithms;
    std::vector<BindingIntent> binding_intents;
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

struct PreparedModelPlan {
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

struct AlgorithmPlan {
    std::string occurrence_id;
    PackageLock package;
    std::string algorithm_id;
    std::string algorithm_version;
    std::string composition_model_id;
    SourceRef source;
};

struct BindingPlanEntry {
    std::string binding_id;
    std::string provider_occurrence_id;
    std::string provider_port_id;
    std::string consumer_occurrence_id;
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
    std::string consumer_occurrence_id;
    std::string binding_id;
};

// This is a static, in-process descriptor. It intentionally contains no
// runtime instance, function address, Session identity, or mutable state.
struct ExecutionPlanDescriptor {
    std::uint32_t revision = 1U;
    std::string plan_id;
    std::string mission_id;
    std::vector<PackageLock> dependency_lock;
    std::vector<PreparedModelPlan> prepared_models;
    std::vector<AlgorithmPlan> algorithms;
    std::vector<BindingPlanEntry> bindings;
    std::vector<BindingProof> binding_proofs;
    std::vector<CompiledObligation> obligations;
};

struct StaticCompilation {
    MissionIr ir;
    ExecutionPlanDescriptor plan;
};

[[nodiscard]] inline CompileOutcome<StaticCompilation> compile_static_plan(
    const SourceTree& source, const Catalog& catalog) {
    CompileOutcome<StaticCompilation> outcome;
    if (source.source_tree_version != kTypedSourceTreeVersion ||
        source.mission_id.empty() || source.plan_id.empty()) {
        outcome.diagnostics.push_back(
            {DiagnosticCode::InvalidSourceTree,
             {"typed://mission", "/"}, source.mission_id,
             "source tree version, mission identity, and plan identity are "
             "required"});
        return outcome;
    }

    MissionIr ir;
    ir.mission_id = source.mission_id;
    std::set<std::string> occurrence_ids;

    auto model_sources = source.models;
    std::sort(model_sources.begin(), model_sources.end(),
              [](const SourceModelOccurrence& lhs,
                 const SourceModelOccurrence& rhs) {
                  return lhs.occurrence_id < rhs.occurrence_id;
              });
    for (const auto& model : model_sources) {
        if (model.occurrence_id.empty() ||
            !occurrence_ids.insert(model.occurrence_id).second) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::DuplicateOccurrence, model.source,
                 model.occurrence_id,
                 "occurrence identity is empty or duplicated"});
            continue;
        }
        const auto* descriptor =
            catalog.find_model(model.model_id, model.model_version);
        if (descriptor == nullptr) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownDefinition, model.source,
                 model.occurrence_id,
                 "exact model definition is absent from the Catalog"});
            continue;
        }
        ir.models.push_back({model.occurrence_id, descriptor->package,
                             descriptor->descriptor, model.source});
    }

    auto algorithm_sources = source.algorithms;
    std::sort(algorithm_sources.begin(), algorithm_sources.end(),
              [](const SourceAlgorithmOccurrence& lhs,
                 const SourceAlgorithmOccurrence& rhs) {
                  return lhs.occurrence_id < rhs.occurrence_id;
              });
    for (const auto& algorithm : algorithm_sources) {
        if (algorithm.occurrence_id.empty() ||
            !occurrence_ids.insert(algorithm.occurrence_id).second) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::DuplicateOccurrence, algorithm.source,
                 algorithm.occurrence_id,
                 "occurrence identity is empty or duplicated"});
            continue;
        }
        const auto* descriptor = catalog.find_algorithm(
            algorithm.algorithm_id, algorithm.algorithm_version);
        if (descriptor == nullptr) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownAlgorithm, algorithm.source,
                 algorithm.occurrence_id,
                 "exact algorithm descriptor is absent from the Catalog"});
            continue;
        }
        ir.algorithms.push_back(
            {algorithm.occurrence_id, descriptor->package,
             descriptor->descriptor, algorithm.source});
    }

    auto binding_sources = source.bindings;
    std::sort(binding_sources.begin(), binding_sources.end(),
              [](const SourceBinding& lhs, const SourceBinding& rhs) {
                  return lhs.binding_id < rhs.binding_id;
              });
    std::set<std::string> binding_ids;
    for (const auto& binding : binding_sources) {
        if (binding.binding_id.empty() ||
            !binding_ids.insert(binding.binding_id).second) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::InvalidSourceTree, binding.source,
                 binding.binding_id,
                 "binding identity is empty or duplicated"});
        }
        ir.binding_intents.push_back({binding});
    }

    if (!outcome.diagnostics.empty()) {
        return outcome;
    }

    std::map<std::string, const ResolvedModelOccurrence*> model_by_id;
    for (const auto& model : ir.models) {
        model_by_id.emplace(model.occurrence_id, &model);
    }
    std::map<std::string, const ResolvedAlgorithmOccurrence*> algorithm_by_id;
    for (const auto& algorithm : ir.algorithms) {
        algorithm_by_id.emplace(algorithm.occurrence_id, &algorithm);
    }

    std::vector<BindingPlanEntry> resolved_bindings;
    std::vector<BindingProof> proofs;
    std::map<std::string, std::size_t> provider_counts;
    std::map<std::string, std::size_t> consumer_counts;

    for (const auto& intent : ir.binding_intents) {
        const auto& binding = intent.source_binding;
        const auto model_found =
            model_by_id.find(binding.provider_occurrence_id);
        const auto algorithm_found =
            algorithm_by_id.find(binding.consumer_occurrence_id);
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
            model_found->second->descriptor, binding.provider_port_id);
        const auto* consumer_port = detail::find_port(
            algorithm_found->second->descriptor, binding.consumer_port_id);
        if (provider_port == nullptr || consumer_port == nullptr) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::UnknownEndpoint, binding.source,
                 binding.binding_id,
                 "binding references an unknown provider or consumer port"});
            continue;
        }
        if (provider_port->direction !=
                gnc::model_sdk::StaticPortDirection::Output ||
            consumer_port->direction !=
                gnc::model_sdk::StaticPortDirection::Input) {
            outcome.diagnostics.push_back(
                {DiagnosticCode::PortDirectionMismatch, binding.source,
                 binding.binding_id,
                 "binding direction must be output to input"});
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
            binding.consumer_occurrence_id, binding.consumer_port_id);
        ++provider_counts[provider_key];
        ++consumer_counts[consumer_key];
        resolved_bindings.push_back(
            {binding.binding_id, binding.provider_occurrence_id,
             binding.provider_port_id, binding.consumer_occurrence_id,
             binding.consumer_port_id, provider_port->contract_id});
        proofs.push_back(
            {"proof.binding." + binding.binding_id,
             "GNC.PLAN.BINDING.CONTRACT.EXACT", binding.binding_id,
             provider_port->contract_id,
             {model_found->second->source, binding.source,
              algorithm_found->second->source},
             BindingProofResult::Proven});
    }

    for (const auto& model : ir.models) {
        for (const auto& port : model.descriptor.ports) {
            if (port.required && port.direction ==
                                     gnc::model_sdk::StaticPortDirection::Output &&
                provider_counts[detail::endpoint_key(model.occurrence_id,
                                                     port.port_id)] == 0U) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::MissingRequiredBinding, model.source,
                     model.occurrence_id + "." + port.port_id,
                     "required model output has no consumer"});
            }
        }
    }
    for (const auto& algorithm : ir.algorithms) {
        for (const auto& port : algorithm.descriptor.ports) {
            if (!port.required || port.direction !=
                                      gnc::model_sdk::StaticPortDirection::Input) {
                continue;
            }
            const auto count = consumer_counts[detail::endpoint_key(
                algorithm.occurrence_id, port.port_id)];
            if (count == 0U) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::MissingRequiredBinding, algorithm.source,
                     algorithm.occurrence_id + "." + port.port_id,
                     "required algorithm input has no provider"});
            } else if (count > 1U) {
                outcome.diagnostics.push_back(
                    {DiagnosticCode::MultipleRequiredBindings,
                     algorithm.source,
                     algorithm.occurrence_id + "." + port.port_id,
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
    for (const auto& model : ir.models) {
        dependency_locks.emplace(
            detail::exact_key(model.package.package_id,
                              model.package.package_version),
            model.package);
        plan.prepared_models.push_back(
            {model.occurrence_id, model.package,
             model.descriptor.definition.model_id,
             model.descriptor.definition.model_version,
             model.descriptor.definition.execution_form,
             model.descriptor.preparation_algorithm_id,
             model.descriptor.preparation_algorithm_version, model.source});
    }
    for (const auto& algorithm : ir.algorithms) {
        dependency_locks.emplace(
            detail::exact_key(algorithm.package.package_id,
                              algorithm.package.package_version),
            algorithm.package);
        plan.algorithms.push_back(
            {algorithm.occurrence_id, algorithm.package,
             algorithm.descriptor.algorithm_id,
             algorithm.descriptor.algorithm_version,
             algorithm.descriptor.composition_model_id, algorithm.source});
    }
    for (const auto& dependency : dependency_locks) {
        plan.dependency_lock.push_back(dependency.second);
    }

    plan.bindings = std::move(resolved_bindings);
    plan.binding_proofs = std::move(proofs);
    for (std::size_t index = 0U; index < plan.bindings.size(); ++index) {
        const auto& binding = plan.bindings[index];
        const auto provider = model_by_id.at(binding.provider_occurrence_id);
        const auto form = provider->descriptor.definition.execution_form;
        const auto kind = form == gnc::model_sdk::ModelExecutionForm::PureQuery
                              ? CompiledObligationKind::PureQueryEvaluation
                              : CompiledObligationKind::ClosureEvaluation;
        plan.obligations.push_back(
            {"obligation." + binding.binding_id, index, kind,
             binding.provider_occurrence_id,
             binding.consumer_occurrence_id, binding.binding_id});
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
    for (const auto& model : plan.prepared_models) {
        stream << "prepare " << model.occurrence_id << ' '
               << model.model_id << '@' << model.model_version << ' '
               << gnc::model_sdk::to_string(model.execution_form) << ' '
               << model.preparation_algorithm_id << '@'
               << model.preparation_algorithm_version << '\n';
    }
    for (const auto& algorithm : plan.algorithms) {
        stream << "algorithm " << algorithm.occurrence_id << ' '
               << algorithm.algorithm_id << '@'
               << algorithm.algorithm_version << " model "
               << algorithm.composition_model_id << '\n';
    }
    for (const auto& binding : plan.bindings) {
        stream << "bind " << binding.binding_id << ' '
               << binding.provider_occurrence_id << '.'
               << binding.provider_port_id << " -> "
               << binding.consumer_occurrence_id << '.'
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
               << obligation.consumer_occurrence_id << '\n';
    }
    return stream.str();
}

} // namespace gnc::compiler
