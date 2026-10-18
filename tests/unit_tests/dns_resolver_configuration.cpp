// SPDX-License-Identifier: BSD-3-Clause
// A separate executable keeps fake libunbound symbols out of other unit tests.
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <unbound.h>
#include "gtest/gtest.h"
#include "common/dns_utils.h"

struct ub_ctx
{
  bool system_resolver = false;
  bool hosts = false;
  bool forwarder = false;
};

extern "C"
{
ub_ctx* ub_ctx_create() { return new ub_ctx; }
void ub_ctx_delete(ub_ctx* context) { delete context; }
int ub_ctx_resolvconf(ub_ctx* context, const char*) { context->system_resolver = true; return 0; }
int ub_ctx_hosts(ub_ctx* context, const char*) { context->hosts = true; return 0; }
int ub_ctx_set_fwd(ub_ctx* context, const char*) { context->forwarder = true; return 0; }
int ub_ctx_set_option(ub_ctx*, const char*, const char*) { return 0; }
int ub_ctx_add_ta(ub_ctx*, const char*) { return 0; }

int ub_resolve(ub_ctx* context, const char* name, int type, int, ub_result** output)
{
  *output = static_cast<ub_result*>(std::calloc(1, sizeof(ub_result)));
  std::string data;
  if (std::strcmp(name, "seed.fixture.invalid") == 0 && type == tools::DNS_TYPE_TXT &&
      (context->system_resolver || context->forwarder))
  {
    const std::string endpoint = "192.0.2.1:34567";
    data = std::string(1, static_cast<char>(endpoint.size())) + endpoint;
  }
  if (std::strcmp(name, "host.fixture.invalid") == 0 && type == tools::DNS_TYPE_A && context->hosts)
    data = std::string("\x7f\x00\x00\x07", 4);
  if (!data.empty())
  {
    (*output)->havedata = 1;
    (*output)->data = static_cast<char**>(std::calloc(2, sizeof(char*)));
    (*output)->data[0] = static_cast<char*>(std::malloc(data.size()));
    std::memcpy((*output)->data[0], data.data(), data.size());
    (*output)->len = static_cast<int*>(std::malloc(sizeof(int)));
    (*output)->len[0] = static_cast<int>(data.size());
  }
  return 0;
}

void ub_resolve_free(ub_result* result)
{
  if (!result) return;
  if (result->data)
  {
    for (char** data = result->data; *data; ++data) std::free(*data);
    std::free(result->data);
  }
  std::free(result->len);
  std::free(result);
}
}

namespace
{
class ResolverConfiguration : public testing::Test
{
  bool had_override = false;
  std::string previous;
protected:
  static void set_override(const char* value)
  {
#ifdef _WIN32
    _putenv_s("DNS_PUBLIC", value ? value : "");
#else
    if (value) setenv("DNS_PUBLIC", value, 1);
    else unsetenv("DNS_PUBLIC");
#endif
  }
  void SetUp() override
  {
    const char* value = std::getenv("DNS_PUBLIC");
    had_override = value != nullptr;
    if (value) previous = value;
    set_override(nullptr);
  }
  void TearDown() override { set_override(had_override ? previous.c_str() : nullptr); }
};

TEST_F(ResolverConfiguration, KeepsSystemResolverWithoutBundledFallbacks)
{
  auto resolver = tools::DNSResolver::create();
  bool available, valid;
  auto records = resolver.get_txt_record("seed.fixture.invalid", available, valid);
  EXPECT_NE(records.end(), std::find(records.begin(), records.end(), "192.0.2.1:34567"));
  EXPECT_FALSE(available);
  EXPECT_FALSE(valid);
}

TEST_F(ResolverConfiguration, KeepsSystemHostsWithoutBundledFallbacks)
{
  auto resolver = tools::DNSResolver::create();
  bool available, valid;
  auto addresses = resolver.get_ipv4("host.fixture.invalid", available, valid);
  EXPECT_NE(addresses.end(), std::find(addresses.begin(), addresses.end(), "127.0.0.7"));
}

TEST_F(ResolverConfiguration, PreservesExplicitForwarderSelection)
{
  set_override("tcp://127.0.0.1");
  auto resolver = tools::DNSResolver::create();
  bool available, valid;
  auto records = resolver.get_txt_record("seed.fixture.invalid", available, valid);
  EXPECT_NE(records.end(), std::find(records.begin(), records.end(), "192.0.2.1:34567"));
}
}
