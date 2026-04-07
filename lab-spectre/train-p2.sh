for t in 1 2 3 4 5 8 10 12 16 20 24 32 40; do
  sed -i "s/const size_t training_rounds = [0-9]\\+;/const size_t training_rounds = $t;/" part2-src/attacker-part2.c
  make clean >/dev/null && make >/dev/null
  echo "training_rounds=$t"
  ./check.py 2 | tail -n 3
done