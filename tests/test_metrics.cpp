#include "monggle/metrics/metrics.h"

#include <gtest/gtest.h>

using namespace monggle;

namespace {

bool contains(const std::string& body, const std::string& needle) {
    return body.find(needle) != std::string::npos;
}

} // namespace

TEST(Metrics, CounterRendersWithLabels) {
    Metrics& m = Metrics::instance();
    m.describeCounter("test_counter_total", "for test");
    m.incCounter("test_counter_total",
                 {{"method", "GET"}, {"path", "/x"}, {"status", "200"}});
    m.incCounter("test_counter_total",
                 {{"method", "GET"}, {"path", "/x"}, {"status", "200"}}, 4);

    auto out = m.render();
    EXPECT_TRUE(contains(out, "# TYPE test_counter_total counter"));
    EXPECT_TRUE(contains(out, "test_counter_total{"));
    EXPECT_TRUE(contains(out, "method=\"GET\""));
    EXPECT_TRUE(contains(out, "} 5\n")); // 1 + 4
}

TEST(Metrics, HistogramAccumulatesIntoBuckets) {
    Metrics& m = Metrics::instance();
    m.describeHistogram("test_hist_seconds", "latency");
    // bucket boundaries from defaultBuckets()
    m.observeHistogram("test_hist_seconds", {{"path", "/y"}}, 0.0001);  // smallest bucket
    m.observeHistogram("test_hist_seconds", {{"path", "/y"}}, 0.05);    // mid bucket
    m.observeHistogram("test_hist_seconds", {{"path", "/y"}}, 99.0);    // +Inf bucket

    auto out = m.render();
    EXPECT_TRUE(contains(out, "# TYPE test_hist_seconds histogram"));
    EXPECT_TRUE(contains(out, "test_hist_seconds_count{"));
    EXPECT_TRUE(contains(out, "} 3\n")); // count = 3
    EXPECT_TRUE(contains(out, "le=\"+Inf\""));
}

TEST(Metrics, GaugeStoresDouble) {
    Metrics& m = Metrics::instance();
    m.describeGauge("test_gauge", "g");
    m.setGauge("test_gauge", {{"role", "primary"}}, 3.14);
    m.setGauge("test_gauge", {{"role", "primary"}}, 7.0); // overwrite

    auto out = m.render();
    EXPECT_TRUE(contains(out, "# TYPE test_gauge gauge"));
    EXPECT_TRUE(contains(out, "test_gauge{role=\"primary\"} 7"));
}

TEST(Metrics, LabelValueEscaping) {
    Metrics& m = Metrics::instance();
    m.incCounter("escape_test", {{"k", std::string("a\"b\\c\nd")}});
    auto out = m.render();
    EXPECT_TRUE(contains(out, "k=\"a\\\"b\\\\c\\nd\""));
}
