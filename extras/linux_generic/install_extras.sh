#!/usr/bin/env bash
# Copyright 2018 Minim Inc
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


declare -r working_dir=$(dirname "$BASH_SOURCE")
declare -r install_dir=$(readlink -e "$working_dir/../..")

declare install_etc_dir="/etc/opt/unum"
declare install_var_dir="/var/opt/unum"

if [[ "$install_dir" != "/opt/unum" ]]; then
    #TODO: prompt user for etc and var paths?
    echo "warning, might not install correctly"
fi

pushd "$working_dir" > /dev/null 2>&1

trap 'popd > /dev/null 2>&1' EXIT

mkdir -p "$install_etc_dir"
mkdir -p "$install_var_dir"

ln -sf "$working_dir/etc/profile.d/99-unum.sh" "/etc/profile.d/99-unum.sh"

mkdir -p "$install_etc_dir/config"
cp -f "$working_dir/etc/defaults/"* "$install_etc_dir/config"
