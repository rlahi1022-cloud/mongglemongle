#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace monggle {

// Tiny Prometheus-compatible metrics registry.
// Externally lock-free for hot counters; mutex only for label expansion.
//
// Design: counters/gauges/histograms keyed by (name, sorted-label-pair-string).
// Render is /metrics text format. No third-party dep.
class Metrics {
public:
    using Labels = std::vector<std::pair<std::string, std::string>>;

    static Metrics& instance();

    // Counter — monotonically increasing.
    void incCounter(const std::string& name, const Labels& labels, std::int64_t by = 1);

    // Gauge — set absolute value.
    void setGauge(const std::string& name, const Labels& labels, double value);

    // Histogram — observe a single value (e.g., latency seconds).
    // Buckets are fixed to common HTTP latency ranges.
    void observeHistogram(const std::string& name, const Labels& labels, double valueSeconds);

    // Help text annotations (optional but nice for discoverability).
    void describeCounter  (const std::string& name, const std::string& help);
    void describeGauge    (const std::string& name, const std::string& help);
    void describeHistogram(const std::string& name, const std::string& help);

    // Render in Prometheus text exposition format.
    std::string render() const;

    static const std::vector<double>& defaultBuckets();

private:
    Metrics() = default;

    struct CounterSeries {
        std::atomic<std::int64_t> value{0};
    };
    struct GaugeSeries {
        std::atomic<std::uint64_t> bits{0}; // double bit-cast for atomic store
    };
    struct HistogramSeries {
        std::vector<std::atomic<std::int64_t>> bucketCounts; // size = buckets.size() + 1 (last = +Inf)
        std::atomic<std::int64_t>              count{0};
        std::atomic<std::uint64_t>             sumBits{0};

        explicit HistogramSeries(std::size_t n) : bucketCounts(n) {}
        HistogramSeries(const HistogramSeries&) = delete;
        HistogramSeries& operator=(const HistogramSeries&) = delete;
    };

    static std::string keyOf(const Labels& labels);
    static std::string renderLabels(const Labels& labels);

    mutable std::mutex                                                       mu_;
    std::map<std::string, std::map<std::string, CounterSeries>>              counters_;
    std::map<std::string, std::map<std::string, GaugeSeries>>                gauges_;
    std::map<std::string, std::map<std::string, std::shared_ptr<HistogramSeries>>> histograms_;
    std::map<std::string, std::string>                                       helpCounter_;
    std::map<std::string, std::string>                                       helpGauge_;
    std::map<std::string, std::string>                                       helpHistogram_;
    std::map<std::string, Labels>                                            labelOrder_; // for stable label rendering per series
};

} // namespace monggle
