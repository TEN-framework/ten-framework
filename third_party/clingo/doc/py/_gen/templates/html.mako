<%
  import pdoc
  import re
  import clingo
  from pdoc.html_helpers import extract_toc, glimpse, to_markdown
  import pyparsing as pp

  class Group(pp.TokenConverter):
    def __init__(self, expr):
      super(Group, self).__init__(expr)
      self.saveAsList = False

    def postParse(self, instring, loc, tokenlist):
      return [ tokenlist ]

  identifier = pp.Word(pp.alphas, pp.alphanums + '_')

  def str_a(s, l, t):
    # return to_html("`" + (".".join(x.replace("Tuple", "Tuple_") for x in t[0])) + "`")[3:-4].replace("Tuple_", "Tuple")
    return to_html("`" + (".".join(t[0])) + "`")[3:-4]

  identifier_q = pp.Group(pp.delimitedList(identifier, delim=".")).setParseAction(str_a)


  # expression_list: [ expression ("," expression)* ]
  expression = pp.Forward()
  expression_list = pp.Group(pp.Optional(pp.delimitedList(expression, delim=",")))

  # expression: id | number | "[" expression_list "]"
  expression <<= (
    Group(pp.Suppress("[") + expression_list.setResultsName("arguments") + pp.Suppress("]")) |
    Group(pp.Word(pp.nums).setResultsName("name")) |
    Group(identifier_q.setResultsName("name")))

  # annotation_list: [ annotation ("," annotation)* ]
  annotation = pp.Forward()
  annotation_list = pp.Group(pp.Optional(pp.delimitedList(annotation, delim=",")))

  # annotation: id [ "[" annotation_list "]" ]
  annotation <<= (
    Group(identifier_q.setResultsName("name") + pp.Optional(pp.Suppress("[") + annotation_list.setResultsName("arguments") + pp.Suppress("]"))) |
    Group(pp.Optional(pp.Suppress("[") + annotation_list.setResultsName("arguments") + pp.Suppress("]"))))

  # prefix: "**" | "*" | ""
  prefix = pp.MatchFirst([pp.Word("**"), pp.Word("*")])

  # parameter: id [ ":" annotation ] [ "=" expression ]
  parameter = pp.Group(
    pp.Optional(prefix, "").setResultsName("prefix") +
    identifier.setResultsName("name") +
    pp.Optional(pp.Suppress(":") + annotation.setResultsName("type")) +
    pp.Optional(pp.Suppress("=") + expression.setResultsName("default")))

  # id "(" [ parameter ("," parameter)* ] ")" "->" annotation
  function = (
    identifier.setResultsName("name") +
    pp.Suppress("(") + pp.Group(pp.Optional(pp.delimitedList(parameter, delim=","))).setResultsName("arguments") + pp.Suppress(")") +
    pp.Suppress("->") + annotation.setResultsName("type"))

  def p_parameter(x):
    default = "={}".format(p_annotation(x["default"])) if "default" in x else ""
    annotation = (":" + p_annotation(x["type"])) if "type" in x else ""
    return "{}{}{}{}".format(x["prefix"], x["name"], annotation, default)

  def p_annotation(x):
    params = ("[" + ",".join([ p_annotation(y) for y in x["arguments"] ]) + "]") if "arguments" in x else ""
    return "{}{}".format(x["name"] if "name" in x else "", params)

  def p_function(d, x, p_ret=True):
    params = ", ".join([ p_parameter(y) for y in x["arguments"] ])
    annotation = " -> {}".format(p_annotation(x["type"])) if p_ret and x["type"] else ""
    return '<span>{} <span class="ident">{}</span></span><span>({}){}</span>'.format(d, x["name"], params, annotation)

  base_url = "clingo/python-api/{}".format(".".join(clingo.__version__.split(".")[0:2]))

  def link_replace(match):
    if match.group(1) == "clingo":
      path = "/"
    if match.group(1) == "ast":
      path = "/ast/"
    return '{{site.baseurl}}{% link ' + base_url + path + "index.html" + ' %}'

  def link(d, name=None, fmt='{}'):
    name = fmt.format(name or d.qualname + ('()' if isinstance(d, pdoc.Function) else ''))
    if not isinstance(d, pdoc.Doc) or isinstance(d, pdoc.External) and not external_links:
      return name
    url = d.url(relative_to=module, link_prefix=link_prefix,
                top_ancestor=not show_inherited_members)
    if url.endswith(".ext"):
      return name
    url = re.sub('.*(clingo).html', link_replace, url)
    url = re.sub('.*(ast).html', link_replace, url)
    return '<a title="{}" href="{}">{}</a>'.format(d.refname, url, name)

  _re_returns = re.compile(r"^(?<=Returns\n-{7}\n)(?P<type>[^\n]*)(?P<desc>(\n    )?)", re.MULTILINE)
  def _sub_returns(match):
    desc = match.group("desc")
    if not desc:
      desc = "\n    DUMMY DESRIPTION TO REMOVE"
    return 'DUMMYNAME : {}{}'.format(match.group('type'), desc)

  def to_html(text):
    text = text.replace('Tuple[', '_Tuple[')
    text = text.replace('Answer:', 'AnswerDUMMY')
    text = _re_returns.sub(_sub_returns, text)

    # TODO: this kills links in the type lists 
    #md = to_markdown(text, module=module, link=link, _code_refs=re.compile(r'(?<![\\])`(?!])(?:[^`]|(?<=\\)`)+`').sub)
    # TODO: regex `is_type_annotation` in `pdoc/html_helpers.py` needs an `=` now
    md = to_markdown(text, module=module, link=link)
    text = pdoc.html_helpers._md.reset().convert(md)

    text = text.replace('<dd>DUMMY DESRIPTION TO REMOVE</dd>', '')
    text = text.replace('<strong><code>DUMMYNAME</code></strong> :&ensp;', '')
    text = text.replace('_Tuple', 'Tuple')
    text = text.replace('AnswerDUMMY', 'Answer:')
    text = text.replace('<pre><code>', '<pre><code class="hljs python">')
    text = text.replace('<pre><code class="python">', '<pre><code class="hljs python">')
    for x in ("Number()", "String()", "Function()", "Tuple()", "Infimum", "Supremum", "Symbol"):
      for t in ("SymbolType", "TheoryTermType"):
        text = text.replace('<dt><strong><a title="clingo.{x}" href="#clingo.{x}"><code>{y}</code></a></strong> :&ensp;'
                            '<a title="clingo.{t}" href="#clingo.{t}"><code>{t}</code></a></dt>'.format(x=x.replace("()", ""), y=x, t=t),
                            '<dt><strong><code>{x}</code></strong> : '
                            '<a title="clingo.{t}" href="#clingo.{t}"><code>{t}</code></a></dt>'.format(x=x.replace("()", ""), t=t))
    return text

  def parse_fun_docstring(x):
    try:
      if x.source:
        line = re.search("def (.*? -> [^:]*?):", x.source, flags=re.MULTILINE | re.DOTALL).group(1)
        doc = x.docstring
        if x.source.startswith("@abstractmethod"):
          fdef = "@abstractmethod " + x.funcdef()
        else:
          fdef = x.funcdef()
      else:
        lines = x.docstring.splitlines()
        line = lines[0]
        doc = "\n".join(lines[1:]).strip()
        fdef = x.funcdef()
      sig = function.parseString(line, parseAll=True).asDict()
      return doc, p_function(fdef, sig)
    except Exception as e:
      return x.docstring, p_function(x.funcdef(), {"name": x.name, "arguments": [], "type": None, "default": None})

  def parse_var_docstring(f):
    try:
      lines = f.docstring.splitlines()
      name, rettype = lines[0].split(":")
      rettype = p_annotation(annotation.parseString(rettype, parseAll=True)[0].asDict())
      return ("\n".join(lines[1:]).strip(), rettype)
    except:
      pass
    return (f.docstring, None)

  def parse_class_docstring(f):
    try:
      lines = f.docstring.splitlines()
      signature = lines[0]
      if signature.startswith(f.name) and signature.find("->") >= 0:
        sig = function.parseString(lines[0], parseAll=True).asDict()
        return "\n".join(lines[1:]).strip(), p_function("class", sig, False)
    except:
      pass
    return f.docstring, None

%>

## Import template configuration from potentially-overridden config.mako.
## It may override above imported/defined functions as well.
<%namespace file="config.mako" name="config"/>
<%
  html_lang = getattr(config.attr, 'html_lang', 'en')
  show_inherited_members = getattr(config.attr, 'show_inherited_members', True)
  extract_module_toc_into_sidebar = getattr(config.attr, 'extract_module_toc_into_sidebar', True)
  list_class_variables_in_index = getattr(config.attr, 'list_class_variables_in_index', False)
  sort_identifiers = getattr(config.attr, 'sort_identifiers', True)
  hljs_style = getattr(config.attr, 'hljs_style', 'github')

  link = getattr(config.attr, 'link', link)
  to_html = getattr(config.attr, 'to_html', to_html)
  glimpse = getattr(config.attr, 'glimpse', glimpse)
  extract_toc = getattr(config.attr, 'extract_toc', extract_toc)
%>

<%def name="ident(name)"><span class="ident">${name}</span></%def>

<%def name="show_source(d)">
  % if show_source_code and d.source and d.obj is not getattr(d.inherits, 'obj', None):
    <details class="source">
      <summary>Source code</summary>
      <pre><code class="python">${d.source | h}</code></pre>
    </details>
  %endif
</%def>

<%def name="show_desc(d, short=False)">
  <%
    inherits = ' inherited' if d.inherits else ''
    docstring = glimpse(d.docstring) if short or inherits else d.docstring
  %>
  % if d.inherits:
    <p class="inheritance">
      <em>Inherited from:</em>
      % if hasattr(d.inherits, 'cls'):
        <code>${link(d.inherits.cls)}</code>.<code>${link(d.inherits, d.name)}</code>
      % else:
        <code>${link(d.inherits)}</code>
      % endif
    </p>
  % endif
  <section class="desc${inherits}">${docstring | to_html}</section>
  % if not isinstance(d, pdoc.Module):
    ${show_source(d)}
  % endif
</%def>

<%def name="show_str_desc(docstring)">
  <section class="desc">${docstring | to_html}</section>
</%def>

<%def name="show_module_list(modules)">
<h1>Python module list</h1>

% if not modules:
  <p>No modules found.</p>
% else:
  <dl id="http-server-module-list">
    % for name, desc in modules:
      <div class="flex">
        <dt><a href="${link_prefix}${name}">${name}</a></dt>
        <dd>${desc | glimpse, to_html}</dd>
      </div>
    % endfor
  </dl>
% endif
</%def>

<%def name="show_column_list(items)">
  <%
    two_column = len(items) >= 2 and all(len(i.name) < 30 for i in items)
  %>
  <ul class="${'two-column' if two_column else ''}">
    % for item in items:
      <li><code>${link(item, item.name)}</code></li>
    % endfor
  </ul>
</%def>

<%def name="show_module(module)">
  <%
    variables = module.variables(sort=sort_identifiers)
    classes = module.classes(sort=sort_identifiers)
    functions = module.functions(sort=sort_identifiers)
    submodules = module.submodules()
  %>

  <%def name="show_func(f)">
    <%
      docstring, sig = parse_fun_docstring(f)
    %>
    <dt id="${f.refname}"><code class="name flex">
        ${sig}
    </code></dt>
    <dd>${show_str_desc(docstring)}</dd>
  </%def>

  <header>
    % if 'http_server' in context.keys():
      <nav class="http-server-breadcrumbs">
        <a href="/">All packages</a>
        <% parts = module.name.split('.')[:-1] %>
        % for i, m in enumerate(parts):
          <% parent = '.'.join(parts[:i+1]) %>
          :: <a href="/${parent.replace('.', '/')}/">${parent}</a>
        % endfor
      </nav>
    % endif
    <h1 class="title"><code>${module.name}</code> module</h1>
  </header>

  <section id="section-intro">
    ${module.docstring | to_html}
    ${show_source(module)}
  </section>

  <section id="section-toc">
    <h2 id="header-toc" class="section-toc">Overview</h2>
    ${module_toc(module)}
  </section>

  <section>
    % if submodules:
      <h2 class="section-title" id="header-submodules">Sub-modules</h2>
      <dl>
        % for m in submodules:
          <dt><code class="name">${link(m)}</code></dt>
          <dd>${show_desc(m, short=True)}</dd>
        % endfor
      </dl>
    % endif
  </section>

  <section>
    % if variables:
      <h2 class="section-title" id="header-variables">Global variables</h2>
      <dl>
        % for v in variables:
          <%
            docstring, rettype = parse_var_docstring(v)
          %>
          <dt id="${v.refname}"><code class="name">var ${ident(v.name)}${"<span> : {}</span>".format(rettype) if rettype else ""}</code></dt>
          <dd>${show_str_desc(docstring)}</dd>
        % endfor
      </dl>
    % endif
  </section>

  <section>
    % if functions:
      <h2 class="section-title" id="header-functions">Functions</h2>
      <dl>
        % for f in functions:
          ${show_func(f)}
        % endfor
      </dl>
    % endif
  </section>

  <section>
    % if classes:
      <h2 class="section-title" id="header-classes">Classes</h2>
      <dl>
        % for c in classes:
          <%
            class_vars = c.class_variables(show_inherited_members, sort=sort_identifiers)
            smethods = c.functions(show_inherited_members, sort=sort_identifiers)
            inst_vars = c.instance_variables(show_inherited_members, sort=sort_identifiers)
            methods = c.methods(show_inherited_members, sort=sort_identifiers)
            mro = c.mro()
            subclasses = c.subclasses()
            docstring, signature = parse_class_docstring(c)
          %>
          <dt id="${c.refname}"><code class="flex name class">
            % if signature:
              ${signature}
            % else:
              <span>class ${ident(c.name)}<span>
            % endif
          </code></dt>

          <dd>
            ${show_str_desc(docstring)}

            % if subclasses:
              <h3>Subclasses</h3>
              <ul class="hlist">
                % for sub in subclasses:
                  <li>${link(sub)}</li>
                % endfor
              </ul>
            % endif
            % if class_vars:
              <h3>Class variables</h3>
              <dl>
                % for v in class_vars:
                  <dt id="${v.refname}"><code class="name">var ${ident(v.name)}</code></dt>
                  <dd>${show_desc(v)}</dd>
                % endfor
              </dl>
            % endif
            % if smethods:
              <h3>Static methods</h3>
              <dl>
                % for f in smethods:
                  ${show_func(f)}
                % endfor
              </dl>
            % endif
            % if inst_vars:
              <h3>Instance variables</h3>
              <dl>
                % for v in inst_vars:
                  <%
                    docstring, rettype = parse_var_docstring(v)
                  %>
                  <dt id="${v.refname}"><code class="name">var ${ident(v.name)}${"<span> : {}</span>".format(rettype) if rettype else ""}</code></dt>
                  <dd>${show_str_desc(docstring)}</dd>
                % endfor
              </dl>
            % endif
            % if methods:
              <h3>Methods</h3>
              <dl>
                % for f in methods:
                  % if f.name != "__init__":
                    ${show_func(f)}
                  % endif
                % endfor
              </dl>
            % endif
            % if not show_inherited_members:
              <%
                members = c.inherited_members()
              %>
              % if members:
                <h3>Inherited members</h3>
                <ul class="hlist">
                  % for cls, mems in members:
                    <li><code><b>${link(cls)}</b></code>:
                      <ul class="hlist">
                        % for m in mems:
                          <li><code>${link(m, name=m.name)}</code></li>
                        % endfor
                      </ul>
                    </li>
                  % endfor
                </ul>
              % endif
            % endif
          </dd>
        % endfor
      </dl>
    % endif
  </section>
</%def>

<%def name="module_index(module)">
  <%
    variables = module.variables(sort=sort_identifiers)
    classes = module.classes(sort=sort_identifiers)
    functions = module.functions(sort=sort_identifiers)
    submodules = module.submodules()
    supermodule = module.supermodule
  %>
  <nav class="sidebar">

    <h1>Index</h1>
    ${extract_toc(module.docstring) if extract_module_toc_into_sidebar else ''}
    <ul class="index">

      % if supermodule:
        <li><h3>Super-module</h3>
          <ul>
            <li><code>${link(supermodule)}</code></li>
          </ul>
        </li>
      % endif

      % if submodules:
        <li><h3><a href="#header-submodules">Sub-modules</a></h3>
          ${show_column_list(submodules)}
        </li>
      % endif

      % if variables:
        <li><h3><a href="#header-variables">Global variables</a></h3>
          ${show_column_list(variables)}
        </li>
      % endif

      % if functions:
        <li><h3><a href="#header-functions">Functions</a></h3>
          ${show_column_list(functions)}
        </li>
      % endif

      % if classes:
        <li><h3><a href="#header-classes">Classes</a></h3>
          <ul>
            % for c in classes:
              <li>
                <h4><code>${link(c)}</code></h4>
                <%
                  members = c.functions(sort=sort_identifiers) + c.methods(sort=sort_identifiers)
                  if list_class_variables_in_index:
                    members += (c.instance_variables(sort=sort_identifiers) +
                    c.class_variables(sort=sort_identifiers))
                    if not show_inherited_members:
                      members = [i for i in members if not i.inherits]
                      if sort_identifiers:
                        members = sorted(members)
                %>
                % if members:
                  ${show_column_list(members)}
                % endif
              </li>
            % endfor
          </ul>
        </li>
      % endif

    </ul>
  </nav>
</%def>

<%def name="module_toc(module)">
  <%
    variables = module.variables(sort=sort_identifiers)
    classes = module.classes(sort=sort_identifiers)
    functions = module.functions(sort=sort_identifiers)
    submodules = module.submodules()
    supermodule = module.supermodule
  %>
  <nav class="sidebar">
    <ul class="index">
      % if supermodule:
        <li><h3>Super-module</h3>
          <ul>
            <li><code>${link(supermodule)}</code></li>
          </ul>
        </li>
      % endif

      % if submodules:
        <li><h3><a href="#header-submodules">Sub-modules</a></h3>
          ${show_column_list(submodules)}
        </li>
      % endif

      % if variables:
        <li><h3><a href="#header-variables">Global variables</a></h3>
          ${show_column_list(variables)}
        </li>
      % endif

      % if functions:
        <li><h3><a href="#header-functions">Functions</a></h3>
          ${show_column_list(functions)}
        </li>
      % endif

      % if classes:
        <li><h3><a href="#header-classes">Classes</a></h3>
          ${show_column_list(classes)}
        </li>
      % endif

    </ul>
  </nav>
</%def>

<%
  module_list = 'modules' in context.keys()  # Whether we're showing module list in server mode
  module_path = module.name.replace(".", "/").replace("clingo/", "").replace("clingo", "")
%>

---
layout: page
% if module_list:
title: Python module list
description: A list of documented Python modules.
% else:
title: ${module.name} API documentation
description: ${module.docstring | glimpse, trim, h}
% endif
css:
  - /css/pdoc.css
  - /css/github.min.css
permalink: /${base_url}/${module_path}/
---

<main>
  % if module_list:
    <article id="content">
      ${show_module_list(modules)}
    </article>
  % else:
    <article id="content">
      ${show_module(module)}
    </article>
    ${module_index(module)}
  % endif
</main>

% if show_source_code:
  <script src="{{site.baseurl}}/js/highlight.min.js"></script>
  <script>hljs.initHighlightingOnLoad()</script>
% endif
