from typing import Any, Dict, List, Optional, Iterator

from pydottalk import Dbf  # alias to xbase.DbArea


class Table:
    def __init__(self, path: str):
        self._path = path
        self._a = Dbf()
        self._open = False

    # ---- lifecycle ----

    def open(self) -> None:
        self._a.open(self._path)
        self._open = True

    def close(self) -> None:
        if self._open:
            self._a.close()
            self._open = False

    def is_open(self) -> bool:
        return self._open and self._a.isOpen()

    # ---- metadata ----

    def field_names(self) -> List[str]:
        return [f["name"] for f in self._a.fields()]

    def fields(self) -> List[Dict[str, Any]]:
        return list(self._a.fields())

    def field_count(self) -> int:
        return self._a.fieldCount()

    def rec_count(self) -> int:
        return self._a.recCount()

    def recno(self) -> int:
        return self._a.recno()

    # ---- navigation ----

    def top(self) -> None:
        self._a.top()

    def bottom(self) -> None:
        self._a.bottom()

    def goto(self, recno: int) -> None:
        self._a.gotoRec(recno)

    def skip(self, n: int = 1) -> None:
        self._a.skip(n)

    def bof(self) -> bool:
        return self._a.bof()

    def eof(self) -> bool:
        return self._a.eof()

    # ---- record access ----

    def read(self) -> Dict[str, Any]:
        """
        Returns current record as a dict keyed by field name.
        """
        raw = self._a.readCurrent()  # assumed list-like
        names = self.field_names()

        if isinstance(raw, dict):
            return raw  # future-proof if binding changes

        # map list/tuple to dict
        out = {}
        for i, name in enumerate(names):
            if i < len(raw):
                out[name] = raw[i]
            else:
                out[name] = None
        return out

    def get(self, field_name: str) -> Any:
        idx = self._field_index(field_name)
        return self._a.get(idx)

    def set(self, field_name: str, value: Any) -> None:
        idx = self._field_index(field_name)
        self._a.set(idx, value)

    def write(self) -> None:
        self._a.writeCurrent()

    def update(self, values: Dict[str, Any]) -> None:
        for k, v in values.items():
            self.set(k, v)
        self.write()

    # ---- mutation ----

    def append_blank(self) -> int:
        self._a.appendBlank()
        return self._a.recno()

    def delete(self) -> None:
        self._a.deleteCurrent()

    def is_deleted(self) -> bool:
        return self._a.isDeleted()

    # ---- iteration ----

    def records(self, limit: Optional[int] = None) -> Iterator[Dict[str, Any]]:
        self.top()
        count = 0

        while not self.eof():
            yield self.read()
            count += 1
            if limit is not None and count >= limit:
                break
            self.skip(1)

    # ---- helpers ----

    def _field_index(self, name: str) -> int:
        names = self.field_names()
        for i, n in enumerate(names):
            if n.upper() == name.upper():
                return i + 1  # xbase is 1-based
        raise KeyError(f"Field not found: {name}")