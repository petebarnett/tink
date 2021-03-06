// Copyright 2018 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
///////////////////////////////////////////////////////////////////////////////

#include "tink/signature/rsa_ssa_pss_sign_key_manager.h"

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "tink/public_key_sign.h"
#include "tink/signature/rsa_ssa_pss_verify_key_manager.h"
#include "tink/signature/sig_util.h"
#include "tink/subtle/rsa_ssa_pss_sign_boringssl.h"
#include "tink/subtle/subtle_util_boringssl.h"
#include "tink/util/enums.h"
#include "tink/util/errors.h"
#include "tink/util/protobuf_helper.h"
#include "tink/util/status.h"
#include "tink/util/statusor.h"
#include "tink/util/validation.h"
#include "proto/rsa_ssa_pss.pb.h"
#include "proto/tink.pb.h"

namespace crypto {
namespace tink {

using crypto::tink::util::Enums;
using crypto::tink::util::Status;
using crypto::tink::util::StatusOr;
using google::crypto::tink::RsaSsaPssKeyFormat;
using google::crypto::tink::RsaSsaPssParams;
using google::crypto::tink::RsaSsaPssPrivateKey;

namespace {
std::unique_ptr<RsaSsaPssPrivateKey> RsaPrivateKeySubtleToProto(
    const subtle::SubtleUtilBoringSSL::RsaPrivateKey& private_key) {
  auto key_proto = absl::make_unique<RsaSsaPssPrivateKey>();
  key_proto->set_version(RsaSsaPssSignKeyManager().get_version());
  key_proto->set_d(private_key.d);
  key_proto->set_p(private_key.p);
  key_proto->set_q(private_key.q);
  key_proto->set_dp(private_key.dp);
  key_proto->set_dq(private_key.dq);
  key_proto->set_crt(private_key.crt);
  auto* public_key_proto = key_proto->mutable_public_key();
  public_key_proto->set_version(RsaSsaPssSignKeyManager().get_version());
  public_key_proto->set_n(private_key.n);
  public_key_proto->set_e(private_key.e);
  return key_proto;
}

subtle::SubtleUtilBoringSSL::RsaPrivateKey RsaPrivateKeyProtoToSubtle(
    const RsaSsaPssPrivateKey& key_proto) {
  subtle::SubtleUtilBoringSSL::RsaPrivateKey key;
  key.n = key_proto.public_key().n();
  key.e = key_proto.public_key().e();
  key.d = key_proto.d();
  key.p = key_proto.p();
  key.q = key_proto.q();
  key.dp = key_proto.dp();
  key.dq = key_proto.dq();
  key.crt = key_proto.crt();
  return key;
}

}  // namespace

StatusOr<RsaSsaPssPrivateKey> RsaSsaPssSignKeyManager::CreateKey(
    const RsaSsaPssKeyFormat& key_format) const {
  auto e = subtle::SubtleUtilBoringSSL::str2bn(
      key_format.public_exponent());
  if (!e.ok()) return e.status();

  subtle::SubtleUtilBoringSSL::RsaPrivateKey private_key;
  subtle::SubtleUtilBoringSSL::RsaPublicKey public_key;
  util::Status status = subtle::SubtleUtilBoringSSL::GetNewRsaKeyPair(
      key_format.modulus_size_in_bits(), e.ValueOrDie().get(),
      &private_key, &public_key);
  if (!status.ok()) return status;

  RsaSsaPssPrivateKey key_proto =
      std::move(*RsaPrivateKeySubtleToProto(private_key));
  *key_proto.mutable_public_key()->mutable_params() = key_format.params();
  return key_proto;
}

StatusOr<std::unique_ptr<PublicKeySign>>
RsaSsaPssSignKeyManager::PublicKeySignFactory::Create(
    const RsaSsaPssPrivateKey& private_key) const {
  auto key = RsaPrivateKeyProtoToSubtle(private_key);
  subtle::SubtleUtilBoringSSL::RsaSsaPssParams params;
  const RsaSsaPssParams& params_proto = private_key.public_key().params();
  params.sig_hash = Enums::ProtoToSubtle(params_proto.sig_hash());
  params.mgf1_hash = Enums::ProtoToSubtle(params_proto.mgf1_hash());
  params.salt_length = params_proto.salt_length();
  auto signer = subtle::RsaSsaPssSignBoringSsl::New(key, params);
  if (!signer.ok()) return signer.status();
  // To check that the key is correct, we sign a test message with private key
  // and verify with public key.
  auto verifier = RsaSsaPssVerifyKeyManager().GetPrimitive<PublicKeyVerify>(
      private_key.public_key());
  if (!verifier.ok()) return verifier.status();
  auto sign_verify_result =
      SignAndVerify(signer.ValueOrDie().get(), verifier.ValueOrDie().get());
  if (!sign_verify_result.ok()) {
    return util::Status(util::error::INTERNAL,
                        "security bug: signing with private key followed by "
                        "verifying with public key failed");
  }
  return signer;
}

Status RsaSsaPssSignKeyManager::ValidateKey(
    const RsaSsaPssPrivateKey& key) const {
  Status status = ValidateVersion(key.version(), get_version());
  if (!status.ok()) return status;
  return RsaSsaPssVerifyKeyManager().ValidateKey(key.public_key());
}

Status RsaSsaPssSignKeyManager::ValidateKeyFormat(
    const RsaSsaPssKeyFormat& key_format) const {
  auto modulus_status = subtle::SubtleUtilBoringSSL::ValidateRsaModulusSize(
      key_format.modulus_size_in_bits());
  if (!modulus_status.ok()) return modulus_status;
  return RsaSsaPssVerifyKeyManager().ValidateParams(key_format.params());
}

}  // namespace tink
}  // namespace crypto
