# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: No rights reserved

FROM ghcr.io/andyholmes/actuary:latest

# Install dependencies
#
# See: https://github.com/andyholmes/copr/tree/main/glib2
RUN dnf install -y 'dnf-command(copr)' && \
    dnf copr -y enable andyholmes/main && \
    dnf install -y --enablerepo=fedora-debuginfo,updates-debuginfo \
        --setopt=install_weak_deps=False \
        gi-docgen graphviz rsync \
        glib2-devel         glib2-debuginfo \
        geoclue2-devel      geoclue2-debuginfo \
        geocode-glib2-devel geocode-glib2-debuginfo \
        gtk4-devel          gtk4-debuginfo \
        json-glib-devel     json-glib-debuginfo \
        libadwaita-devel    libadwaita-debuginfo \
        libportal-devel     libportal-debuginfo \
        libshumate-devel    libshumate-debuginfo \
        libsoup3-devel      libsoup3-debuginfo \
        protobuf-c-devel    protobuf-c-debuginfo \
        sqlite-devel        sqlite-debuginfo && \
    dnf clean all && rm -rf /var/cache/dnf
