/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "yyjson", "index.html", [
    [ "Introduction", "index.html", [
      [ "Features", "index.html#autotoc_md1", null ],
      [ "Limitations", "index.html#autotoc_md2", null ],
      [ "Performance", "index.html#autotoc_md3", null ],
      [ "Sample Code", "index.html#autotoc_md7", null ],
      [ "Documentation", "index.html#autotoc_md12", null ],
      [ "Packaging status", "index.html#autotoc_md13", null ],
      [ "Built With yyjson", "index.html#autotoc_md14", null ],
      [ "TODO for v1.0", "index.html#autotoc_md15", null ],
      [ "License", "index.html#autotoc_md16", null ]
    ] ],
    [ "Building and testing", "md_doc__build_and_test.html", [
      [ "Source code", "md_doc__build_and_test.html#autotoc_md17", null ],
      [ "Package manager", "md_doc__build_and_test.html#autotoc_md18", [
        [ "Use vcpkg", "md_doc__build_and_test.html#autotoc_md19", null ]
      ] ],
      [ "CMake", "md_doc__build_and_test.html#autotoc_md20", [
        [ "Use CMake to build a library", "md_doc__build_and_test.html#autotoc_md21", null ],
        [ "Use CMake as a dependency", "md_doc__build_and_test.html#autotoc_md22", null ],
        [ "Use CMake to generate project", "md_doc__build_and_test.html#autotoc_md23", null ],
        [ "Use CMake to generate documentation", "md_doc__build_and_test.html#autotoc_md24", null ],
        [ "Testing With CMake and CTest", "md_doc__build_and_test.html#autotoc_md25", null ]
      ] ],
      [ "Compile-time Options", "md_doc__build_and_test.html#autotoc_md26", null ]
    ] ],
    [ "API", "md_doc__a_p_i.html", [
      [ "API Design", "md_doc__a_p_i.html#autotoc_md27", [
        [ "API prefix", "md_doc__a_p_i.html#autotoc_md28", null ],
        [ "API for immutable/mutable data", "md_doc__a_p_i.html#autotoc_md29", null ],
        [ "API for string", "md_doc__a_p_i.html#autotoc_md30", null ]
      ] ],
      [ "Reading JSON", "md_doc__a_p_i.html#autotoc_md32", [
        [ "Read JSON from string", "md_doc__a_p_i.html#autotoc_md33", null ],
        [ "Read JSON from file", "md_doc__a_p_i.html#autotoc_md34", null ],
        [ "Read JSON from file pointer", "md_doc__a_p_i.html#autotoc_md35", null ],
        [ "Read JSON with options", "md_doc__a_p_i.html#autotoc_md36", null ],
        [ "Reader error handling", "md_doc__a_p_i.html#autotoc_md37", null ],
        [ "Reader flag", "md_doc__a_p_i.html#autotoc_md38", null ]
      ] ],
      [ "Writing JSON", "md_doc__a_p_i.html#autotoc_md40", [
        [ "Write JSON to string", "md_doc__a_p_i.html#autotoc_md41", null ],
        [ "Write JSON to file", "md_doc__a_p_i.html#autotoc_md42", null ],
        [ "Write JSON to file pointer", "md_doc__a_p_i.html#autotoc_md43", null ],
        [ "Write JSON with options", "md_doc__a_p_i.html#autotoc_md44", null ],
        [ "Writer flag", "md_doc__a_p_i.html#autotoc_md45", null ]
      ] ],
      [ "Accessing JSON Document", "md_doc__a_p_i.html#autotoc_md47", [
        [ "JSON Document", "md_doc__a_p_i.html#autotoc_md48", null ],
        [ "JSON Value", "md_doc__a_p_i.html#autotoc_md49", null ],
        [ "JSON Array", "md_doc__a_p_i.html#autotoc_md50", null ],
        [ "JSON Array Iterator", "md_doc__a_p_i.html#autotoc_md51", null ],
        [ "JSON Object", "md_doc__a_p_i.html#autotoc_md52", null ],
        [ "JSON Object Iterator", "md_doc__a_p_i.html#autotoc_md53", null ]
      ] ],
      [ "Creating JSON Document", "md_doc__a_p_i.html#autotoc_md55", [
        [ "Mutable Document", "md_doc__a_p_i.html#autotoc_md56", null ],
        [ "JSON Value Creation", "md_doc__a_p_i.html#autotoc_md57", null ],
        [ "JSON Array Creation", "md_doc__a_p_i.html#autotoc_md58", null ],
        [ "JSON Array Modification", "md_doc__a_p_i.html#autotoc_md59", null ],
        [ "JSON Object Creation", "md_doc__a_p_i.html#autotoc_md60", null ],
        [ "JSON Object Modification", "md_doc__a_p_i.html#autotoc_md61", null ]
      ] ],
      [ "JSON Pointer and Patch", "md_doc__a_p_i.html#autotoc_md63", [
        [ "JSON Pointer", "md_doc__a_p_i.html#autotoc_md64", null ],
        [ "JSON Patch", "md_doc__a_p_i.html#autotoc_md65", null ],
        [ "JSON Merge Patch", "md_doc__a_p_i.html#autotoc_md66", null ]
      ] ],
      [ "Number Processing", "md_doc__a_p_i.html#autotoc_md68", [
        [ "Number reader", "md_doc__a_p_i.html#autotoc_md69", null ],
        [ "Number writer", "md_doc__a_p_i.html#autotoc_md70", null ]
      ] ],
      [ "Text Processing", "md_doc__a_p_i.html#autotoc_md71", [
        [ "Character Encoding", "md_doc__a_p_i.html#autotoc_md72", null ],
        [ "NUL Character", "md_doc__a_p_i.html#autotoc_md73", null ]
      ] ],
      [ "Memory Allocator", "md_doc__a_p_i.html#autotoc_md74", [
        [ "Single allocator for multiple JSON", "md_doc__a_p_i.html#autotoc_md75", null ],
        [ "Stack memory allocator", "md_doc__a_p_i.html#autotoc_md76", null ],
        [ "Use a third-party allocator library", "md_doc__a_p_i.html#autotoc_md77", null ]
      ] ],
      [ "Stack Memory Usage", "md_doc__a_p_i.html#autotoc_md78", null ],
      [ "Null Check", "md_doc__a_p_i.html#autotoc_md79", null ],
      [ "Thread Safety", "md_doc__a_p_i.html#autotoc_md80", null ],
      [ "Locale Independence", "md_doc__a_p_i.html#autotoc_md81", null ]
    ] ],
    [ "Data Structures", "md_doc__data_structure.html", [
      [ "Immutable Value", "md_doc__data_structure.html#autotoc_md83", null ],
      [ "Immutable Document", "md_doc__data_structure.html#autotoc_md84", null ],
      [ "Mutable Value", "md_doc__data_structure.html#autotoc_md86", null ],
      [ "Mutable Document", "md_doc__data_structure.html#autotoc_md87", null ],
      [ "Memory Management", "md_doc__data_structure.html#autotoc_md89", null ]
    ] ],
    [ "Changelog", "md__c_h_a_n_g_e_l_o_g.html", [
      [ "0.10.0 (2024-07-09)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md91", null ],
      [ "0.9.0 (2024-04-08)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md95", null ],
      [ "0.8.0 (2023-09-13)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md99", null ],
      [ "0.7.0 (2023-05-25)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md103", null ],
      [ "0.6.0 (2022-12-12)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md107", null ],
      [ "0.5.1 (2022-06-17)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md110", null ],
      [ "0.5.0 (2022-05-25)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md112", null ],
      [ "0.4.0 (2021-12-12)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md116", null ],
      [ "0.3.0 (2021-05-25)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md120", null ],
      [ "0.2.0 (2020-12-12)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md124", null ],
      [ "0.1.0 (2020-10-26)", "md__c_h_a_n_g_e_l_o_g.html#autotoc_md129", null ]
    ] ],
    [ "Deprecated List", "deprecated.html", null ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "Globals", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", "globals_func" ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ],
      [ "Data Structures", "annotated.html", [
        [ "Data Structures", "annotated.html", "annotated_dup" ],
        [ "Data Structure Index", "classes.html", null ],
        [ "Data Fields", "functions.html", [
          [ "All", "functions.html", null ],
          [ "Variables", "functions_vars.html", null ]
        ] ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"yyjson_8h.html#a41f9c6e4641f813a7a94f12ea79b34ce",
"yyjson_8h.html#acd2884309c99b42f916fffd50c018c59"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';