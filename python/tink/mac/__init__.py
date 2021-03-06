# Copyright 2020 Google LLC.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
"""Mac package."""
from __future__ import absolute_import
from __future__ import division
# Placeholder for import for type annotations
from __future__ import print_function

from tink.mac import mac
from tink.mac import mac_key_manager
from tink.mac import mac_key_templates
from tink.mac import mac_wrapper


Mac = mac.Mac
MacWrapper = mac_wrapper.MacWrapper
key_manager_from_cc_registry = mac_key_manager.from_cc_registry
