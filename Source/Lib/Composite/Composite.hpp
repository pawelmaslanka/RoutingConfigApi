/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

#include "Node.hpp"
#include "Visitor.hpp"

#include <Defer.hpp>
#include <MultiinheritShared.hpp>
#include <StdLib.hpp>

namespace Composite {
class Composite
        : public Node, public InheritableEnableSharedFromThis<Composite> {
public:
    explicit Composite(String name, SharedPtr<INode> parent = nullptr) : Node(std::move(name), std::move(parent)) {
    }

    virtual ~Composite() = default;

    virtual bool Add(const SharedPtr<INode> &node) {
        if (!node) {
            return false;
        }

        auto [nodeIt, success] = mNodeByName.insert({node->Name(), node});
        if (success) {
            // NOTE: To avoid the error "member 'shared_from_this' found in multiple base classes of different types" it has be to downcasted
            nodeIt->second->SetParent(Node::downcasted_shared_from_this<Composite>());
        }

        return success;
    }

    virtual bool Remove(const String &nodeName) {
        mNodeByName.erase(nodeName);
        return true;
    }

    virtual SharedPtr<INode> MakeCopy() const override {
        auto copyComposite = std::make_shared<Composite>(Name(), Parent());
        for (auto &[_, node]: mNodeByName) {
            copyComposite->Add(node->MakeCopy());
        }

        return copyComposite;
    }

    void Accept(IVisitor &visitor) override {
        for (auto &[_, node]: mNodeByName) {
            // if (!visitor.Visit(*nodePtr)) {
            //     break;
            // }

            if (auto nodePtr = std::dynamic_pointer_cast<Node>(node)) {
                nodePtr->Accept(visitor);
            }
        }
    }

    virtual SharedPtr<INode> FindNode(const String &nodeName) {
        auto nodeIt = mNodeByName.find(nodeName);
        if (nodeIt != mNodeByName.end()) {
            return nodeIt->second;
        }

        return nullptr;
    }

    virtual size_t Count() const {
        return mNodeByName.size();
    }

private:
    Map<String, SharedPtr<INode> > mNodeByName;
};
} // namespace Composite

