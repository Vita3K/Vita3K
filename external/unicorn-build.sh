#!/usr/bin/env bash
set -ex

UNICORN_ARCHS=arm UNICORN_DEBUG=no UNICORN_SHARED=no ./make.sh
