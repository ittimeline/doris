// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#pragma once

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>
#include <aws/s3/S3Errors.h>
#include <bvar/bvar.h>
#include <fmt/format.h>
#include <gen_cpp/AgentService_types.h>
#include <gen_cpp/cloud.pb.h>

#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "common/status.h"
#include "cpp/aws_common.h"
#include "cpp/s3_rate_limiter.h"
#include "io/fs/obj_storage_client.h"
#include "vec/common/string_ref.h"

namespace Aws::S3 {
class S3Client;
} // namespace Aws::S3

namespace bvar {
template <typename T>
class Adder;
}

namespace doris {

namespace s3_bvar {
extern bvar::LatencyRecorder s3_get_latency;
extern bvar::LatencyRecorder s3_put_latency;
extern bvar::LatencyRecorder s3_delete_object_latency;
extern bvar::LatencyRecorder s3_delete_objects_latency;
extern bvar::LatencyRecorder s3_head_latency;
extern bvar::LatencyRecorder s3_multi_part_upload_latency;
extern bvar::LatencyRecorder s3_list_latency;
extern bvar::LatencyRecorder s3_list_object_versions_latency;
extern bvar::LatencyRecorder s3_get_bucket_version_latency;
extern bvar::LatencyRecorder s3_copy_object_latency;
}; // namespace s3_bvar

class S3URI;
struct S3ClientConf {
    std::string endpoint;
    std::string region;
    std::string ak;
    std::string sk;
    std::string token;
    // For azure we'd better support the bucket at the first time init azure blob container client
    std::string bucket;
    io::ObjStorageType provider = io::ObjStorageType::AWS;
    int max_connections = -1;
    int request_timeout_ms = -1;
    int connect_timeout_ms = -1;
    bool use_virtual_addressing = true;
    // For aws s3, no need to override endpoint
    bool need_override_endpoint = true;

    CredProviderType cred_provider_type = CredProviderType::Default;
    std::string role_arn;
    std::string external_id;

    uint64_t get_hash() const {
        uint64_t hash_code = 0;
        hash_code ^= crc32_hash(ak);
        hash_code ^= crc32_hash(sk);
        hash_code ^= crc32_hash(token);
        hash_code ^= crc32_hash(endpoint);
        hash_code ^= crc32_hash(region);
        hash_code ^= crc32_hash(bucket);
        hash_code ^= max_connections;
        hash_code ^= request_timeout_ms;
        hash_code ^= connect_timeout_ms;
        hash_code ^= use_virtual_addressing;
        hash_code ^= static_cast<int>(provider);

        hash_code ^= static_cast<int>(cred_provider_type);
        hash_code ^= crc32_hash(role_arn);
        hash_code ^= crc32_hash(external_id);
        return hash_code;
    }

    std::string to_string() const {
        return fmt::format(
                "(ak={}, token={}, endpoint={}, region={}, bucket={}, max_connections={}, "
                "request_timeout_ms={}, connect_timeout_ms={}, use_virtual_addressing={}, "
                "cred_provider_type={},role_arn={}, external_id={}",
                ak, token, endpoint, region, bucket, max_connections, request_timeout_ms,
                connect_timeout_ms, use_virtual_addressing, cred_provider_type, role_arn,
                external_id);
    }
};

struct S3Conf {
    std::string bucket;
    std::string prefix;
    S3ClientConf client_conf;

    bool sse_enabled = false;
    static S3Conf get_s3_conf(const cloud::ObjectStoreInfoPB&);
    static S3Conf get_s3_conf(const TS3StorageParam&);

    std::string to_string() const {
        return fmt::format("(bucket={}, prefix={}, client_conf={}, sse_enabled={})", bucket, prefix,
                           client_conf.to_string(), sse_enabled);
    }
};

class S3ClientFactory {
public:
    ~S3ClientFactory();

    static S3ClientFactory& instance();

    std::shared_ptr<io::ObjStorageClient> create(const S3ClientConf& s3_conf);

    static Status convert_properties_to_s3_conf(const std::map<std::string, std::string>& prop,
                                                const S3URI& s3_uri, S3Conf* s3_conf);

    static Aws::Client::ClientConfiguration& getClientConfiguration() {
        // The default constructor of ClientConfiguration will do some http call
        // such as Aws::Internal::GetEC2MetadataClient and other init operation,
        // which is unnecessary.
        // So here we use a static instance, and deep copy every time
        // to avoid unnecessary operations.
        static Aws::Client::ClientConfiguration instance;
        return instance;
    }

    S3RateLimiterHolder* rate_limiter(S3RateLimitType type);

private:
    std::shared_ptr<io::ObjStorageClient> _create_s3_client(const S3ClientConf& s3_conf);
    std::shared_ptr<io::ObjStorageClient> _create_azure_client(const S3ClientConf& s3_conf);
    std::shared_ptr<Aws::Auth::AWSCredentialsProvider> get_aws_credentials_provider(
            const S3ClientConf& s3_conf);

    S3ClientFactory();

    Aws::SDKOptions _aws_options;
    std::mutex _lock;
    std::unordered_map<uint64_t, std::shared_ptr<io::ObjStorageClient>> _cache;
    std::string _ca_cert_file_path;
    std::array<std::unique_ptr<S3RateLimiterHolder>, 2> _rate_limiters;
};

} // end namespace doris
