#!/bin/bash

if [[ "$1" != "" ]]; then
    MY_BOOST="$1"
else
    MY_BOOST=../boost-root
fi


echo 'compiling test/fuzzing/test_fuzzing_add.cpp'   && clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I$MY_BOOST test/fuzzing/test_fuzzing_add.cpp   -o test_fuzzing_add
echo 'compiling test/fuzzing/test_fuzzing_sub.cpp'   && clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I$MY_BOOST test/fuzzing/test_fuzzing_sub.cpp   -o test_fuzzing_sub
echo 'compiling test/fuzzing/test_fuzzing_mul.cpp'   && clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I$MY_BOOST test/fuzzing/test_fuzzing_mul.cpp   -o test_fuzzing_mul
echo 'compiling test/fuzzing/test_fuzzing_div.cpp'   && clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I$MY_BOOST test/fuzzing/test_fuzzing_div.cpp   -o test_fuzzing_div
echo 'compiling test/fuzzing/test_fuzzing_sdiv.cpp'  && clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I$MY_BOOST test/fuzzing/test_fuzzing_sdiv.cpp  -o test_fuzzing_sdiv
echo 'compiling test/fuzzing/test_fuzzing_sqrt.cpp'  && clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I$MY_BOOST test/fuzzing/test_fuzzing_sqrt.cpp  -o test_fuzzing_sqrt
echo 'compiling test/fuzzing/test_fuzzing_powm.cpp'  && clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I$MY_BOOST test/fuzzing/test_fuzzing_powm.cpp  -o test_fuzzing_powm
echo 'compiling test/fuzzing/test_fuzzing_prime.cpp' && clang++ -std=c++20 -g -O2 -Wall -Wextra -fsanitize=fuzzer -I. -I$MY_BOOST test/fuzzing/test_fuzzing_prime.cpp -o test_fuzzing_prime


ls -la test_fuzzing_add test_fuzzing_sub test_fuzzing_mul test_fuzzing_div test_fuzzing_sdiv test_fuzzing_sqrt test_fuzzing_powm test_fuzzing_prime
exit_compile=$?


# Start each executable in the background and save their process IDs
./test_fuzzing_add -max_total_time=900 -max_len=34 -verbosity=0 -close_fd_mask=3 &
pid_add=$!

./test_fuzzing_sub -max_total_time=900 -max_len=34 -verbosity=0 -close_fd_mask=3 &
pid_sub=$!

./test_fuzzing_mul -max_total_time=900 -max_len=34 -verbosity=0 -close_fd_mask=3 &
pid_mul=$!

./test_fuzzing_div -max_total_time=900 -max_len=34 -verbosity=0 -close_fd_mask=3 &
pid_div=$!

./test_fuzzing_sdiv -max_total_time=900 -max_len=34 -verbosity=0 -close_fd_mask=3 &
pid_sdiv=$!

./test_fuzzing_sqrt -max_total_time=900 -max_len=34 -verbosity=0 -close_fd_mask=3 &
pid_sqrt=$!

./test_fuzzing_powm -max_total_time=900 -max_len=34 -verbosity=0 -close_fd_mask=3 &
pid_powm=$!

./test_fuzzing_prime -max_total_time=900 -max_len=34 -verbosity=0 -close_fd_mask=3 &
pid_prime=$!


# Wait for each job and capture its exit status
wait $pid_add
exit_add=$?
wait $pid_sub
exit_sub=$?
wait $pid_mul
exit_mul=$?
wait $pid_div
exit_div=$?
wait $pid_sdiv
exit_sdiv=$?
wait $pid_sqrt
exit_sqrt=$?
wait $pid_powm
exit_powm=$?
wait $pid_prime
exit_prime=$?

# Check the status of compilation and of each executable

echo "exit_compile         : "  "$exit_compile"
echo "exit_add             : "  "$exit_add"
echo "exit_sub             : "  "$exit_sub"
echo "exit_mul             : "  "$exit_mul"
echo "exit_div             : "  "$exit_div"
echo "exit_sdiv            : "  "$exit_sdiv"
echo "exit_sqrt            : "  "$exit_sqrt"
echo "exit_powm            : "  "$exit_powm"
echo "exit_prime           : "  "$exit_prime"

result_total=$((exit_compile+exit_add+exit_sub+exit_mul+exit_div+exit_sdiv+exit_sqrt+exit_powm+exit_prime))

echo "result_total         : "  "$result_total"

exit $result_total
