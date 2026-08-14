#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr const char* kOracleId = "ORACLE-YYZ-CSV-05";
constexpr const char* kTimeColumn = "time";
constexpr const char* kAltitudeColumn =
    "vehicle.dynamics.position.z";
constexpr const char* kVelocityColumn =
    "vehicle.dynamics.velocity.z";
constexpr double kTimeTolerance = 1.0e-12;
constexpr double kStateTolerance = 1.0e-9;

struct SemanticRow {
    std::size_t sample_index = 0;
    double sample_time_s = 0.0;
    double altitude_m = 0.0;
    double vertical_velocity_mps = 0.0;
};

using EncodedRows = std::vector<std::vector<double>>;

struct ProbeResult {
    std::vector<SemanticRow> rows;
    bool column_permutation_accepted = false;
    bool missing_t0_rejected = false;
    bool shifted_tk_rejected = false;
    bool stale_published_state_rejected = false;
    bool duplicate_required_header_rejected = false;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

bool nearlyEqual(double lhs, double rhs, double tolerance) {
    return std::abs(lhs - rhs) <= tolerance;
}

std::vector<SemanticRow> analyticRows() {
    constexpr double altitude0_m = 1000.0;
    constexpr double velocity0_mps = 10.0;
    constexpr double acceleration_mps2 = -2.0;
    const std::vector<double> sample_times{0.0, 0.5, 1.0};

    std::vector<SemanticRow> result;
    result.reserve(sample_times.size());
    for (std::size_t index = 0; index < sample_times.size(); ++index) {
        const double time_s = sample_times[index];
        result.push_back({
            index,
            time_s,
            altitude0_m + velocity0_mps * time_s +
                0.5 * acceleration_mps2 * time_s * time_s,
            velocity0_mps + acceleration_mps2 * time_s,
        });
    }
    return result;
}

std::vector<SemanticRow> decodeByHeader(
    const std::vector<std::string>& header,
    const EncodedRows& encoded_rows) {
    std::unordered_map<std::string, std::size_t> index_by_name;
    for (std::size_t index = 0; index < header.size(); ++index) {
        require(index_by_name.emplace(header[index], index).second,
                "encoded dataset contains a duplicate header");
    }
    for (const char* required :
         {kTimeColumn, kAltitudeColumn, kVelocityColumn}) {
        require(index_by_name.count(required) == 1,
                std::string("encoded dataset is missing ") + required);
    }

    std::vector<SemanticRow> result;
    result.reserve(encoded_rows.size());
    for (std::size_t row_index = 0;
         row_index < encoded_rows.size();
         ++row_index) {
        const auto& encoded = encoded_rows[row_index];
        require(encoded.size() == header.size(),
                "encoded row width differs from its header");
        result.push_back({
            row_index,
            encoded[index_by_name.at(kTimeColumn)],
            encoded[index_by_name.at(kAltitudeColumn)],
            encoded[index_by_name.at(kVelocityColumn)],
        });
    }
    return result;
}

bool sameRows(const std::vector<SemanticRow>& lhs,
              const std::vector<SemanticRow>& rhs) {
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (lhs[index].sample_index != rhs[index].sample_index ||
            !nearlyEqual(lhs[index].sample_time_s,
                         rhs[index].sample_time_s,
                         kTimeTolerance) ||
            !nearlyEqual(lhs[index].altitude_m,
                         rhs[index].altitude_m,
                         kStateTolerance) ||
            !nearlyEqual(lhs[index].vertical_velocity_mps,
                         rhs[index].vertical_velocity_mps,
                         kStateTolerance)) {
            return false;
        }
    }
    return true;
}

bool validDataset(const std::vector<SemanticRow>& rows) {
    return sameRows(rows, analyticRows());
}

template <typename Function>
bool rejected(Function&& function) {
    try {
        function();
        return false;
    } catch (const std::exception&) {
        return true;
    }
}

ProbeResult runProbe() {
    const std::vector<std::string> canonical_header{
        kTimeColumn, kAltitudeColumn, kVelocityColumn, "unused.encoding"};
    const EncodedRows canonical_encoded{
        {0.0, 1000.0, 10.0, 101.0},
        {0.5, 1004.75, 9.0, 102.0},
        {1.0, 1009.0, 8.0, 103.0},
    };

    ProbeResult result;
    result.rows = decodeByHeader(canonical_header, canonical_encoded);

    const std::vector<std::string> permuted_header{
        "unused.encoding", kVelocityColumn, kTimeColumn, kAltitudeColumn};
    const EncodedRows permuted_encoded{
        {101.0, 10.0, 0.0, 1000.0},
        {102.0, 9.0, 0.5, 1004.75},
        {103.0, 8.0, 1.0, 1009.0},
    };
    result.column_permutation_accepted = sameRows(
        result.rows, decodeByHeader(permuted_header, permuted_encoded));

    auto missing_t0 = result.rows;
    missing_t0.erase(missing_t0.begin());
    result.missing_t0_rejected = !validDataset(missing_t0);

    auto shifted_tk = result.rows;
    shifted_tk[1].sample_time_s = 0.75;
    result.shifted_tk_rejected = !validDataset(shifted_tk);

    auto stale_state = result.rows;
    stale_state[1].altitude_m = 1000.0;
    result.stale_published_state_rejected = !validDataset(stale_state);

    auto duplicate_header = canonical_header;
    duplicate_header[3] = kAltitudeColumn;
    result.duplicate_required_header_rejected = rejected([&]() {
        (void)decodeByHeader(duplicate_header, canonical_encoded);
    });
    return result;
}

void writeJson(const ProbeResult& result) {
    std::cout << std::setprecision(17)
              << "{\"oracle_id\":\"" << kOracleId
              << "\",\"status\":\"passed\",\"semantic_rows\":[";
    for (std::size_t index = 0; index < result.rows.size(); ++index) {
        if (index > 0) {
            std::cout << ',';
        }
        const auto& row = result.rows[index];
        std::cout << "{\"sample_index\":" << row.sample_index
                  << ",\"sample_time_s\":" << row.sample_time_s
                  << ",\"altitude_m\":" << row.altitude_m
                  << ",\"vertical_velocity_mps\":"
                  << row.vertical_velocity_mps << '}';
    }
    std::cout << "]"
              << ",\"column_permutation_accepted\":"
              << (result.column_permutation_accepted ? "true" : "false")
              << ",\"missing_t0_rejected\":"
              << (result.missing_t0_rejected ? "true" : "false")
              << ",\"shifted_tk_rejected\":"
              << (result.shifted_tk_rejected ? "true" : "false")
              << ",\"stale_published_state_rejected\":"
              << (result.stale_published_state_rejected ? "true" : "false")
              << ",\"duplicate_required_header_rejected\":"
              << (result.duplicate_required_header_rejected ? "true" : "false")
              << "}\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2 || std::string(argv[1]) != "--self-check") {
        std::cerr << "usage: gnc_legacy_csv_probe --self-check\n";
        return 2;
    }

    try {
        const ProbeResult result = runProbe();
        require(validDataset(result.rows),
                "analytic CSV semantic rows differ");
        require(result.column_permutation_accepted,
                "column permutation changed semantic rows");
        require(result.missing_t0_rejected,
                "missing initial sample was accepted");
        require(result.shifted_tk_rejected,
                "shifted t_k sample was accepted");
        require(result.stale_published_state_rejected,
                "stale published state was accepted");
        require(result.duplicate_required_header_rejected,
                "duplicate required header was accepted");
        writeJson(result);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
