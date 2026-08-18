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
using gnc::compiler::CompileOutcome;
using gnc::compiler::CompiledObligationKind;
using gnc::compiler::Diagnostic;
using gnc::compiler::DiagnosticCode;
using gnc::compiler::ExecutionPlanDescriptor;
using gnc::compiler::SourceAlgorithmOccurrence;
using gnc::compiler::SourceBinding;
using gnc::compiler::SourceModelOccurrence;
using gnc::compiler::SourceRef;
using gnc::compiler::SourceTree;
using gnc::compiler::StaticCompilation;
using gnc::model_sdk::ModelExecutionForm;

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
    std::is_same_v<decltype(gnc::compiler::PreparedModelPlan::execution_form),
                   ModelExecutionForm>,
    "prepared entries must preserve the accepted execution-form type");

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

[[nodiscard]] SourceTree source_tree() {
    SourceTree source;
    source.source_tree_version =
        std::string(gnc::compiler::kTypedSourceTreeVersion);
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
    require(plan.plan_id == kPlanId && plan.mission_id == kMissionId,
            "static plan identity changed");
    require(plan.dependency_lock.size() == 2U &&
                plan.prepared_models.size() == 2U &&
                plan.algorithms.size() == 2U &&
                plan.bindings.size() == 2U &&
                plan.binding_proofs.size() == 2U &&
                plan.obligations.size() == 2U,
            "static plan closure count changed");

    const auto& cavh_model = plan.prepared_models[0U];
    require(cavh_model.occurrence_id == "cavh.envelope" &&
                cavh_model.model_id ==
                    gnc::packages::cavh::kGlideEnvelopeModelIdentity &&
                cavh_model.execution_form == ModelExecutionForm::PureQuery &&
                cavh_model.preparation_algorithm_id ==
                    gnc::packages::cavh::
                        kGlideEnvelopePreparationIdentity.id,
            "CAVH plan entry lost its real query definition");
    const auto& yyz_model = plan.prepared_models[1U];
    require(yyz_model.occurrence_id == "yyz.closure" &&
                yyz_model.model_id ==
                    gnc::packages::yyz::
                        kForceMomentClosureModelIdentity &&
                yyz_model.execution_form == ModelExecutionForm::Closure &&
                yyz_model.preparation_algorithm_id ==
                    gnc::packages::yyz::
                        kForceMomentClosurePreparationIdentity.id,
            "YYZ plan entry lost its real closure definition");

    require(plan.algorithms[0U].composition_model_id ==
                gnc::packages::cavh::kCavhFormulaModelIdentity &&
                plan.algorithms[1U].composition_model_id ==
                    gnc::packages::yyz::kRigidStepModelIdentity,
            "algorithm consumers lost their product-model identities");
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

void verify_deterministic_order(std::string_view expected_explain) {
    auto packages = package_descriptors();
    std::reverse(packages.begin(), packages.end());
    const auto catalog_outcome = Catalog::build(std::move(packages));
    const auto& catalog =
        require_value(catalog_outcome, "reordered Catalog build failed");

    auto source = source_tree();
    std::reverse(source.models.begin(), source.models.end());
    std::reverse(source.algorithms.begin(), source.algorithms.end());
    std::reverse(source.bindings.begin(), source.bindings.end());
    const auto compile_outcome =
        gnc::compiler::compile_static_plan(source, catalog);
    const auto& compilation = require_value(
        compile_outcome, "reordered typed source compilation failed");
    require(gnc::compiler::explain_static_plan(compilation.plan) ==
                expected_explain,
            "Catalog or SourceTree insertion order changed the static plan");
}

void verify_negative_cases() {
    const auto catalog_outcome = Catalog::build(package_descriptors());
    const auto& catalog =
        require_value(catalog_outcome, "fixture Catalog build failed");

    auto unknown = source_tree();
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

    auto missing = source_tree();
    missing.bindings.erase(missing.bindings.begin() + 1);
    const auto missing_outcome =
        gnc::compiler::compile_static_plan(missing, catalog);
    require(!missing_outcome.value.has_value() &&
                has_diagnostic(missing_outcome.diagnostics,
                               DiagnosticCode::MissingRequiredBinding),
            "missing closure consumer binding produced a static plan");

    auto duplicate_target = source_tree();
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

    auto duplicate_occurrence = source_tree();
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
        source_tree(), incompatible_catalog);
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
    const auto compile_outcome =
        gnc::compiler::compile_static_plan(source_tree(), catalog);
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
        "prepare cavh.envelope "
        "gnc.package.cavh.glide-envelope.parabolic.experimental@1@0.1.0 "
        "PureQuery gnc.package.cavh.glide-envelope.prepare@1@0.1.0\n"
        "prepare yyz.closure "
        "gnc.package.yyz.force-moment-closure.frozen-interval.experimental@1"
        "@0.1.0 Closure "
        "gnc.package.yyz.force-moment-closure.prepare@1@0.1.0\n"
        "algorithm cavh.formula "
        "gnc.package.cavh.formula.composite@1@0.1.0 model "
        "gnc.package.cavh.formula.legacy-transcribed.experimental@1\n"
        "algorithm yyz.rigid-step "
        "gnc.package.yyz.rigid-step.kernel@1@0.1.0 model "
        "gnc.package.yyz.rigid-step.frozen-interval.experimental@1\n"
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
    verify_deterministic_order(explain);
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
            std::cout << "R2 typed SourceTree static plan self-check passed\n";
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "R2 static plan self-check failed: " << error.what()
                  << '\n';
        return 1;
    }
}
