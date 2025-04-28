#!/usr/bin/env python3

import os
import re
import importlib.machinery

import pdoc
import clingo

SUB = [
    ("clingo_main", "Application", clingo.clingo_main),
    ("Control.register_observer", "Observer", clingo.Control.register_observer),
    ("Control.register_propagator", "Propagator", clingo.Control.register_propagator)]


def parse_class(aux, name, doc):
    """
    Extract a class declaration from a docstring.
    """
    match = re.search(r"class ({}(\([^)]*\))?):".format(name), doc)

    start = match.start()
    end = doc.find("```", start)
    doc = doc[start:end]

    aux.write(doc)


def parse_aux():
    with open("aux.py", "w") as aux:
        aux.write("from clingo import *\nfrom abc import *\nfrom typing import *\n")
        for _, name, obj in SUB:
            parse_class(aux, name, obj.__doc__)
    import aux
    return aux


clingo.ast.__spec__ = importlib.machinery.ModuleSpec("clingo.ast", None)

clingo.__pdoc__ = {}
for key, name, obj in SUB:
    clingo.__pdoc__[key] = re.sub(r"```python.*?class ({}(\([^)]*\))?):.*?```".format(name), "", obj.__doc__, flags=re.MULTILINE|re.DOTALL)

pdoc.tpl_lookup.directories.insert(0, './templates')
ctx = pdoc.Context()

cmod = pdoc.Module(clingo, context=ctx)
amod = pdoc.Module(clingo.ast, supermodule=cmod, context=ctx)
xmod = parse_aux()

cmod.doc["Application"] = pdoc.Class("Application", cmod, xmod.Application)
cmod.doc["Observer"] = pdoc.Class("Observer", cmod, xmod.Observer)
cmod.doc["Propagator"] = pdoc.Class("Propagator", cmod, xmod.Propagator)
cmod.doc["ast"] = amod
cmod.doc["__version__"] = pdoc.Variable("__version__", cmod, "__version__: str\n\nVersion of the clingo module (`'{}'`).".format(clingo.__version__))
cmod.doc["Infimum"] = pdoc.Variable("Infimum", cmod, '''Infimum: Symbol\n\nRepresents a symbol of type `clingo.SymbolType.Infimum`.''')
cmod.doc["Supremum"] = pdoc.Variable("Supremum", cmod, '''Supremum: Symbol\n\nRepresents a symbol of type `clingo.SymbolType.Supremum`.''')
pdoc.link_inheritance(ctx)

prefix = "../clingo/python-api/{}".format(".".join(clingo.__version__.split(".")[:2]))
cprefix = "../clingo/python-api/current"

os.makedirs("{}/ast".format(prefix), exist_ok=True)
os.makedirs("{}/ast".format(cprefix), exist_ok=True)

cmod_html = cmod.html(external_links=True)
amod_html = amod.html(external_links=True)

open("{}/index.html".format(prefix), "w").write(cmod_html)
open("{}/ast/index.html".format(prefix), "w").write(amod_html)

open("{}/index.html".format(cprefix), "w").write(cmod_html.replace("clingo/python-api/5.4", "clingo/python-api/current"))
open("{}/ast/index.html".format(cprefix), "w").write(amod_html.replace("clingo/python-api/5.4", "clingo/python-api/current"))
