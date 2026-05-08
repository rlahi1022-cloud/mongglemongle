#include "monggle/metrics/metrics.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <sstream>

namespace monggle {

namespace {

double doubleFromBits(std::uint64_t bits) {
    double d;
    std::memcpy(&d, &bits, sizeof(d));
    return d;
}

std::uint64_t bitsFromDouble(double d) {
    std::uint64_t bits;
    std::memcpy(&bits, &d, sizeof(bits));
    return bits;
}

std::string escapeLabelValue(const std::string& v) {
    std::string out;
    out.reserve(v.size());
    for (char c : v) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            default:   out.push_back(c);
        }
    }
    return out;
}

} // namespace

Metrics& Metrics::instance() {
    static Metrics inst;
    return inst;
}

const std::vector<double>& Metrics::defaultBuckets() {
    // 0.5ms … 10s — covers cached responses up to slow DB queries.
    static const std::vector<double> b = {
        0.0005, 0.001, 0.0025, 0.005, 0.01,
        0.025, 0.05, 0.1, 0.25, 0.5, 1.0, 2.5, 5.0, 10.0
    };
    return b;
}

std::string Metrics::keyOf(const Labels& labels) {
    Labels sorted = labels;
    std::sort(sorted.begin(), sorted.end());
    std::ostringstream os;
    for (auto& [k, v] : sorted) {
        os << k << '=' << v << ';';
    }
    return os.str();
}

std::string Metrics::renderLabels(const Labels& labels) {
    if (labels.empty()) return {};
    std::ostringstream os;
    os << '{';
    for (std::size_t i = 0; i < labels.size(); ++i) {
        if (i) os << ',';
        os << labels[i].first << "=\"" << escapeLabelValue(labels[i].second) << '"';
    }
    os << '}';
    return os.str();
}

void Metrics::incCounter(const std::string& name, const Labels& labels, std::int64_t by) {
    auto key = keyOf(labels);
    std::lock_guard<std::mutex> lk(mu_);
    auto& series = counters_[name][key];
    if (labelOrder_.find(name + "|" + key) == labelOrder_.end()) {
        labelOrder_[name + "|" + key] = labels;
    }
    series.value.fetch_add(by, std::memory_order_relaxed);
}

void Metrics::setGauge(const std::string& name, const Labels& labels, double value) {
    auto key = keyOf(labels);
    std::lock_guard<std::mutex> lk(mu_);
    auto& series = gauges_[name][key];
    if (labelOrder_.find(name + "|" + key) == labelOrder_.end()) {
        labelOrder_[name + "|" + key] = labels;
    }
    series.bits.store(bitsFromDouble(value), std::memory_order_relaxed);
}

void Metrics::observeHistogram(const std::string& name, const Labels& labels, double valueSeconds) {
    auto key = keyOf(labels);
    std::shared_ptr<HistogramSeries> h;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto& slot = histograms_[name][key];
        if (!slot) {
            slot = std::make_shared<HistogramSeries>(defaultBuckets().size() + 1);
        }
        if (labelOrder_.find(name + "|" + key) == labelOrder_.end()) {
            labelOrder_[name + "|" + key] = labels;
        }
        h = slot;
    }
    const auto& buckets = defaultBuckets();
    bool placed = false;
    for (std::size_t i = 0; i < buckets.size(); ++i) {
        if (valueSeconds <= buckets[i]) {
            h->bucketCounts[i].fetch_add(1, std::memory_order_relaxed);
            placed = true;
            break;
        }
    }
    if (!placed) {
        h->bucketCounts.back().fetch_add(1, std::memory_order_relaxed);
    }
    h->count.fetch_add(1, std::memory_order_relaxed);
    // Atomic add for double via CAS.
    std::uint64_t prev = h->sumBits.load(std::memory_order_relaxed);
    while (true) {
        double cur  = doubleFromBits(prev);
        std::uint64_t next = bitsFromDouble(cur + valueSeconds);
        if (h->sumBits.compare_exchange_weak(prev, next, std::memory_order_relaxed)) break;
    }
}

void Metrics::describeCounter(const std::string& name, const std::string& help) {
    std::lock_guard<std::mutex> lk(mu_);
    helpCounter_[name] = help;
}
void Metrics::describeGauge(const std::string& name, const std::string& help) {
    std::lock_guard<std::mutex> lk(mu_);
    helpGauge_[name] = help;
}
void Metrics::describeHistogram(const std::string& name, const std::string& help) {
    std::lock_guard<std::mutex> lk(mu_);
    helpHistogram_[name] = help;
}

std::string Metrics::render() const {
    std::ostringstream os;
    std::lock_guard<std::mutex> lk(mu_);

    for (const auto& [name, byKey] : counters_) {
        auto h = helpCounter_.find(name);
        if (h != helpCounter_.end()) os << "# HELP " << name << ' ' << h->second << '\n';
        os << "# TYPE " << name << " counter\n";
        for (const auto& [key, series] : byKey) {
            auto labelsIt = labelOrder_.find(name + "|" + key);
            const Labels& labels = (labelsIt == labelOrder_.end()) ? Labels{} : labelsIt->second;
            os << name << renderLabels(labels) << ' ' << series.value.load() << '\n';
        }
    }

    for (const auto& [name, byKey] : gauges_) {
        auto h = helpGauge_.find(name);
        if (h != helpGauge_.end()) os << "# HELP " << name << ' ' << h->second << '\n';
        os << "# TYPE " << name << " gauge\n";
        for (const auto& [key, series] : byKey) {
            auto labelsIt = labelOrder_.find(name + "|" + key);
            const Labels& labels = (labelsIt == labelOrder_.end()) ? Labels{} : labelsIt->second;
            os << name << renderLabels(labels) << ' ' << doubleFromBits(series.bits.load()) << '\n';
        }
    }

    const auto& buckets = defaultBuckets();
    for (const auto& [name, byKey] : histograms_) {
        auto h = helpHistogram_.find(name);
        if (h != helpHistogram_.end()) os << "# HELP " << name << ' ' << h->second << '\n';
        os << "# TYPE " << name << " histogram\n";
        for (const auto& [key, series] : byKey) {
            auto labelsIt = labelOrder_.find(name + "|" + key);
            const Labels& labels = (labelsIt == labelOrder_.end()) ? Labels{} : labelsIt->second;

            std::int64_t cumulative = 0;
            for (std::size_t i = 0; i < buckets.size(); ++i) {
                cumulative += series->bucketCounts[i].load();
                Labels withLe = labels;
                withLe.push_back({"le", std::to_string(buckets[i])});
                os << name << "_bucket" << renderLabels(withLe) << ' ' << cumulative << '\n';
            }
            cumulative += series->bucketCounts.back().load();
            Labels withInf = labels;
            withInf.push_back({"le", "+Inf"});
            os << name << "_bucket" << renderLabels(withInf) << ' ' << cumulative << '\n';
            os << name << "_sum"   << renderLabels(labels) << ' ' << doubleFromBits(series->sumBits.load()) << '\n';
            os << name << "_count" << renderLabels(labels) << ' ' << series->count.load() << '\n';
        }
    }
    return os.str();
}

} // namespace monggle
