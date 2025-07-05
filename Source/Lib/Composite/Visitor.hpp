/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
 *  @license The GNU General Public License v3.0
 */
#pragma once

// Headers arranged in alphabetical order
#include "INode.hpp"

#include <StdLib.hpp>

namespace Composite {
using namespace StdLib;

/** The IVistor interface is used by a derived class to fulfill a contract in order to be able to pass it to an object that can be visited */
class IVisitor {
public:
    virtual ~IVisitor() = default;

    virtual bool Visit(INode &node) = 0;
};

/** The IVisitable interface is used by a derived class to create a contract that allows a visitor to visit an object */
class IVisitable {
public:
    virtual ~IVisitable() = default;

    virtual void Accept(IVisitor &visitor) = 0;
};
} // namespace Composite
