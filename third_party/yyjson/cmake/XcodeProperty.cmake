# Copyright (C) 2019 Yaoyuan <ibireme@gmail.com>.
# Released under the MIT License:
# https://github.com/ibireme/yyjson/blob/master/LICENSE


# This module contains some macros for Xcode project.
# For example:
# if(XCODE)
#    set_default_xcode_property(yyjson)
#    set_xcode_deployment_version(yyjson "10.11" "9.0" "9.0" "2.0")
#    set_xcode_property(yyjson GCC_C_LANGUAGE_STANDARD "c89")
#    set_xcode_property(yyjson CLANG_CXX_LANGUAGE_STANDARD "c++98")
# endif()


# Set Xcode property to target
# For example: set_xcode_property(yyjson GCC_C_LANGUAGE_STANDARD "c89")
macro(set_xcode_property TARGET XCODE_PROPERTY XCODE_VALUE)
    set_property(TARGET ${TARGET} PROPERTY XCODE_ATTRIBUTE_${XCODE_PROPERTY} ${XCODE_VALUE})
endmacro(set_xcode_property)


# Set default Xcode properties to target
# For example: set_default_xcode_property(yyjson)
macro(set_default_xcode_property TARGET)     

    # Standard   
    set_xcode_property(${TARGET} GCC_C_LANGUAGE_STANDARD "gnu99")
    set_xcode_property(${TARGET} CLANG_CXX_LANGUAGE_STANDARD "gnu++11")
    set_xcode_property(${TARGET} CLANG_CXX_LIBRARY "libc++")

    # Compiler Flags
    set_xcode_property(${TARGET} OTHER_CFLAGS[variant=Debug] " ")
    set_xcode_property(${TARGET} OTHER_CFLAGS[variant=MinSizeRel] " ")
    set_xcode_property(${TARGET} OTHER_CFLAGS[variant=RelWithDebInfo] " ")
    set_xcode_property(${TARGET} OTHER_CFLAGS[variant=Release] " ")
    set_xcode_property(${TARGET} OTHER_CPLUSPLUSFLAGS[variant=Debug] "$(OTHER_CFLAGS)")
    set_xcode_property(${TARGET} OTHER_CPLUSPLUSFLAGS[variant=MinSizeRel] "$(OTHER_CFLAGS)")
    set_xcode_property(${TARGET} OTHER_CPLUSPLUSFLAGS[variant=RelWithDebInfo] "$(OTHER_CFLAGS)")
    set_xcode_property(${TARGET} OTHER_CPLUSPLUSFLAGS[variant=Release] "$(OTHER_CFLAGS)")
    
    # Macros
    set_xcode_property(${TARGET} GCC_PREPROCESSOR_DEFINITIONS[variant=Debug] "DEBUG=1")
    set_xcode_property(${TARGET} GCC_PREPROCESSOR_DEFINITIONS[variant=MinSizeRel] "NDEBUG=1")
    set_xcode_property(${TARGET} GCC_PREPROCESSOR_DEFINITIONS[variant=RelWithDebInfo] "NDEBUG=1")
    set_xcode_property(${TARGET} GCC_PREPROCESSOR_DEFINITIONS[variant=Release] "NDEBUG=1")

    # Architectures
    set_xcode_property(${TARGET} ARCHS "$(ARCHS_STANDARD)")
    set_xcode_property(${TARGET} ONLY_ACTIVE_ARCH[variant=Debug] "YES")
    set_xcode_property(${TARGET} ONLY_ACTIVE_ARCH[variant=MinSizeRel] "NO")
    set_xcode_property(${TARGET} ONLY_ACTIVE_ARCH[variant=RelWithDebInfo] "NO")
    set_xcode_property(${TARGET} ONLY_ACTIVE_ARCH[variant=Release] "NO")
    set_xcode_property(${TARGET} SDKROOT "macosx")
    
    # Debug Information
    set_xcode_property(${TARGET} DEBUG_INFORMATION_FORMAT[variant=Debug] "dwarf")
    set_xcode_property(${TARGET} DEBUG_INFORMATION_FORMAT[variant=MinSizeRel] "dwarf-with-dsym")
    set_xcode_property(${TARGET} DEBUG_INFORMATION_FORMAT[variant=RelWithDebInfo] "dwarf")
    set_xcode_property(${TARGET} DEBUG_INFORMATION_FORMAT[variant=Release] "dwarf-with-dsym")
    set_xcode_property(${TARGET} GCC_GENERATE_DEBUGGING_SYMBOLS[variant=Debug] "YES")
    set_xcode_property(${TARGET} GCC_GENERATE_DEBUGGING_SYMBOLS[variant=MinSizeRel] "NO")
    set_xcode_property(${TARGET} GCC_GENERATE_DEBUGGING_SYMBOLS[variant=RelWithDebInfo] "YES")
    set_xcode_property(${TARGET} GCC_GENERATE_DEBUGGING_SYMBOLS[variant=Release] "NO")
    set_xcode_property(${TARGET} GCC_NO_COMMON_BLOCKS "YES")
    
    # Common Warnings
    set_xcode_property(${TARGET} CLANG_WARN_BLOCK_CAPTURE_AUTORELEASING "YES")
    set_xcode_property(${TARGET} CLANG_WARN_DOCUMENTATION_COMMENTS "YES")
    set_xcode_property(${TARGET} CLANG_WARN_EMPTY_BODY "YES")
    set_xcode_property(${TARGET} GCC_WARN_SHADOW "YES") ###
    set_xcode_property(${TARGET} CLANG_WARN_BOOL_CONVERSION "YES")
    set_xcode_property(${TARGET} CLANG_WARN_CONSTANT_CONVERSION "YES")
    set_xcode_property(${TARGET} GCC_WARN_64_TO_32_BIT_CONVERSION "YES")
    set_xcode_property(${TARGET} CLANG_WARN_ENUM_CONVERSION "YES")
    set_xcode_property(${TARGET} CLANG_WARN_FLOAT_CONVERSION "YES") ###
    set_xcode_property(${TARGET} CLANG_WARN_INT_CONVERSION "YES")
    set_xcode_property(${TARGET} CLANG_WARN_NON_LITERAL_NULL_CONVERSION "YES")
    set_xcode_property(${TARGET} CLANG_WARN_INFINITE_RECURSION "YES")
    set_xcode_property(${TARGET} GCC_WARN_ABOUT_RETURN_TYPE "YES_ERROR")
    set_xcode_property(${TARGET} GCC_WARN_ABOUT_MISSING_NEWLINE "YES") ###
    set_xcode_property(${TARGET} CLANG_WARN_ASSIGN_ENUM "YES") ###
    set_xcode_property(${TARGET} CLANG_WARN_QUOTED_INCLUDE_IN_FRAMEWORK_HEADER "YES")
    set_xcode_property(${TARGET} GCC_WARN_SIGN_COMPARE "YES") ###
    set_xcode_property(${TARGET} CLANG_WARN_STRICT_PROTOTYPES "YES")
    set_xcode_property(${TARGET} CLANG_WARN_COMMA "YES")
    set_xcode_property(${TARGET} CLANG_WARN_SUSPICIOUS_IMPLICIT_CONVERSION "YES") ###
    set_xcode_property(${TARGET} CLANG_WARN_UNGUARDED_AVAILABILITY "YES_AGGRESSIVE")
    set_xcode_property(${TARGET} GCC_WARN_UNINITIALIZED_AUTOS "YES_AGGRESSIVE")
    set_xcode_property(${TARGET} CLANG_WARN_UNREACHABLE_CODE "YES")
    set_xcode_property(${TARGET} GCC_WARN_UNUSED_FUNCTION "YES")
    set_xcode_property(${TARGET} GCC_WARN_UNUSED_VALUE "YES")
    set_xcode_property(${TARGET} GCC_WARN_UNUSED_VARIABLE "YES") ###

    # C++ Warnings
    set_xcode_property(${TARGET} CLANG_WARN_RANGE_LOOP_ANALYSIS "YES")
    set_xcode_property(${TARGET} CLANG_WARN_SUSPICIOUS_MOVE "YES")

    # ObjC Warnings
    set_xcode_property(${TARGET} CLANG_WARN_DIRECT_OBJC_ISA_USAGE "YES_ERROR")
    set_xcode_property(${TARGET} CLANG_WARN__DUPLICATE_METHOD_MATCH "YES")
    set_xcode_property(${TARGET} CLANG_WARN_OBJC_LITERAL_CONVERSION "YES")
    set_xcode_property(${TARGET} CLANG_WARN_DEPRECATED_OBJC_IMPLEMENTATIONS "YES")
    set_xcode_property(${TARGET} GCC_WARN_UNDECLARED_SELECTOR "YES")
    set_xcode_property(${TARGET} CLANG_WARN_OBJC_ROOT_CLASS "YES_ERROR")
    set_xcode_property(${TARGET} CLANG_WARN_OBJC_IMPLICIT_RETAIN_SELF "YES")

    # ObjC Options
    set_xcode_property(${TARGET} CLANG_ENABLE_OBJC_ARC "YES")
    set_xcode_property(${TARGET} CLANG_ENABLE_OBJC_WEAK "YES")
    set_xcode_property(${TARGET} ENABLE_NS_ASSERTIONS[variant=Debug] "YES")
    set_xcode_property(${TARGET} ENABLE_NS_ASSERTIONS[variant=MinSizeRel] "NO")
    set_xcode_property(${TARGET} ENABLE_NS_ASSERTIONS[variant=RelWithDebInfo] "NO")
    set_xcode_property(${TARGET} ENABLE_NS_ASSERTIONS[variant=Release] "NO")

endmacro(set_default_xcode_property)


# Set Xcode deployment version (macOS, iOS, tvOS, watchOS)
# For example: set_xcode_deployment_version(some_target "10.11" "9.0" "9.0" "2.0")
macro(set_xcode_deployment_version TARGET macOS iOS tvOS watchOS)
    set_xcode_property(${TARGET} MACOSX_DEPLOYMENT_TARGET ${macOS})
    set_xcode_property(${TARGET} IPHONEOS_DEPLOYMENT_TARGET ${iOS})
    set_xcode_property(${TARGET} TVOS_DEPLOYMENT_TARGET ${tvOS})
    set_xcode_property(${TARGET} WATCHOS_DEPLOYMENT_TARGET ${watchOS})
endmacro(set_xcode_deployment_version)


# Set Xcode language standard (C, CXX)
# For example: set_xcode_language_standard(some_target "gnu11" "gnu++17")
macro(set_xcode_language_standard TARGET C CXX)
    set_xcode_property(${TARGET} GCC_C_LANGUAGE_STANDARD ${C})
    set_xcode_property(${TARGET} CLANG_CXX_LANGUAGE_STANDARD ${CXX})
endmacro(set_xcode_language_standard)
