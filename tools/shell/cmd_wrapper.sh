#!/bin/sh

# Execute the command passed as arguments to this script.
echo "==========================================="
echo "cmd wrapper executes the following command:"
echo "$@"
echo "==========================================="
"$@"

# Capture the exit status of the command.
status=$?

# Write the exit status to a file.
echo $status >_cmd_wrapper_exit_status.txt

# Exit with the captured status.
exit $status
