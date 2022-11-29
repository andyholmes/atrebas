# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: No rights reserved

FROM fedora:34

# The packages below are roughly grouped into build tooling and build
# dependencies (with debug symbols)
RUN dnf install -y --enablerepo=fedora-debuginfo,updates-debuginfo \
        glibc-langpack-en glibc-locale-source clang gcc gettext git libasan \
        libtsan libubsan meson appstream desktop-file-utils dbus-daemon lcov \
        rsync xorg-x11-server-Xvfb \
        geoclue2-devel     geoclue2-debuginfo \
        geocode-glib-devel geocode-glib-debuginfo \
        glib2-devel        glib2-debuginfo \
        gtk4-devel         gtk4-debuginfo \
        json-glib-devel    json-glib-debuginfo \
        libadwaita-devel   libadwaita-debuginfo \
        protobuf-c-devel   protobuf-c-debuginfo \
        libsoup-devel      libsoup-debuginfo \
        sqlite-devel       sqlite-debuginfo && \
    dnf clean all && rm -rf /var/cache/dnf

# Set the locale
ENV LANG=en_US.UTF-8
RUN localedef -c -i en_US -f UTF-8 en_US.UTF-8

