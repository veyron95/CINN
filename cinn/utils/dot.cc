#include "cinn/utils/dot.h"

namespace cinn {
namespace utils {

struct Attr {
  std::string key;
  std::string value;

  Attr(const std::string& key, const std::string& value) : key(key), value(value) {}

  std::string repr() const;
};

struct Node {
  std::string name;
  std::vector<Attr> attrs;

  Node(const std::string& name, const std::vector<Attr>& attrs);

  std::string id() const { return id_; }

  std::string repr() const;

 private:
  std::string id_;
};

struct Edge {
  std::string source;
  std::string target;
  std::vector<Attr> attrs;

  Edge(const std::string& source, const std::string& target, const std::vector<Attr>& attrs)
      : source(source), target(target), attrs(attrs) {}

  std::string repr() const;
};

std::string Attr::repr() const {
  std::stringstream ss;
  ss << key << "=" << '"' << value << '"';
  return ss.str();
}
Node::Node(const std::string& name, const std::vector<Attr>& attrs) : name(name), attrs(attrs) {
  std::stringstream ss;
  ss << "node_" << dot_node_counter++;
  id_ = ss.str();
}
std::string Node::repr() const {
  std::stringstream ss;
  CHECK(!name.empty());
  ss << id_;
  if (attrs.empty()) {
    ss << "[label=" << '"' << name << '"' << "]";
    return ss.str();
  }
  for (size_t i = 0; i < attrs.size(); i++) {
    if (i == 0) {
      ss << "[label=" << '"' << name << '"' << " ";
    }
    ss << attrs[i].repr();
    ss << ((i < attrs.size() - 1) ? " " : "]");
  }
  return ss.str();
}
std::string Edge::repr() const {
  std::stringstream ss;
  CHECK(!source.empty());
  CHECK(!target.empty());
  ss << source << "->" << target;
  for (size_t i = 0; i < attrs.size(); i++) {
    if (i == 0) {
      ss << "[";
    }
    ss << attrs[i].repr();
    ss << ((i < attrs.size() - 1) ? " " : "]");
  }
  return ss.str();
}
void Dot::AddNode(const std::string& id, const std::vector<Attr>& attrs, std::string label) {
  CHECK(!nodes_.count(id)) << "duplicate Node '" << id << "'";
  if (label.empty()) label = id;
  nodes_.emplace(id, Node{label, attrs});
}
void Dot::AddEdge(const std::string& source, const std::string& target, const std::vector<Attr>& attrs) {
  CHECK(!source.empty());
  CHECK(!target.empty());
  auto sid = nodes_.at(source).id();
  auto tid = nodes_.at(target).id();
  edges_.emplace_back(sid, tid, attrs);
}
std::string Dot::Build() const {
  std::stringstream ss;
  const std::string indent = "   ";
  ss << "digraph G {" << '\n';

  // Add graph attrs
  for (const auto& attr : attrs_) {
    ss << indent << attr.repr() << '\n';
  }
  // add nodes
  for (auto& item : nodes_) {
    ss << indent << item.second.repr() << '\n';
  }
  // add edges
  for (auto& edge : edges_) {
    ss << indent << edge.repr() << '\n';
  }
  ss << "} // end G";
  return ss.str();
}

}  // namespace utils
}  // namespace cinn
