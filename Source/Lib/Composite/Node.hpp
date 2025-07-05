/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

// Headers arranged in alphabetical order
#include "INode.hpp"
#include "Visitor.hpp"

#include <MultiinheritShared.hpp>
#include <StdLib.hpp>

namespace Composite {
using namespace StdLib;

class Node : public INode, public IVisitable, public InheritableEnableSharedFromThis<Node> {
public:
    explicit Node(String name, SharedPtr<INode> parent = nullptr) : INode(std::move(name), std::move(parent)) {
    }

    virtual ~Node() = default;

    SharedPtr<INode> MakeCopy() const override { return std::make_shared<Node>(Name(), Parent()); }

    void Accept(IVisitor &visitor) override { visitor.Visit(*this); }
};
} // namespace Composite
