
import copy
from typing import List, Optional, TypeVar


class QAPISchemaPragma:

    def __init__(self) -> None:
        self.doc_required = False
        self.command_name_exceptions: List[str] = []
        self.command_returns_exceptions: List[str] = []
        self.documentation_exceptions: List[str] = []
        self.member_name_exceptions: List[str] = []


class QAPISourceInfo:
    T = TypeVar('T', bound='QAPISourceInfo')

    def __init__(self, fname: str, parent: Optional['QAPISourceInfo']):
        self.fname = fname
        self.line = 1
        self.parent = parent
        self.pragma: QAPISchemaPragma = (
            parent.pragma if parent else QAPISchemaPragma()
        )
        self.defn_meta: Optional[str] = None
        self.defn_name: Optional[str] = None

    def set_defn(self, meta: str, name: str) -> None:
        self.defn_meta = meta
        self.defn_name = name

    def next_line(self: T, n: int = 1) -> T:
        info = copy.copy(self)
        info.line += n
        return info

    def loc(self) -> str:
        return f"{self.fname}:{self.line}"

    def in_defn(self) -> str:
        if self.defn_name:
            return "%s: In %s '%s':\n" % (self.fname,
                                          self.defn_meta, self.defn_name)
        return ''

    def include_path(self) -> str:
        ret = ''
        parent = self.parent
        while parent:
            ret = 'In file included from %s:\n' % parent.loc() + ret
            parent = parent.parent
        return ret

    def __str__(self) -> str:
        return self.include_path() + self.in_defn() + self.loc()
