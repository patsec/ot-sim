#ifndef OTSIM_MSGBUS_ENVELOPE_HPP
#define OTSIM_MSGBUS_ENVELOPE_HPP

#include <map>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace otsim {
namespace msgbus {

typedef std::map<std::string, std::string> Metadata;
typedef std::map<std::string, std::string> ConfirmationErrors;

template<typename T>
struct Envelope {
  std::string version  {};
  std::string kind     {};
  Metadata    metadata {};
  T           contents {};
};

struct Point {
  std::string   tag   {};
  double        value {};
  std::uint64_t ts    {};
};

using Points = std::vector<Point>;

struct Status {
  Points measurements {};
};

struct Update {
  Points      updates   {};
  std::string recipient {};
  std::string confirm   {};
};

struct Confirmation {
  std::string        confirm {};
  ConfirmationErrors errors  {};
};

struct Metric {
  std::string kind  {};
  std::string name  {};
  std::string desc  {};
  double      value {};
};

struct Metrics {
  std::vector<Metric> metrics {};
};

template<typename T>
Envelope<T> NewEnvelope(const std::string &sender, T contents) {
  Envelope<T> env = {
    .version = "v1",
    .metadata = std::map<std::string, std::string> {{ "sender", sender}},
    .contents = contents,
  };

  if (std::is_same_v<T, Status>) {
    env.kind = "Status";
  } else if (std::is_same_v<T, Update>) {
    env.kind = "Update";
  } else if (std::is_same_v<T, Metrics>) {
    env.kind = "Metric";
  }

  return env;
}

template<typename T>
std::string GetEnvelopeSender(Envelope<T> env) {
  if (env.metadata.count("sender")) {
    return env.metadata.at("sender");
  }

  return "";
}

template<typename T>
void to_json(json& j, const Envelope<T>& e) {
  j["version"] = e.version;
  j["kind"] = e.kind;
  j["metadata"] = e.metadata;
  j["contents"] = e.contents;
}

template<typename T>
void from_json(const json& j, Envelope<T>& e) {
  j.at("version").get_to(e.version);
  j.at("kind").get_to(e.kind);
  j.at("metadata").get_to(e.metadata);
  j.at("contents").get_to(e.contents);
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Point, tag, value, ts)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Status, measurements)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Update, updates, recipient, confirm)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Confirmation, confirm, errors)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Metric, kind, name, desc, value)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Metrics, metrics)

} // namespace msgbus
} // namespace otsim

#endif // OTSIM_MSGBUS_ENVELOPE_HPP