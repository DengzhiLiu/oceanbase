/**
 * Copyright (c) 2021 OceanBase
 * OceanBase CE is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#ifndef _OB_SHARE_OB_GAIS_CLIENT_H_
#define _OB_SHARE_OB_GAIS_CLIENT_H_

#include "lib/net/ob_addr.h"
#include "share/ob_gais_msg.h"
#include "share/ob_gais_rpc.h"
#include "storage/tx/ob_trans_service.h"

namespace oceanbase
{
namespace share
{
class ObGAISClient
{
public:
  ObGAISClient() : is_inited_(false), self_(), gais_request_rpc_(nullptr),
                   location_adapter_(nullptr), gais_cache_leader_() { }
  ~ObGAISClient() { }
  int init(const common::ObAddr &self, share::ObGAISRequestRpc *gais_request_rpc,
           transaction::ObILocationAdapter *location_adapter);
  void reset();
  int refresh_location(const uint64_t tenant_id) { return refresh_location_(tenant_id); }
  TO_STRING_KV(K_(self), K_(gais_cache_leader));

public:
  int get_value(const AutoincKey &key,
                const uint64_t offset,
                const uint64_t increment,
                const uint64_t max_value,
                const uint64_t table_auto_increment,
                const uint64_t desired_count,
                const uint64_t cache_size,
                uint64_t &sync_value,
                uint64_t &start_inclusive,
                uint64_t &end_inclusive);
  int get_sequence_value(const AutoincKey &key, uint64_t &sequence_value);
  int get_auto_increment_values(const common::ObIArray<AutoincKey> &autoinc_keys,
                                common::hash::ObHashMap<AutoincKey, uint64_t> &seq_values);
  int local_push_to_global_value(const AutoincKey &key,
                                 const uint64_t max_value,
                                 const uint64_t local_sync_value,
                                 uint64_t &global_sync_value);
  int local_sync_with_global_value(const AutoincKey &key, uint64_t &global_sync_value);
  int clear_global_autoinc_cache(const AutoincKey &key);

private:
  int get_leader_(const uint64_t tenant_id, common::ObAddr &leader);
  int refresh_location_(const uint64_t tenant_id);
  inline void reset_cache_leader_()
  {
    lib::ObMutexGuard guard(cache_leader_mutex_);
    gais_cache_leader_.reset();
  }

private:
  bool is_inited_;
  common::ObAddr self_;
  share::ObGAISRequestRpc *gais_request_rpc_;
  transaction::ObILocationAdapter *location_adapter_;
  common::ObAddr gais_cache_leader_;
  lib::ObMutex cache_leader_mutex_;
};

} // share
} // oceanbase

#endif // _OB_SHARE_OB_GAIS_CLIENT_H_
