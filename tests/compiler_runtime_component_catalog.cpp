#include <cavh/formula.hpp>
#include <gnc/compiler/static_mission_compiler.hpp>
#include <yyz/mass_commit.hpp>

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using gnc::compiler::Catalog;
using gnc::compiler::DiagnosticCode;
using gnc::model_sdk::BindingKind;
using gnc::model_sdk::CoarsePhase;
using gnc::model_sdk::HoldPolicy;
using gnc::model_sdk::ModelExecutionForm;
using gnc::model_sdk::ModelPlacement;
using gnc::model_sdk::PortCardinality;
using gnc::model_sdk::RuntimeCellProfile;
using gnc::model_sdk::RuntimeExecutionObligation;
using gnc::model_sdk::RuntimeLifecycleCapability;
using gnc::model_sdk::StaticModelDescriptor;
using gnc::model_sdk::StaticPackageDescriptor;
using gnc::model_sdk::StaticPortDirection;
using gnc::model_sdk::TemporalRelation;
using namespace gnc::packages::yyz;

void require(bool condition, std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

[[nodiscard]] bool has_diagnostic(
    const std::vector<gnc::compiler::Diagnostic>& diagnostics,
    DiagnosticCode code) {
    return std::any_of(
        diagnostics.begin(), diagnostics.end(),
        [code](const auto& diagnostic) {
            return diagnostic.code == code;
        });
}

[[nodiscard]] const StaticModelDescriptor& find_model(
    const StaticPackageDescriptor& package, std::string_view model_id) {
    const auto found = std::find_if(
        package.models.begin(), package.models.end(),
        [model_id](const auto& model) {
            return model.definition.model_id == model_id;
        });
    require(found != package.models.end(),
            "package contribution omitted the requested model");
    return *found;
}

[[nodiscard]] StaticModelDescriptor& find_model(
    StaticPackageDescriptor& package, std::string_view model_id) {
    const auto found = std::find_if(
        package.models.begin(), package.models.end(),
        [model_id](const auto& model) {
            return model.definition.model_id == model_id;
        });
    require(found != package.models.end(),
            "package contribution omitted the requested model");
    return *found;
}

[[nodiscard]] AltitudePitchGuidanceDefinition guidance_definition() {
    AltitudePitchGuidanceDefinition definition;
    definition.model_id =
        std::string(kAltitudePitchGuidanceModelIdentity);
    definition.model_version =
        std::string(kAltitudePitchGuidanceModelVersion);
    definition.inertial_frame.id =
        "frame.fixture.yyz.inertial-cartesian@1";
    definition.clock_domain.id = "clock.fixture.yyz.simulation@1";
    definition.configuration_revision = 11;
    definition.target_altitude_meters = 1000.0;
    definition.altitude_error_gain_radians_per_meter = 0.02;
    definition.vertical_speed_gain_radian_seconds_per_meter = 0.05;
    definition.pitch_command_limit_radians = 0.04;
    definition.attitude_policy.numerical = {
        2.0e-12,
        2.0e-12,
        gnc::foundation::FiniteCheck::EveryStage,
        1.0e-14,
        1.0e12,
    };
    definition.attitude_policy.normalization =
        gnc::foundation::QuaternionNormalizationPolicy::NormalizeWithFlag;
    return definition;
}

void expect_invalid_catalog(StaticPackageDescriptor package,
                            std::string_view message) {
    const auto outcome = Catalog::build({std::move(package)});
    require(!outcome.value.has_value() &&
                has_diagnostic(outcome.diagnostics,
                               DiagnosticCode::InvalidCatalogDescriptor),
            message);
}

[[nodiscard]] std::size_t verify_catalog() {
    auto yyz = describe_yyz_rigid_step_package();
    auto cavh = gnc::packages::cavh::describe_cavh_formula_package();
    const auto catalog_outcome = Catalog::build({yyz, cavh});
    require(catalog_outcome.succeeded(),
            "YYZ/CAVH Catalog rejected the runtime contribution");
    const auto& catalog = *catalog_outcome.value;
    const auto* record = catalog.find_model(
        kAltitudePitchGuidanceModelIdentity,
        kAltitudePitchGuidanceModelVersion);
    require(record != nullptr &&
                record->package.package_id == kYyzRigidStepPackageIdentity &&
                record->package.package_version ==
                    kYyzRigidStepPackageVersion,
            "package-independent exact RuntimeComponent lookup failed");

    const auto& guidance = record->descriptor;
    require(guidance.definition.execution_form ==
                ModelExecutionForm::RuntimeComponent &&
                guidance.placement == ModelPlacement::VehicleProcess &&
                guidance.preparation_algorithm_id.empty() &&
                guidance.preparation_algorithm_version.empty() &&
                guidance.runtime_component.has_value() &&
                guidance.asset_slots.empty() &&
                guidance.ports.size() == 2U,
            "RuntimeComponent top-level descriptor facts differ");
    const auto& runtime = *guidance.runtime_component;
    require(runtime.recipe_id == kAltitudePitchGuidanceRecipeIdentity &&
                runtime.profile == RuntimeCellProfile::SampledTransform &&
                runtime.obligations ==
                    std::vector<RuntimeExecutionObligation>{
                        RuntimeExecutionObligation::BoundaryEvaluation} &&
                runtime.state_schemas.empty() &&
                runtime.schedule.phase == CoarsePhase::Process &&
                runtime.schedule.step_interval == 1U &&
                runtime.schedule.offset == 0U &&
                runtime.schedule.output_hold ==
                    HoldPolicy::ZeroOrderHold &&
                runtime.schedule.max_input_age_steps == 0U &&
                runtime.lifecycle_capabilities ==
                    std::vector<RuntimeLifecycleCapability>{
                        RuntimeLifecycleCapability::Instantiate,
                        RuntimeLifecycleCapability::Dispose} &&
                runtime.algorithm_entry_id ==
                    kAltitudePitchGuidanceKernelIdentity.id &&
                runtime.algorithm_entry_version ==
                    kAltitudePitchGuidanceKernelIdentity.version,
            "SampledTransform recipe, schedule, lifecycle, or entry differs");
    require(guidance.ports[0U].direction ==
                StaticPortDirection::Input &&
                guidance.ports[0U].contract_id ==
                    kCommittedRigidObservationContractIdentity &&
                guidance.ports[0U].binding_kind ==
                    BindingKind::SampledSignal &&
                guidance.ports[0U].cardinality ==
                    PortCardinality::ExactlyOne &&
                guidance.ports[0U].temporal_relation ==
                    TemporalRelation::CurrentCycle &&
                guidance.ports[1U].direction ==
                    StaticPortDirection::Output &&
                guidance.ports[1U].contract_id ==
                    kAltitudePitchGuidanceOutputContractIdentity &&
                guidance.ports[1U].cardinality ==
                    PortCardinality::OneOrMore,
            "RuntimeComponent typed port contract differs");

    const auto invalid_prepare =
        gnc::model_sdk::prepare_model_metadata(
            guidance.definition,
            kAltitudePitchGuidanceKernelIdentity);
    require(!invalid_prepare.has_value(),
            "RuntimeComponent entered the PreparedModel-only path");

    const auto definition = guidance_definition();
    const auto configuration =
        canonical_altitude_pitch_guidance_config(definition);
    const auto rebuilt =
        build_altitude_pitch_guidance_definition(configuration);
    require(rebuilt.has_value() &&
                rebuilt.value().model_id == definition.model_id &&
                rebuilt.value().model_version == definition.model_version &&
                rebuilt.value().inertial_frame ==
                    definition.inertial_frame &&
                rebuilt.value().clock_domain == definition.clock_domain &&
                rebuilt.value().configuration_revision ==
                    definition.configuration_revision &&
                rebuilt.value().target_altitude_meters ==
                    definition.target_altitude_meters &&
                rebuilt.value().pitch_command_limit_radians ==
                    definition.pitch_command_limit_radians &&
                canonical_altitude_pitch_guidance_config(rebuilt.value()) ==
                    configuration,
            "guidance canonical configuration did not rebuild exactly");

    auto invalid_configuration = configuration;
    invalid_configuration.fields[0U].value = -0.01;
    require(!build_altitude_pitch_guidance_definition(
                 invalid_configuration)
                 .has_value(),
            "invalid guidance gain rebuilt from canonical configuration");

    const auto reversed_catalog = Catalog::build({cavh, yyz});
    require(reversed_catalog.succeeded() &&
                reversed_catalog.value->find_model(
                    kAltitudePitchGuidanceModelIdentity,
                    kAltitudePitchGuidanceModelVersion) != nullptr,
            "package declaration order changed Catalog lookup");

    auto duplicate = describe_yyz_rigid_step_package();
    duplicate.models.push_back(find_model(
        duplicate, kAltitudePitchGuidanceModelIdentity));
    const auto duplicate_outcome = Catalog::build({std::move(duplicate)});
    require(!duplicate_outcome.value.has_value() &&
                has_diagnostic(duplicate_outcome.diagnostics,
                               DiagnosticCode::DuplicateCatalogIdentity),
            "duplicate RuntimeComponent identity entered the Catalog");

    auto invalid_form = describe_yyz_rigid_step_package();
    find_model(invalid_form, kAltitudePitchGuidanceModelIdentity)
        .definition.execution_form = ModelExecutionForm::Closure;
    expect_invalid_catalog(std::move(invalid_form),
                           "RuntimeComponent facts entered Closure form");

    auto invalid_profile = describe_yyz_rigid_step_package();
    find_model(invalid_profile, kAltitudePitchGuidanceModelIdentity)
        .runtime_component->profile =
        static_cast<RuntimeCellProfile>(255U);
    expect_invalid_catalog(std::move(invalid_profile),
                           "invalid RuntimeCellProfile entered the Catalog");

    auto missing_runtime = describe_yyz_rigid_step_package();
    find_model(missing_runtime, kAltitudePitchGuidanceModelIdentity)
        .runtime_component.reset();
    expect_invalid_catalog(
        std::move(missing_runtime),
        "RuntimeComponent without runtime facts entered the Catalog");

    auto missing_recipe = describe_yyz_rigid_step_package();
    find_model(missing_recipe, kAltitudePitchGuidanceModelIdentity)
        .runtime_component->recipe_id.clear();
    expect_invalid_catalog(std::move(missing_recipe),
                           "RuntimeComponent without recipe entered Catalog");

    auto missing_obligation = describe_yyz_rigid_step_package();
    find_model(missing_obligation, kAltitudePitchGuidanceModelIdentity)
        .runtime_component->obligations.clear();
    expect_invalid_catalog(
        std::move(missing_obligation),
        "RuntimeComponent without obligation entered the Catalog");

    auto invalid_schedule = describe_yyz_rigid_step_package();
    find_model(invalid_schedule, kAltitudePitchGuidanceModelIdentity)
        .runtime_component->schedule.step_interval = 0U;
    expect_invalid_catalog(
        std::move(invalid_schedule),
        "RuntimeComponent without integer schedule entered the Catalog");

    auto invalid_lifecycle = describe_yyz_rigid_step_package();
    find_model(invalid_lifecycle, kAltitudePitchGuidanceModelIdentity)
        .runtime_component->lifecycle_capabilities = {
        RuntimeLifecycleCapability::Instantiate};
    expect_invalid_catalog(
        std::move(invalid_lifecycle),
        "incomplete stateless lifecycle entered the Catalog");

    auto runtime_on_query = describe_yyz_rigid_step_package();
    const auto runtime_facts =
        find_model(runtime_on_query,
                   kAltitudePitchGuidanceModelIdentity)
            .runtime_component;
    find_model(runtime_on_query, kAerodynamicTableModelIdentity)
        .runtime_component = runtime_facts;
    expect_invalid_catalog(
        std::move(runtime_on_query),
        "PureQuery carried RuntimeComponent-only fields");

    auto no_runtime_input = describe_yyz_rigid_step_package();
    auto& no_input_model = find_model(
        no_runtime_input, kAltitudePitchGuidanceModelIdentity);
    no_input_model.ports.erase(no_input_model.ports.begin());
    expect_invalid_catalog(std::move(no_runtime_input),
                           "RuntimeComponent without input entered Catalog");

    gnc::compiler::TypedStaticCompositionSource source;
    source.source_version = std::string(
        gnc::compiler::kTypedStaticCompositionSourceVersion);
    source.mission_id = "mission.r2.runtime-catalog-selection@1";
    source.mission_source = {"typed://runtime-catalog-test", "/mission"};
    source.model_occurrences.push_back({
        "guidance",
        std::string(kAltitudePitchGuidanceModelIdentity),
        std::string(kAltitudePitchGuidanceModelVersion),
        {"typed://runtime-catalog-test", "/models/guidance"},
        {},
        {}});
    const auto unavailable =
        gnc::compiler::build_canonical_mission_ir(source, catalog);
    require(!unavailable.value.has_value() &&
                has_diagnostic(
                    unavailable.diagnostics,
                    DiagnosticCode::RuntimeComponentPlanUnavailable),
            "Catalog selection silently lowered an unclosed runtime graph");

    source.model_occurrences[0U].model_id += ".missing";
    const auto unknown =
        gnc::compiler::build_canonical_mission_ir(source, catalog);
    require(!unknown.value.has_value() &&
                has_diagnostic(unknown.diagnostics,
                               DiagnosticCode::UnknownDefinition),
            "unknown RuntimeComponent definition was accepted");
    return 18U;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string_view(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_compiler_runtime_component_catalog_probe "
                     "--self-check\n";
        return 2;
    }
    try {
        const auto checks = verify_catalog();
        std::cout
            << "{\"schema_version\":\"gnczmkn.compiler-runtime-catalog/1\","
               "\"status\":\"passed\",\"checks\":"
            << checks << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
