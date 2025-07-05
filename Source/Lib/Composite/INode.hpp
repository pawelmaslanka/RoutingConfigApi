/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

// Headers arranged in alphabetical order
#include <StdLib.hpp>

namespace Composite {
using namespace StdLib;

class INode {
public:
    explicit INode(String name, SharedPtr<INode> parent = nullptr) : mParent(std::move(parent)), mName(std::move(name)) {
    }

    virtual ~INode() { mParent = nullptr; }

    /** Name - Get name of the node
     * @return The name of the node
     */
    String Name() const { return mName; }

    /** Parent - Get parent of the node
     * @return Parent of the node
     */
    SharedPtr<INode> Parent() const { return mParent; }
    void SetParent(SharedPtr<INode> parent) { mParent = std::move(parent); }

    /** MakeCopy - Makes deep copy of the node. It has to be implemented by derived class
     * @return Copy of the node
     */
    virtual SharedPtr<INode> MakeCopy() const = 0;

private:
    SharedPtr<INode> mParent;
    String mName;
};
} // namespace Composite
