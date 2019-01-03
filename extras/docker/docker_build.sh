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

# Utility for building an image then configuring and starting a container
# with the `unum` agent configured and running.

set -eo pipefail

usage() {
    echo "Usage: $0 [-X|-B <builder>] <WAN ifname>"
}

declare -i skip_build
declare -i builder_image_provided
declare builder_image_name
declare -i is_darwin
if [[ $(uname -s) == "Darwin" ]]; then
    is_darwin=1
fi
while getopts 'hXB:' opt; do
    case "$opt" in
        X ) skip_build=1
            ;;
        B ) builder_image_provided=1
            builder_image_name="$OPTARG"
            skip_build=
            ;;
        h ) usage; exit 0
            ;;
        * ) usage; exit 1
            ;;
    esac
done
shift $(( OPTIND - 1 ))

ifname_wan=$1
if [[ "$ifname_wan" == "" ]]; then
    echo "error: must specify host WAN ifname"
    echo
    usage
    exit 1
fi

wlan_phyname=${2:-phy0}

if [[ $(uname -s) == "Linux" ]]; then
    # On Linux, managing `docker` requires superuser privileges, but some
    # setups may use a 'docker' user group instead of root. This checks if the
    # uid of the current user is 0 (ie. root), failing that uses the `docker`
    # command as the final, absolute test.
    if [[ $(id -u) != "0" && \
        $(docker version > /dev/null || echo FAIL) == "FAIL" ]]
    then
        # Fail early and in a controlled manner
        echo "This command requires superuser privileges on Linux!"
        exit 1
    fi
fi

declare -r project_root=$(readlink -e "$(dirname "$BASH_SOURCE")/../../")
if [[ -z "$project_root" ]]; then
    echo "error: unable to determine unum project root"
    exit 1
fi

echo "---> project root: $project_root"

declare -r dockerfile=$(readlink -e "$project_root/extras/docker/Dockerfile")
declare -r dockerfile_build="$dockerfile.build"

declare -r image_suffix=$(grep -e '^FROM ' "$dockerfile" | grep -v ' as ' | head -n1 | sed -e 's:^FROM ::' -e 's: as .*$::' -e 's/:/-/')
declare -r image_name="minimsecure/unum:$image_suffix"
builder_image_name=${builder_image_name:-"minimsecure/unum-builder:$image_suffix"}

declare -r container_name="unum-demo"

docker kill "$container_name" > /dev/null 2>&1 || :
docker rm "$container_name" > /dev/null 2>&1 || :

pushd "$project_root" > /dev/null 2>&1
ln -sf "$project_root/extras/docker/dockerignore" "$project_root/.dockerignore"
trap "rm -f $project_root/.dockerignore; popd > /dev/null 2>&1" EXIT

if (( ! skip_build )); then
    # Build in two steps.
    # First, build "unum-builder" image which is a container configured to
    # build unum ... unless one was provided with the -B option.
    if (( ! builder_image_provided )); then
        echo "---> building image: $builder_image_name"
        echo "---> from dockerfile: $dockerfile_build"
        docker image build --file "$dockerfile_build" --tag "$builder_image_name" .
    fi

    # Second, build the actual distributable container image which includes the
    # already-built unum tarball and installs it as normal.
    echo "---> building image: $image_name"
    echo "---> from dockerfile: $dockerfile"
    echo "---> using builder image: $builder_image_name"
    docker image build --file "$dockerfile" \
        --build-arg builder="$builder_image_name" \
        --tag "$image_name" .
else
    echo "---> not building image (reusing existing, -X option)"
fi

rm -f $project_root/.dockerignore
trap - EXIT
popd

# Start a detached bash shell running the newly built image
echo "---> starting container: $container_name"
declare volume_opt
if (( ! is_darwin )); then
    volume_opt="--volume /dev/bus/usb:/dev/bus/usb"
fi
docker run                   \
    --privileged $volume_opt \
    --name "$container_name" \
    -itd "$image_name" /bin/bash -l

# Setup and connect the docker networks
if ! $(dirname "$BASH_SOURCE")/docker_network.sh "$container_name" "$ifname_wan" "$wlan_phyname"; then
    echo "---> failed to bring up docker networks, aborting"
    exit 1
fi

# Replace the current process with another bash session to explore the container.
echo "---> starting bash session"
echo "---> running: docker exec -it \"$container_name\" /bin/bash -l"
echo
echo
echo "  1. Sign up for a Minim Labs developer account on the Minim website"
echo "     at https://my.minim.co/labs"
echo
echo "  2. Configure and start all services, including Unum:"
echo "         minim-config"
echo
exec docker exec --interactive --tty "$container_name" /bin/bash -l
