#include <cavh/formula.hpp>
#include <gnc/compiler/static_mission_compiler.hpp>
#include <yyz/rigid_step.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using gnc::compiler::Catalog;
using gnc::compiler::CanonicalMissionIr;
using gnc::compiler::CompileOutcome;
using gnc::compiler::CompiledObligationKind;
using gnc::compiler::Diagnostic;
using gnc::compiler::DiagnosticCode;
using gnc::compiler::ExecutionPlanDescriptor;
using gnc::compiler::SourceAlgorithmOccurrence;
using gnc::compiler::SourceBinding;
using gnc::compiler::SourceModelOccurrence;
using gnc::compiler::SourceRef;
using gnc::compiler::StaticCompilation;
using gnc::compiler::TypedStaticCompositionSource;
using gnc::model_sdk::ModelExecutionForm;
using gnc::model_sdk::StaticAlgorithmDescriptor;
using gnc::model_sdk::StaticPortDescriptor;
using gnc::model_sdk::StaticPortDirection;

constexpr std::string_view kMissionId =
    "mission.r2.yyz-cavh-static-composition@1";
constexpr std::string_view kPlanId =
    "plan.r2.yyz-cavh-static-composition@1";
constexpr std::string_view kDocumentUri =
    "typed://fixture/r2-yyz-cavh-static-plan";

static_assert(
    std::is_same_v<decltype(ExecutionPlanDescriptor::obligations),
                   std::vector<gnc::compiler::CompiledObligation>>,
    "the static descriptor must expose typed compiled obligations");
static_assert(
    std::is_same_v<
        decltype(gnc::compiler::ModelPreparationIdentityPlan::execution_form),
        ModelExecutionForm>,
    "prepared entries must preserve the accepted execution-form type");

template <typename Value, typename = void>
struct has_required_member : std::false_type {};

template <typename Value>
struct has_required_member<
    Value, std::void_t<decltype(std::declval<Value>().required)>>
    : std::true_type {};

template <typename Value, typename = void>
struct has_composition_model_id_member : std::false_type {};

template <typename Value>
struct has_composition_model_id_member<
    Value,
    std::void_t<decltype(std::declval<Value>().composition_model_id)>>
    : std::true_type {};

template <typename Value, typename = void>
struct has_runtime_instance_id_member : std::false_type {};

template <typename Value>
struct has_runtime_instance_id_member<
    Value,
    std::void_t<decltype(std::declval<Value>().runtime_instance_id)>>
    : std::true_type {};

static_assert(!has_required_member<StaticPortDescriptor>::value,
              "the current static composition has no optional ports");
static_assert(
    !has_composition_model_id_member<StaticAlgorithmDescriptor>::value,
    "unverified composition model identity must stay outside descriptors");
static_assert(
    !has_runtime_instance_id_member<
        gnc::compiler::CanonicalModelOccurrence>::value,
    "canonical PureQuery/Closure IR must not allocate runtime instances");

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

template <typename Value>
const Value& require_value(const CompileOutcome<Value>& outcome,
                           std::string_view message) {
    require(outcome.succeeded(), message);
    return *outcome.value;
}

[[nodiscard]] SourceRef ref(std::string path) {
    return {std::string(kDocumentUri), std::move(path)};
}

[[nodiscard]] gnc::packages::cavh::CavhFormulaDefinition
cavh_definition() {
    gnc::packages::cavh::CavhFormulaDefinition definition;
    definition.envelope.metadata = {
        std::string(gnc::packages::cavh::kGlideEnvelopeModelIdentity),
        std::string(gnc::packages::cavh::kGlideEnvelopeModelVersion),
        ModelExecutionForm::PureQuery};
    return definition;
}

[[nodiscard]] gnc::packages::yyz::RigidStepModelDefinition yyz_definition() {
    gnc::packages::yyz::RigidStepModelDefinition definition;
    definition.force_moment_closure.metadata = {
        std::string(gnc::packages::yyz::kForceMomentClosureModelIdentity),
        std::string(
            gnc::packages::yyz::kForceMomentClosureModelVersion),
        ModelExecutionForm::Closure};
    return definition;
}

[[nodiscard]] std::vector<gnc::model_sdk::StaticPackageDescriptor>
package_descriptors() {
    const auto cavh = cavh_definition();
    const auto yyz = yyz_definition();
    auto cavh_package =
        gnc::packages::cavh::describe_cavh_formula_package();
    auto yyz_package =
        gnc::packages::yyz::describe_yyz_rigid_step_package();
    require(cavh_package.models[0U].definition.model_id ==
                cavh.envelope.metadata.model_id &&
                cavh_package.models[0U].definition.model_version ==
                    cavh.envelope.metadata.model_version &&
                cavh_package.models[0U].definition.execution_form ==
                    cavh.envelope.metadata.execution_form,
            "CAVH package descriptor diverged from the real definition");
    require(yyz_package.models[0U].definition.model_id ==
                yyz.force_moment_closure.metadata.model_id &&
                yyz_package.models[0U].definition.model_version ==
                    yyz.force_moment_closure.metadata.model_version &&
                yyz_package.models[0U].definition.execution_form ==
                    yyz.force_moment_closure.metadata.execution_form,
            "YYZ package descriptor diverged from the real definition");
    return {std::move(cavh_package), std::move(yyz_package)};
}

[[nodiscard]] TypedStaticCompositionSource composition_source() {
    TypedStaticCompositionSource source;
    source.source_version = std::string(
        gnc::compiler::kTypedStaticCompositionSourceVersion);
    source.mission_id = std::string(kMissionId);
    source.plan_id = std::string(kPlanId);
    source.models = {
        SourceModelOccurrence{
            "cavh.envelope",
            std::string(
                gnc::packages::cavh::kGlideEnvelopeModelIdentity),
            std::string(
                gnc::packages::cavh::kGlideEnvelopeModelVersion),
            ref("/models/cavh.envelope")},
        SourceModelOccurrence{
            "yyz.closure",
            std::string(
                gnc::packages::yyz::kForceMomentClosureModelIdentity),
            std::string(
                gnc::packages::yyz::kForceMomentClosureModelVersion),
            ref("/models/yyz.closure")},
    };
    source.algorithms = {
        SourceAlgorithmOccurrence{
            "cavh.formula",
            std::string(
                gnc::packages::cavh::kCavhFormulaKernelIdentity.id),
            std::string(
                gnc::packages::cavh::kCavhFormulaKernelIdentity.version),
            ref("/algorithms/cavh.formula")},
        SourceAlgorithmOccurrence{
            "yyz.rigid-step",
            std::string(gnc::packages::yyz::kRigidStepKernelIdentity.id),
            std::string(
                gnc::packages::yyz::kRigidStepKernelIdentity.version),
            ref("/algorithms/yyz.rigid-step")},
    };
    source.bindings = {
        SourceBinding{
            "cavh.envelope-to-formula", "cavh.envelope", "envelope",
            "cavh.formula", "glide-envelope",
            ref("/bindings/cavh.envelope-to-formula")},
        SourceBinding{
            "yyz.closure-to-rigid", "yyz.closure", "form-input",
            "yyz.rigid-step", "form-input",
            ref("/bindings/yyz.closure-to-rigid")},
    };
    return source;
}

[[nodiscard]] bool has_diagnostic(
    const std::vector<Diagnostic>& diagnostics, DiagnosticCode code) {
    return std::any_of(
        diagnostics.begin(), diagnostics.end(),
        [code](const Diagnostic& diagnostic) {
            return diagnostic.code == code;
        });
}

const Diagnostic& require_diagnostic(
    const std::vector<Diagnostic>& diagnostics, DiagnosticCode code,
    std::string_view message) {
    const auto found = std::find_if(
        diagnostics.begin(), diagnostics.end(),
        [code](const Diagnostic& diagnostic) {
            return diagnostic.code == code;
        });
    require(found != diagnostics.end(), message);
    return *found;
}

void verify_success_product(const StaticCompilation& compilation) {
    const auto& ir = compilation.ir;
    const auto& plan = compilation.plan;
    require(ir.revision == 1U && ir.mission_id == kMissionId,
            "typed source did not produce the expected minimal IR");
    require(ir.models.size() == 2U && ir.algorithms.size() == 2U &&
                ir.binding_intents.size() == 2U,
            "minimal IR occurrence or binding count changed");
    require(ir.models[0U].occurrence_id == "cavh.envelope" &&
                ir.models[0U].model_id ==
                    gnc::packages::cavh::kGlideEnvelopeModelIdentity &&
                ir.models[0U].output_ports.size() == 1U &&
                ir.models[0U].output_ports[0U].contract_id ==
                    gnc::packages::cavh::
                        kGlideEnvelopeOutputContractIdentity &&
                ir.models[1U].occurrence_id == "yyz.closure" &&
                ir.models[1U].model_id ==
                    gnc::packages::yyz::
                        kForceMomentClosureModelIdentity &&
                ir.models[1U].output_ports.size() == 1U &&
                ir.models[1U].output_ports[0U].contract_id ==
                    gnc::packages::yyz::kRigidFormInputContractIdentity,
            "canonical IR lost exact model or output identities");
    require(ir.algorithms[0U].occurrence_id == "cavh.formula" &&
                ir.algorithms[0U].input_ports.size() == 1U &&
                ir.algorithms[0U].input_ports[0U].contract_id ==
                    gnc::packages::cavh::
                        kGlideEnvelopeOutputContractIdentity &&
                ir.algorithms[1U].occurrence_id == "yyz.rigid-step" &&
                ir.algorithms[1U].input_ports.size() == 1U &&
                ir.algorithms[1U].input_ports[0U].contract_id ==
                    gnc::packages::yyz::kRigidFormInputContractIdentity &&
                ir.binding_intents[0U].binding_id ==
                    "cavh.envelope-to-formula" &&
                ir.binding_intents[1U].binding_id ==
                    "yyz.closure-to-rigid",
            "canonical IR lost exact algorithm inputs or binding intents");
    require(plan.plan_id == kPlanId && plan.mission_id == kMissionId,
            "static plan identity changed");
    require(plan.dependency_lock.size() == 2U &&
                plan.model_preparation_identities.size() == 2U &&
                plan.algorithms.size() == 2U &&
                plan.bindings.size() == 2U &&
                plan.binding_proofs.size() == 2U &&
                plan.obligations.size() == 2U,
            "static plan closure count changed");

    const auto& cavh_model = plan.model_preparation_identities[0U];
    require(cavh_model.occurrence_id == "cavh.envelope" &&
                cavh_model.model_id ==
                    gnc::packages::cavh::kGlideEnvelopeModelIdentity &&
                cavh_model.execution_form == ModelExecutionForm::PureQuery &&
                cavh_model.preparation_algorithm_id ==
                    gnc::packages::cavh::
                        kGlideEnvelopePreparationIdentity.id,
            "CAVH plan entry lost its real query definition");
    const auto& yyz_model = plan.model_preparation_identities[1U];
    require(yyz_model.occurrence_id == "yyz.closure" &&
                yyz_model.model_id ==
                    gnc::packages::yyz::
                        kForceMomentClosureModelIdentity &&
                yyz_model.execution_form == ModelExecutionForm::Closure &&
                yyz_model.preparation_algorithm_id ==
                    gnc::packages::yyz::
                        kForceMomentClosurePreparationIdentity.id,
            "YYZ plan entry lost its real closure definition");

    require(plan.algorithms[0U].algorithm_id ==
                gnc::packages::cavh::kCavhFormulaKernelIdentity.id &&
                plan.algorithms[0U].algorithm_version ==
                    gnc::packages::cavh::
                        kCavhFormulaKernelIdentity.version &&
                plan.algorithms[1U].algorithm_id ==
                    gnc::packages::yyz::kRigidStepKernelIdentity.id &&
                plan.algorithms[1U].algorithm_version ==
                    gnc::packages::yyz::kRigidStepKernelIdentity.version,
            "algorithm consumers lost exact identities");
    require(plan.bindings[0U].contract_id ==
                gnc::packages::cavh::
                    kGlideEnvelopeOutputContractIdentity &&
                plan.bindings[1U].contract_id ==
                    gnc::packages::yyz::kRigidFormInputContractIdentity,
            "formal query or closure output contract changed");
    require(plan.binding_proofs[0U].assertion_code ==
                "GNC.PLAN.BINDING.CONTRACT.EXACT" &&
                plan.binding_proofs[0U].source_refs.size() == 3U &&
                plan.binding_proofs[0U].source_refs[1U].node_path ==
                    "/bindings/cavh.envelope-to-formula" &&
                plan.binding_proofs[1U].source_refs[1U].node_path ==
                    "/bindings/yyz.closure-to-rigid",
            "binding proof lost direct source locations");
    require(plan.obligations[0U].kind ==
                CompiledObligationKind::PureQueryEvaluation &&
                plan.obligations[0U].consumer_occurrence_id ==
                    "cavh.formula" &&
                plan.obligations[1U].kind ==
                    CompiledObligationKind::ClosureEvaluation &&
                plan.obligations[1U].consumer_occurrence_id ==
                    "yyz.rigid-step",
            "compiled obligations no longer express true package consumers");
}

void verify_deterministic_order(std::string_view expected_ir_explain,
                                std::string_view expected_plan_explain) {
    auto packages = package_descriptors();
    std::reverse(packages.begin(), packages.end());
    const auto catalog_outcome = Catalog::build(std::move(packages));
    const auto& catalog =
        require_value(catalog_outcome, "reordered Catalog build failed");

    auto source = composition_source();
    std::reverse(source.models.begin(), source.models.end());
    std::reverse(source.algorithms.begin(), source.algorithms.end());
    std::reverse(source.bindings.begin(), source.bindings.end());
    for (std::size_t index = 0U; index < source.models.size(); ++index) {
        source.models[index].source =
            {"typed://alternate/source.yaml",
             "/model_occurrences/" + std::to_string(index)};
    }
    for (std::size_t index = 0U; index < source.algorithms.size(); ++index) {
        source.algorithms[index].source =
            {"typed://alternate/source.yaml",
             "/algorithm_occurrences/" + std::to_string(index)};
    }
    for (std::size_t index = 0U; index < source.bindings.size(); ++index) {
        source.bindings[index].source =
            {"typed://alternate/source.yaml",
             "/binding_intents/" + std::to_string(index)};
    }

    auto ir_source = source;
    ir_source.plan_id = "plan.representation-local-alternate@1";
    const auto ir_outcome =
        gnc::compiler::build_canonical_mission_ir(ir_source, catalog);
    const auto& ir = require_value(
        ir_outcome, "reordered canonical Mission IR build failed");
    require(gnc::compiler::explain_canonical_mission_ir(ir) ==
                expected_ir_explain,
            "source order, locations, or plan identity changed canonical IR "
            "semantics");

    const auto compile_outcome =
        gnc::compiler::compile_static_plan(source, catalog);
    const auto& compilation = require_value(
        compile_outcome, "reordered typed source compilation failed");
    require(gnc::compiler::explain_static_plan(compilation.plan) ==
                expected_plan_explain,
        "Catalog or composition-source insertion order changed the plan");
    require(compilation.plan.binding_proofs[0U]
                .source_refs[0U]
                .document_uri == "typed://alternate/source.yaml",
            "canonical semantics discarded source provenance");
}

void verify_negative_cases() {
    const auto catalog_outcome = Catalog::build(package_descriptors());
    const auto& catalog =
        require_value(catalog_outcome, "fixture Catalog build failed");

    auto empty = composition_source();
    empty.models.clear();
    empty.algorithms.clear();
    empty.bindings.clear();
    const auto empty_ir_outcome =
        gnc::compiler::build_canonical_mission_ir(empty, catalog);
    require(!empty_ir_outcome.value.has_value() &&
                has_diagnostic(
                    empty_ir_outcome.diagnostics,
                    DiagnosticCode::InvalidStaticCompositionSource),
            "fully empty typed source produced canonical Mission IR");
    const auto empty_compile_outcome =
        gnc::compiler::compile_static_plan(empty, catalog);
    require(!empty_compile_outcome.value.has_value() &&
                has_diagnostic(
                    empty_compile_outcome.diagnostics,
                    DiagnosticCode::InvalidStaticCompositionSource),
            "fully empty typed source produced a static plan");

    auto model_only = composition_source();
    model_only.algorithms.clear();
    model_only.bindings.clear();
    const auto model_only_ir_outcome =
        gnc::compiler::build_canonical_mission_ir(model_only, catalog);
    require(model_only_ir_outcome.succeeded(),
            "nonempty model-only source hit the fully empty-source guard");
    const auto model_only_compile_outcome =
        gnc::compiler::compile_static_plan(model_only, catalog);
    require(!model_only_compile_outcome.value.has_value() &&
                has_diagnostic(
                    model_only_compile_outcome.diagnostics,
                    DiagnosticCode::MissingRequiredBinding) &&
                !has_diagnostic(
                    model_only_compile_outcome.diagnostics,
                    DiagnosticCode::InvalidStaticCompositionSource),
            "partial source did not reach normal binding validation");

    auto missing_plan_identity = composition_source();
    missing_plan_identity.plan_id.clear();
    const auto mission_ir_without_plan =
        gnc::compiler::build_canonical_mission_ir(
            missing_plan_identity, catalog);
    require(mission_ir_without_plan.succeeded(),
            "descriptor plan identity contaminated canonical Mission IR");
    const auto missing_plan_outcome =
        gnc::compiler::compile_static_plan(
            missing_plan_identity, catalog);
    require(!missing_plan_outcome.value.has_value() &&
                has_diagnostic(
                    missing_plan_outcome.diagnostics,
                    DiagnosticCode::InvalidStaticCompositionSource),
            "static plan lowering accepted an empty plan identity");

    auto unknown = composition_source();
    unknown.models[0U].model_id += ".missing";
    const auto unknown_outcome =
        gnc::compiler::compile_static_plan(unknown, catalog);
    require(!unknown_outcome.value.has_value(),
            "unknown definition produced a static plan");
    const auto& unknown_diagnostic = require_diagnostic(
        unknown_outcome.diagnostics, DiagnosticCode::UnknownDefinition,
        "unknown definition diagnostic missing");
    require(unknown_diagnostic.source.node_path ==
                "/models/cavh.envelope",
            "unknown definition diagnostic lost its source path");

    auto missing = composition_source();
    missing.bindings.erase(missing.bindings.begin() + 1);
    const auto missing_outcome =
        gnc::compiler::compile_static_plan(missing, catalog);
    require(!missing_outcome.value.has_value() &&
                has_diagnostic(missing_outcome.diagnostics,
                               DiagnosticCode::MissingRequiredBinding),
            "missing closure consumer binding produced a static plan");

    auto duplicate_target = composition_source();
    auto second = duplicate_target.bindings[1U];
    second.binding_id = "yyz.second-closure-to-rigid";
    second.source = ref("/bindings/yyz.second-closure-to-rigid");
    duplicate_target.bindings.push_back(std::move(second));
    const auto duplicate_target_outcome =
        gnc::compiler::compile_static_plan(duplicate_target, catalog);
    require(!duplicate_target_outcome.value.has_value() &&
                has_diagnostic(
                    duplicate_target_outcome.diagnostics,
                    DiagnosticCode::MultipleRequiredBindings),
            "ambiguous rigid-step input produced a static plan");

    auto duplicate_occurrence = composition_source();
    duplicate_occurrence.algorithms[0U].occurrence_id =
        duplicate_occurrence.models[0U].occurrence_id;
    const auto duplicate_occurrence_outcome =
        gnc::compiler::compile_static_plan(duplicate_occurrence, catalog);
    require(!duplicate_occurrence_outcome.value.has_value() &&
                has_diagnostic(duplicate_occurrence_outcome.diagnostics,
                               DiagnosticCode::DuplicateOccurrence),
            "duplicate occurrence identity produced a static plan");

    auto incompatible_packages = package_descriptors();
    incompatible_packages[0U].algorithms[0U].ports[0U].contract_id =
        "gnc.contract.fixture.incompatible@1";
    const auto incompatible_catalog_outcome =
        Catalog::build(std::move(incompatible_packages));
    const auto& incompatible_catalog = require_value(
        incompatible_catalog_outcome,
        "incompatible-contract Catalog setup failed");
    const auto incompatible_outcome = gnc::compiler::compile_static_plan(
        composition_source(), incompatible_catalog);
    const auto& incompatible_diagnostic = require_diagnostic(
        incompatible_outcome.diagnostics, DiagnosticCode::ContractMismatch,
        "contract mismatch diagnostic missing");
    require(!incompatible_outcome.value.has_value() &&
                incompatible_diagnostic.source.node_path ==
                    "/bindings/cavh.envelope-to-formula",
            "contract mismatch produced a plan or lost its source path");

    auto duplicate_packages = package_descriptors();
    duplicate_packages.push_back(duplicate_packages[0U]);
    const auto duplicate_catalog_outcome =
        Catalog::build(std::move(duplicate_packages));
    require(!duplicate_catalog_outcome.value.has_value() &&
                has_diagnostic(duplicate_catalog_outcome.diagnostics,
                               DiagnosticCode::DuplicateCatalogIdentity),
            "duplicate package contribution produced a Catalog");

    auto model_input_packages = package_descriptors();
    model_input_packages[0U].models[0U].ports[0U].direction =
        StaticPortDirection::Input;
    const auto model_input_outcome =
        Catalog::build(std::move(model_input_packages));
    require(!model_input_outcome.value.has_value() &&
                has_diagnostic(model_input_outcome.diagnostics,
                               DiagnosticCode::InvalidCatalogDescriptor),
            "model Input entered the output-only static composition");

    auto algorithm_output_packages = package_descriptors();
    algorithm_output_packages[0U].algorithms[0U].ports[0U].direction =
        StaticPortDirection::Output;
    const auto algorithm_output_outcome =
        Catalog::build(std::move(algorithm_output_packages));
    require(!algorithm_output_outcome.value.has_value() &&
                has_diagnostic(
                    algorithm_output_outcome.diagnostics,
                    DiagnosticCode::InvalidCatalogDescriptor),
            "algorithm Output entered the input-only static composition");

    auto invalid_direction_packages = package_descriptors();
    invalid_direction_packages[0U].models[0U].ports[0U].direction =
        static_cast<StaticPortDirection>(255U);
    const auto invalid_direction_outcome =
        Catalog::build(std::move(invalid_direction_packages));
    require(!invalid_direction_outcome.value.has_value() &&
                has_diagnostic(
                    invalid_direction_outcome.diagnostics,
                    DiagnosticCode::InvalidCatalogDescriptor),
            "invalid port-direction enum entered the Catalog");

    auto invalid_form_packages = package_descriptors();
    invalid_form_packages[0U].models[0U].definition.execution_form =
        ModelExecutionForm::Unspecified;
    const auto invalid_form_outcome =
        Catalog::build(std::move(invalid_form_packages));
    require(!invalid_form_outcome.value.has_value() &&
                has_diagnostic(invalid_form_outcome.diagnostics,
                               DiagnosticCode::InvalidCatalogDescriptor),
            "invalid execution form entered the Catalog");
}

[[nodiscard]] std::string run_self_check() {
    const auto catalog_outcome = Catalog::build(package_descriptors());
    const auto& catalog =
        require_value(catalog_outcome, "fixture Catalog build failed");
    const auto source = composition_source();
    const auto ir_outcome =
        gnc::compiler::build_canonical_mission_ir(source, catalog);
    const CanonicalMissionIr& ir = require_value(
        ir_outcome, "canonical Mission IR build failed");
    const auto ir_explain =
        gnc::compiler::explain_canonical_mission_ir(ir);
    const std::string expected_ir =
        "mission-ir 1 mission "
        "mission.r2.yyz-cavh-static-composition@1\n"
        "model cavh.envelope "
        "gnc.package.cavh-formula.experimental@1@0.1.0 "
        "gnc.package.cavh.glide-envelope.parabolic.experimental@1@0.1.0 "
        "PureQuery preparation "
        "gnc.package.cavh.glide-envelope.prepare@1@0.1.0\n"
        "output cavh.envelope.envelope "
        "gnc.contract.cavh.glide-envelope-query-output@1\n"
        "model yyz.closure "
        "gnc.package.yyz-rigid-step.experimental@1@0.1.0 "
        "gnc.package.yyz.force-moment-closure.frozen-interval.experimental@1"
        "@0.1.0 Closure preparation "
        "gnc.package.yyz.force-moment-closure.prepare@1@0.1.0\n"
        "output yyz.closure.form-input "
        "gnc.contract.yyz.rigid-form-input@1\n"
        "algorithm cavh.formula "
        "gnc.package.cavh-formula.experimental@1@0.1.0 "
        "gnc.package.cavh.formula.composite@1@0.1.0\n"
        "input cavh.formula.glide-envelope "
        "gnc.contract.cavh.glide-envelope-query-output@1\n"
        "algorithm yyz.rigid-step "
        "gnc.package.yyz-rigid-step.experimental@1@0.1.0 "
        "gnc.package.yyz.rigid-step.kernel@1@0.1.0\n"
        "input yyz.rigid-step.form-input "
        "gnc.contract.yyz.rigid-form-input@1\n"
        "intent cavh.envelope-to-formula cavh.envelope.envelope -> "
        "cavh.formula.glide-envelope\n"
        "intent yyz.closure-to-rigid yyz.closure.form-input -> "
        "yyz.rigid-step.form-input\n";
    require(ir_explain == expected_ir,
            "canonical Mission IR golden changed");

    const auto compile_outcome =
        gnc::compiler::compile_static_plan(source, catalog);
    const auto& compilation = require_value(
        compile_outcome, "typed source compilation failed");
    verify_success_product(compilation);

    const auto explain =
        gnc::compiler::explain_static_plan(compilation.plan);
    const std::string expected =
        "plan plan.r2.yyz-cavh-static-composition@1 mission "
        "mission.r2.yyz-cavh-static-composition@1\n"
        "lock gnc.package.cavh-formula.experimental@1@0.1.0\n"
        "lock gnc.package.yyz-rigid-step.experimental@1@0.1.0\n"
        "model cavh.envelope "
        "gnc.package.cavh.glide-envelope.parabolic.experimental@1@0.1.0 "
        "PureQuery preparation "
        "gnc.package.cavh.glide-envelope.prepare@1@0.1.0\n"
        "model yyz.closure "
        "gnc.package.yyz.force-moment-closure.frozen-interval.experimental@1"
        "@0.1.0 Closure preparation "
        "gnc.package.yyz.force-moment-closure.prepare@1@0.1.0\n"
        "consumer cavh.formula "
        "gnc.package.cavh.formula.composite@1@0.1.0\n"
        "consumer yyz.rigid-step "
        "gnc.package.yyz.rigid-step.kernel@1@0.1.0\n"
        "bind cavh.envelope-to-formula cavh.envelope.envelope -> "
        "cavh.formula.glide-envelope "
        "gnc.contract.cavh.glide-envelope-query-output@1\n"
        "bind yyz.closure-to-rigid yyz.closure.form-input -> "
        "yyz.rigid-step.form-input gnc.contract.yyz.rigid-form-input@1\n"
        "prove proof.binding.cavh.envelope-to-formula "
        "GNC.PLAN.BINDING.CONTRACT.EXACT "
        "gnc.contract.cavh.glide-envelope-query-output@1\n"
        "prove proof.binding.yyz.closure-to-rigid "
        "GNC.PLAN.BINDING.CONTRACT.EXACT "
        "gnc.contract.yyz.rigid-form-input@1\n"
        "obligation 0 obligation.cavh.envelope-to-formula "
        "PureQueryEvaluation cavh.envelope -> cavh.formula\n"
        "obligation 1 obligation.yyz.closure-to-rigid "
        "ClosureEvaluation yyz.closure -> yyz.rigid-step\n";
    require(explain == expected, "static dry-run explain changed");
    require(gnc::compiler::explain_canonical_mission_ir(compilation.ir) ==
                ir_explain,
            "static plan did not consume the standalone canonical IR result");
    verify_deterministic_order(ir_explain, explain);
    verify_negative_cases();
    return explain;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2 || (std::string_view(argv[1]) != "--self-check" &&
                          std::string_view(argv[1]) != "--explain")) {
            std::cerr << "usage: gnc_compiler_static_plan_probe "
                         "--self-check|--explain\n";
            return 2;
        }
        const auto explain = run_self_check();
        if (std::string_view(argv[1]) == "--explain") {
            std::cout << explain;
        } else {
            std::cout << "R2 typed static composition self-check passed\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "R2 static plan self-check failed: " << error.what()
                  << '\n';
        return 1;
    }
}
