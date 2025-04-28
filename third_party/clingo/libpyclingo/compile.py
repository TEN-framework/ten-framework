#!/usr/bin/env python
"""
Helper to generate constructor function for clingo's python API.

Attention: Because this code exists just for code generation and won't run in
production, the functions in this module have been written with the least
amount of effort possible. Always check the generated code!
"""

import sys
import re
import argparse
import os.path
from itertools import chain
from cffi import FFI

try:
    from _clingo import ffi as _ffi, lib as _lib
    imported = True

    _an = _lib.g_clingo_ast_attribute_names
    _cs = _lib.g_clingo_ast_constructors
except ImportError:
    imported = False

def to_camel_case(s):
    components = s.split('_')
    return ''.join(x.title() for x in components)

def argument_type_str(idx):
    if idx == _lib.clingo_ast_attribute_type_number:
        return 'int'
    if idx == _lib.clingo_ast_attribute_type_string:
        return 'str'
    if idx == _lib.clingo_ast_attribute_type_symbol:
        return 'Symbol'
    if idx == _lib.clingo_ast_attribute_type_location:
        return 'Location'
    if idx == _lib.clingo_ast_attribute_type_ast:
        return 'AST'
    if idx == _lib.clingo_ast_attribute_type_optional_ast:
        return 'Optional[AST]'
    if idx == _lib.clingo_ast_attribute_type_ast_array:
        return 'Sequence[AST]'
    assert idx == _lib.clingo_ast_attribute_type_string_array
    return 'Sequence[str]'

def generate_arguments(constructor):
    args = []
    for i in range(constructor.size):
        argument = constructor.arguments[i]
        argument_type = argument_type_str(argument.type)
        name = _ffi.string(_an.names[argument.attribute]).decode()
        args.append((name, argument_type))
    return args

def generate_parameter(name, idx):
    if idx == _lib.clingo_ast_attribute_type_number:
        return [f"_ffi.cast('int', {name})"]
    if idx == _lib.clingo_ast_attribute_type_string:
        return [f"_ffi.new('char const[]', {name}.encode())"]
    if idx == _lib.clingo_ast_attribute_type_symbol:
        return [f"_ffi.cast('clingo_symbol_t', {name}._rep)"]
    if idx == _lib.clingo_ast_attribute_type_location:
        return [f'c_{name}[0]']
    if idx == _lib.clingo_ast_attribute_type_ast:
        return [f'{name}._rep']
    if idx == _lib.clingo_ast_attribute_type_optional_ast:
        return [f'_ffi.NULL if {name} is None else {name}._rep']
    if idx == _lib.clingo_ast_attribute_type_ast_array:
        return [f"_ffi.new('clingo_ast_t*[]', [ x._rep for x in {name} ])", f"_ffi.cast('size_t', len({name}))"]
    assert idx == _lib.clingo_ast_attribute_type_string_array
    return [f"_ffi.new('char*[]', c_{name})", f"_ffi.cast('size_t', len({name}))"]

def generate_aux(name, idx):
    if idx == _lib.clingo_ast_attribute_type_string_array:
        return [f"c_{name} = [ _ffi.new('char[]', x.encode()) for x in {name} ]"]
    if idx == _lib.clingo_ast_attribute_type_location:
        return [f"c_{name} = _c_location({name})"]
    return []

def generate_parameters(constructor):
    args, aux = [], []
    for i in range(constructor.size):
        argument = constructor.arguments[i]
        name = _ffi.string(_an.names[argument.attribute]).decode()
        args.extend(generate_parameter(name, argument.type))
        aux.extend(generate_aux(name, argument.type))
    return args, aux

def generate_sig(name, arguments):
    comma = False
    ret = f'def {name}('
    m = len(ret)
    n = m
    for a, t in arguments:
        if comma:
            ret += ", "
            n += 2
        else:
            comma = True
        arg = f'{a}: {t}'
        if n + len(arg) <= 111:
            n += len(arg)
        else:
            ret = ret[:-1] + "\n" + m * " "
            n = m + len(arg)
        ret += arg
    ret += ")"
    arg = ' -> AST:'
    assert n + len(arg) <= 120
    ret += arg
    return ret

def generate_python():
    for i in range(_cs.size):
        constructor = _cs.constructors[i]
        c_name = _ffi.string(constructor.name).decode()
        name = to_camel_case(c_name)
        sys.stdout.write(f'\n{generate_sig(name, generate_arguments(constructor))}\n')
        sys.stdout.write("    '''\n")
        sys.stdout.write(f'    Construct an AST node of type `ASTType.{name}`.\n')
        sys.stdout.write("    '''\n")
        parameters, aux = generate_parameters(constructor)
        sys.stdout.write(f"    p_ast = _ffi.new('clingo_ast_t**')\n")
        for x in aux:
            sys.stdout.write(f"    {x}\n")
        sys.stdout.write(f"    _handle_error(_lib.clingo_ast_build(\n")
        sys.stdout.write(f"        _lib.clingo_ast_type_{c_name}, p_ast")
        for param in parameters:
            sys.stdout.write(f",\n        {param}")
        sys.stdout.write(f"))\n")

        sys.stdout.write(f"    return AST(p_ast[0])\n")

def generate_c(action):
    clingo_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

    ffi = FFI()

    cnt = []
    with open(f'{clingo_dir}/libclingo/clingo.h') as f:
        for line in f:
            if not re.match(r' *(#|//|extern *"C" *{|}$|$)', line):
                cnt.append(line.replace('CLINGO_VISIBILITY_DEFAULT ', '').strip())

    # callbacks
    cnt.append('extern "Python" bool pyclingo_solve_event_callback(clingo_solve_event_type_t type, void *event, void *data, bool *goon);')
    cnt.append('extern "Python" void pyclingo_logger_callback(clingo_warning_t code, char const *message, void *data);')
    cnt.append('extern "Python" bool pyclingo_ground_callback(clingo_location_t const *location, char const *name, clingo_symbol_t const *arguments, size_t arguments_size, void *data, clingo_symbol_callback_t symbol_callback, void *symbol_callback_data);')
    # propagator callbacks
    cnt.append('extern "Python" bool pyclingo_propagator_init(clingo_propagate_init_t *init, void *data);')
    cnt.append('extern "Python" bool pyclingo_propagator_propagate(clingo_propagate_control_t *control, clingo_literal_t const *changes, size_t size, void *data);')
    cnt.append('extern "Python" void pyclingo_propagator_undo(clingo_propagate_control_t const *control, clingo_literal_t const *changes, size_t size, void *data);')
    cnt.append('extern "Python" bool pyclingo_propagator_check(clingo_propagate_control_t *control, void *data);')
    cnt.append('extern "Python" bool pyclingo_propagator_decide(clingo_id_t thread_id, clingo_assignment_t const *assignment, clingo_literal_t fallback, void *data, clingo_literal_t *decision);')
    # observer callbacks
    cnt.append('extern "Python" bool pyclingo_observer_init_program(bool incremental, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_begin_step(void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_end_step(void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_rule(bool choice, clingo_atom_t const *head, size_t head_size, clingo_literal_t const *body, size_t body_size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_weight_rule(bool choice, clingo_atom_t const *head, size_t head_size, clingo_weight_t lower_bound, clingo_weighted_literal_t const *body, size_t body_size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_minimize(clingo_weight_t priority, clingo_weighted_literal_t const* literals, size_t size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_project(clingo_atom_t const *atoms, size_t size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_output_atom(clingo_symbol_t symbol, clingo_atom_t atom, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_output_term(clingo_symbol_t symbol, clingo_literal_t const *condition, size_t size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_external(clingo_atom_t atom, clingo_external_type_t type, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_assume(clingo_literal_t const *literals, size_t size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_heuristic(clingo_atom_t atom, clingo_heuristic_type_t type, int bias, unsigned priority, clingo_literal_t const *condition, size_t size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_acyc_edge(int node_u, int node_v, clingo_literal_t const *condition, size_t size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_theory_term_number(clingo_id_t term_id, int number, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_theory_term_string(clingo_id_t term_id, char const *name, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_theory_term_compound(clingo_id_t term_id, int name_id_or_type, clingo_id_t const *arguments, size_t size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_theory_element(clingo_id_t element_id, clingo_id_t const *terms, size_t terms_size, clingo_literal_t const *condition, size_t condition_size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_theory_atom(clingo_id_t atom_id_or_zero, clingo_id_t term_id, clingo_id_t const *elements, size_t size, void *data);')
    cnt.append('extern "Python" bool pyclingo_observer_theory_atom_with_guard(clingo_id_t atom_id_or_zero, clingo_id_t term_id, clingo_id_t const *elements, size_t size, clingo_id_t operator_id, clingo_id_t right_hand_side_id, void *data);')
    # application callbacks
    cnt.append('extern "Python" char const *pyclingo_application_program_name(void *data);')
    cnt.append('extern "Python" char const *pyclingo_application_version(void *data);')
    cnt.append('extern "Python" unsigned pyclingo_application_message_limit(void *data);')
    cnt.append('extern "Python" bool pyclingo_application_main(clingo_control_t *control, char const *const * files, size_t size, void *data);')
    cnt.append('extern "Python" void pyclingo_application_logger(clingo_warning_t code, char const *message, void *data);')
    cnt.append('extern "Python" bool pyclingo_application_print_model(clingo_model_t const *model, clingo_default_model_printer_t printer, void *printer_data, void *data);')
    cnt.append('extern "Python" bool pyclingo_application_register_options(clingo_options_t *options, void *data);')
    cnt.append('extern "Python" bool pyclingo_application_validate_options(void *data);')
    # application options callbacks
    cnt.append('extern "Python" bool pyclingo_application_options_parse(char const *value, void *data);')
    # ast callbacks
    cnt.append('extern "Python" bool pyclingo_ast_callback(clingo_ast_t const *, void *);')
    # script callbacks
    cnt.append('extern "Python" bool pyclingo_script_execute(clingo_location_t *loc, char const *code, void *data);')
    cnt.append('extern "Python" bool pyclingo_script_call(clingo_location_t *loc, char const *name, void *arguments, size_t size, void *symbol_callback, void *symbol_callback_data, void *data);')
    cnt.append('extern "Python" bool pyclingo_script_callable(char const * name, bool *ret, void *data);')
    cnt.append('extern "Python" bool pyclingo_script_main(clingo_control_t *ctl, void *data);')

    code = ''

    if action == "embed":
        ffi.embedding_api('''\
bool pyclingo_execute(void *loc, char const *code, void *data);
bool pyclingo_call(void *loc, char const *name, void *arguments, size_t size, void *symbol_callback, void *symbol_callback_data, void *data);
bool pyclingo_callable(char const * name, bool *ret, void *data);
bool pyclingo_main(void *ctl, void *data);
''')

        ffi.embedding_init_code(f"""\
import os
import sys
import clingo.script

sys.path.insert(0, os.getcwd())
""")

        code = '''\
#ifdef CFFI_DLLEXPORT
#undef CFFI_DLLEXPORT
#define CFFI_DLLEXPORT
#endif
#ifdef PYPY_VERSION
void pyclingo_finalize() { }
#else
void pyclingo_finalize() {
    if (Py_IsInitialized()) {
        PyGILState_Ensure();
        Py_Finalize();
    }
}
#endif
'''
    else:
        cnt.append('extern "Python" bool pyclingo_execute(void *loc, char const *code, void *data);')
        cnt.append('extern "Python" bool pyclingo_call(void *loc, char const *name, void *arguments, size_t size, void *symbol_callback, void *symbol_callback_data, void *data);')
        cnt.append('extern "Python" bool pyclingo_callable(char const * name, bool *ret, void *data);')
        cnt.append('extern "Python" bool pyclingo_main(void *ctl, void *data);')


    if action != "header":
        ffi.set_source(
            '_clingo',
            f'''\
#include <clingo.h>
{code}
''')
        ffi.cdef('\n'.join(cnt))
        ffi.emit_c_code('_clingo.c')
    else:
        with open('_clingo.cdef', 'w') as f:
            f.write(''.join(f'{line}\n' for line in cnt))

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="generate code for clingo python binding")
    subparsers = parser.add_subparsers(dest="command", required=True, help='type of code to generate')

    parser_c = subparsers.add_parser('c', help='Generate C code.')
    parser_c.add_argument("-e", "--embed", action="store_true", help="add support for embedding")

    parser_h = subparsers.add_parser('h', help='Generate C header for cffi.')

    if imported:
        parser_py = subparsers.add_parser('python', help='generate Python code')

    args = parser.parse_args()
    if args.command == "h":
        generate_c("header")
    if args.command == "c":
        generate_c("embed" if args.embed else "source")
    if args.command == "python":
        generate_python()
