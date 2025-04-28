module extension_b

go 1.18

replace ten_framework => ../../../ten_packages/system/ten_runtime_go/interface

replace go_common_dep => ../../../go_common_dep

require ten_framework v0.0.0-00010101000000-000000000000

require go_common_dep v0.0.0-00010101000000-000000000000
