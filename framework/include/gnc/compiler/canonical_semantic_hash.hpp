#pragma once

#include "gnc/compiler/static_mission_compiler.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace gnc::compiler {

inline constexpr std::string_view kCanonicalSemanticEncodingIdentity =
    "gnc.canonical-mission-ir.semantic-bytes@2";
inline constexpr std::string_view kCanonicalSemanticHashAlgorithm =
    "SHA-256";

struct CanonicalSemanticHash {
    std::string encoding_id;
    std::string algorithm;
    std::string hex_digest;
};

namespace semantic_hash_detail {

class Encoder {
  public:
    void record(std::uint32_t record_tag) {
        bytes_.push_back(0x08U);
        raw_u32(record_tag);
    }

    void string(std::string_view value) {
        bytes_.push_back(0x01U);
        raw_length(value.size());
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

    void uint32(std::uint32_t value) {
        bytes_.push_back(0x02U);
        raw_u32(value);
    }

    void integer(std::int64_t value) {
        bytes_.push_back(0x03U);
        raw_u64(static_cast<std::uint64_t>(value));
    }

    void enumeration(std::uint32_t domain, std::uint32_t value) {
        bytes_.push_back(0x04U);
        bytes_.push_back(0x01U);
        raw_u32(domain);
        raw_u32(value);
    }

    void enum_token(std::string_view value) {
        bytes_.push_back(0x04U);
        bytes_.push_back(0x02U);
        raw_length(value.size());
        bytes_.insert(bytes_.end(), value.begin(), value.end());
    }

    void float64(double value) {
        static_assert(sizeof(double) == sizeof(std::uint64_t),
                      "canonical encoding requires binary64 storage");
        static_assert(std::numeric_limits<double>::is_iec559,
                      "canonical encoding requires IEC 60559 binary64");
        std::uint64_t bits = 0U;
        std::memcpy(&bits, &value, sizeof(bits));
        bytes_.push_back(0x05U);
        raw_u64(bits);
    }

    void collection(std::size_t count) {
        bytes_.push_back(0x06U);
        raw_length(count);
    }

    void optional(bool present) {
        bytes_.push_back(0x07U);
        bytes_.push_back(present ? 1U : 0U);
    }

    [[nodiscard]] const std::vector<std::uint8_t>& bytes() const noexcept {
        return bytes_;
    }

  private:
    void raw_length(std::size_t value) {
        if (value >
            static_cast<std::size_t>(
                std::numeric_limits<std::uint32_t>::max())) {
            throw std::length_error("canonical semantic value is too large");
        }
        raw_u32(static_cast<std::uint32_t>(value));
    }

    void raw_u32(std::uint32_t value) {
        for (int shift = 24; shift >= 0; shift -= 8) {
            bytes_.push_back(static_cast<std::uint8_t>(
                (value >> static_cast<unsigned int>(shift)) & 0xffU));
        }
    }

    void raw_u64(std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            bytes_.push_back(static_cast<std::uint8_t>(
                (value >> static_cast<unsigned int>(shift)) & 0xffU));
        }
    }

    std::vector<std::uint8_t> bytes_;
};

[[nodiscard]] inline std::uint32_t rotate_right(
    std::uint32_t value, unsigned int count) noexcept {
    return (value >> count) | (value << (32U - count));
}

[[nodiscard]] inline std::string sha256_hex(
    const std::vector<std::uint8_t>& input) {
    static constexpr std::array<std::uint32_t, 64U> kRoundConstants{
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
        0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
        0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
        0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
    };
    std::vector<std::uint8_t> message = input;
    const auto bit_length =
        static_cast<std::uint64_t>(message.size()) * 8U;
    message.push_back(0x80U);
    while ((message.size() % 64U) != 56U) {
        message.push_back(0U);
    }
    for (int shift = 56; shift >= 0; shift -= 8) {
        message.push_back(static_cast<std::uint8_t>(
            (bit_length >> static_cast<unsigned int>(shift)) & 0xffU));
    }

    std::array<std::uint32_t, 8U> state{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    };
    for (std::size_t offset = 0U; offset < message.size(); offset += 64U) {
        std::array<std::uint32_t, 64U> words{};
        for (std::size_t index = 0U; index < 16U; ++index) {
            const auto base = offset + index * 4U;
            words[index] =
                (static_cast<std::uint32_t>(message[base]) << 24U) |
                (static_cast<std::uint32_t>(message[base + 1U]) << 16U) |
                (static_cast<std::uint32_t>(message[base + 2U]) << 8U) |
                static_cast<std::uint32_t>(message[base + 3U]);
        }
        for (std::size_t index = 16U; index < words.size(); ++index) {
            const auto s0 = rotate_right(words[index - 15U], 7U) ^
                            rotate_right(words[index - 15U], 18U) ^
                            (words[index - 15U] >> 3U);
            const auto s1 = rotate_right(words[index - 2U], 17U) ^
                            rotate_right(words[index - 2U], 19U) ^
                            (words[index - 2U] >> 10U);
            words[index] = words[index - 16U] + s0 +
                           words[index - 7U] + s1;
        }

        auto a = state[0U];
        auto b = state[1U];
        auto c = state[2U];
        auto d = state[3U];
        auto e = state[4U];
        auto f = state[5U];
        auto g = state[6U];
        auto h = state[7U];
        for (std::size_t index = 0U; index < words.size(); ++index) {
            const auto sum1 = rotate_right(e, 6U) ^
                              rotate_right(e, 11U) ^
                              rotate_right(e, 25U);
            const auto choose = (e & f) ^ ((~e) & g);
            const auto temporary1 = h + sum1 + choose +
                                    kRoundConstants[index] + words[index];
            const auto sum0 = rotate_right(a, 2U) ^
                              rotate_right(a, 13U) ^
                              rotate_right(a, 22U);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temporary2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        state[0U] += a;
        state[1U] += b;
        state[2U] += c;
        state[3U] += d;
        state[4U] += e;
        state[5U] += f;
        state[6U] += g;
        state[7U] += h;
    }

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (const auto word : state) {
        stream << std::setw(8) << word;
    }
    return stream.str();
}

template <typename Value, typename Key>
[[nodiscard]] inline bool strictly_sorted_by(
    const std::vector<Value>& values, Key key) {
    for (std::size_t index = 1U; index < values.size(); ++index) {
        if (!(key(values[index - 1U]) < key(values[index]))) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] inline std::optional<std::string> validate_canonical_ir(
    const CanonicalMissionIr& ir) {
    if (ir.revision != 2U || ir.mission_id.empty()) {
        return "mission revision and identity must be canonical";
    }
    if (!strictly_sorted_by(
            ir.entities,
            [](const CanonicalEntity& value) {
                return value.entity_id;
            })) {
        return "entity collection is not in unique canonical order";
    }
    std::set<std::string> entity_ids;
    for (const auto& entity : ir.entities) {
        if (entity.entity_id.empty() ||
            entity.lifecycle != EntityLifecycle::ActiveAtInitialize ||
            !entity_ids.insert(entity.entity_id).second) {
            return "entity semantics are invalid or duplicated";
        }
    }
    if (!strictly_sorted_by(
            ir.scopes,
            [](const CanonicalScope& value) {
                return value.key;
            })) {
        return "scope collection is not in unique canonical order";
    }
    std::set<ScopeKey> scopes;
    for (const auto& scope : ir.scopes) {
        if (scope.key.kind != ScopeKind::Vehicle ||
            scope.key.subject_entity_id.empty() ||
            entity_ids.find(scope.key.subject_entity_id) ==
                entity_ids.end() ||
            !scopes.insert(scope.key).second) {
            return "Vehicle scope is invalid, duplicated, or unanchored";
        }
    }
    if (!strictly_sorted_by(
            ir.model_occurrences,
            [](const CanonicalModelOccurrence& value) {
                return value.occurrence_id;
            })) {
        return "model collection is not in unique canonical order";
    }
    std::map<std::string, const CanonicalModelOccurrence*> models;
    for (const auto& model : ir.model_occurrences) {
        if (model.occurrence_id.empty() || model.package.package_id.empty() ||
            model.package.package_version.empty() || model.model_id.empty() ||
            model.model_version.empty() ||
            !gnc::model_sdk::valid_model_execution_form(
                model.execution_form) ||
            !gnc::model_sdk::valid_model_placement(model.placement) ||
            model.preparation_algorithm_id.empty() ||
            model.preparation_algorithm_version.empty()) {
            return "model occurrence identity or package policy is invalid";
        }
        if (!model.subject_entity_id.empty() &&
            entity_ids.find(model.subject_entity_id) == entity_ids.end()) {
            return "model occurrence subject is absent";
        }
        if (model.scope.has_value() &&
            (scopes.find(*model.scope) == scopes.end() ||
             model.scope->kind != ScopeKind::Vehicle ||
             model.scope->subject_entity_id !=
                 model.subject_entity_id)) {
            return "model occurrence scope and subject are inconsistent";
        }
        if (!strictly_sorted_by(
                model.output_ports,
                [](const CanonicalPort& value) {
                    return value.port_id;
                })) {
            return "model output ports are not in unique canonical order";
        }
        for (const auto& port : model.output_ports) {
            const auto expected_kind =
                model.execution_form ==
                        gnc::model_sdk::ModelExecutionForm::PureQuery
                    ? gnc::model_sdk::BindingKind::PureQuery
                    : gnc::model_sdk::BindingKind::
                          ContinuousClosureLink;
            if (port.port_id.empty() || port.contract_id.empty() ||
                port.binding_kind != expected_kind ||
                port.cardinality !=
                    gnc::model_sdk::PortCardinality::OneOrMore ||
                !gnc::model_sdk::valid_temporal_relation(
                    port.temporal_relation) ||
                (port.binding_kind ==
                         gnc::model_sdk::BindingKind::PureQuery &&
                 port.temporal_relation !=
                     gnc::model_sdk::TemporalRelation::NotApplicable) ||
                (port.binding_kind ==
                         gnc::model_sdk::BindingKind::
                             ContinuousClosureLink &&
                 port.temporal_relation ==
                     gnc::model_sdk::TemporalRelation::NotApplicable)) {
                return "model output port binding semantics are invalid";
            }
        }
        const auto& configuration = model.configuration;
        if (configuration.schema_id.empty() ||
            configuration.schema_version == 0U ||
            configuration.fields.empty() ||
            !strictly_sorted_by(
                configuration.fields,
                [](const gnc::model_sdk::CanonicalConfigField& value) {
                    return value.field_id;
                })) {
            return "model configuration is empty or noncanonical";
        }
        for (const auto& field : configuration.fields) {
            if (field.field_id.empty() ||
                !detail::valid_canonical_config_value(field.value)) {
                return "model configuration contains a noncanonical value";
            }
        }
        if (!strictly_sorted_by(
                model.asset_bindings,
                [](const CanonicalAssetBinding& value) {
                    return value.role;
                })) {
            return "asset bindings are not in unique canonical order";
        }
        for (const auto& asset : model.asset_bindings) {
            if (asset.role.empty() || asset.asset_schema_id.empty() ||
                asset.asset_id.empty() ||
                asset.cardinality !=
                    gnc::model_sdk::PortCardinality::ExactlyOne) {
                return "asset binding identity is invalid";
            }
        }
        models.emplace(model.occurrence_id, &model);
    }

    if (!strictly_sorted_by(
            ir.algorithm_consumers,
            [](const CanonicalAlgorithmConsumer& value) {
                return value.consumer_id;
            })) {
        return "algorithm collection is not in unique canonical order";
    }
    std::map<std::string, const CanonicalAlgorithmConsumer*> algorithms;
    for (const auto& algorithm : ir.algorithm_consumers) {
        if (algorithm.consumer_id.empty() ||
            algorithm.package.package_id.empty() ||
            algorithm.package.package_version.empty() ||
            algorithm.algorithm_id.empty() ||
            algorithm.algorithm_version.empty() ||
            !strictly_sorted_by(
                algorithm.input_ports,
                [](const CanonicalPort& value) {
                    return value.port_id;
                })) {
            return "algorithm consumer identity or ports are noncanonical";
        }
        if (algorithm.scope.has_value() &&
            scopes.find(*algorithm.scope) == scopes.end()) {
            return "algorithm consumer scope is undeclared";
        }
        for (const auto& port : algorithm.input_ports) {
            if (port.port_id.empty() || port.contract_id.empty() ||
                !gnc::model_sdk::valid_binding_kind(
                    port.binding_kind) ||
                port.binding_kind ==
                    gnc::model_sdk::BindingKind::AssetBinding ||
                port.cardinality !=
                    gnc::model_sdk::PortCardinality::ExactlyOne ||
                !gnc::model_sdk::valid_temporal_relation(
                    port.temporal_relation) ||
                (port.binding_kind ==
                         gnc::model_sdk::BindingKind::PureQuery &&
                 port.temporal_relation !=
                     gnc::model_sdk::TemporalRelation::NotApplicable) ||
                (port.binding_kind ==
                         gnc::model_sdk::BindingKind::
                             ContinuousClosureLink &&
                 port.temporal_relation ==
                     gnc::model_sdk::TemporalRelation::NotApplicable)) {
                return "algorithm input port binding semantics are invalid";
            }
        }
        algorithms.emplace(algorithm.consumer_id, &algorithm);
    }

    if (!strictly_sorted_by(
            ir.binding_intents,
            [](const CanonicalBindingIntent& value) {
                return value.binding_id;
            })) {
        return "binding collection is not in unique canonical order";
    }
    for (const auto& binding : ir.binding_intents) {
        const auto model = models.find(binding.provider_occurrence_id);
        const auto algorithm = algorithms.find(binding.consumer_id);
        const CanonicalPort* provider_port = nullptr;
        const CanonicalPort* consumer_port = nullptr;
        if (model != models.end()) {
            provider_port = detail::find_port(
                model->second->output_ports,
                binding.provider_port_id);
        }
        if (algorithm != algorithms.end()) {
            consumer_port = detail::find_port(
                algorithm->second->input_ports,
                binding.consumer_port_id);
        }
        if (binding.binding_id.empty() || model == models.end() ||
            algorithm == algorithms.end() ||
            provider_port == nullptr || consumer_port == nullptr) {
            return "binding endpoints are invalid";
        }
        if (provider_port->contract_id != consumer_port->contract_id ||
            provider_port->binding_kind != consumer_port->binding_kind ||
            provider_port->temporal_relation !=
                consumer_port->temporal_relation) {
            return "binding contract, kind, or temporal semantics differ";
        }
        if (model->second->scope.has_value() !=
                algorithm->second->scope.has_value() ||
            (model->second->scope.has_value() &&
             !(*model->second->scope ==
               *algorithm->second->scope))) {
            return "binding endpoint scopes differ";
        }
    }
    return std::nullopt;
}

inline void encode_config_value(
    Encoder& encoder,
    const gnc::model_sdk::CanonicalConfigValue& value) {
    if (const auto* text = std::get_if<std::string>(&value)) {
        encoder.string(*text);
    } else if (const auto* integer =
                   std::get_if<std::int64_t>(&value)) {
        encoder.integer(*integer);
    } else if (const auto* token =
                   std::get_if<gnc::model_sdk::CanonicalEnumValue>(&value)) {
        encoder.enum_token(token->token);
    } else {
        encoder.float64(std::get<double>(value));
    }
}

[[nodiscard]] inline std::vector<std::uint8_t> encode(
    const CanonicalMissionIr& ir) {
    Encoder encoder;
    encoder.record(1U);
    encoder.string(kCanonicalSemanticEncodingIdentity);
    encoder.uint32(ir.revision);
    encoder.string(ir.mission_id);

    encoder.collection(ir.entities.size());
    for (const auto& entity : ir.entities) {
        encoder.record(2U);
        encoder.string(entity.entity_id);
        encoder.enumeration(
            1U, static_cast<std::uint32_t>(entity.lifecycle));
    }

    encoder.collection(ir.scopes.size());
    for (const auto& scope : ir.scopes) {
        encoder.record(3U);
        encoder.enumeration(
            2U, static_cast<std::uint32_t>(scope.key.kind));
        encoder.string(scope.key.subject_entity_id);
    }

    encoder.collection(ir.model_occurrences.size());
    for (const auto& model : ir.model_occurrences) {
        encoder.record(4U);
        encoder.string(model.occurrence_id);
        encoder.string(model.package.package_id);
        encoder.string(model.package.package_version);
        encoder.string(model.model_id);
        encoder.string(model.model_version);
        encoder.enumeration(
            3U, static_cast<std::uint32_t>(model.execution_form));
        encoder.enumeration(
            4U, static_cast<std::uint32_t>(model.placement));
        encoder.string(model.preparation_algorithm_id);
        encoder.string(model.preparation_algorithm_version);
        encoder.optional(!model.subject_entity_id.empty());
        if (!model.subject_entity_id.empty()) {
            encoder.string(model.subject_entity_id);
        }
        encoder.optional(model.scope.has_value());
        if (model.scope.has_value()) {
            encoder.enumeration(
                2U, static_cast<std::uint32_t>(model.scope->kind));
            encoder.string(model.scope->subject_entity_id);
        }
        encoder.collection(model.output_ports.size());
        for (const auto& port : model.output_ports) {
            encoder.record(5U);
            encoder.string(port.port_id);
            encoder.string(port.contract_id);
            encoder.enumeration(
                5U, static_cast<std::uint32_t>(port.binding_kind));
            encoder.enumeration(
                6U, static_cast<std::uint32_t>(port.cardinality));
            encoder.enumeration(
                7U, static_cast<std::uint32_t>(
                        port.temporal_relation));
        }
        encoder.record(6U);
        encoder.string(model.configuration.schema_id);
        encoder.uint32(model.configuration.schema_version);
        encoder.collection(model.configuration.fields.size());
        for (const auto& field : model.configuration.fields) {
            encoder.record(7U);
            encoder.string(field.field_id);
            encode_config_value(encoder, field.value);
        }
        encoder.collection(model.asset_bindings.size());
        for (const auto& asset : model.asset_bindings) {
            encoder.record(8U);
            encoder.string(asset.role);
            encoder.string(asset.asset_schema_id);
            encoder.string(asset.asset_id);
            encoder.enumeration(
                6U, static_cast<std::uint32_t>(asset.cardinality));
        }
    }

    encoder.collection(ir.algorithm_consumers.size());
    for (const auto& algorithm : ir.algorithm_consumers) {
        encoder.record(9U);
        encoder.string(algorithm.consumer_id);
        encoder.string(algorithm.package.package_id);
        encoder.string(algorithm.package.package_version);
        encoder.string(algorithm.algorithm_id);
        encoder.string(algorithm.algorithm_version);
        encoder.optional(algorithm.scope.has_value());
        if (algorithm.scope.has_value()) {
            encoder.enumeration(
                2U, static_cast<std::uint32_t>(
                        algorithm.scope->kind));
            encoder.string(algorithm.scope->subject_entity_id);
        }
        encoder.collection(algorithm.input_ports.size());
        for (const auto& port : algorithm.input_ports) {
            encoder.record(5U);
            encoder.string(port.port_id);
            encoder.string(port.contract_id);
            encoder.enumeration(
                5U, static_cast<std::uint32_t>(port.binding_kind));
            encoder.enumeration(
                6U, static_cast<std::uint32_t>(port.cardinality));
            encoder.enumeration(
                7U, static_cast<std::uint32_t>(
                        port.temporal_relation));
        }
    }

    encoder.collection(ir.binding_intents.size());
    for (const auto& binding : ir.binding_intents) {
        encoder.record(10U);
        encoder.string(binding.binding_id);
        encoder.string(binding.provider_occurrence_id);
        encoder.string(binding.provider_port_id);
        encoder.string(binding.consumer_id);
        encoder.string(binding.consumer_port_id);
    }
    return encoder.bytes();
}

} // namespace semantic_hash_detail

[[nodiscard]] inline CompileOutcome<CanonicalSemanticHash>
hash_canonical_mission_ir(const CanonicalMissionIr& ir) {
    CompileOutcome<CanonicalSemanticHash> outcome;
    if (const auto error =
            semantic_hash_detail::validate_canonical_ir(ir)) {
        outcome.diagnostics.push_back(
            {DiagnosticCode::NonCanonicalIr, ir.mission_source,
             ir.mission_id, *error});
        return outcome;
    }
    try {
        const auto bytes = semantic_hash_detail::encode(ir);
        outcome.value = CanonicalSemanticHash{
            std::string(kCanonicalSemanticEncodingIdentity),
            std::string(kCanonicalSemanticHashAlgorithm),
            semantic_hash_detail::sha256_hex(bytes)};
    } catch (const std::length_error& error) {
        outcome.diagnostics.push_back(
            {DiagnosticCode::NonCanonicalIr, ir.mission_source,
             ir.mission_id, error.what()});
    }
    return outcome;
}

} // namespace gnc::compiler
