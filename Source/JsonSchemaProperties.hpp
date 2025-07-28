#pragma once

namespace Json {
namespace Schema {
namespace Property {

static constexpr auto ACTION = "action";
static constexpr auto ADDRESS = "address";
static constexpr auto ADDRESS_FAMILY = "address-family";
static constexpr auto AS = "as";
static constexpr auto ASN = "asn";
static constexpr auto AS_PATH_EQ = "as-path-eq";
static constexpr auto AS_PATH_IN = "as-path-in";
static constexpr auto AS_PATH_LIST = "as-path-list";
static constexpr auto AS_PATH_PREPEND = "as-path-prepend";
static constexpr auto BGP = "bgp";
static constexpr auto COMMUNITY_ADD = "community-add";
static constexpr auto COMMUNITY_EQ = "community-eq";
static constexpr auto COMMUNITY_IN = "community-in";
static constexpr auto COMMUNITY_LIST = "community-list";
static constexpr auto COMMUNITY_REMOVE = "community-remove";
static constexpr auto DEFAULT_ACTION = "default-action";
static constexpr auto EBGP = "ebgp";
static constexpr auto EXT_COMMUNITY_EQ = "ext-community-eq";
static constexpr auto EXT_COMMUNITY_IN = "ext-community-in";
static constexpr auto EXT_COMMUNITY_LIST = "ext-community-list";
static constexpr auto IBGP = "ibgp";
static constexpr auto IF_MATCH = "if-match";
static constexpr auto INTERFACE = "interface";
static constexpr auto LARGE_COMMUNITY_EQ = "large-community-eq";
static constexpr auto LARGE_COMMUNITY_IN = "large-community-in";
static constexpr auto LARGE_COMMUNITY_LIST = "large-community-list";
static constexpr auto LINK_LOCAL = "link-local";
static constexpr auto LOCAL = "local";
static constexpr auto LOCAL_PREFERENCE_SET = "local-preference-set";
static constexpr auto MATCH_TYPE = "match-type";
static constexpr auto MED_SET = "med-set";
static constexpr auto MULTIHOP = "multihop";
static constexpr auto N_TIMES = "n-times";
static constexpr auto NET_EQ = "net-eq";
static constexpr auto NET_IN = "net-in";
static constexpr auto NET_TYPE_EQ = "net-type-eq";
static constexpr auto NEXT_HOP_SELF = "next-hop-self";
static constexpr auto PEER = "peer";
static constexpr auto POLICY_IN = "policy-in";
static constexpr auto POLICY_LIST = "policy-list";
static constexpr auto POLICY_OUT = "policy-out";
static constexpr auto PREFIX_GE_ATTR = "ge";
static constexpr auto PREFIX_LE_ATTR = "le";
static constexpr auto PREFIX_V4 = "prefix-v4";
static constexpr auto PREFIX_V6 = "prefix-v6";
static constexpr auto PREFIX_V4_LIST = "prefix-v4-list";
static constexpr auto PREFIX_V6_LIST = "prefix-v6-list";
static constexpr auto PORT = "port";
static constexpr auto RANGE = "range";
static constexpr auto ROUTER_ID = "router-id";
static constexpr auto SESSIONS = "sessions";
static constexpr auto SOURCE_PROTOCOL_EQ = "source-protocol-eq";
static constexpr auto THEN = "then";
static constexpr auto TTL = "ttl";

} // namespace Properties
} // namespace Schema
} // namespace Json
