# Copyright (C) 2012 Paul Kocialkowski <contact@paulk.fr>
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
#

$(call add-clean-step, find $(PRODUCT_OUT) -name "*.apk" | xargs rm)
$(call add-clean-step, rm -rf $(TARGET_OUT)/build.prop)
$(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/PACKAGING/systemimage_intermediates)
# $(call add-clean-step, rm -rf $(PRODUCT_OUT)/obj/SHARED_LIBRARIES/libaudio_intermediates)
# add newer instructions according to what we'll have in this directory
