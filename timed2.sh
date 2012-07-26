arecord -fS16_LE -r8000 | ../cwtbk/timed --in=${1:-aac.wav} | ./3psk $2 $3 $4 $5 $6 $7 $8 $9 | ../cwtbk/timed --in=${1:-aac.wav} | aplay
