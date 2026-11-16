"""Moves the game's nested functions to file scope, for compilers without them.

The game's code defines some functions inside others, a GNU C extension that
KMC GCC compiled with a static chain, and that the N64 build must keep to
match. Clang has no nested functions, so the PC build compiles a copy of each
source in which every nested function is an ordinary static function placed
just before the function that held it, its parent.

A nested function that uses its parent's variables, which it reached through
the static chain, takes a pointer to each as an extra parameter of the same
name: its body reads (*name) where it read name, and each call passes &name,
or, from another nested function, its own pointer. So a variable keeps one
home, and a write on either side is seen on the other, as before. A function
nested two deep comes out in a second pass, from the function it was in.

Of a preprocessor conditional's branches, the first gives the code's shape,
its braces and declarations, which the others repeat; captures are passed in
every branch, so that whichever one compiles is right.

A macro of the file's that calls a nested function passes the captures too,
as long as only the parent expands it.

Line markers keep the compiler's messages and the debugger at the source's
lines. What the move can't do faithfully, such as taking a nested function's
address, stops it with an error rather than change the program's meaning. It
relies on the game's naming, in which no global has a local's name, so that a
variable the move missed fails to compile rather than find a global.
"""

from __future__ import annotations

import bisect
import dataclasses
import re

# The tokens that the move tells apart. A directive is a whole line, which
# neither counts as braces nor moves apart from its neighbors.
_TOKEN_PATTERN = re.compile(
    r"""
    (?P<directive>(?<![^\n])[ \t]*\#(?:\\\n|[^\n])*)
    | (?P<space>(?:[ \t\r\f\v\n]|\\\n)+)
    | (?P<comment>//[^\n]*|/\*.*?\*/)
    | (?P<string>[LuU]?"(?:\\.|[^"\\\n])*")
    | (?P<char>[LuU]?'(?:\\.|[^'\\\n])*')
    | (?P<number>\.?[0-9](?:[eEpP][+-]|[0-9a-zA-Z_.])*)
    | (?P<identifier>[A-Za-z_][A-Za-z_0-9]*)
    | (?P<punctuator>\.\.\.|<<=|>>=|->|\+\+|--|<<|>>|&&|\|\||[-+*/%&|^!=<>]=|\#\#
        |[][(){}.,;:?~!+\-*/%&|^=<>\#])
    """,
    re.VERBOSE | re.DOTALL,
)

# Words that start a statement but never a declaration
_STATEMENT_KEYWORDS = frozenset(
    "break case continue default do else for goto if return sizeof switch while".split()
)
# Words of a declaration that say where a variable lives, not what it is
_STORAGE_CLASSES = frozenset("auto extern register static".split())
_OPENING_BRACKETS = ("(", "[", "{")
_CLOSING_BRACKETS = {"(": ")", "[": "]", "{": "}"}
_MEMBER_ACCESS = (".", "->")
# More passes than this means a function nested deeper than the game's are
_MAX_PASSES = 8


class NestedFunctionError(Exception):
    """A nested function that can't be moved without changing its meaning."""


@dataclasses.dataclass(frozen=True)
class Token:
    kind: str
    text: str
    start: int  # Offsets in the source's text
    end: int
    line: int  # The line that the compiler gives it, after any #line
    # The conditionals that it's in, outermost first, as their opening
    # directives' offsets, and whether it's in one's later branch
    conditionals: tuple[int, ...] = ()
    is_alternate: bool = False


@dataclasses.dataclass(frozen=True)
class Variable:
    """A parent's variable, and how to declare a pointer to it."""

    name: str
    pointer_parameter: str  # Empty where the move can't spell its type


@dataclasses.dataclass
class NestedFunction:
    name: str
    first: int  # Indices of its tokens
    open_paren: int
    close_paren: int
    open_brace: int
    close_brace: int
    visible: dict[str, Variable]  # The parent's variables where it's defined
    own_names: set[str] = dataclasses.field(default_factory=set)
    # The names that it uses but doesn't declare, its own nested functions'
    # included
    free_names: set[str] = dataclasses.field(default_factory=set)
    captures: list[str] = dataclasses.field(default_factory=list)


# --- Reading ---


def _directive_words(text: str) -> list[str]:
    return re.findall(r"[^\s#]+", text.split("//", 1)[0])


def tokenize(text: str) -> list[Token]:
    """The source's tokens, without spaces and comments, and an end token."""
    tokens = []
    line = 1
    position = 0
    # The open conditionals: their directives' offsets, and whether a later
    # branch than the first has begun
    open_conditionals: list[tuple[int, bool]] = []
    while position < len(text):
        match = _TOKEN_PATTERN.match(text, position)
        if match is None:
            raise NestedFunctionError("line {}: can't read {!r}".format(line, text[position]))
        kind = match.lastgroup
        words = _directive_words(match.group()) if kind == "directive" else []
        directive = words[0] if words else ""
        if directive in ("elif", "else", "endif") and not open_conditionals:
            raise NestedFunctionError("line {}: #{} without #if".format(line, directive))
        if directive in ("elif", "else"):
            open_conditionals[-1] = (open_conditionals[-1][0], True)
        elif directive == "endif":
            open_conditionals.pop()
        if kind not in ("space", "comment"):
            tokens.append(
                Token(
                    kind,
                    match.group(),
                    match.start(),
                    match.end(),
                    line,
                    tuple(offset for offset, _ in open_conditionals),
                    # A directive always counts, as the end of a statement
                    kind != "directive" and any(alternate for _, alternate in open_conditionals),
                )
            )
        if directive in ("if", "ifdef", "ifndef"):
            open_conditionals.append((match.start(), False))
        line += match.group().count("\n")
        if directive == "line":
            # The line break after it starts the line that it names
            line = int(words[1]) - 1
        position = match.end()
    if open_conditionals:
        raise NestedFunctionError("line {}: #if without #endif".format(line))
    tokens.append(Token("end", "", len(text), len(text), line))
    return tokens


def _span(tokens: list[Token], start: Token, end: Token) -> range:
    """The indices of tokens from start to end, which are tokens of the
    source's that tokens may hold more or fewer of."""
    starts = [token.start for token in tokens]
    return range(bisect.bisect_left(starts, start.start), bisect.bisect_right(starts, end.start))


def _find_closing(tokens: list[Token], open_index: int) -> int:
    """The index of the bracket that closes the one at open_index."""
    opening = tokens[open_index].text
    closing = _CLOSING_BRACKETS[opening]
    depth = 0
    for index in range(open_index, len(tokens)):
        token = tokens[index]
        if token.kind != "punctuator":
            continue
        if token.text == opening:
            depth += 1
        elif token.text == closing:
            depth -= 1
            if depth == 0:
                return index
    raise NestedFunctionError("line {}: unbalanced {!r}".format(tokens[open_index].line, opening))


def _find_statement_end(tokens: list[Token], index: int) -> int:
    """The index of the semicolon that ends the statement at index."""
    while tokens[index].text != ";":
        if tokens[index].kind == "end":
            raise NestedFunctionError("line {}: unended statement".format(tokens[index].line))
        if tokens[index].text in _OPENING_BRACKETS:
            index = _find_closing(tokens, index)
        index += 1
    return index


def _is_statement_start(tokens: list[Token], index: int) -> bool:
    if index == 0:
        return True
    previous = tokens[index - 1]
    return previous.kind == "directive" or previous.text in (";", "{", "}")


def _is_member(tokens: list[Token], index: int) -> bool:
    return tokens[index - 1].text in _MEMBER_ACCESS


def _is_macro_name(name: str) -> bool:
    return re.fullmatch(r"[A-Z][A-Z0-9_]*", name) is not None


# --- Declarations ---


def _split_top_level(tokens: list[Token], separator: str) -> list[list[Token]]:
    parts: list[list[Token]] = [[]]
    depth = 0
    for token in tokens:
        if token.text in _OPENING_BRACKETS:
            depth += 1
        elif token.text in _CLOSING_BRACKETS.values():
            depth -= 1
        if depth == 0 and token.text == separator:
            parts.append([])
        else:
            parts[-1].append(token)
    return parts


def _parse_declarator(
    declarator: list[Token], type_text: str, is_parameter: bool
) -> Variable | None:
    """The variable that a declarator, without its type or initializer,
    declares: its name is the first word that isn't a macro's, such as an
    attribute, as later ones are sizes or a function's parameters."""
    name_index = next(
        (
            i
            for i, t in enumerate(declarator)
            if t.kind == "identifier" and not _is_macro_name(t.text)
        ),
        None,
    )
    if name_index is None:
        return None
    name = declarator[name_index].text

    def substitute(replacement: str) -> str:
        words = [type_text]
        for index, token in enumerate(declarator):
            if index == name_index:
                words.append(replacement)
            elif not _is_macro_name(token.text):
                words.append(token.text)
        return " ".join(words)

    if not is_parameter:
        # A pointer to it has its declarator with (*name) for name, as
        # f32 (*corners)[3] for f32 corners[3]
        return Variable(name, substitute("(*{})".format(name)))
    # A parameter declared as an array or a function, perhaps through a
    # typedef, is a pointer, which a conditional's operand becomes too
    pointer = "({})0".format(substitute("(*)"))
    return Variable(name, "__typeof__(0 ? *{0} : *{0}) (*{1})".format(pointer, name))


def _find_declarator_start(tokens: list[Token]) -> int | None:
    """The index of the first pointer or parenthesis that isn't part of a
    __typeof__, which a moved function's parameters have, or None."""
    index = 0
    while index < len(tokens):
        if tokens[index].text in ("__typeof__", "typeof") and index + 1 < len(tokens):
            index = _find_closing(tokens, index + 1) + 1
            continue
        if tokens[index].text in ("*", "("):
            return index
        index += 1
    return None


def _parse_declaration(tokens: list[Token], is_parameter: bool = False) -> list[Variable]:
    """The variables that a declaration's tokens, without its semicolon,
    declare: a type's words, then declarators separated by commas."""
    variables = []
    type_text: str | None = None
    for part in _split_top_level(tokens, ","):
        initializer = next((i for i, t in enumerate(part) if t.text == "="), len(part))
        declarator = part[:initializer]
        if type_text is None:
            # The type is the words before the first pointer or parenthesis,
            # or else before the name, the last word that isn't a macro's
            # ahead of any array's sizes
            stop = _find_declarator_start(declarator)
            if stop is None:
                bracket = next(
                    (i for i, t in enumerate(declarator) if t.text == "["), len(declarator)
                )
                words = [
                    i
                    for i, t in enumerate(declarator[:bracket])
                    if t.kind == "identifier" and not _is_macro_name(t.text)
                ]
                stop = words[-1] if words else bracket
            type_text = " ".join(
                t.text
                for t in declarator[:stop]
                if t.text not in _STORAGE_CLASSES and not _is_macro_name(t.text)
            )
            declarator = declarator[stop:]
        variable = _parse_declarator(declarator, type_text, is_parameter)
        if variable is not None:
            variables.append(variable)
    return variables


def _parse_parameters(tokens: list[Token]) -> list[Variable]:
    variables = []
    for part in _split_top_level(tokens, ","):
        if [t.text for t in part] in ([], ["void"], ["..."]):
            continue
        variables.extend(_parse_declaration(part, is_parameter=True))
    return variables


def _is_declaration(tokens: list[Token], index: int) -> bool:
    first = tokens[index]
    if first.kind != "identifier" or first.text in _STATEMENT_KEYWORDS:
        return False
    second = tokens[index + 1]
    return second.kind == "identifier" or second.text == "*"


# --- Nested functions ---


def _match_nested_function(tokens: list[Token], index: int) -> tuple[int, int, int] | None:
    """If a nested function's definition, or its declaration with auto,
    starts at index: the indices of its parentheses and of the brace or
    semicolon after them."""
    open_paren = index
    while tokens[open_paren].kind == "identifier" or tokens[open_paren].text == "*":
        if tokens[open_paren].text in _STATEMENT_KEYWORDS:
            return None
        open_paren += 1
    if (
        open_paren - index < 2
        or tokens[open_paren].text != "("
        or tokens[open_paren - 1].kind != "identifier"
    ):
        return None
    close_paren = _find_closing(tokens, open_paren)
    after = close_paren + 1
    if tokens[after].text == "{":
        return open_paren, close_paren, after
    if tokens[after].text == ";" and any(t.text == "auto" for t in tokens[index:open_paren]):
        return open_paren, close_paren, after
    return None


def _make_nested_function(
    tokens: list[Token], index: int, match: tuple[int, int, int], visible: dict[str, Variable]
) -> NestedFunction:
    open_paren, close_paren, open_brace = match
    return NestedFunction(
        tokens[open_paren - 1].text,
        index,
        open_paren,
        close_paren,
        open_brace,
        _find_closing(tokens, open_brace),
        visible,
    )


def _scan_names(tokens: list[Token], every: list[Token], function: NestedFunction) -> None:
    """Fills in the names that a nested function declares, and those that it
    uses, in any branch, without declaring. Those of a function nested in it
    that it doesn't declare are its own to pass on."""
    parameters = tokens[function.open_paren + 1 : function.close_paren]
    function.own_names.update(v.name for v in _parse_parameters(parameters))
    inner_functions = []
    index = function.open_brace + 1
    while index < function.close_brace:
        if _is_statement_start(tokens, index) and tokens[index].kind == "identifier":
            match = _match_nested_function(tokens, index)
            if match is not None and tokens[match[2]].text == "{":
                inner = _make_nested_function(tokens, index, match, {})
                _scan_names(tokens, every, inner)
                inner_functions.append(inner)
                index = inner.close_brace + 1
                continue
            if _is_declaration(tokens, index):
                declaration = tokens[index : _find_statement_end(tokens, index)]
                function.own_names.update(v.name for v in _parse_declaration(declaration))
        index += 1

    used = set()
    inner_spans = [
        _span(every, tokens[inner.first], tokens[inner.close_brace]) for inner in inner_functions
    ]
    for index in _span(every, tokens[function.open_brace], tokens[function.close_brace]):
        if any(index in span for span in inner_spans):
            continue
        if every[index].kind == "identifier" and not _is_member(every, index):
            used.add(every[index].text)
    function.free_names = used - function.own_names
    for inner in inner_functions:
        function.own_names.add(inner.name)
        passed_on = inner.free_names - function.own_names
        function.free_names |= passed_on


@dataclasses.dataclass
class _Parent:
    """A file-scope function that holds nested functions."""

    first: int  # Indices of its tokens
    open_brace: int
    close_brace: int
    nested: list[NestedFunction]
    declarations: list[tuple[int, int]]  # Nested functions declared with auto


def _scan_parent(
    tokens: list[Token], every: list[Token], first: int, open_paren: int, open_brace: int
) -> _Parent:
    """Finds a function's nested functions and the variables that each sees."""
    parent = _Parent(first, open_brace, _find_closing(tokens, open_brace), [], [])
    parameters = tokens[open_paren + 1 : _find_closing(tokens, open_paren)]
    scopes: list[dict[str, Variable]] = [{v.name: v for v in _parse_parameters(parameters)}]
    index = open_brace + 1
    while index < parent.close_brace:
        token = tokens[index]
        if token.text == "{":
            scopes.append({})
        elif token.text == "}":
            scopes.pop()
        elif _is_statement_start(tokens, index) and token.kind == "identifier":
            match = _match_nested_function(tokens, index)
            if match is not None and tokens[match[2]].text == ";":
                parent.declarations.append((index, match[2]))
                index = match[2] + 1
                continue
            if match is not None:
                visible: dict[str, Variable] = {}
                for scope in scopes:
                    visible.update(scope)
                function = _make_nested_function(tokens, index, match, visible)
                # Moved out of a conditional, it would no longer depend on it.
                # The parent's end is in those that hold all its branches.
                if not (
                    tokens[parent.close_brace].conditionals
                    == tokens[index].conditionals
                    == tokens[function.close_brace].conditionals
                ):
                    raise NestedFunctionError(
                        "line {}: {} is in a conditional of its parent's".format(
                            token.line, function.name
                        )
                    )
                _scan_names(tokens, every, function)
                parent.nested.append(function)
                index = function.close_brace + 1
                continue
            if _is_declaration(tokens, index):
                declaration = tokens[index : _find_statement_end(tokens, index)]
                scopes[-1].update((v.name, v) for v in _parse_declaration(declaration))
        index += 1
    return parent


def _find_captures(parent: _Parent) -> None:
    """Works out each nested function's captures: the parent's variables
    that it uses, and those of the nested functions that it calls."""
    names = {function.name for function in parent.nested}
    called = {}
    captures = {}
    for function in parent.nested:
        called[function.name] = (function.free_names & names) - {function.name}
        captures[function.name] = function.free_names & function.visible.keys() - names
    is_changed = True
    while is_changed:
        is_changed = False
        for function in parent.nested:
            for callee in called[function.name]:
                added = captures[callee] - captures[function.name]
                if added:
                    captures[function.name] |= added
                    is_changed = True
    for function in parent.nested:
        function.captures = sorted(captures[function.name])
        for name in function.captures:
            if name in function.own_names:
                raise NestedFunctionError(
                    "{} declares a {} of its own, but calls a function that uses "
                    "its parent's".format(function.name, name)
                )
            if name not in function.visible or not function.visible[name].pointer_parameter:
                raise NestedFunctionError(
                    "{} uses its parent's {}, which the move can't pass".format(function.name, name)
                )


# --- Writing ---


class _Editor:
    """Replacements in the source's text, made together."""

    def __init__(self, text: str):
        self.text = text
        self.edits: list[tuple[int, int, str]] = []

    def replace(self, start: int, end: int, replacement: str) -> None:
        self.edits.append((start, end, replacement))

    def apply(self, start: int = 0, end: int | None = None) -> str:
        """The text from start to end, with the edits inside it made."""
        end = len(self.text) if end is None else end
        pieces = []
        position = start
        for edit_start, edit_end, replacement in sorted(self.edits):
            if edit_start < start or edit_end > end:
                continue
            if edit_start < position:
                raise NestedFunctionError("overlapping edits at offset {}".format(edit_start))
            pieces.append(self.text[position:edit_start])
            pieces.append(replacement)
            position = edit_end
        pieces.append(self.text[position:end])
        return "".join(pieces)


def _pass_captures(
    every: list[Token],
    editor: _Editor,
    span: range,
    nested: dict[str, NestedFunction],
    caller: NestedFunction | None,
) -> None:
    """Adds the captures to each call of a nested function in the span of
    tokens: the variables' addresses from the parent, or the caller's
    pointers from a nested function. Fails at any other use of a nested
    function's name."""
    for index in span:
        token = every[index]
        callee = nested.get(token.text)
        if token.kind != "identifier" or callee is None or _is_member(every, index):
            continue
        if caller is not None and token.text in caller.own_names:
            continue
        if every[index + 1].text != "(":
            raise NestedFunctionError(
                "line {}: {}'s address is taken".format(token.line, token.text)
            )
        if not callee.captures:
            continue
        close_paren = _find_closing(every, index + 1)
        prefix = "" if caller is not None else "&"
        arguments = ", ".join(prefix + name for name in callee.captures)
        if close_paren > index + 2:
            arguments = ", " + arguments
        editor.replace(every[close_paren].start, every[close_paren].start, arguments)


@dataclasses.dataclass
class _Macro:
    """A #define of the file's."""

    name: str
    directive: Token
    body: list[Token]  # Its replacement's tokens, with the source's offsets
    parameters: set[str]


def _find_macros(every: list[Token]) -> list[_Macro]:
    macros = []
    for token in every:
        if token.kind != "directive" or _directive_words(token.text)[:1] != ["define"]:
            continue
        # Read as code, without its #, which would make it a directive again
        hash_index = token.text.index("#")
        code = token.text[:hash_index] + " " + token.text[hash_index + 1 :]
        inner = [
            dataclasses.replace(t, start=t.start + token.start, end=t.end + token.start)
            for t in tokenize(code)[:-1]
        ]
        # define, the name, and any parameters, which follow it directly
        name = inner[1]
        body_start = 2
        parameters: set[str] = set()
        if len(inner) > 2 and inner[2].text == "(" and inner[2].start == name.end:
            body_start = _find_closing(inner, 2) + 1
            parameters = {t.text for t in inner[3 : body_start - 1] if t.kind == "identifier"}
        macros.append(_Macro(name.text, token, inner[body_start:], parameters))
    return macros


def _check_macros(
    every: list[Token], tokens: list[Token], macros: list[_Macro], parent: _Parent
) -> list[_Macro]:
    """The file's macros that call the parent's nested functions with
    captures, which only the parent may expand, as they pass the parent's
    variables. Fails where a nested function expands a macro that names one
    of its captures, which it would take for the pointer."""
    capturing = {function.name for function in parent.nested if function.captures}
    calling = []
    for macro in macros:
        names = {t.text for t in macro.body if t.kind == "identifier"} - macro.parameters
        for function in parent.nested:
            body = _span(every, tokens[function.first], tokens[function.close_brace])
            if not any(every[i].text == macro.name for i in body):
                continue
            if names & capturing:
                raise NestedFunctionError(
                    "{} expands {}, which calls a nested function".format(function.name, macro.name)
                )
            for name in sorted(names & set(function.captures)):
                raise NestedFunctionError(
                    "{} expands {}, which names its parent's {}".format(
                        function.name, macro.name, name
                    )
                )
        if names & capturing:
            calling.append(macro)
    return calling


def _static_header(tokens: list[Token], text: str, function: NestedFunction) -> str:
    """The function's specifiers, name and parameters as a static
    function's, with pointers to its captures last."""
    specifiers = [
        t.text for t in tokens[function.first : function.open_paren - 1] if t.text != "auto"
    ]
    if "static" not in specifiers:
        specifiers.insert(0, "static")
    # The parameters keep their line breaks, so that the body keeps its lines
    parameters = text[tokens[function.open_paren].end : tokens[function.close_paren].start]
    pointers = ", ".join(function.visible[name].pointer_parameter for name in function.captures)
    if pointers and parameters.strip() in ("", "void"):
        parameters = pointers
    elif pointers:
        parameters = parameters + ", " + pointers
    return "{} {}({})".format(" ".join(specifiers), function.name, parameters)


def _static_definition(
    tokens: list[Token],
    every: list[Token],
    text: str,
    function: NestedFunction,
    nested: dict[str, NestedFunction],
) -> str:
    """The nested function as a static one, reading its captures through
    their pointers."""
    editor = _Editor(text)
    captured = set(function.captures)
    body = _span(every, tokens[function.open_brace], tokens[function.close_brace])
    for index in body:
        token = every[index]
        if token.kind == "identifier" and token.text in captured and not _is_member(every, index):
            editor.replace(token.start, token.end, "(*{})".format(token.text))
    _pass_captures(every, editor, body, nested, function)
    start = tokens[function.open_brace].start
    end = tokens[function.close_brace].end
    return _static_header(tokens, text, function) + " " + editor.apply(start, end)


def _blank_out(editor: _Editor, text: str, start: int, end: int) -> None:
    """Removes the text from start to end, keeping its line breaks so that
    the lines after keep their numbers."""
    editor.replace(start, end, "\n" * text.count("\n", start, end))


def _insertion_point(every: list[Token], tokens: list[Token], parent: _Parent) -> Token:
    """The token at whose line the moved functions go: the parent's first,
    or the opening of the conditional that its start is in but its end
    isn't, as when two branches give it different parameters."""
    first = tokens[parent.first]
    shared = tokens[parent.close_brace].conditionals
    for depth, opening in enumerate(first.conditionals):
        if depth >= len(shared) or shared[depth] != opening:
            return next(token for token in every if token.start == opening)
    return first


def _move_nested_functions(
    tokens: list[Token], every: list[Token], text: str, editor: _Editor, parent: _Parent, path: str
) -> None:
    """Removes the parent's nested functions from its body, passes their
    captures in its calls, and puts them before it as static functions."""
    _find_captures(parent)
    nested = {function.name: function for function in parent.nested}
    for macro in _check_macros(every, tokens, _find_macros(every), parent):
        _pass_captures(macro.body + [every[-1]], editor, range(len(macro.body)), nested, None)
    pieces = [_static_header(tokens, text, function) + ";\n" for function in parent.nested]
    removed = []
    for function in parent.nested:
        pieces.append('#line {} "{}"\n'.format(tokens[function.first].line, path))
        pieces.append(_static_definition(tokens, every, text, function, nested) + "\n")
        removed.append((function.first, function.close_brace))
    removed.extend(parent.declarations)
    removed.sort()
    # The parent's calls, in every branch, outside what's removed
    start = tokens[parent.open_brace]
    for first, last in removed:
        _blank_out(editor, text, tokens[first].start, tokens[last].end)
        _pass_captures(every, editor, _span(every, start, tokens[first - 1]), nested, None)
        start = tokens[last + 1]
    _pass_captures(every, editor, _span(every, start, tokens[parent.close_brace]), nested, None)

    point = _insertion_point(every, tokens, parent)
    pieces.append('#line {} "{}"\n'.format(point.line, path))
    line_start = text.rfind("\n", 0, point.start) + 1
    editor.replace(line_start, line_start, "".join(pieces))


def _hoist_one_level(text: str, path: str) -> str:
    """The text with the nested functions of its file-scope functions moved
    before them, but not yet those nested in those."""
    every = tokenize(text)
    tokens = [token for token in every if not token.is_alternate]
    editor = _Editor(text)
    statement_start = 0
    index = 0
    while tokens[index].kind != "end":
        if _is_statement_start(tokens, index):
            statement_start = index
        if tokens[index].text != "{":
            index += 1
            continue
        statement = [t.text for t in tokens[statement_start:index]]
        if "(" in statement and "=" not in statement:
            open_paren = statement_start + statement.index("(")
            parent = _scan_parent(tokens, every, statement_start, open_paren, index)
            if parent.nested:
                _move_nested_functions(tokens, every, text, editor, parent, path)
        index = _find_closing(tokens, index) + 1
    return editor.apply()


def hoist_nested_functions(text: str, path: str) -> str:
    """The source's text with all its nested functions at file scope, its
    lines marked as path's."""
    for _ in range(_MAX_PASSES):
        hoisted = _hoist_one_level(text, path)
        if hoisted == text:
            return text
        text = hoisted
    raise NestedFunctionError("functions nested more than {} deep".format(_MAX_PASSES))
