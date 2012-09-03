#!/bin/sh

arecord -fS16_LE -r16000 | ./3psk "$@" | aplay -B 250000
