#ifndef DEPENDENCY_NODE_H_
#define DEPENDENCY_NODE_H_
#include "PDGExceptions.hpp"
#include "PDGEnums.hpp"
#include "InstructionWrapper.hpp"
#include <vector>

namespace pdg
{
template <typename NodeT>
class DependencyLinkIterator;

template <typename NodeT>
class DependencyNode
{
public:
  using DependencyLink = std::pair<DependencyNode<NodeT> *, DependencyType>;
  using DependencyLinkList = std::vector<DependencyLink>;
  using iterator = DependencyLinkIterator<NodeT>;
  using const_iterator = DependencyLinkIterator<NodeT>;

  DependencyNode() = delete; 
  DependencyNode(const NodeT *pData) : dataNode(pData) {}
  void addDependencyTo(DependencyNode<NodeT> *pNode, DependencyType type);
  const NodeT *getData() const { return dataNode; }
  DependencyLinkList &getDependencyList() { return dependencyList; }
  bool isDepends(const DependencyNode<NodeT> *dNode);
  DependencyType getDependencyType(const DependencyNode<NodeT> *dNode) const;
  DependencyLinkList getNodesWithDepType(DependencyType type) const;

private:
  const NodeT *dataNode;
  DependencyLinkList dependencyList;
};

template <class NodeT>
class DependencyLinkIterator : public std::iterator<std::input_iterator_tag, NodeT>
{
  using InnerIterator = typename DependencyNode<NodeT>::DependencyLinkList::const_iterator;

public:
  DependencyLinkIterator() : itr() {}

  DependencyLinkIterator(const InnerIterator &r) : itr(r) {}

  DependencyLinkIterator<NodeT> &operator++()
  {
    ++itr;
    return *this;
  }

  DependencyLinkIterator<NodeT> operator++(int)
  {
    DependencyLinkIterator<NodeT> old = *this;
    ++itr;
    return old;
  }

  DependencyNode<NodeT> *operator->()
  {
    typename DependencyNode<NodeT>::DependencyLink L = *itr;
    return L.first;
  }

  DependencyNode<NodeT> *operator*()
  {
    typename DependencyNode<NodeT>::DependencyLink L = *itr;
    return L.first;
  }

  bool operator!=(const DependencyLinkIterator &r) const
  {
    return itr != r.itr;
  }

  bool operator==(const DependencyLinkIterator &r) const
  {
    return !(operator!=(r));
  }

  DependencyType getDependencyType() const
  {
    typename DependencyNode<NodeT>::DependencyLink L = *itr;
    return L.second;
  }

  const NodeT *getDependencyNode() const
  {
    typename DependencyNode<NodeT>::DependencyLink L = *itr;
    return L.first->getData();
  }

private:
  InnerIterator itr;
};

// Template implementatinos
template <typename NodeT>
void DependencyNode<NodeT>::addDependencyTo(DependencyNode<NodeT> *pNode,
                                            DependencyType type) {
  if (pNode == this) return;
  DependencyLink link = DependencyLink(pNode, type);
  if (std::find(dependencyList.begin(), dependencyList.end(), link) ==
      dependencyList.end()) {
    dependencyList.push_back(link);
  }
}

template <typename NodeT>
bool DependencyNode<NodeT>::isDepends(const DependencyNode<NodeT> *dNode) {
  if (dNode == nullptr) {
    throw DependencyNodeIsNullptrException("Dependency Node is nullptr.");
  }
  for (auto link : dependencyList) {
    if (link.first == dNode) {
      return true;
    }
  }
  return false;
}

template <typename NodeT>
DependencyType DependencyNode<NodeT>::getDependencyType(
    const DependencyNode<NodeT> *dNode) const {
  if (dNode == nullptr) {
    throw DependencyNodeIsNullptrException("Dependency Node is nullptr.");
  }

  for (auto link : dependencyList) {
    if (link.first == dNode) {
      return link.second;
    }
  }

  return DependencyType::NO_DEPENDENCY;
}

template <typename NodeT>
typename DependencyNode<NodeT>::DependencyLinkList
DependencyNode<NodeT>::getNodesWithDepType(DependencyType depType) const {
  DependencyLinkList ret;
  for (auto pair : dependencyList) {
    if (pair.second == depType) ret.push_back(pair);
  }
  return ret;
}
} // namespace pdg

#endif