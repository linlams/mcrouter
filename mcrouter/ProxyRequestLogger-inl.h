/**
 *  Copyright (c) 2014, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 */
#include "mcrouter/lib/McOperation.h"
#include "mcrouter/proxy.h"
#include "mcrouter/ProxyMcReply.h"
#include "mcrouter/ProxyMcRequest.h"
#include "mcrouter/stats.h"

namespace facebook { namespace memcache { namespace mcrouter {

namespace {

#define REQUEST_CLASS_STATS(proxy, OP, SUFFIX, reqClass)                       \
    do{ switch (reqClass) {                                                    \
        case RequestClass::NORMAL:                                             \
          stat_incr(proxy.stats, cmd_ ## OP ## _ ## SUFFIX ## _stat, 1);       \
          stat_incr(proxy.stats, cmd_ ## OP ## _ ## SUFFIX ## _count_stat, 1); \
          break;                                                               \
        case RequestClass::FAILOVER:                                           \
          stat_incr(proxy.stats,                                               \
                    cmd_ ## OP ## _ ## SUFFIX ## _failover_stat, 1);           \
          stat_incr(proxy.stats,                                               \
                    cmd_ ## OP ## _ ## SUFFIX ## _failover_count_stat, 1);     \
          break;                                                               \
        case RequestClass::SHADOW:                                             \
          stat_incr(proxy.stats,                                               \
                    cmd_ ## OP ## _ ## SUFFIX ## _shadow_stat, 1);             \
          stat_incr(proxy.stats,                                               \
                    cmd_ ## OP ## _ ## SUFFIX ## _shadow_count_stat, 1);       \
          break;}} while(0)

template <int operation>
inline void logOutlier(proxy_t& proxy, McOperation<operation>,
                       RequestClass reqClass) {
  switch (operation) {
    case mc_op_get:
      REQUEST_CLASS_STATS(proxy, get, outlier, reqClass);
      break;
    case mc_op_set:
      REQUEST_CLASS_STATS(proxy, set, outlier, reqClass);
      break;
    case mc_op_delete:
      REQUEST_CLASS_STATS(proxy, delete, outlier, reqClass);
      break;
    default:
      REQUEST_CLASS_STATS(proxy, other, outlier, reqClass);
      break;
  }
}

template <int operation>
inline void logRequestClass(proxy_t& proxy, McOperation<operation>,
                            RequestClass reqClass) {
  switch (operation) {
    case mc_op_get:
      REQUEST_CLASS_STATS(proxy, get, out, reqClass);
      break;
    case mc_op_metaget:
      REQUEST_CLASS_STATS(proxy, meta, out, reqClass);
      break;
    case mc_op_add:
      REQUEST_CLASS_STATS(proxy, add, out, reqClass);
      break;
    case mc_op_replace:
      REQUEST_CLASS_STATS(proxy, replace, out, reqClass);
      break;
    case mc_op_set:
      REQUEST_CLASS_STATS(proxy, set, out, reqClass);
      break;
    case mc_op_incr:
      REQUEST_CLASS_STATS(proxy, incr, out, reqClass);
      break;
    case mc_op_decr:
      REQUEST_CLASS_STATS(proxy, decr, out, reqClass);
      break;
    case mc_op_delete:
      REQUEST_CLASS_STATS(proxy, delete, out, reqClass);
      break;
    case mc_op_lease_set:
      REQUEST_CLASS_STATS(proxy, lease_set, out, reqClass);
      break;
    case mc_op_lease_get:
      REQUEST_CLASS_STATS(proxy, lease_get, out, reqClass);
      break;
    default:
      REQUEST_CLASS_STATS(proxy, other, out, reqClass);
      break;
  }
}

#define REQUEST_CLASS_ERROR_STATS(proxy, ERROR, reqClass)                      \
    do{ switch (reqClass) {                                                    \
          case RequestClass::NORMAL:                                           \
            stat_incr(proxy.stats, result_ ## ERROR ## _stat, 1);              \
            stat_incr(proxy.stats, result_ ## ERROR ## _count_stat, 1);        \
            break;                                                             \
          case RequestClass::FAILOVER:                                         \
            stat_incr(proxy.stats, result_ ## ERROR ## _failover_stat, 1);     \
            stat_incr(proxy.stats,                                             \
                      result_ ## ERROR ## _failover_count_stat, 1);            \
            break;                                                             \
          case RequestClass::SHADOW:                                           \
            stat_incr(proxy.stats, result_ ## ERROR ## _shadow_stat, 1);       \
            stat_incr(proxy.stats, result_ ## ERROR ## _shadow_count_stat, 1); \
            break;                                                             \
        }                                                                      \
        stat_incr(proxy.stats, result_ ## ERROR ## _all_stat, 1);              \
        stat_incr(proxy.stats, result_ ## ERROR ## _all_count_stat, 1);        \
      } while(0)

inline void logError(proxy_t& proxy, const McReplyBase& reply,
                     RequestClass reqClass) {
  if (reply.isError()) {
    REQUEST_CLASS_ERROR_STATS(proxy, error, reqClass);
  }
  if (reply.isConnectError()) {
    REQUEST_CLASS_ERROR_STATS(proxy, connect_error, reqClass);
  }
  if (reply.isConnectTimeout()) {
    REQUEST_CLASS_ERROR_STATS(proxy, connect_timeout, reqClass);
  }
  if (reply.isDataTimeout()) {
    REQUEST_CLASS_ERROR_STATS(proxy, data_timeout, reqClass);
  }
  if (reply.isRedirect()) {
    REQUEST_CLASS_ERROR_STATS(proxy, busy, reqClass);
  }
}

}

template <class Operation>
void ProxyRequestLogger::log(const ProxyClientCommon& pclient,
                             const ProxyMcRequest& request,
                             const ProxyMcReply& reply,
                             const int64_t startTimeUs,
                             const int64_t endTimeUs,
                             Operation) {

  auto durationUs = endTimeUs - startTimeUs;
  bool isOutlier = proxy_->opts.logging_rtt_outlier_threshold_us > 0 &&
    durationUs >= proxy_->opts.logging_rtt_outlier_threshold_us;

  logError(*proxy_, reply, request.getRequestClass());
  logRequestClass(*proxy_, Operation(), request.getRequestClass());
  proxy_->durationUs.insertSample(durationUs);

  if (isOutlier) {
    logOutlier(*proxy_, Operation(), request.getRequestClass());
  }
}

}}}  // facebook::memcache::mcrouter
