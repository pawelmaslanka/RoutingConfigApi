/** @copyright Copyright (C) 2025 Pawel Maslanka (pawmas@hotmail.com)
*  @license The GNU General Public License v3.0
 */
#pragma once

#include "Composite.hpp"

#include <spdlog/spdlog.h>

#include <cstdio>

namespace Composite::Test {
class PrintingNodeNameVisitor final : public IVisitor {
public:
    bool Visit(INode &node) override {
        SPDLOG_INFO("{} -> {}", (node.Parent() ? node.Parent()->Name() : "ROOT"), node.Name());
        return true;
    }
};

inline bool VisitCompositeNodes() {
    SPDLOG_INFO("[TEST] Visit all composite nodes and print their names");
    SPDLOG_INFO("[BEGIN]");
    auto composite = std::make_shared<Composite>("TestComposite");
    composite->Add(std::make_shared<Node>("TestNode #1"));
    composite->Add(std::make_shared<Node>("TestNode #2"));
    composite->Add(std::make_shared<Node>("TestNode #3"));
    SPDLOG_INFO("There are {} nodes to visit", composite->Count());
    PrintingNodeNameVisitor visitor;
    composite->Accept(visitor);
    SPDLOG_INFO("[END]");
    return true;
}

inline bool VisitCompositeNodesIncludingOtherComposite() {
    SPDLOG_INFO("[TEST] Visit all composite nodes including other composite and print their names");
    SPDLOG_INFO("[BEGIN]");
    auto composite = std::make_shared<Composite>("TestComposite #1");
    auto nodeNo1 = std::make_shared<Node>("TestNode #1");
    composite->Add(nodeNo1);
    auto nodeNo2 = std::make_shared<Node>("TestNode #2");
    composite->Add(nodeNo2);
    auto nodeNo3 = std::make_shared<Node>("TestNode #3");
    composite->Add(nodeNo3);
    auto otherComposite = std::make_shared<Composite>("TestComposite #2");
    composite->Add(otherComposite);
    otherComposite->Add(std::make_shared<Node>("Other TestNode #1"));
    otherComposite->Add(std::make_shared<Node>("Other TestNode #2"));
    otherComposite->Add(std::make_shared<Node>("Other TestNode #3"));
    SPDLOG_INFO("There are {} nodes to visit", composite->Count());
    PrintingNodeNameVisitor visitor;
    composite->Accept(visitor);
    SPDLOG_INFO("[END]");
    return true;
}

} // namespace Composite
