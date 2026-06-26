
import re
from typing import (
    Any,
    Dict,
    Match,
    Optional,
    Sequence,
    Union,
)


EATSPACE = '\033EATSPACE.'
POINTER_SUFFIX = ' *' + EATSPACE


def camel_to_upper(value: str) -> str:
    """
    Converts CamelCase to CAMEL_CASE.

    Examples::

        ENUMName -> ENUM_NAME
        EnumName1 -> ENUM_NAME1
        ENUM_NAME -> ENUM_NAME
        ENUM_NAME1 -> ENUM_NAME1
        ENUM_Name2 -> ENUM_NAME2
        ENUM24_Name -> ENUM24_NAME
    """
    ret = value[0]
    upc = value[0].isupper()

    for ch in value[1:]:
        if ch.isupper() == upc:
            pass
        elif upc:
            if len(ret) > 2 and ret[-2].isalnum():
                ret = ret[:-1] + '_' + ret[-1]
        else:
            if ret[-1].isalnum():
                ret += '_'
        ret += ch
        upc = ch.isupper()

    return c_name(ret.upper()).lstrip('_')


def c_enum_const(type_name: str,
                 const_name: str,
                 prefix: Optional[str] = None) -> str:
    """
    Generate a C enumeration constant name.

    :param type_name: The name of the enumeration.
    :param const_name: The name of this constant.
    :param prefix: Optional, prefix that overrides the type_name.
    """
    if prefix is None:
        prefix = camel_to_upper(type_name)
    return prefix + '_' + c_name(const_name, False).upper()


def c_name(name: str, protect: bool = True) -> str:
    """
    Map ``name`` to a valid C identifier.

    Used for converting 'name' from a 'name':'type' qapi definition
    into a generated struct member, as well as converting type names
    into substrings of a generated C function name.

    '__a.b_c' -> '__a_b_c', 'x-foo' -> 'x_foo'
    protect=True: 'int' -> 'q_int'; protect=False: 'int' -> 'int'

    :param name: The name to map.
    :param protect: If true, avoid returning certain ticklish identifiers
                    (like C keywords) by prepending ``q_``.
    """
    c89_words = set(['auto', 'break', 'case', 'char', 'const', 'continue',
                     'default', 'do', 'double', 'else', 'enum', 'extern',
                     'float', 'for', 'goto', 'if', 'int', 'long', 'register',
                     'return', 'short', 'signed', 'sizeof', 'static',
                     'struct', 'switch', 'typedef', 'union', 'unsigned',
                     'void', 'volatile', 'while'])
    c99_words = set(['inline', 'restrict', '_Bool', '_Complex', '_Imaginary'])
    c11_words = set(['_Alignas', '_Alignof', '_Atomic', '_Generic',
                     '_Noreturn', '_Static_assert', '_Thread_local'])
    gcc_words = set(['asm', 'typeof'])
    cpp_words = set(['bool', 'catch', 'class', 'const_cast', 'delete',
                     'dynamic_cast', 'explicit', 'false', 'friend', 'mutable',
                     'namespace', 'new', 'operator', 'private', 'protected',
                     'public', 'reinterpret_cast', 'static_cast', 'template',
                     'this', 'throw', 'true', 'try', 'typeid', 'typename',
                     'using', 'virtual', 'wchar_t',
                     'and', 'and_eq', 'bitand', 'bitor', 'compl', 'not',
                     'not_eq', 'or', 'or_eq', 'xor', 'xor_eq'])
    polluted_words = set(['unix', 'errno', 'mips', 'sparc', 'i386', 'linux'])
    name = re.sub(r'[^A-Za-z0-9_]', '_', name)
    if protect and (name in (c89_words | c99_words | c11_words | gcc_words
                             | cpp_words | polluted_words)
                    or name[0].isdigit()):
        return 'q_' + name
    return name


class Indentation:
    """
    Indentation level management.

    :param initial: Initial number of spaces, default 0.
    """
    def __init__(self, initial: int = 0) -> None:
        self._level = initial

    def __repr__(self) -> str:
        return "{}({:d})".format(type(self).__name__, self._level)

    def __str__(self) -> str:
        """Return the current indentation as a string of spaces."""
        return ' ' * self._level

    def increase(self, amount: int = 4) -> None:
        """Increase the indentation level by ``amount``, default 4."""
        self._level += amount

    def decrease(self, amount: int = 4) -> None:
        """Decrease the indentation level by ``amount``, default 4."""
        assert amount <= self._level
        self._level -= amount


indent = Indentation()


def cgen(code: str, **kwds: object) -> str:
    """
    Generate ``code`` with ``kwds`` interpolated.

    Obey `indent`, and strip `EATSPACE`.
    """
    raw = code % kwds
    pfx = str(indent)
    if pfx:
        raw = re.sub(r'^(?!(#|$))', pfx, raw, flags=re.MULTILINE)
    return re.sub(re.escape(EATSPACE) + r' *', '', raw)


def mcgen(code: str, **kwds: object) -> str:
    if code[0] == '\n':
        code = code[1:]
    return cgen(code, **kwds)


def c_fname(filename: str) -> str:
    return re.sub(r'[^A-Za-z0-9_]', '_', filename)


def guardstart(name: str) -> str:
    return mcgen('''
#ifndef %(name)s
#define %(name)s

''',
                 name=c_fname(name).upper())


def guardend(name: str) -> str:
    return mcgen('''

#endif /* %(name)s */
''',
                 name=c_fname(name).upper())


def gen_ifcond(ifcond: Optional[Union[str, Dict[str, Any]]],
               cond_fmt: str, not_fmt: str,
               all_operator: str, any_operator: str) -> str:

    def do_gen(ifcond: Union[str, Dict[str, Any]],
               need_parens: bool) -> str:
        if isinstance(ifcond, str):
            return cond_fmt % ifcond
        assert isinstance(ifcond, dict) and len(ifcond) == 1
        if 'not' in ifcond:
            return not_fmt % do_gen(ifcond['not'], True)
        if 'all' in ifcond:
            gen = gen_infix(all_operator, ifcond['all'])
        else:
            gen = gen_infix(any_operator, ifcond['any'])
        if need_parens:
            gen = '(' + gen + ')'
        return gen

    def gen_infix(operator: str, operands: Sequence[Any]) -> str:
        return operator.join([do_gen(o, True) for o in operands])

    if not ifcond:
        return ''
    return do_gen(ifcond, False)


def cgen_ifcond(ifcond: Optional[Union[str, Dict[str, Any]]]) -> str:
    return gen_ifcond(ifcond, 'defined(%s)', '!%s', ' && ', ' || ')


def docgen_ifcond(ifcond: Optional[Union[str, Dict[str, Any]]]) -> str:
    return gen_ifcond(ifcond, '%s', 'not %s', ' and ', ' or ')


def gen_if(cond: str) -> str:
    if not cond:
        return ''
    return mcgen('''
#if %(cond)s
''', cond=cond)


def gen_endif(cond: str) -> str:
    if not cond:
        return ''
    return mcgen('''
#endif /* %(cond)s */
''', cond=cond)


def must_match(pattern: str, string: str) -> Match[str]:
    match = re.match(pattern, string)
    assert match is not None
    return match
