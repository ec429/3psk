arecord -fS16_LE -r16000 | ./3psk "$@" | aplay -B 1000000
