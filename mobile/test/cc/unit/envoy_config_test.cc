#include <string>
#include <vector>

#include "test/test_common/utility.h"

#include "absl/strings/str_replace.h"
#include "absl/synchronization/notification.h"
#include "gtest/gtest.h"
#include "library/cc/engine_builder.h"
#include "library/cc/log_level.h"
#include "library/common/api/external.h"
#include "library/common/config/internal.h"
#include "library/common/data/utility.h"

#if defined(__APPLE__)
#include "source/extensions/network/dns_resolver/apple/apple_dns_impl.h"
#endif

using testing::HasSubstr;
using testing::Not;
extern const char* alternate_protocols_cache_filter_insert;

namespace Envoy {
namespace {

using namespace Platform;

TEST(TestConfig, ConfigIsApplied) {
  EngineBuilder engine_builder;
  engine_builder.addGrpcStatsDomain("asdf.fake.website")
      .addConnectTimeoutSeconds(123)
      .addDnsRefreshSeconds(456)
      .addDnsMinRefreshSeconds(567)
      .addDnsFailureRefreshSeconds(789, 987)
      .addDnsQueryTimeoutSeconds(321)
      .addH2ConnectionKeepaliveIdleIntervalMilliseconds(222)
      .addH2ConnectionKeepaliveTimeoutSeconds(333)
      .addStatsFlushSeconds(654)
      .setAppVersion("1.2.3")
      .setAppId("1234-1234-1234")
      .enableDnsCache(true, /* save_interval_seconds */ 101)
      .addDnsPreresolveHostnames({"lyft.com", "google.com"})
      .enableAdminInterface(true)
      .setForceAlwaysUsev6(true)
      .setDeviceOs("probably-ubuntu-on-CI");
  std::string config_str = engine_builder.generateConfigStr();

  std::vector<std::string> must_contain = {"- &stats_domain asdf.fake.website",
                                           "- &connect_timeout 123s",
                                           "- &dns_refresh_rate 456s",
                                           "- &dns_fail_base_interval 789s",
                                           "- &dns_fail_max_interval 987s",
                                           "- &dns_min_refresh_rate 567s",
                                           "- &dns_query_timeout 321s",
                                           "- &h2_connection_keepalive_idle_interval 0.222s",
                                           "- &h2_connection_keepalive_timeout 333s",
                                           "- &stats_flush_interval 654s",
                                           "  key: dns_persistent_cache",
                                           "- &force_ipv6 true",
                                           "- &persistent_dns_cache_save_interval 101",
                                           ("- &metadata { device_os: probably-ubuntu-on-CI, "
                                            "app_version: 1.2.3, app_id: 1234-1234-1234 }"),
                                           R"(- &validation_context
  trusted_ca:)"};
  for (const auto& string : must_contain) {
    ASSERT_NE(config_str.find(string), std::string::npos)
        << "'" << string << "' not found in" << config_str;
  }
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  engine_builder.generateBootstrap();
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));
}

TEST(TestConfig, ConfigIsValid) {
  EngineBuilder engine_builder;
  std::string config_str = engine_builder.generateConfigStr();
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  // Test per-platform DNS fixes.
#if defined(__APPLE__)
  ASSERT_THAT(bootstrap.DebugString(), Not(HasSubstr("envoy.network.dns_resolver.getaddrinfo")));
  ASSERT_THAT(bootstrap.DebugString(), HasSubstr("envoy.network.dns_resolver.apple"));
#else
  ASSERT_THAT(bootstrap.DebugString(), HasSubstr("envoy.network.dns_resolver.getaddrinfo"));
  ASSERT_THAT(bootstrap.DebugString(), Not(HasSubstr("envoy.network.dns_resolver.apple")));
#endif

  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));
}

TEST(TestConfig, SetGzipDecompression) {
  EngineBuilder engine_builder;

  engine_builder.enableGzipDecompression(false);
  std::string config_str = engine_builder.generateConfigStr();
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), Not(HasSubstr("envoy.filters.http.decompressor")));
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));

  engine_builder.enableGzipDecompression(true);
  config_str = engine_builder.generateConfigStr();
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), HasSubstr("envoy.filters.http.decompressor"));
}

#ifdef ENVOY_MOBILE_REQUEST_COMPRESSION
TEST(TestConfig, SetGzipCompression) {
  EngineBuilder engine_builder;

  engine_builder.enableGzipCompression(false);
  std::string config_str = engine_builder.generateConfigStr();
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), Not(HasSubstr("envoy.filters.http.compressor")));
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));

  engine_builder.enableGzipCompression(true);
  config_str = engine_builder.generateConfigStr();
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), HasSubstr("envoy.filters.http.compressor"));
}
#endif

TEST(TestConfig, SetBrotliDecompression) {
  EngineBuilder engine_builder;

  engine_builder.enableBrotliDecompression(false);
  std::string config_str = engine_builder.generateConfigStr();
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), Not(HasSubstr("brotli.decompressor.v3.Brotli")));
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));

  engine_builder.enableBrotliDecompression(true);
  config_str = engine_builder.generateConfigStr();
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), HasSubstr("brotli.decompressor.v3.Brotli"));
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));
}

#ifdef ENVOY_MOBILE_REQUEST_COMPRESSION
TEST(TestConfig, SetBrotliCompression) {
  EngineBuilder engine_builder;

  engine_builder.enableBrotliCompression(false);
  std::string config_str = engine_builder.generateConfigStr();
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), Not(HasSubstr("brotli.compressor.v3.Brotli")));
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));

  engine_builder.enableBrotliCompression(true);
  config_str = engine_builder.generateConfigStr();
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), HasSubstr("brotli.compressor.v3.Brotli"));
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));
}
#endif

TEST(TestConfig, SetSocketTag) {
  EngineBuilder engine_builder;

  engine_builder.enableSocketTagging(false);
  std::string config_str = engine_builder.generateConfigStr();
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), Not(HasSubstr("http.socket_tag.SocketTag")));
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));

  engine_builder.enableSocketTagging(true);
  config_str = engine_builder.generateConfigStr();
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(), HasSubstr("http.socket_tag.SocketTag"));
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));
}

TEST(TestConfig, SetAltSvcCache) {
  EngineBuilder engine_builder;

  std::string config_str = absl::StrCat(config_header, engine_builder.generateConfigStr());

  absl::StrReplaceAll({{"#{custom_filters}", alternate_protocols_cache_filter_insert}},
                      &config_str);

  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(config_str, bootstrap);
}

TEST(TestConfig, StreamIdleTimeout) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&stream_idle_timeout 15s"));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));

  engine_builder.setStreamIdleTimeoutSeconds(42);
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&stream_idle_timeout 42s"));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));
}

TEST(TestConfig, PerTryIdleTimeout) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&per_try_idle_timeout 15s"));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));

  engine_builder.setPerTryIdleTimeoutSeconds(42);
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&per_try_idle_timeout 42s"));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));
}

TEST(TestConfig, EnableAdminInterface) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, Not(HasSubstr("admin: *admin_interface")));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  engine_builder.enableAdminInterface(true);
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("admin: *admin_interface"));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
}

TEST(TestConfig, EnableInterfaceBinding) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&enable_interface_binding false"));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));

  engine_builder.enableInterfaceBinding(true);
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&enable_interface_binding true"));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  EXPECT_TRUE(TestUtility::protoEqual(bootstrap, *engine_builder.generateBootstrap()));
}

TEST(TestConfig, EnableDrainPostDnsRefresh) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&enable_drain_post_dns_refresh false"));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  engine_builder.enableDrainPostDnsRefresh(true);
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&enable_drain_post_dns_refresh true"));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
}

TEST(TestConfig, EnableHappyEyeballs) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  config_str = absl::StrCat(config_header, engine_builder.generateConfigStr());
  ASSERT_THAT(config_str, Not(HasSubstr("&dns_lookup_family V4_PREFERRED")));
  ASSERT_THAT(config_str, HasSubstr("&dns_lookup_family ALL"));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  engine_builder.enableHappyEyeballs(false);
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&dns_lookup_family V4_PREFERRED"));
  ASSERT_THAT(config_str, Not(HasSubstr("&dns_lookup_family ALL")));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
}

TEST(TestConfig, EnforceTrustChainVerification) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&trust_chain_verification VERIFY_TRUST_CHAIN"));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  engine_builder.enforceTrustChainVerification(false);
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&trust_chain_verification ACCEPT_UNTRUSTED"));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
}

TEST(TestConfig, AddMaxConnectionsPerHost) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&max_connections_per_host 7"));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  engine_builder.addMaxConnectionsPerHost(16);
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("&max_connections_per_host 16"));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
}

std::string statsdSinkConfig(int port) {
  std::string config = R"({ name: envoy.stat_sinks.statsd,
      typed_config: {
        "@type": type.googleapis.com/envoy.config.metrics.v3.StatsdSink,
        address: { socket_address: { address: 127.0.0.1, port_value: )" +
                       fmt::format("{}", port) + " } } } }";
  return config;
}

TEST(TestConfig, AddStatsSinks) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, Not(HasSubstr("&stats_sinks")));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  engine_builder.addStatsSinks({statsdSinkConfig(1), statsdSinkConfig(2)});
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr(statsdSinkConfig(1)));
  ASSERT_THAT(config_str, HasSubstr(statsdSinkConfig(2)));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
}

TEST(TestConfig, DisableHttp3) {
  EngineBuilder engine_builder;

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str,
              HasSubstr("envoy.extensions.filters.http.alternate_protocols_cache.v3.FilterConfig"));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  engine_builder.enableHttp3(false);
  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(
      config_str,
      Not(HasSubstr("envoy.extensions.filters.http.alternate_protocols_cache.v3.FilterConfig")));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
}

TEST(TestConfig, RemainingTemplatesThrows) {
  EngineBuilder engine_builder("{{ template_that_i_will_not_fill }}");
  try {
    engine_builder.generateConfigStr();
    FAIL() << "Expected std::runtime_error";
  } catch (std::runtime_error& err) {
    EXPECT_EQ(err.what(), std::string("could not resolve all template keys in config"));
  }
}

TEST(TestConfig, RtdsWithoutAds) {
  EngineBuilder engine_builder;
  engine_builder.addRtdsLayer("some rtds layer");
  try {
    engine_builder.generateConfigStr();
    FAIL() << "Expected std::runtime_error";
  } catch (std::runtime_error& err) {
    EXPECT_EQ(err.what(), std::string("ADS must be configured when using RTDS"));
  }
}

TEST(TestConfig, EnablePlatformCertificatesValidation) {
  EngineBuilder engine_builder;
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  engine_builder.enablePlatformCertificatesValidation(false);
  auto config_str1 = engine_builder.generateConfigStr();
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str1), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(),
              Not(HasSubstr("envoy_mobile.cert_validator.platform_bridge_cert_validator")));
  ASSERT_THAT(bootstrap.DebugString(), HasSubstr("trusted_ca"));

  engine_builder.enablePlatformCertificatesValidation(true);
  auto config_str2 = engine_builder.generateConfigStr();
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str2), bootstrap);
  ASSERT_THAT(bootstrap.DebugString(),
              HasSubstr("envoy_mobile.cert_validator.platform_bridge_cert_validator"));
  ASSERT_THAT(bootstrap.DebugString(), Not(HasSubstr("trusted_ca")));
}

// Implementation of StringAccessor which tracks the number of times it was used.
class TestStringAccessor : public StringAccessor {
public:
  explicit TestStringAccessor(std::string data) : data_(data) {}
  ~TestStringAccessor() override = default;

  // StringAccessor
  const std::string& get() const override {
    ++count_;
    return data_;
  }

  int count() { return count_; }

private:
  std::string data_;
  mutable int count_ = 0;
};

TEST(TestConfig, AddNativeFilters) {
  EngineBuilder engine_builder;

  std::string filter_name1 = "envoy.filters.http.buffer1";
  std::string filter_name2 = "envoy.filters.http.buffer2";
  std::string filter_config =
      "{\"@type\":\"type.googleapis.com/envoy.extensions.filters.http.buffer.v3.Buffer\","
      "\"max_request_bytes\":5242880}";

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, Not(HasSubstr("- name: " + filter_name1)));
  ASSERT_THAT(config_str, Not(HasSubstr("  typed_config: " + filter_config)));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  engine_builder.addNativeFilter(filter_name1, filter_config);
  engine_builder.addNativeFilter(filter_name2, filter_config);

  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("- name: " + filter_name1));
  ASSERT_THAT(config_str, HasSubstr("- name: " + filter_name2));
  ASSERT_THAT(config_str, HasSubstr("  typed_config: " + filter_config));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
}

TEST(TestConfig, AddPlatformFilter) {
  EngineBuilder engine_builder;

  std::string filter_name = "test_platform_filter";

  std::string config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, Not(HasSubstr("http.platform_bridge.PlatformBridge")));
  ASSERT_THAT(config_str, Not(HasSubstr("platform_filter_name: " + filter_name)));
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);

  engine_builder.addPlatformFilter(filter_name);

  config_str = engine_builder.generateConfigStr();
  ASSERT_THAT(config_str, HasSubstr("http.platform_bridge.PlatformBridge"));
  ASSERT_THAT(config_str, HasSubstr("platform_filter_name: " + filter_name));
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
}

// TODO(RyanTheOptimist): This test seems to be flaky. #2641
TEST(TestConfig, DISABLED_StringAccessors) {
  std::string name("accessor_name");
  EngineBuilder engine_builder;
  std::string data_string = "envoy string";
  auto accessor = std::make_shared<TestStringAccessor>(data_string);
  engine_builder.addStringAccessor(name, accessor);
  EngineSharedPtr engine = engine_builder.build();
  auto c_accessor = static_cast<envoy_string_accessor*>(Envoy::Api::External::retrieveApi(name));
  ASSERT_TRUE(c_accessor != nullptr);
  EXPECT_EQ(0, accessor->count());
  envoy_data data = c_accessor->get_string(c_accessor->context);
  EXPECT_EQ(1, accessor->count());
  EXPECT_EQ(data_string, Data::Utility::copyToString(data));
  release_envoy_data(data);
}

TEST(TestConfig, AddVirtualCluster) {
  EngineBuilder engine_builder;
  envoy::config::bootstrap::v3::Bootstrap bootstrap;
  engine_builder.addVirtualCluster(
      "{headers: [{name: ':method', string_match: {exact: POST}}], name: cluster1}");
  std::string config_str = engine_builder.generateConfigStr();
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(config_str, HasSubstr("cluster1"));

  engine_builder.addVirtualCluster(
      "{headers: [{name: ':method', string_match: {exact: GET}}], name: cluster2}");
  config_str = engine_builder.generateConfigStr();
  TestUtility::loadFromYaml(absl::StrCat(config_header, config_str), bootstrap);
  ASSERT_THAT(config_str, HasSubstr("cluster2"));
}

} // namespace
} // namespace Envoy
