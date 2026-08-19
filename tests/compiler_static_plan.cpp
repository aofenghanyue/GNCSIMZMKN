#include <cavh/formula.hpp>
#include <gnc/compiler/canonical_semantic_hash.hpp>
#include <gnc/compiler/static_mission_compiler.hpp>
#include <yyz/rigid_step.hpp>

#include <algorithm>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using gnc::compiler::Catalog;
using gnc::compiler::BindingEndpointKind;
using gnc::compiler::BindingPhase;
using gnc::compiler::BindingProofAssertion;
using gnc::compiler::CanonicalMissionIr;
using gnc::compiler::CompileOutcome;
using gnc::compiler::CompiledObligationKind;
using gnc::compiler::Diagnostic;
using gnc::compiler::DiagnosticCode;
using gnc::compiler::EntityLifecycle;
using gnc::compiler::ExecutionPlanDescriptor;
using gnc::compiler::ScopeKey;
using gnc::compiler::ScopeKind;
using gnc::compiler::SourceAlgorithmConsumer;
using gnc::compiler::SourceAssetBinding;
using gnc::compiler::SourceBinding;
using gnc::compiler::SourceConfigFieldProvenance;
using gnc::compiler::SourceEntity;
using gnc::compiler::SourceModelOccurrence;
using gnc::compiler::SourceRef;
using gnc::compiler::SourceScope;
using gnc::compiler::StaticCompilation;
using gnc::compiler::TypedStaticCompositionSource;
using gnc::model_sdk::ModelExecutionForm;
using gnc::model_sdk::ModelPlacement;
using gnc::model_sdk::BindingKind;
using gnc::model_sdk::PortCardinality;
using gnc::model_sdk::StaticAlgorithmDescriptor;
using gnc::model_sdk::StaticPortDescriptor;
using gnc::model_sdk::StaticPortDirection;
using gnc::model_sdk::TemporalRelation;

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
static_assert(
    std::is_same_v<decltype(StaticPortDescriptor::binding_kind),
                   BindingKind> &&
        std::is_same_v<decltype(StaticPortDescriptor::cardinality),
                       PortCardinality> &&
        std::is_same_v<decltype(StaticPortDescriptor::temporal_relation),
                       TemporalRelation>,
    "package ports must expose typed binding semantics");
static_assert(
    std::is_same_v<decltype(ExecutionPlanDescriptor::binding_plan),
                   gnc::compiler::BindingPlan> &&
        std::is_same_v<
            decltype(ExecutionPlanDescriptor::temporal_binding_plan),
            gnc::compiler::TemporalBindingPlan>,
    "the execution descriptor must own typed binding plans");

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

void attach_configuration(
    SourceModelOccurrence& occurrence,
    gnc::model_sdk::CanonicalConfigBlock configuration,
    SourceRef schema_source, std::string field_document_uri,
    std::string field_path_prefix) {
    occurrence.configuration = std::move(configuration);
    occurrence.configuration_source = std::move(schema_source);
    for (const auto& field : occurrence.configuration.fields) {
        occurrence.configuration_field_sources.push_back(
            {field.field_id,
             {field_document_uri,
              field_path_prefix + "/" + field.field_id}});
    }
}

void set_field_source(SourceModelOccurrence& occurrence,
                      std::string_view field_id, SourceRef source) {
    const auto found = std::find_if(
        occurrence.configuration_field_sources.begin(),
        occurrence.configuration_field_sources.end(),
        [field_id](const SourceConfigFieldProvenance& value) {
            return value.field_id == field_id;
        });
    require(found != occurrence.configuration_field_sources.end(),
            "configuration provenance field is absent");
    found->source = std::move(source);
}

[[nodiscard]] gnc::packages::cavh::CavhFormulaDefinition
cavh_definition() {
    gnc::packages::cavh::CavhFormulaDefinition definition;
    definition.envelope.metadata = {
        std::string(gnc::packages::cavh::kGlideEnvelopeModelIdentity),
        std::string(gnc::packages::cavh::kGlideEnvelopeModelVersion),
        ModelExecutionForm::PureQuery};
    definition.envelope.polar = {0.0, 2.0, 0.02, 0.0,
                                 0.08, 0.0, 0.5};
    return definition;
}

[[nodiscard]] gnc::packages::yyz::RigidStepModelDefinition yyz_definition() {
    gnc::packages::yyz::RigidStepModelDefinition definition;
    definition.force_moment_closure.metadata = {
        std::string(gnc::packages::yyz::kForceMomentClosureModelIdentity),
        std::string(
            gnc::packages::yyz::kForceMomentClosureModelVersion),
        ModelExecutionForm::Closure};
    definition.force_moment_closure.body_frame.id =
        "frame.fixture.yyz.body@1";
    definition.force_moment_closure.clock_domain.id =
        "clock.fixture.yyz.simulation@1";
    definition.force_moment_closure.configuration_revision = 11;
    definition.force_moment_closure.numerical_policy = {
        2.0e-12, 2.0e-12,
        gnc::foundation::FiniteCheck::EveryStage, 1.0e-14, 1.0e12};
    definition.aerodynamics.metadata = {
        std::string(gnc::packages::yyz::kAerodynamicTableModelIdentity),
        std::string(gnc::packages::yyz::kAerodynamicTableModelVersion),
        ModelExecutionForm::PureQuery};
    definition.aerodynamics.source_id = "aero.body";
    definition.aerodynamics.configuration_id =
        "configuration.fixture.yyz.clean@1";
    definition.aerodynamics.reference_area_square_meters = 1.0;
    definition.aerodynamics.reference_span_meters = 1.0;
    definition.aerodynamics.reference_chord_meters = 1.0;
    definition.aerodynamics.body_origin_to_application.value =
        gnc::foundation::Vec3{0.2, 0.0, -25.0 / 18.0};
    definition.aerodynamics.table_asset_id =
        "aero-table.fixture.yyz.multiaffine@1";
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
    const auto find_model = [](const auto& package,
                               std::string_view model_id)
        -> const gnc::model_sdk::StaticModelDescriptor& {
        const auto found = std::find_if(
            package.models.begin(), package.models.end(),
            [model_id](const auto& model) {
                return model.definition.model_id == model_id;
            });
        require(found != package.models.end(),
                "package descriptor omitted a real model");
        return *found;
    };
    const auto& cavh_envelope = find_model(
        cavh_package,
        gnc::packages::cavh::kGlideEnvelopeModelIdentity);
    const auto& yyz_closure = find_model(
        yyz_package,
        gnc::packages::yyz::kForceMomentClosureModelIdentity);
    const auto& yyz_aero = find_model(
        yyz_package,
        gnc::packages::yyz::kAerodynamicTableModelIdentity);
    require(cavh_envelope.definition.model_id ==
                cavh.envelope.metadata.model_id &&
                cavh_envelope.definition.model_version ==
                    cavh.envelope.metadata.model_version &&
                cavh_envelope.definition.execution_form ==
                    cavh.envelope.metadata.execution_form &&
                cavh_envelope.placement == ModelPlacement::VehicleOutput &&
                cavh_envelope.ports[0U].binding_kind ==
                    BindingKind::PureQuery &&
                cavh_envelope.ports[0U].cardinality ==
                    PortCardinality::OneOrMore,
            "CAVH package descriptor diverged from the real definition");
    require(yyz_closure.definition.model_id ==
                yyz.force_moment_closure.metadata.model_id &&
                yyz_closure.definition.model_version ==
                    yyz.force_moment_closure.metadata.model_version &&
                yyz_closure.definition.execution_form ==
                    yyz.force_moment_closure.metadata.execution_form &&
                yyz_closure.placement ==
                    ModelPlacement::InteractionClosure &&
                yyz_aero.definition.model_id ==
                    yyz.aerodynamics.metadata.model_id &&
                yyz_aero.definition.execution_form ==
                    ModelExecutionForm::PureQuery &&
                yyz_aero.placement == ModelPlacement::VehicleOutput &&
                yyz_aero.asset_slots.size() == 1U &&
                yyz_aero.asset_slots[0U].asset_schema_id ==
                    gnc::packages::yyz::
                        kAerodynamicTableAssetSchemaIdentity &&
                yyz_aero.asset_slots[0U].cardinality ==
                    PortCardinality::ExactlyOne &&
                yyz_aero.ports[0U].binding_kind ==
                    BindingKind::PureQuery &&
                yyz_closure.ports[0U].binding_kind ==
                    BindingKind::ContinuousClosureLink &&
                yyz_closure.ports[0U].temporal_relation ==
                    TemporalRelation::IntervalModel,
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
    const ScopeKey yyz_vehicle_scope{
        ScopeKind::Vehicle, std::string(kYyzQualificationSubject)};
    source.entities = {
        SourceEntity{
            std::string(kYyzQualificationSubject),
            EntityLifecycle::ActiveAtInitialize,
            ref("/entities/vehicle.fixture.yyz@1/id"),
            ref("/entities/vehicle.fixture.yyz@1/lifecycle")},
    };
    source.scopes = {
        SourceScope{yyz_vehicle_scope,
                    ref("/scopes/vehicle.fixture.yyz@1")},
    };
    source.model_occurrences = {
        SourceModelOccurrence{
            "cavh.envelope",
            std::string(
                gnc::packages::cavh::kGlideEnvelopeModelIdentity),
            std::string(
                gnc::packages::cavh::kGlideEnvelopeModelVersion),
            ref("/models/cavh.envelope"), {}, {}},
        SourceModelOccurrence{
            "yyz.aerodynamics",
            std::string(
                gnc::packages::yyz::kAerodynamicTableModelIdentity),
            std::string(
                gnc::packages::yyz::kAerodynamicTableModelVersion),
            ref("/models/yyz.aerodynamics"), {}, {}},
        SourceModelOccurrence{
            "yyz.closure",
            std::string(
                gnc::packages::yyz::kForceMomentClosureModelIdentity),
            std::string(
                gnc::packages::yyz::kForceMomentClosureModelVersion),
            ref("/models/yyz.closure"), {}, {}},
    };
    const auto cavh = cavh_definition();
    const auto yyz = yyz_definition();
    attach_configuration(
        source.model_occurrences[0U],
        gnc::packages::cavh::canonical_glide_envelope_config(
            cavh.envelope),
        ref("/models/cavh.envelope/configuration/schema"),
        std::string(kDocumentUri),
        "/models/cavh.envelope/configuration/fields");
    attach_configuration(
        source.model_occurrences[1U],
        gnc::packages::yyz::canonical_aerodynamic_table_config(
            yyz.aerodynamics),
        ref("/models/yyz.aerodynamics/configuration/schema"),
        std::string(kDocumentUri),
        "/models/yyz.aerodynamics/configuration/fields");
    source.model_occurrences[1U].placement =
        ModelPlacement::VehicleOutput;
    source.model_occurrences[1U].placement_source =
        ref("/models/yyz.aerodynamics/placement");
    source.model_occurrences[1U].asset_bindings = {
        SourceAssetBinding{
            "aerodynamics",
            std::string(gnc::packages::yyz::
                            kAerodynamicTableAssetSchemaIdentity),
            yyz.aerodynamics.table_asset_id,
            ref("/models/yyz.aerodynamics/assets/aerodynamics")},
    };
    source.model_occurrences[1U].subject_entity_id =
        std::string(kYyzQualificationSubject);
    source.model_occurrences[1U].subject_source =
        ref("/models/yyz.aerodynamics/subject");
    source.model_occurrences[1U].scope = yyz_vehicle_scope;
    source.model_occurrences[1U].scope_source =
        ref("/models/yyz.aerodynamics/scope");
    attach_configuration(
        source.model_occurrences[2U],
        gnc::packages::yyz::canonical_force_moment_closure_config(
            yyz.force_moment_closure),
        ref("/models/yyz.closure/configuration/schema"),
        std::string(kDocumentUri),
        "/models/yyz.closure/configuration/fields");
    source.model_occurrences[2U].placement =
        ModelPlacement::InteractionClosure;
    source.model_occurrences[2U].placement_source =
        ref("/models/yyz.closure/placement");
    source.model_occurrences[2U].subject_entity_id =
        std::string(kYyzQualificationSubject);
    source.model_occurrences[2U].subject_source =
        ref("/models/yyz.closure/subject");
    source.model_occurrences[2U].scope = yyz_vehicle_scope;
    source.model_occurrences[2U].scope_source =
        ref("/models/yyz.closure/scope");
    source.algorithm_consumers = {
        SourceAlgorithmConsumer{
            "cavh.formula",
            std::string(
                gnc::packages::cavh::kCavhFormulaKernelIdentity.id),
            std::string(
                gnc::packages::cavh::kCavhFormulaKernelIdentity.version),
            ref("/algorithms/cavh.formula"), std::nullopt, {}},
        SourceAlgorithmConsumer{
            "yyz.rigid-step",
            std::string(gnc::packages::yyz::kRigidStepKernelIdentity.id),
            std::string(
                gnc::packages::yyz::kRigidStepKernelIdentity.version),
            ref("/algorithms/yyz.rigid-step"), yyz_vehicle_scope,
            ref("/algorithms/yyz.rigid-step/scope")},
    };
    source.binding_intents = {
        SourceBinding{
            "cavh.envelope-to-formula", "cavh.envelope", "envelope",
            "cavh.formula", "glide-envelope",
            ref("/bindings/cavh.envelope-to-formula")},
        SourceBinding{
            "yyz.aero-to-rigid", "yyz.aerodynamics", "coefficients",
            "yyz.rigid-step", "aerodynamic-coefficients",
            ref("/bindings/yyz.aero-to-rigid")},
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
    source.plan_id = "plan.fixture.yyz.lookup-altitude-hold@1";
    source.entities = {
        SourceEntity{
            std::string(kYyzQualificationSubject),
            EntityLifecycle::ActiveAtInitialize,
            {std::string(kYyzQualificationSourceUri),
             "/profiles/qualification/vehicle/subject"},
            {std::string(kYyzQualificationSourceUri),
             "/profiles/qualification/vehicle/lifecycle"}},
    };
    const ScopeKey vehicle_scope{
        ScopeKind::Vehicle, std::string(kYyzQualificationSubject)};
    source.scopes = {
        SourceScope{
            vehicle_scope,
            {std::string(kYyzQualificationSourceUri),
             "/profiles/qualification/vehicle/subject"}},
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
        SourceModelOccurrence{
            "aero_lookup",
            std::string(
                gnc::packages::yyz::kAerodynamicTableModelIdentity),
            std::string(
                gnc::packages::yyz::kAerodynamicTableModelVersion),
            {std::string(kYyzQualificationAssetIndexUri),
             "/component_bindings/4/role"},
            std::string(kYyzQualificationSubject),
            {std::string(kYyzQualificationSourceUri),
             "/profiles/qualification/vehicle/subject"}},
    };
    const auto yyz = yyz_definition();
    auto& closure = source.model_occurrences[0U];
    closure.scope = vehicle_scope;
    closure.scope_source = {
        std::string(kYyzQualificationSourceUri),
        "/profiles/qualification/vehicle/subject"};
    closure.placement = ModelPlacement::InteractionClosure;
    closure.placement_source = {
        std::string(kYyzQualificationAssetIndexUri),
        "/component_bindings/1/role"};
    attach_configuration(
        closure,
        gnc::packages::yyz::canonical_force_moment_closure_config(
            yyz.force_moment_closure),
        {"package://gnc.package.yyz-rigid-step.experimental@1",
         "/schemas/force-moment-closure-config/1"},
        "package://gnc.package.yyz-rigid-step.experimental@1",
        "/defaults/force-moment-closure");
    set_field_source(
        closure, "body_frame_id",
        {std::string(kYyzQualificationSourceUri),
         "/profiles/qualification/vehicle/body_frame_id"});
    set_field_source(
        closure, "clock_domain_id",
        {std::string(kYyzQualificationSourceUri),
         "/profiles/qualification/clock/clock_domain"});
    set_field_source(
        closure, "configuration_revision",
        {std::string(kYyzQualificationSourceUri),
         "/profiles/qualification/vehicle/configuration_revision"});

    auto& aero = source.model_occurrences[1U];
    aero.scope = vehicle_scope;
    aero.scope_source = {
        std::string(kYyzQualificationSourceUri),
        "/profiles/qualification/vehicle/subject"};
    aero.placement = ModelPlacement::VehicleOutput;
    aero.placement_source = {
        std::string(kYyzQualificationAssetIndexUri),
        "/component_bindings/4/role"};
    attach_configuration(
        aero,
        gnc::packages::yyz::canonical_aerodynamic_table_config(
            yyz.aerodynamics),
        {"package://gnc.package.yyz-rigid-step.experimental@1",
         "/schemas/aerodynamic-table-config/1"},
        std::string(kYyzQualificationAssetIndexUri),
        "/selected_assets/2/payload");
    set_field_source(
        aero, "body_origin_to_application.x_m",
        {std::string(kYyzQualificationAssetIndexUri),
         "/selected_assets/2/payload/"
         "r_body_origin_to_application_B_m/0"});
    set_field_source(
        aero, "body_origin_to_application.y_m",
        {std::string(kYyzQualificationAssetIndexUri),
         "/selected_assets/2/payload/"
         "r_body_origin_to_application_B_m/1"});
    set_field_source(
        aero, "body_origin_to_application.z_m",
        {std::string(kYyzQualificationAssetIndexUri),
         "/selected_assets/2/payload/"
         "r_body_origin_to_application_B_m/2"});
    set_field_source(
        aero, "configuration_id",
        {std::string(kYyzQualificationAssetIndexUri),
         "/selected_assets/2/payload/configuration_id"});
    set_field_source(
        aero, "reference_area_square_meters",
        {std::string(kYyzQualificationAssetIndexUri),
         "/selected_assets/2/payload/reference_area_m2"});
    set_field_source(
        aero, "reference_chord_meters",
        {std::string(kYyzQualificationAssetIndexUri),
         "/selected_assets/2/payload/reference_chord_m"});
    set_field_source(
        aero, "reference_span_meters",
        {std::string(kYyzQualificationAssetIndexUri),
         "/selected_assets/2/payload/reference_span_m"});
    set_field_source(
        aero, "source_id",
        {"package://gnc.package.yyz-rigid-step.experimental@1",
         "/defaults/aerodynamic-table/source_id"});
    aero.asset_bindings = {
        SourceAssetBinding{
            "aerodynamics",
            std::string(gnc::packages::yyz::
                            kAerodynamicTableAssetSchemaIdentity),
            yyz.aerodynamics.table_asset_id,
            {std::string(kYyzQualificationAssetIndexUri),
             "/selected_assets/2/asset_id"}},
    };
    source.algorithm_consumers = {
        SourceAlgorithmConsumer{
            "yyz.rigid-step",
            std::string(gnc::packages::yyz::kRigidStepKernelIdentity.id),
            std::string(
                gnc::packages::yyz::kRigidStepKernelIdentity.version),
            {"repo://packages/yyz-rigid-step/src/rigid_step.cpp",
             "/RigidStepKernel/evaluate"},
            vehicle_scope,
            {std::string(kYyzQualificationSourceUri),
             "/profiles/qualification/vehicle/subject"}},
    };
    source.binding_intents = {
        SourceBinding{
            "yyz.aero-to-rigid", "aero_lookup", "coefficients",
            "yyz.rigid-step", "aerodynamic-coefficients",
            {std::string(kYyzQualificationAssetIndexUri),
             "/component_bindings/4/role"}},
        SourceBinding{
            "yyz.closure-to-rigid", "force_moment_closure",
            "form-input", "yyz.rigid-step", "form-input",
            {std::string(kYyzQualificationAssetIndexUri),
             "/selected_assets/5/payload/integration_strategy"}},
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
    require(ir.scopes.size() == 1U &&
                ir.scopes[0U].key.kind == ScopeKind::Vehicle &&
                ir.scopes[0U].key.subject_entity_id ==
                    kYyzQualificationSubject &&
                ir.model_occurrences.size() == 2U &&
                ir.algorithm_consumers.size() == 1U &&
                ir.binding_intents.size() == 2U &&
                ir.algorithm_consumers[0U].consumer_id ==
                    "yyz.rigid-step" &&
                ir.algorithm_consumers[0U].scope.has_value() &&
                *ir.algorithm_consumers[0U].scope == ir.scopes[0U].key,
            "YYZ canonical entity/scope/model partition changed");
    const auto& aero = ir.model_occurrences[0U];
    const auto& closure = ir.model_occurrences[1U];
    require(aero.occurrence_id == "aero_lookup" &&
                aero.model_id ==
                    gnc::packages::yyz::
                        kAerodynamicTableModelIdentity &&
                aero.execution_form == ModelExecutionForm::PureQuery &&
                aero.placement == ModelPlacement::VehicleOutput &&
                aero.subject_entity_id == kYyzQualificationSubject &&
                aero.scope.has_value() &&
                *aero.scope == ir.scopes[0U].key &&
                aero.asset_bindings.size() == 1U &&
                aero.asset_bindings[0U].role == "aerodynamics" &&
                aero.asset_bindings[0U].asset_schema_id ==
                    gnc::packages::yyz::
                        kAerodynamicTableAssetSchemaIdentity &&
                aero.asset_bindings[0U].asset_id ==
                    "aero-table.fixture.yyz.multiaffine@1" &&
                aero.asset_bindings[0U].cardinality ==
                    PortCardinality::ExactlyOne &&
                aero.asset_bindings[0U].source.node_path ==
                    "/selected_assets/2/asset_id",
            "YYZ aero query lost its real scope, placement, or asset");
    require(closure.occurrence_id == "force_moment_closure" &&
                closure.model_id ==
                    gnc::packages::yyz::
                        kForceMomentClosureModelIdentity &&
                closure.execution_form == ModelExecutionForm::Closure &&
                closure.placement ==
                    ModelPlacement::InteractionClosure &&
                closure.subject_entity_id ==
                    kYyzQualificationSubject &&
                closure.scope.has_value() &&
                *closure.scope == ir.scopes[0U].key &&
                closure.source.document_uri ==
                    kYyzQualificationAssetIndexUri &&
                closure.source.node_path ==
                    "/component_bindings/1/role" &&
                closure.subject_source.document_uri ==
                    kYyzQualificationSourceUri &&
                closure.subject_source.node_path ==
                    "/profiles/qualification/vehicle/subject" &&
                closure.output_ports[0U].binding_kind ==
                    BindingKind::ContinuousClosureLink &&
                closure.output_ports[0U].temporal_relation ==
                    TemporalRelation::IntervalModel,
            "YYZ closure occurrence lost its real definition, role, or "
            "subject relation");
    const auto rebuilt_closure =
        gnc::packages::yyz::build_force_moment_closure_definition(
            closure.configuration);
    const auto rebuilt_aero =
        gnc::packages::yyz::build_aerodynamic_table_definition(
            aero.configuration, aero.asset_bindings[0U].asset_id);
    const auto rebuilt_closure_again =
        gnc::packages::yyz::build_force_moment_closure_definition(
            closure.configuration);
    const auto rebuilt_aero_again =
        gnc::packages::yyz::build_aerodynamic_table_definition(
            aero.configuration, aero.asset_bindings[0U].asset_id);
    require(rebuilt_closure.has_value() && rebuilt_aero.has_value() &&
                rebuilt_closure_again.has_value() &&
                rebuilt_aero_again.has_value() &&
                rebuilt_closure.value().body_frame.id ==
                    "frame.fixture.yyz.body@1" &&
                rebuilt_closure.value().clock_domain.id ==
                    "clock.fixture.yyz.simulation@1" &&
                rebuilt_closure.value().configuration_revision == 11 &&
                rebuilt_aero.value().configuration_id ==
                    "configuration.fixture.yyz.clean@1" &&
                rebuilt_aero.value().table_asset_id ==
                    "aero-table.fixture.yyz.multiaffine@1" &&
                gnc::packages::yyz::
                        canonical_force_moment_closure_config(
                            rebuilt_closure.value())
                        .fields ==
                    gnc::packages::yyz::
                        canonical_force_moment_closure_config(
                            rebuilt_closure_again.value())
                        .fields &&
                gnc::packages::yyz::canonical_aerodynamic_table_config(
                    rebuilt_aero.value()).fields ==
                    gnc::packages::yyz::canonical_aerodynamic_table_config(
                        rebuilt_aero_again.value()).fields,
            "canonical YYZ config did not rebuild typed definitions");

    const auto qualification_compilation_outcome =
        gnc::compiler::compile_static_plan(source, catalog);
    const auto& qualification_plan = require_value(
        qualification_compilation_outcome,
        "REF-YYZ-001 typed BindingPlan compilation failed").plan;
    require(qualification_plan.binding_plan.entries.size() == 3U &&
                qualification_plan.binding_plan.entries[0U].binding_id ==
                    "asset.aero_lookup.aerodynamics" &&
                qualification_plan.binding_plan.entries[0U].phase ==
                    BindingPhase::PrepareTime &&
                qualification_plan.binding_plan.entries[1U].binding_id ==
                    "yyz.aero-to-rigid" &&
                qualification_plan.binding_plan.entries[1U]
                    .scope_resolution.has_value() &&
                qualification_plan.binding_plan.entries[2U].binding_id ==
                    "yyz.closure-to-rigid" &&
                qualification_plan.binding_plan.entries[2U].binding_kind ==
                    BindingKind::ContinuousClosureLink &&
                qualification_plan.binding_plan.entries[2U]
                    .scope_resolution.has_value() &&
                qualification_plan.temporal_binding_plan.entries.size() ==
                    1U &&
                qualification_plan.temporal_binding_plan.entries[0U]
                        .relation ==
                    TemporalRelation::IntervalModel &&
                qualification_plan.obligations.size() == 2U,
            "REF-YYZ-001 binding proof lost asset, scope, or frozen time");

    const auto explain =
        gnc::compiler::explain_canonical_mission_ir(ir);
    require(explain.find(
                "scope Vehicle subject vehicle.fixture.yyz@1\n") !=
                std::string::npos &&
                explain.find(
                    "placement interaction/closure subject "
                    "vehicle.fixture.yyz@1 scope "
                    "Vehicle:vehicle.fixture.yyz@1\n") !=
                    std::string::npos &&
                explain.find(
                    "asset aero_lookup.aerodynamics "
                    "gnc.asset.yyz.aerodynamic-table.multiaffine@1 "
                    "aero-table.fixture.yyz.multiaffine@1 cardinality "
                    "exactly-one\n") !=
                    std::string::npos &&
                explain.find(
                    "algorithm-consumer yyz.rigid-step "
                    "gnc.package.yyz-rigid-step.experimental@1@0.1.0 "
                    "gnc.package.yyz.rigid-step.kernel@1@0.1.0 scope "
                    "Vehicle:vehicle.fixture.yyz@1\n") !=
                    std::string::npos,
            "YYZ canonical explain omitted scope, placement, or asset");

    auto relocated = source;
    std::reverse(relocated.entities.begin(), relocated.entities.end());
    std::reverse(relocated.scopes.begin(), relocated.scopes.end());
    std::reverse(relocated.model_occurrences.begin(),
                 relocated.model_occurrences.end());
    relocated.mission_source =
        {"repo://relocated/qualification.json", "/mission"};
    relocated.entities[0U].identity_source =
        {"repo://relocated/qualification.json", "/entities/0/id"};
    relocated.entities[0U].lifecycle_source =
        {"repo://relocated/qualification.json", "/entities/0/lifecycle"};
    relocated.scopes[0U].source =
        {"repo://relocated/qualification.json", "/scopes/0"};
    for (std::size_t index = 0U;
         index < relocated.model_occurrences.size(); ++index) {
        auto& model = relocated.model_occurrences[index];
        model.source = {"repo://relocated/assets.json",
                        "/models/" + std::to_string(index)};
        model.subject_source = {
            "repo://relocated/qualification.json",
            "/models/" + std::to_string(index) + "/subject"};
        model.scope_source = {
            "repo://relocated/qualification.json",
            "/models/" + std::to_string(index) + "/scope"};
        model.placement_source = {
            "repo://relocated/assets.json",
            "/models/" + std::to_string(index) + "/placement"};
        model.configuration_source = {
            "repo://relocated/config.json",
            "/models/" + std::to_string(index) + "/configuration"};
        std::reverse(model.configuration.fields.begin(),
                     model.configuration.fields.end());
        std::reverse(model.configuration_field_sources.begin(),
                     model.configuration_field_sources.end());
        for (auto& field : model.configuration_field_sources) {
            field.source = {"repo://relocated/config.json",
                            "/fields/" + field.field_id};
        }
        std::reverse(model.asset_bindings.begin(),
                     model.asset_bindings.end());
        for (auto& asset : model.asset_bindings) {
            asset.source = {"repo://relocated/assets.json",
                            "/assets/" + asset.role};
        }
    }
    for (auto& algorithm : relocated.algorithm_consumers) {
        algorithm.source = {"repo://relocated/algorithm.cpp",
                            "/algorithm"};
        algorithm.scope_source = {
            "repo://relocated/qualification.json", "/algorithm/scope"};
    }
    for (auto& binding : relocated.binding_intents) {
        binding.source = {"repo://relocated/bindings.json",
                          "/bindings/" + binding.binding_id};
    }
    const auto relocated_outcome =
        gnc::compiler::build_canonical_mission_ir(relocated, catalog);
    const auto& relocated_ir = require_value(
        relocated_outcome, "relocated YYZ canonical IR build failed");
    require(gnc::compiler::explain_canonical_mission_ir(relocated_ir) ==
                explain &&
                relocated_ir.entities[0U]
                        .lifecycle_source.document_uri ==
                    "repo://relocated/qualification.json" &&
                relocated_ir.model_occurrences[0U]
                        .configuration_source.document_uri ==
                    "repo://relocated/config.json",
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

    auto unknown_scope_entity = yyz_qualification_source();
    unknown_scope_entity.scopes[0U].key.subject_entity_id =
        "vehicle.fixture.unknown@1";
    const auto unknown_scope_entity_outcome =
        gnc::compiler::build_canonical_mission_ir(
            unknown_scope_entity, catalog);
    require(!unknown_scope_entity_outcome.value.has_value() &&
                has_diagnostic(
                    unknown_scope_entity_outcome.diagnostics,
                    DiagnosticCode::UnknownScopeEntity),
            "Vehicle scope anchored to an unknown entity entered IR");

    auto unknown_occurrence_scope = yyz_qualification_source();
    unknown_occurrence_scope.model_occurrences[0U].scope = ScopeKey{
        ScopeKind::Vehicle, "vehicle.fixture.unbound@1"};
    const auto unknown_occurrence_scope_outcome =
        gnc::compiler::build_canonical_mission_ir(
            unknown_occurrence_scope, catalog);
    require(!unknown_occurrence_scope_outcome.value.has_value() &&
                has_diagnostic(
                    unknown_occurrence_scope_outcome.diagnostics,
                    DiagnosticCode::UnknownScope),
            "occurrence with an undeclared scope entered IR");

    auto mismatched_placement = yyz_qualification_source();
    mismatched_placement.model_occurrences[0U].placement =
        ModelPlacement::VehicleOutput;
    const auto mismatched_placement_outcome =
        gnc::compiler::build_canonical_mission_ir(
            mismatched_placement, catalog);
    require(!mismatched_placement_outcome.value.has_value() &&
                has_diagnostic(
                    mismatched_placement_outcome.diagnostics,
                    DiagnosticCode::PlacementMismatch),
            "source placement incompatible with package policy entered IR");

    auto mismatched_subject_scope = yyz_qualification_source();
    mismatched_subject_scope.model_occurrences[0U]
        .subject_entity_id.clear();
    const auto mismatched_subject_scope_outcome =
        gnc::compiler::build_canonical_mission_ir(
            mismatched_subject_scope, catalog);
    require(!mismatched_subject_scope_outcome.value.has_value() &&
                has_diagnostic(
                    mismatched_subject_scope_outcome.diagnostics,
                    DiagnosticCode::SubjectScopeMismatch),
            "Vehicle scope and occurrence subject mismatch entered IR");

    auto invalid_configuration = yyz_qualification_source();
    invalid_configuration.model_occurrences[0U]
        .configuration.fields[2U].value = std::string("eleven");
    const auto invalid_configuration_outcome =
        gnc::compiler::build_canonical_mission_ir(
            invalid_configuration, catalog);
    require(!invalid_configuration_outcome.value.has_value() &&
                has_diagnostic(
                    invalid_configuration_outcome.diagnostics,
                    DiagnosticCode::InvalidConfiguration),
            "configuration field with the wrong canonical type entered IR");

    auto missing_asset = yyz_qualification_source();
    missing_asset.model_occurrences[1U].asset_bindings.clear();
    const auto missing_asset_outcome =
        gnc::compiler::build_canonical_mission_ir(
            missing_asset, catalog);
    require(!missing_asset_outcome.value.has_value() &&
                has_diagnostic(missing_asset_outcome.diagnostics,
                               DiagnosticCode::MissingAssetBinding),
            "asset-bearing aero model entered IR without its asset");

    auto duplicate_asset = yyz_qualification_source();
    auto duplicate_asset_binding =
        duplicate_asset.model_occurrences[1U].asset_bindings[0U];
    duplicate_asset_binding.source.node_path =
        "/selected_assets/2/duplicate-asset-id";
    duplicate_asset.model_occurrences[1U].asset_bindings.push_back(
        std::move(duplicate_asset_binding));
    const auto duplicate_asset_outcome =
        gnc::compiler::build_canonical_mission_ir(
            duplicate_asset, catalog);
    require(!duplicate_asset_outcome.value.has_value() &&
                has_diagnostic(
                    duplicate_asset_outcome.diagnostics,
                    DiagnosticCode::DuplicateAssetBinding),
            "multiple assets satisfied an exactly-one prepared-model slot");

    auto incompatible_asset_schema = yyz_qualification_source();
    incompatible_asset_schema.model_occurrences[1U]
        .asset_bindings[0U]
        .asset_schema_id = "gnc.asset.fixture.incompatible@1";
    const auto incompatible_asset_schema_outcome =
        gnc::compiler::build_canonical_mission_ir(
            incompatible_asset_schema, catalog);
    require(!incompatible_asset_schema_outcome.value.has_value() &&
                has_diagnostic(
                    incompatible_asset_schema_outcome.diagnostics,
                    DiagnosticCode::AssetSchemaMismatch),
            "incompatible aerodynamic asset schema entered IR");
}

void verify_success_product(const StaticCompilation& compilation) {
    const auto& ir = compilation.ir;
    const auto& plan = compilation.plan;
    require(ir.revision == 2U && ir.mission_id == kMissionId,
            "typed source did not produce the expected minimal IR");
    require(ir.entities.size() == 1U &&
                ir.scopes.size() == 1U &&
                ir.model_occurrences.size() == 3U &&
                ir.algorithm_consumers.size() == 2U &&
                ir.binding_intents.size() == 3U,
            "minimal IR occurrence or binding count changed");
    require(ir.model_occurrences[0U].occurrence_id == "cavh.envelope" &&
                ir.model_occurrences[0U].model_id ==
                    gnc::packages::cavh::kGlideEnvelopeModelIdentity &&
                ir.model_occurrences[0U].output_ports.size() == 1U &&
                ir.model_occurrences[0U].output_ports[0U].contract_id ==
                    gnc::packages::cavh::
                        kGlideEnvelopeOutputContractIdentity &&
                ir.model_occurrences[0U].placement ==
                    ModelPlacement::VehicleOutput &&
                ir.model_occurrences[0U].subject_entity_id.empty() &&
                !ir.model_occurrences[0U].scope.has_value() &&
                ir.model_occurrences[0U]
                        .output_ports[0U]
                        .binding_kind == BindingKind::PureQuery &&
                ir.model_occurrences[0U]
                        .placement_source.document_uri ==
                    "catalog://gnc.package.cavh-formula.experimental@1" &&
                ir.model_occurrences[1U].occurrence_id ==
                    "yyz.aerodynamics" &&
                ir.model_occurrences[1U].model_id ==
                    gnc::packages::yyz::kAerodynamicTableModelIdentity &&
                ir.model_occurrences[1U].placement ==
                    ModelPlacement::VehicleOutput &&
                ir.model_occurrences[1U].scope.has_value() &&
                ir.model_occurrences[1U].scope->subject_entity_id ==
                    kYyzQualificationSubject &&
                ir.model_occurrences[1U].asset_bindings.size() == 1U &&
                ir.model_occurrences[2U].occurrence_id == "yyz.closure" &&
                ir.model_occurrences[2U].model_id ==
                    gnc::packages::yyz::
                        kForceMomentClosureModelIdentity &&
                ir.model_occurrences[2U].placement ==
                    ModelPlacement::InteractionClosure &&
                ir.model_occurrences[2U].scope.has_value() &&
                ir.model_occurrences[2U].output_ports.size() == 1U &&
                ir.model_occurrences[2U].output_ports[0U].contract_id ==
                    gnc::packages::yyz::kRigidFormInputContractIdentity &&
                ir.model_occurrences[2U]
                        .output_ports[0U]
                        .binding_kind ==
                    BindingKind::ContinuousClosureLink &&
                ir.model_occurrences[2U]
                        .output_ports[0U]
                        .temporal_relation ==
                    TemporalRelation::IntervalModel,
            "canonical IR lost exact model or output identities");
    require(ir.algorithm_consumers[0U].consumer_id == "cavh.formula" &&
                ir.algorithm_consumers[0U].input_ports.size() == 1U &&
                ir.algorithm_consumers[0U].input_ports[0U].contract_id ==
                    gnc::packages::cavh::
                        kGlideEnvelopeOutputContractIdentity &&
                ir.algorithm_consumers[1U].consumer_id ==
                    "yyz.rigid-step" &&
                ir.algorithm_consumers[1U].scope.has_value() &&
                ir.algorithm_consumers[1U].scope->subject_entity_id ==
                    kYyzQualificationSubject &&
                ir.algorithm_consumers[1U].input_ports.size() == 2U &&
                ir.algorithm_consumers[1U]
                        .input_ports[0U]
                        .contract_id ==
                    gnc::packages::yyz::
                        kAerodynamicCoefficientsContractIdentity &&
                ir.algorithm_consumers[1U]
                        .input_ports[1U]
                        .contract_id ==
                    gnc::packages::yyz::kRigidFormInputContractIdentity &&
                ir.binding_intents[0U].binding_id ==
                    "cavh.envelope-to-formula" &&
                ir.binding_intents[1U].binding_id ==
                    "yyz.aero-to-rigid" &&
                ir.binding_intents[2U].binding_id ==
                    "yyz.closure-to-rigid",
            "canonical IR lost exact algorithm inputs or binding intents");
    require(plan.revision == 2U && plan.plan_id == kPlanId &&
                plan.mission_id == kMissionId,
            "static plan identity changed");
    require(plan.dependency_lock.size() == 2U &&
                plan.model_preparation_identities.size() == 3U &&
                plan.algorithms.size() == 2U &&
                plan.binding_plan.entries.size() == 4U &&
                plan.temporal_binding_plan.entries.size() == 1U &&
                plan.binding_proofs.size() == 4U &&
                plan.obligations.size() == 3U,
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
    const auto& yyz_aero_model =
        plan.model_preparation_identities[1U];
    require(yyz_aero_model.occurrence_id == "yyz.aerodynamics" &&
                yyz_aero_model.model_id ==
                    gnc::packages::yyz::kAerodynamicTableModelIdentity &&
                yyz_aero_model.execution_form ==
                    ModelExecutionForm::PureQuery &&
                yyz_aero_model.preparation_algorithm_id ==
                    gnc::packages::yyz::
                        kAerodynamicTablePreparationIdentity.id,
            "YYZ plan entry lost its real aerodynamic query definition");
    const auto& yyz_model = plan.model_preparation_identities[2U];
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
                    gnc::packages::yyz::kRigidStepKernelIdentity.version &&
                plan.algorithms[1U].scope.has_value() &&
                plan.algorithms[1U].scope->subject_entity_id ==
                    kYyzQualificationSubject,
            "algorithm consumers lost exact identities");

    const auto& asset_binding = plan.binding_plan.entries[0U];
    const auto& cavh_binding = plan.binding_plan.entries[1U];
    const auto& aero_binding = plan.binding_plan.entries[2U];
    const auto& closure_binding = plan.binding_plan.entries[3U];
    require(std::all_of(
                plan.binding_plan.entries.begin(),
                plan.binding_plan.entries.end(),
                [](const auto& binding) {
                    return !binding.source.document_uri.empty() &&
                           !binding.source.node_path.empty();
                }) &&
                std::all_of(
                    plan.temporal_binding_plan.entries.begin(),
                    plan.temporal_binding_plan.entries.end(),
                    [](const auto& binding) {
                        return !binding.source.document_uri.empty() &&
                               !binding.source.node_path.empty();
                    }),
            "BindingPlan wrote an invalid direct SourceRef");
    require(asset_binding.binding_id ==
                "asset.yyz.aerodynamics.aerodynamics" &&
                asset_binding.binding_kind == BindingKind::AssetBinding &&
                asset_binding.provider_endpoint.kind ==
                    BindingEndpointKind::Asset &&
                asset_binding.provider_endpoint.owner_id ==
                    "aero-table.fixture.yyz.multiaffine@1" &&
                asset_binding.consumer_endpoint.kind ==
                    BindingEndpointKind::PreparedModel &&
                asset_binding.consumer_endpoint.owner_id ==
                    "yyz.aerodynamics" &&
                asset_binding.exact_contract_id ==
                    gnc::packages::yyz::
                        kAerodynamicTableAssetSchemaIdentity &&
                asset_binding.provider_cardinality ==
                    PortCardinality::ExactlyOne &&
                asset_binding.consumer_cardinality ==
                    PortCardinality::ExactlyOne &&
                asset_binding.phase == BindingPhase::PrepareTime &&
                asset_binding.asset_binding.has_value() &&
                asset_binding.asset_binding->role == "aerodynamics" &&
                !asset_binding.scope_resolution.has_value(),
            "YYZ asset binding lost prepare-time asset/schema identity");
    require(cavh_binding.binding_id ==
                "cavh.envelope-to-formula" &&
                cavh_binding.binding_kind == BindingKind::PureQuery &&
                cavh_binding.provider_endpoint.kind ==
                    BindingEndpointKind::ModelOccurrence &&
                cavh_binding.consumer_endpoint.kind ==
                    BindingEndpointKind::AlgorithmConsumer &&
                cavh_binding.exact_contract_id ==
                    gnc::packages::cavh::
                        kGlideEnvelopeOutputContractIdentity &&
                cavh_binding.provider_cardinality ==
                    PortCardinality::OneOrMore &&
                cavh_binding.consumer_cardinality ==
                    PortCardinality::ExactlyOne &&
                !cavh_binding.scope_resolution.has_value(),
            "CAVH definition-level PureQuery binding changed");
    require(aero_binding.binding_id == "yyz.aero-to-rigid" &&
                aero_binding.binding_kind == BindingKind::PureQuery &&
                aero_binding.exact_contract_id ==
                    gnc::packages::yyz::
                        kAerodynamicCoefficientsContractIdentity &&
                aero_binding.scope_resolution.has_value() &&
                aero_binding.scope_resolution->resolved_scope
                        .subject_entity_id ==
                    kYyzQualificationSubject,
            "YYZ aerodynamic PureQuery lost exact contract or scope");
    require(closure_binding.binding_id ==
                "yyz.closure-to-rigid" &&
                closure_binding.binding_kind ==
                    BindingKind::ContinuousClosureLink &&
                closure_binding.exact_contract_id ==
                    gnc::packages::yyz::kRigidFormInputContractIdentity &&
                closure_binding.scope_resolution.has_value() &&
                closure_binding.scope_resolution->resolved_scope
                        .subject_entity_id ==
                    kYyzQualificationSubject &&
                plan.temporal_binding_plan.entries[0U].binding_id ==
                    closure_binding.binding_id &&
                plan.temporal_binding_plan.entries[0U].relation ==
                    TemporalRelation::IntervalModel,
            "YYZ frozen-interval closure binding changed");

    const auto has_assertion = [](const auto& proof,
                                  BindingProofAssertion assertion) {
        return std::find(proof.assertions.begin(), proof.assertions.end(),
                         assertion) != proof.assertions.end();
    };
    for (const auto& proof : plan.binding_proofs) {
        require(!proof.proof_id.empty() &&
                    !proof.binding_id.empty() &&
                    !proof.exact_contract_id.empty() &&
                    has_assertion(proof,
                                  BindingProofAssertion::EndpointsResolved) &&
                    has_assertion(proof,
                                  BindingProofAssertion::KindCompatible) &&
                    has_assertion(proof,
                                  BindingProofAssertion::ContractExact) &&
                    has_assertion(
                        proof,
                        BindingProofAssertion::CardinalitySatisfied) &&
                    has_assertion(proof,
                                  BindingProofAssertion::SourceLocated) &&
                    !proof.source_refs.empty() &&
                    std::all_of(
                        proof.source_refs.begin(), proof.source_refs.end(),
                        [](const SourceRef& source_ref) {
                            return !source_ref.document_uri.empty() &&
                                   !source_ref.node_path.empty();
                        }),
                "structured binding proof is incomplete or unlocated");
    }
    require(has_assertion(
                plan.binding_proofs[0U],
                BindingProofAssertion::
                    SourceSelectedAssetIdentityPreserved) &&
                !has_assertion(
                    plan.binding_proofs[1U],
                    BindingProofAssertion::ScopeExact) &&
                has_assertion(
                    plan.binding_proofs[2U],
                    BindingProofAssertion::ScopeExact) &&
                has_assertion(
                    plan.binding_proofs[3U],
                    BindingProofAssertion::ScopeExact) &&
                has_assertion(
                    plan.binding_proofs[3U],
                    BindingProofAssertion::TemporalCompatible) &&
                plan.binding_proofs[1U].source_refs[1U].node_path ==
                    "/bindings/cavh.envelope-to-formula" &&
                plan.binding_proofs[2U].source_refs[1U].node_path ==
                    "/bindings/yyz.aero-to-rigid" &&
                plan.binding_proofs[3U].source_refs[1U].node_path ==
                    "/bindings/yyz.closure-to-rigid",
            "binding proof lost scoped, temporal, asset, or source facts");
    require(plan.obligations[0U].kind ==
                CompiledObligationKind::PureQueryEvaluation &&
                plan.obligations[0U].consumer_endpoint.owner_id ==
                    "cavh.formula" &&
                plan.obligations[1U].kind ==
                    CompiledObligationKind::PureQueryEvaluation &&
                plan.obligations[1U].provider_endpoint.owner_id ==
                    "yyz.aerodynamics" &&
                plan.obligations[1U].consumer_endpoint.owner_id ==
                    "yyz.rigid-step" &&
                plan.obligations[2U].kind ==
                    CompiledObligationKind::ClosureEvaluation &&
                plan.obligations[2U].consumer_endpoint.owner_id ==
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
    std::reverse(source.entities.begin(), source.entities.end());
    std::reverse(source.scopes.begin(), source.scopes.end());
    std::reverse(source.model_occurrences.begin(),
                 source.model_occurrences.end());
    std::reverse(source.algorithm_consumers.begin(),
                 source.algorithm_consumers.end());
    std::reverse(source.binding_intents.begin(),
                 source.binding_intents.end());
    source.mission_source = {"typed://alternate/source.yaml",
                             "/mission"};
    for (auto& entity : source.entities) {
        entity.identity_source = {"typed://alternate/source.yaml",
                                  "/entities/id"};
        entity.lifecycle_source = {"typed://alternate/source.yaml",
                                   "/entities/lifecycle"};
    }
    for (auto& scope : source.scopes) {
        scope.source = {"typed://alternate/source.yaml", "/scopes"};
    }
    for (std::size_t index = 0U;
         index < source.model_occurrences.size(); ++index) {
        auto& model = source.model_occurrences[index];
        model.source =
            {"typed://alternate/source.yaml",
             "/model_occurrences/" + std::to_string(index)};
        model.configuration_source =
            {"typed://alternate/config.yaml",
             "/model_configurations/" + std::to_string(index)};
        std::reverse(model.configuration.fields.begin(),
                     model.configuration.fields.end());
        std::reverse(model.configuration_field_sources.begin(),
                     model.configuration_field_sources.end());
        for (auto& field : model.configuration_field_sources) {
            field.source = {"typed://alternate/config.yaml",
                            "/fields/" + field.field_id};
        }
        std::reverse(model.asset_bindings.begin(),
                     model.asset_bindings.end());
        for (auto& asset : model.asset_bindings) {
            asset.source = {"typed://alternate/assets.yaml",
                            "/assets/" + asset.role};
        }
        if (model.placement != ModelPlacement::Unspecified) {
            model.placement_source =
                {"typed://alternate/source.yaml",
                 "/placements/" + std::to_string(index)};
        }
        if (!model.subject_entity_id.empty()) {
            model.subject_source = {"typed://alternate/source.yaml",
                                    "/subjects/" + model.occurrence_id};
        }
        if (model.scope.has_value()) {
            model.scope_source = {"typed://alternate/source.yaml",
                                  "/scopes/" + model.occurrence_id};
        }
    }
    for (std::size_t index = 0U;
         index < source.algorithm_consumers.size(); ++index) {
        source.algorithm_consumers[index].source =
            {"typed://alternate/source.yaml",
             "/algorithm_consumers/" + std::to_string(index)};
        if (source.algorithm_consumers[index].scope.has_value()) {
            source.algorithm_consumers[index].scope_source =
                {"typed://alternate/source.yaml",
                 "/algorithm_scopes/" + std::to_string(index)};
        }
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
    require(compilation.plan.binding_proofs[1U]
                .source_refs[0U]
                .document_uri == "typed://alternate/source.yaml",
            "canonical semantics discarded source provenance");
}

void verify_negative_cases() {
    const auto catalog_outcome = Catalog::build(package_descriptors());
    const auto& catalog =
        require_value(catalog_outcome, "fixture Catalog build failed");

    auto alternate_asset = composition_source();
    constexpr std::string_view kAlternateAssetIdentity =
        "aero-table.fixture.yyz.not-resolved-here@1";
    alternate_asset.model_occurrences[1U]
        .asset_bindings[0U]
        .asset_id = std::string(kAlternateAssetIdentity);
    const auto alternate_asset_outcome =
        gnc::compiler::compile_static_plan(alternate_asset, catalog);
    const auto& alternate_asset_compilation = require_value(
        alternate_asset_outcome,
        "nonempty source-selected asset identity was treated as resolved");
    require(alternate_asset_compilation.ir.model_occurrences[1U]
                    .asset_bindings[0U]
                    .asset_id == kAlternateAssetIdentity &&
                alternate_asset_compilation.plan.binding_plan.entries[0U]
                    .provider_endpoint.owner_id == kAlternateAssetIdentity &&
                alternate_asset_compilation.plan.binding_plan.entries[0U]
                    .asset_binding->asset_id == kAlternateAssetIdentity,
            "source-selected asset identity was not preserved into the plan");

    auto empty_asset_identity = composition_source();
    empty_asset_identity.model_occurrences[1U]
        .asset_bindings[0U]
        .asset_id.clear();
    const auto empty_asset_identity_outcome =
        gnc::compiler::compile_static_plan(empty_asset_identity, catalog);
    const auto& empty_asset_identity_diagnostic = require_diagnostic(
        empty_asset_identity_outcome.diagnostics,
        DiagnosticCode::InvalidAssetIdentity,
        "empty asset identity diagnostic missing");
    require(!empty_asset_identity_outcome.value.has_value() &&
                empty_asset_identity_diagnostic.source.node_path ==
                    "/models/yyz.aerodynamics/assets/aerodynamics",
            "empty source-selected asset identity entered the plan");

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

    auto incompatible_kind_packages = package_descriptors();
    auto& cavh_input =
        incompatible_kind_packages[0U].algorithms[0U].ports[0U];
    cavh_input.binding_kind = BindingKind::ContinuousClosureLink;
    cavh_input.temporal_relation = TemporalRelation::IntervalModel;
    const auto incompatible_kind_catalog_outcome =
        Catalog::build(std::move(incompatible_kind_packages));
    const auto& incompatible_kind_catalog = require_value(
        incompatible_kind_catalog_outcome,
        "incompatible-kind Catalog setup failed");
    const auto incompatible_kind_outcome =
        gnc::compiler::compile_static_plan(
            composition_source(), incompatible_kind_catalog);
    const auto& kind_diagnostic = require_diagnostic(
        incompatible_kind_outcome.diagnostics,
        DiagnosticCode::BindingKindMismatch,
        "binding kind mismatch diagnostic missing");
    require(!incompatible_kind_outcome.value.has_value() &&
                kind_diagnostic.source.node_path ==
                    "/bindings/cavh.envelope-to-formula",
            "incompatible query/closure kind produced a plan");

    auto incompatible_temporal_packages = package_descriptors();
    incompatible_temporal_packages[1U]
        .algorithms[0U]
        .ports[1U]
        .temporal_relation = TemporalRelation::CandidateStateQuery;
    const auto incompatible_temporal_catalog_outcome =
        Catalog::build(std::move(incompatible_temporal_packages));
    const auto& incompatible_temporal_catalog = require_value(
        incompatible_temporal_catalog_outcome,
        "incompatible-temporal Catalog setup failed");
    const auto incompatible_temporal_outcome =
        gnc::compiler::compile_static_plan(
            composition_source(), incompatible_temporal_catalog);
    const auto& temporal_diagnostic = require_diagnostic(
        incompatible_temporal_outcome.diagnostics,
        DiagnosticCode::BindingTemporalMismatch,
        "binding temporal mismatch diagnostic missing");
    require(!incompatible_temporal_outcome.value.has_value() &&
                temporal_diagnostic.source.node_path ==
                    "/bindings/yyz.closure-to-rigid",
            "candidate-state relation entered the frozen interval closure");

    auto incompatible_scope = composition_source();
    const ScopeKey alternate_scope{
        ScopeKind::Vehicle, "vehicle.fixture.yyz.alternate@1"};
    incompatible_scope.entities.push_back(
        {alternate_scope.subject_entity_id,
         EntityLifecycle::ActiveAtInitialize,
         ref("/entities/vehicle.fixture.yyz.alternate@1/id"),
         ref("/entities/vehicle.fixture.yyz.alternate@1/lifecycle")});
    incompatible_scope.scopes.push_back(
        {alternate_scope,
         ref("/scopes/vehicle.fixture.yyz.alternate@1")});
    incompatible_scope.algorithm_consumers[1U].scope = alternate_scope;
    incompatible_scope.algorithm_consumers[1U].scope_source =
        ref("/algorithms/yyz.rigid-step/alternate-scope");
    const auto incompatible_scope_outcome =
        gnc::compiler::compile_static_plan(incompatible_scope, catalog);
    const auto& scope_diagnostic = require_diagnostic(
        incompatible_scope_outcome.diagnostics,
        DiagnosticCode::BindingScopeMismatch,
        "binding scope mismatch diagnostic missing");
    require(!incompatible_scope_outcome.value.has_value() &&
                scope_diagnostic.source.node_path ==
                    "/bindings/yyz.aero-to-rigid",
            "cross-vehicle query binding produced a plan");

    auto missing_binding_source = composition_source();
    missing_binding_source.binding_intents[0U].source = {};
    const auto missing_binding_source_outcome =
        gnc::compiler::compile_static_plan(
            missing_binding_source, catalog);
    const auto& missing_source_diagnostic = require_diagnostic(
        missing_binding_source_outcome.diagnostics,
        DiagnosticCode::MissingSourceReference,
        "missing binding source diagnostic missing");
    require(!missing_binding_source_outcome.value.has_value() &&
                !missing_source_diagnostic.source.document_uri.empty() &&
                !missing_source_diagnostic.source.node_path.empty() &&
                missing_source_diagnostic.source.node_path == "/mission_id",
            "empty SourceBinding source entered proof or diagnostic output");

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

[[nodiscard]] const gnc::compiler::CanonicalSemanticHash&
require_hash(
    const CompileOutcome<gnc::compiler::CanonicalSemanticHash>& outcome,
    std::string_view message) {
    return require_value(outcome, message);
}

[[nodiscard]] std::string verify_semantic_hash(const Catalog& catalog) {
    const auto base_source = yyz_qualification_source();
    const auto base_ir_outcome =
        gnc::compiler::build_canonical_mission_ir(base_source, catalog);
    const auto& base_ir = require_value(
        base_ir_outcome, "semantic-hash base IR build failed");
    const auto base_hash_outcome =
        gnc::compiler::hash_canonical_mission_ir(base_ir);
    const auto& base_hash = require_hash(
        base_hash_outcome, "semantic-hash base encoding failed");
    require(base_hash.algorithm == "SHA-256" &&
                base_hash.encoding_id ==
                    gnc::compiler::kCanonicalSemanticEncodingIdentity &&
                base_hash.hex_digest.size() == 64U,
            "semantic hash identity or digest width differs");

    auto relocated_ir = base_ir;
    relocated_ir.mission_source = {"repo://relocated/source.json",
                                   "/mission"};
    for (auto& entity : relocated_ir.entities) {
        entity.identity_source = {"repo://relocated/source.json",
                                  "/entities/id"};
        entity.lifecycle_source = {"repo://relocated/source.json",
                                   "/entities/lifecycle"};
    }
    for (auto& scope : relocated_ir.scopes) {
        scope.source = {"repo://relocated/source.json", "/scopes"};
    }
    for (auto& model : relocated_ir.model_occurrences) {
        model.source = {"repo://relocated/models.json", "/model"};
        model.subject_source = {"repo://relocated/source.json",
                                "/subject"};
        model.scope_source = {"repo://relocated/source.json", "/scope"};
        model.placement_source = {"repo://relocated/models.json",
                                  "/placement"};
        model.configuration_source = {"repo://relocated/config.json",
                                      "/configuration"};
        for (auto& field : model.configuration_field_sources) {
            field.source = {"repo://relocated/config.json", "/field"};
        }
        for (auto& asset : model.asset_bindings) {
            asset.source = {"repo://relocated/assets.json", "/asset"};
        }
    }
    for (auto& algorithm : relocated_ir.algorithm_consumers) {
        algorithm.source = {"repo://relocated/algorithm.cpp",
                            "/algorithm"};
        algorithm.scope_source = {"repo://relocated/source.json",
                                  "/algorithm/scope"};
    }
    for (auto& binding : relocated_ir.binding_intents) {
        binding.source = {"repo://relocated/bindings.json", "/binding"};
    }
    const auto relocated_hash = require_hash(
        gnc::compiler::hash_canonical_mission_ir(relocated_ir),
        "relocated IR semantic hashing failed");
    require(relocated_hash.hex_digest == base_hash.hex_digest,
            "source URI/path entered canonical semantic hash");

    auto reordered_source = base_source;
    reordered_source.plan_id = "plan.representation-local@1";
    std::reverse(reordered_source.entities.begin(),
                 reordered_source.entities.end());
    std::reverse(reordered_source.scopes.begin(),
                 reordered_source.scopes.end());
    std::reverse(reordered_source.model_occurrences.begin(),
                 reordered_source.model_occurrences.end());
    std::reverse(reordered_source.algorithm_consumers.begin(),
                 reordered_source.algorithm_consumers.end());
    std::reverse(reordered_source.binding_intents.begin(),
                 reordered_source.binding_intents.end());
    for (auto& model : reordered_source.model_occurrences) {
        std::reverse(model.configuration.fields.begin(),
                     model.configuration.fields.end());
        std::reverse(model.configuration_field_sources.begin(),
                     model.configuration_field_sources.end());
        std::reverse(model.asset_bindings.begin(),
                     model.asset_bindings.end());
    }
    const auto reordered_ir_outcome =
        gnc::compiler::build_canonical_mission_ir(
            reordered_source, catalog);
    const auto& reordered_ir = require_value(
        reordered_ir_outcome, "reordered source IR build failed");
    const auto reordered_hash = require_hash(
        gnc::compiler::hash_canonical_mission_ir(reordered_ir),
        "reordered source semantic hashing failed");
    require(reordered_hash.hex_digest == base_hash.hex_digest,
            "source order or plan identity entered canonical semantic hash");

    const auto expect_changed = [&](CanonicalMissionIr changed,
                                    std::string_view label) {
        const auto changed_hash = require_hash(
            gnc::compiler::hash_canonical_mission_ir(changed), label);
        require(changed_hash.hex_digest != base_hash.hex_digest, label);
    };

    auto entity_changed = base_ir;
    entity_changed.entities[0U].entity_id =
        "vehicle.fixture.yyz-renamed@1";
    entity_changed.scopes[0U].key.subject_entity_id =
        entity_changed.entities[0U].entity_id;
    for (auto& model : entity_changed.model_occurrences) {
        model.subject_entity_id = entity_changed.entities[0U].entity_id;
        model.scope->subject_entity_id =
            entity_changed.entities[0U].entity_id;
    }
    for (auto& algorithm : entity_changed.algorithm_consumers) {
        if (algorithm.scope.has_value()) {
            algorithm.scope->subject_entity_id =
                entity_changed.entities[0U].entity_id;
        }
    }
    expect_changed(std::move(entity_changed),
                   "entity semantic mutation did not change hash");

    auto scope_baseline = base_ir;
    scope_baseline.entities.push_back(
        {"vehicle.fixture.yyz.alternate@1",
         EntityLifecycle::ActiveAtInitialize, {}, {}});
    std::sort(scope_baseline.entities.begin(),
              scope_baseline.entities.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.entity_id < rhs.entity_id;
              });
    scope_baseline.scopes.push_back(
        {ScopeKey{ScopeKind::Vehicle,
                  "vehicle.fixture.yyz.alternate@1"},
         {}});
    std::sort(scope_baseline.scopes.begin(),
              scope_baseline.scopes.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.key < rhs.key;
              });
    const auto scope_baseline_hash = require_hash(
        gnc::compiler::hash_canonical_mission_ir(scope_baseline),
        "two-scope baseline hashing failed");
    auto scope_changed = scope_baseline;
    for (auto& model : scope_changed.model_occurrences) {
        model.subject_entity_id = "vehicle.fixture.yyz.alternate@1";
        model.scope = ScopeKey{
            ScopeKind::Vehicle, "vehicle.fixture.yyz.alternate@1"};
    }
    for (auto& algorithm : scope_changed.algorithm_consumers) {
        algorithm.scope = ScopeKey{
            ScopeKind::Vehicle, "vehicle.fixture.yyz.alternate@1"};
    }
    const auto scope_changed_hash = require_hash(
        gnc::compiler::hash_canonical_mission_ir(scope_changed),
        "scope semantic mutation hashing failed");
    require(scope_changed_hash.hex_digest !=
                scope_baseline_hash.hex_digest,
            "scope semantic mutation did not change hash");

    auto placement_changed = base_ir;
    placement_changed.model_occurrences[0U].placement =
        ModelPlacement::InteractionClosure;
    expect_changed(std::move(placement_changed),
                   "placement semantic mutation did not change hash");

    auto model_changed = base_ir;
    model_changed.model_occurrences[0U].model_version = "0.1.1";
    expect_changed(std::move(model_changed),
                   "model semantic mutation did not change hash");

    auto config_changed = base_ir;
    auto& config_fields =
        config_changed.model_occurrences[1U].configuration.fields;
    const auto absolute = std::find_if(
        config_fields.begin(), config_fields.end(),
        [](const auto& field) {
            return field.field_id == "numerical.absolute_tolerance";
        });
    require(absolute != config_fields.end(),
            "closure canonical config field is absent");
    absolute->value = 3.0e-12;
    expect_changed(std::move(config_changed),
                   "config semantic mutation did not change hash");

    auto asset_changed = base_ir;
    asset_changed.model_occurrences[0U]
        .asset_bindings[0U]
        .asset_id = "aero-table.fixture.yyz.alternate@1";
    expect_changed(std::move(asset_changed),
                   "asset semantic mutation did not change hash");

    auto closure_temporal_changed = base_ir;
    auto& closure_output =
        closure_temporal_changed.model_occurrences[1U].output_ports[0U];
    closure_output.temporal_relation =
        TemporalRelation::CandidateStateQuery;
    auto closure_input = std::find_if(
        closure_temporal_changed.algorithm_consumers[0U]
            .input_ports.begin(),
        closure_temporal_changed.algorithm_consumers[0U]
            .input_ports.end(),
        [](const auto& port) {
            return port.port_id == "form-input";
        });
    require(closure_input !=
                closure_temporal_changed.algorithm_consumers[0U]
                    .input_ports.end(),
            "closure consumer port is absent from semantic hash fixture");
    closure_input->temporal_relation =
        TemporalRelation::CandidateStateQuery;
    expect_changed(std::move(closure_temporal_changed),
                   "closure temporal mutation did not change hash");

    auto binding_identity_changed = base_ir;
    binding_identity_changed.binding_intents[0U].binding_id += ".renamed";
    expect_changed(std::move(binding_identity_changed),
                   "binding intent mutation did not change hash");

    auto noncanonical_order = base_ir;
    std::reverse(noncanonical_order.model_occurrences.begin(),
                 noncanonical_order.model_occurrences.end());
    const auto noncanonical_order_hash =
        gnc::compiler::hash_canonical_mission_ir(noncanonical_order);
    require(!noncanonical_order_hash.value.has_value() &&
                has_diagnostic(noncanonical_order_hash.diagnostics,
                               DiagnosticCode::NonCanonicalIr),
            "noncanonical model order reached SHA-256");

    auto negative_zero = base_ir;
    negative_zero.model_occurrences[0U]
        .configuration.fields[0U]
        .value = -0.0;
    const auto negative_zero_hash =
        gnc::compiler::hash_canonical_mission_ir(negative_zero);
    require(!negative_zero_hash.value.has_value() &&
                has_diagnostic(negative_zero_hash.diagnostics,
                               DiagnosticCode::NonCanonicalIr),
            "noncanonical negative zero reached SHA-256");

    const auto expect_noncanonical = [](const CanonicalMissionIr& invalid,
                                        std::string_view message) {
        const auto hash =
            gnc::compiler::hash_canonical_mission_ir(invalid);
        require(!hash.value.has_value() &&
                    has_diagnostic(hash.diagnostics,
                                   DiagnosticCode::NonCanonicalIr),
                message);
    };
    auto cross_collection_identity = base_ir;
    const auto previous_consumer_id =
        cross_collection_identity.algorithm_consumers[0U].consumer_id;
    cross_collection_identity.algorithm_consumers[0U].consumer_id =
        cross_collection_identity.model_occurrences[0U].occurrence_id;
    for (auto& binding : cross_collection_identity.binding_intents) {
        if (binding.consumer_id == previous_consumer_id) {
            binding.consumer_id =
                cross_collection_identity.model_occurrences[0U]
                    .occurrence_id;
        }
    }
    std::sort(
        cross_collection_identity.algorithm_consumers.begin(),
        cross_collection_identity.algorithm_consumers.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.consumer_id < rhs.consumer_id;
        });
    expect_noncanonical(
        cross_collection_identity,
        "cross-collection composition identity reached SHA-256");

    auto empty_model_outputs = base_ir;
    empty_model_outputs.model_occurrences[0U].output_ports.clear();
    expect_noncanonical(empty_model_outputs,
                        "model without outputs reached SHA-256");

    auto empty_algorithm_inputs = base_ir;
    empty_algorithm_inputs.algorithm_consumers[0U].input_ports.clear();
    expect_noncanonical(empty_algorithm_inputs,
                        "algorithm without inputs reached SHA-256");

    auto runtime_component_form = base_ir;
    runtime_component_form.model_occurrences[0U].execution_form =
        ModelExecutionForm::RuntimeComponent;
    expect_noncanonical(
        runtime_component_form,
        "RuntimeComponent entered semantic-bytes@2 without supersession");

    auto runtime_component_placement = base_ir;
    runtime_component_placement.model_occurrences[0U].placement =
        gnc::model_sdk::ModelPlacement::VehicleProcess;
    expect_noncanonical(
        runtime_component_placement,
        "RuntimeComponent placement entered semantic-bytes@2");

    auto current_cycle_closure = base_ir;
    const auto closure = std::find_if(
        current_cycle_closure.model_occurrences.begin(),
        current_cycle_closure.model_occurrences.end(),
        [](const auto& model) {
            return model.execution_form == ModelExecutionForm::Closure;
        });
    require(closure != current_cycle_closure.model_occurrences.end(),
            "semantic hash fixture omitted its closure model");
    closure->output_ports[0U].temporal_relation =
        gnc::model_sdk::TemporalRelation::CurrentCycle;
    expect_noncanonical(
        current_cycle_closure,
        "sampled temporal relation entered semantic-bytes@2 closure");
    return base_hash.hex_digest;
}

[[nodiscard]] std::string run_self_check() {
    const auto catalog_outcome = Catalog::build(package_descriptors());
    const auto& catalog =
        require_value(catalog_outcome, "fixture Catalog build failed");
    static_cast<void>(verify_yyz_entity_subject_slice(catalog));
    verify_yyz_entity_subject_negative_cases(catalog);
    static_cast<void>(verify_semantic_hash(catalog));
    const auto source = composition_source();
    const auto ir_outcome =
        gnc::compiler::build_canonical_mission_ir(source, catalog);
    const CanonicalMissionIr& ir = require_value(
        ir_outcome, "canonical Mission IR build failed");
    const auto ir_explain =
        gnc::compiler::explain_canonical_mission_ir(ir);
    require(ir_explain.find(
                "model cavh.envelope "
                "gnc.package.cavh-formula.experimental@1@0.1.0") !=
                std::string::npos &&
                ir_explain.find(
                    "model yyz.aerodynamics "
                    "gnc.package.yyz-rigid-step.experimental@1@0.1.0") !=
                    std::string::npos &&
                ir_explain.find(
                    "asset yyz.aerodynamics.aerodynamics "
                    "gnc.asset.yyz.aerodynamic-table.multiaffine@1") !=
                    std::string::npos &&
                ir_explain.find(
                    "model yyz.closure "
                    "gnc.package.yyz-rigid-step.experimental@1@0.1.0") !=
                    std::string::npos,
            "canonical Mission IR explain lost a real package model");

    const auto compile_outcome =
        gnc::compiler::compile_static_plan(source, catalog);
    const auto& compilation = require_value(
        compile_outcome, "typed source compilation failed");
    verify_success_product(compilation);

    const auto explain =
        gnc::compiler::explain_static_plan(compilation.plan);
    require(explain.find(
                "obligation 1 obligation.yyz.aero-to-rigid "
                "PureQueryEvaluation "
                "ModelOccurrence:yyz.aerodynamics.coefficients -> "
                "AlgorithmConsumer:yyz.rigid-step."
                "aerodynamic-coefficients\n") != std::string::npos &&
                explain.find(
                    "obligation 2 obligation.yyz.closure-to-rigid "
                    "ClosureEvaluation "
                    "ModelOccurrence:yyz.closure.form-input -> "
                    "AlgorithmConsumer:yyz.rigid-step.form-input\n") !=
                    std::string::npos,
            "static dry-run explain lost query/closure obligations");
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
                          std::string_view(argv[1]) != "--explain" &&
                          std::string_view(argv[1]) !=
                              "--semantic-hash")) {
            std::cerr << "usage: gnc_compiler_static_plan_probe "
                         "--self-check|--explain|--semantic-hash\n";
            return 2;
        }
        const auto explain = run_self_check();
        if (std::string_view(argv[1]) == "--explain") {
            std::cout << explain;
        } else if (std::string_view(argv[1]) == "--semantic-hash") {
            const auto catalog_outcome = Catalog::build(
                package_descriptors());
            const auto& catalog = require_value(
                catalog_outcome,
                "semantic-hash Catalog build failed");
            std::cout << verify_semantic_hash(catalog) << '\n';
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
