#!/bin/bash -e
sudo apt-get update
sudo apt remove -y libunwind-14-dev
sudo apt-get install -qy meson \
    libgtkmm-3.0-dev \
    libconfig++-dev \
    libcurl4-openssl-dev \
    libxml2-dev \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libsecret-1-dev \
    libunrar-dev \
    libzip-dev \
    libpeas-dev