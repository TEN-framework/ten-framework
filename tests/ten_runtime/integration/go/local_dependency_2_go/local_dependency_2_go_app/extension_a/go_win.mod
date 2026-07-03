module extension_a

go 1.20

// Windows resolves symlinks from the link location, not the target location.
// The latter is Linux/MacOS behavior.
replace ten_framework => ../../system/ten_runtime_go/interface

require ten_framework v0.0.0-00010101000000-000000000000
