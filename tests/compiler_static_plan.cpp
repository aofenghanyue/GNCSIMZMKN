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
using gnc::compiler::EntityLifecycle;
using gnc::compiler::ExecutionPlanDescriptor;
using gnc::compiler::SourceAlgorithmConsumer;
using gnc::compiler::SourceBinding;
using gnc::compiler::SourceEntity;
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
constexpr std::string_view kYyzQualificationMissionId =
    "mission.fixture.yyz.lookup-altitude-hold@1";
constexpr std::string_view kYyzQualificationSubject =
    "vehicle.fixture.yyz@1";
constexpr std::string_view kYyzQualificationSourceUri =
    "repo://fixtures/ref-yyz-001/source.json";
constexpr std::string_view kYyzQualificationAssetIndexUri =
    "repo://fixtures/ref-yyz-001/asset-index.json";

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

template <typename Value, typename = void>
struct has_model_id_member : std::false_type {};

template <typename Value>
struct has_model_id_member<
    Value, std::void_t<decltype(std::declval<Value>().model_id)>>
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
static_assert(
    !has_model_id_member<gnc::compiler::CanonicalAlgorithmConsumer>::value,
    "a kernel binding consumer must not become a model occurrence");

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
    source.mission_source = ref("/mission_id");
    source.plan_id = std::string(kPlanId);
    source.model_occurrences = {
        SourceModelOccurrence{
            "cavh.envelope",
            std::string(
                gnc::packages::cavh::kGlideEnvelopeModelIdentity),
            std::string(
                gnc::packages::cavh::kGlideEnvelopeModelVersion),
            ref("/models/cavh.envelope"), {}, {}},
        SourceModelOccurrence{
            "yyz.closure",
            std::string(
                gnc::packages::yyz::kForceMomentClosureModelIdentity),
            std::string(
                gnc::packages::yyz::kForceMomentClosureModelVersion),
            ref("/models/yyz.closure"), {}, {}},
    };
    source.algorithm_consumers = {
        SourceAlgorithmConsumer{
            "cavh.formula",
            std::string(
                gnc::packages::cavh::kCavhFormulaKernelIdentity.id),
            std::string(
                gnc::packages::cavh::kCavhFormulaKernelIdentity.version),
            ref("/algorithms/cavh.formula")},
        SourceAlgorithmConsumer{
            "yyz.rigid-step",
            std::string(gnc::packages::yyz::kRigidStepKernelIdentity.id),
            std::string(
                gnc::packages::yyz::kRigidStepKernelIdentity.version),
            ref("/algorithms/yyz.rigid-step")},
    };
    source.binding_intents = {
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

// Programmatic projection of accepted REF-YYZ-001 qualification facts. The
// occurrence id reuses the exact component role in asset-index.json; the
// product ModelDefinition identity is resolved from the YYZ Catalog.
[[nodiscard]] TypedStaticCompositionSource yyz_qualification_source() {
    TypedStaticCompositionSource source;
    source.source_version = std::string(
        gnc::compiler::kTypedStaticCompositionSourceVersion);
    source.mission_id = std::string(kYyzQualificationMissionId);
    source.mission_source = {
        std::string(kYyzQualificationSourceUri), "/source_id"};
    source.entities = {
        SourceEntity{
            std::string(kYyzQualificationSubject),
            EntityLifecycle::ActiveAtInitialize,
            {std::string(kYyzQualificationSourceUri),
             "/profiles/qualification/vehicle/subject"},
            {std::string(kYyzQualificationSourceUri),
             "/profiles/qualification/vehicle/lifecycle"}},
    };
    source.model_occurrences = {
        SourceModelOccurrence{
            "force_moment_closure",
            std::string(
                gnc::packages::yyz::kForceMomentClosureModelIdentity),
            std::string(
                gnc::packages::yyz::kForceMomentClosureModelVersion),
            {std::string(kYyzQualificationAssetIndexUri),
             "/component_bindings/1/role"},
            std::string(kYyzQualificationSubject),
            {std::string(kYyzQualificationSourceUri),
             "/profiles/qualification/vehicle/subject"}},
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

[[nodiscard]] std::string verify_yyz_entity_subject_slice(
    const Catalog& catalog) {
    const auto source = yyz_qualification_source();
    const auto ir_outcome =
        gnc::compiler::build_canonical_mission_ir(source, catalog);
    const auto& ir = require_value(
        ir_outcome, "YYZ qualification canonical IR build failed");

    require(ir.mission_id == kYyzQualificationMissionId &&
                ir.mission_source.document_uri ==
                    kYyzQualificationSourceUri &&
                ir.mission_source.node_path == "/source_id",
            "YYZ mission/source identity lost REF-YYZ-001 provenance");
    require(ir.entities.size() == 1U &&
                ir.entities[0U].entity_id == kYyzQualificationSubject &&
                ir.entities[0U].lifecycle ==
                    EntityLifecycle::ActiveAtInitialize &&
                ir.entities[0U].identity_source.node_path ==
                    "/profiles/qualification/vehicle/subject" &&
                ir.entities[0U].lifecycle_source.node_path ==
                    "/profiles/qualification/vehicle/lifecycle",
            "YYZ entity identity or initial lifecycle changed");
    require(ir.model_occurrences.size() == 1U &&
                ir.algorithm_consumers.empty() &&
                ir.binding_intents.empty(),
            "YYZ entity slice promoted a kernel consumer to a model "
            "occurrence");
    const auto& closure = ir.model_occurrences[0U];
    require(closure.occurrence_id == "force_moment_closure" &&
                closure.model_id ==
                    gnc::packages::yyz::
                        kForceMomentClosureModelIdentity &&
                closure.execution_form == ModelExecutionForm::Closure &&
                closure.subject_entity_id ==
                    kYyzQualificationSubject &&
                closure.source.document_uri ==
                    kYyzQualificationAssetIndexUri &&
                closure.source.node_path ==
                    "/component_bindings/1/role" &&
                closure.subject_source.document_uri ==
                    kYyzQualificationSourceUri &&
                closure.subject_source.node_path ==
                    "/profiles/qualification/vehicle/subject",
            "YYZ closure occurrence lost its real definition, role, or "
            "subject relation");

    const std::string expected =
        "mission-ir 1 mission "
        "mission.fixture.yyz.lookup-altitude-hold@1\n"
        "entity vehicle.fixture.yyz@1 active_at_initialize\n"
        "model force_moment_closure "
        "gnc.package.yyz-rigid-step.experimental@1@0.1.0 "
        "gnc.package.yyz.force-moment-closure.frozen-interval.experimental@1"
        "@0.1.0 Closure preparation "
        "gnc.package.yyz.force-moment-closure.prepare@1@0.1.0 "
        "subject vehicle.fixture.yyz@1\n"
        "output force_moment_closure.form-input "
        "gnc.contract.yyz.rigid-form-input@1\n";
    const auto explain =
        gnc::compiler::explain_canonical_mission_ir(ir);
    require(explain == expected,
            "YYZ entity/subject canonical IR golden changed");

    auto relocated = source;
    std::reverse(relocated.entities.begin(), relocated.entities.end());
    std::reverse(relocated.model_occurrences.begin(),
                 relocated.model_occurrences.end());
    relocated.mission_source =
        {"repo://relocated/qualification.json", "/mission"};
    relocated.entities[0U].identity_source =
        {"repo://relocated/qualification.json", "/entities/0/id"};
    relocated.entities[0U].lifecycle_source =
        {"repo://relocated/qualification.json", "/entities/0/lifecycle"};
    relocated.model_occurrences[0U].source =
        {"repo://relocated/assets.json", "/models/0"};
    relocated.model_occurrences[0U].subject_source =
        {"repo://relocated/qualification.json", "/models/0/subject"};
    const auto relocated_outcome =
        gnc::compiler::build_canonical_mission_ir(relocated, catalog);
    const auto& relocated_ir = require_value(
        relocated_outcome, "relocated YYZ canonical IR build failed");
    require(gnc::compiler::explain_canonical_mission_ir(relocated_ir) ==
                expected &&
                relocated_ir.entities[0U]
                        .lifecycle_source.document_uri ==
                    "repo://relocated/qualification.json" &&
                relocated_ir.model_occurrences[0U]
                        .subject_source.node_path == "/models/0/subject",
            "YYZ source order/location changed semantics or lost "
            "provenance");
    return explain;
}

void verify_yyz_entity_subject_negative_cases(const Catalog& catalog) {
    auto empty_entity_identity = yyz_qualification_source();
    empty_entity_identity.entities[0U].entity_id.clear();
    const auto empty_entity_outcome =
        gnc::compiler::build_canonical_mission_ir(
            empty_entity_identity, catalog);
    const auto& empty_entity_diagnostic = require_diagnostic(
        empty_entity_outcome.diagnostics, DiagnosticCode::InvalidEntity,
        "empty YYZ entity identity diagnostic missing");
    require(!empty_entity_outcome.value.has_value() &&
                empty_entity_diagnostic.source.node_path ==
                    "/profiles/qualification/vehicle/subject",
            "empty YYZ entity identity entered IR or lost its source path");

    auto duplicate_entity = yyz_qualification_source();
    auto duplicate = duplicate_entity.entities[0U];
    duplicate.identity_source.node_path =
        "/profiles/qualification/vehicle/duplicate-subject";
    duplicate_entity.entities.push_back(std::move(duplicate));
    const auto duplicate_outcome =
        gnc::compiler::build_canonical_mission_ir(
            duplicate_entity, catalog);
    const auto& duplicate_diagnostic = require_diagnostic(
        duplicate_outcome.diagnostics, DiagnosticCode::DuplicateEntity,
        "duplicate YYZ entity diagnostic missing");
    require(!duplicate_outcome.value.has_value() &&
                duplicate_diagnostic.subject ==
                    kYyzQualificationSubject &&
                duplicate_diagnostic.source.node_path ==
                    "/profiles/qualification/vehicle/duplicate-subject",
            "duplicate YYZ entity entered IR or lost its source path");

    auto unknown_subject = yyz_qualification_source();
    unknown_subject.model_occurrences[0U].subject_entity_id =
        "vehicle.fixture.unknown@1";
    const auto unknown_outcome =
        gnc::compiler::build_canonical_mission_ir(
            unknown_subject, catalog);
    const auto& unknown_diagnostic = require_diagnostic(
        unknown_outcome.diagnostics,
        DiagnosticCode::UnknownSubjectEntity,
        "unknown YYZ subject diagnostic missing");
    require(!unknown_outcome.value.has_value() &&
                unknown_diagnostic.subject ==
                    "force_moment_closure" &&
                unknown_diagnostic.source.node_path ==
                    "/profiles/qualification/vehicle/subject",
            "unknown YYZ subject entered IR or lost its source path");
}

void verify_success_product(const StaticCompilation& compilation) {
    const auto& ir = compilation.ir;
    const auto& plan = compilation.plan;
    require(ir.revision == 1U && ir.mission_id == kMissionId,
            "typed source did not produce the expected minimal IR");
    require(ir.entities.empty() &&
                ir.model_occurrences.size() == 2U &&
                ir.algorithm_consumers.size() == 2U &&
                ir.binding_intents.size() == 2U,
            "minimal IR occurrence or binding count changed");
    require(ir.model_occurrences[0U].occurrence_id == "cavh.envelope" &&
                ir.model_occurrences[0U].model_id ==
                    gnc::packages::cavh::kGlideEnvelopeModelIdentity &&
                ir.model_occurrences[0U].output_ports.size() == 1U &&
                ir.model_occurrences[0U].output_ports[0U].contract_id ==
                    gnc::packages::cavh::
                        kGlideEnvelopeOutputContractIdentity &&
                ir.model_occurrences[1U].occurrence_id == "yyz.closure" &&
                ir.model_occurrences[1U].model_id ==
                    gnc::packages::yyz::
                        kForceMomentClosureModelIdentity &&
                ir.model_occurrences[1U].output_ports.size() == 1U &&
                ir.model_occurrences[1U].output_ports[0U].contract_id ==
                    gnc::packages::yyz::kRigidFormInputContractIdentity,
            "canonical IR lost exact model or output identities");
    require(ir.algorithm_consumers[0U].consumer_id == "cavh.formula" &&
                ir.algorithm_consumers[0U].input_ports.size() == 1U &&
                ir.algorithm_consumers[0U].input_ports[0U].contract_id ==
                    gnc::packages::cavh::
                        kGlideEnvelopeOutputContractIdentity &&
                ir.algorithm_consumers[1U].consumer_id ==
                    "yyz.rigid-step" &&
                ir.algorithm_consumers[1U].input_ports.size() == 1U &&
                ir.algorithm_consumers[1U]
                        .input_ports[0U]
                        .contract_id ==
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
                plan.obligations[0U].consumer_id ==
                    "cavh.formula" &&
                plan.obligations[1U].kind ==
                    CompiledObligationKind::ClosureEvaluation &&
                plan.obligations[1U].consumer_id ==
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
    std::reverse(source.model_occurrences.begin(),
                 source.model_occurrences.end());
    std::reverse(source.algorithm_consumers.begin(),
                 source.algorithm_consumers.end());
    std::reverse(source.binding_intents.begin(),
                 source.binding_intents.end());
    for (std::size_t index = 0U;
         index < source.model_occurrences.size(); ++index) {
        source.model_occurrences[index].source =
            {"typed://alternate/source.yaml",
             "/model_occurrences/" + std::to_string(index)};
    }
    for (std::size_t index = 0U;
         index < source.algorithm_consumers.size(); ++index) {
        source.algorithm_consumers[index].source =
            {"typed://alternate/source.yaml",
             "/algorithm_consumers/" + std::to_string(index)};
    }
    for (std::size_t index = 0U;
         index < source.binding_intents.size(); ++index) {
        source.binding_intents[index].source =
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
    empty.model_occurrences.clear();
    empty.algorithm_consumers.clear();
    empty.binding_intents.clear();
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
    model_only.algorithm_consumers.clear();
    model_only.binding_intents.clear();
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
    unknown.model_occurrences[0U].model_id += ".missing";
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
    missing.binding_intents.erase(
        missing.binding_intents.begin() + 1);
    const auto missing_outcome =
        gnc::compiler::compile_static_plan(missing, catalog);
    require(!missing_outcome.value.has_value() &&
                has_diagnostic(missing_outcome.diagnostics,
                               DiagnosticCode::MissingRequiredBinding),
            "missing closure consumer binding produced a static plan");

    auto duplicate_target = composition_source();
    auto second = duplicate_target.binding_intents[1U];
    second.binding_id = "yyz.second-closure-to-rigid";
    second.source = ref("/bindings/yyz.second-closure-to-rigid");
    duplicate_target.binding_intents.push_back(std::move(second));
    const auto duplicate_target_outcome =
        gnc::compiler::compile_static_plan(duplicate_target, catalog);
    require(!duplicate_target_outcome.value.has_value() &&
                has_diagnostic(
                    duplicate_target_outcome.diagnostics,
                    DiagnosticCode::MultipleRequiredBindings),
            "ambiguous rigid-step input produced a static plan");

    auto duplicate_occurrence = composition_source();
    duplicate_occurrence.algorithm_consumers[0U].consumer_id =
        duplicate_occurrence.model_occurrences[0U].occurrence_id;
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
    static_cast<void>(verify_yyz_entity_subject_slice(catalog));
    verify_yyz_entity_subject_negative_cases(catalog);
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
        "algorithm-consumer cavh.formula "
        "gnc.package.cavh-formula.experimental@1@0.1.0 "
        "gnc.package.cavh.formula.composite@1@0.1.0\n"
        "input cavh.formula.glide-envelope "
        "gnc.contract.cavh.glide-envelope-query-output@1\n"
        "algorithm-consumer yyz.rigid-step "
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
