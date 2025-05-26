"""Python/SQLAlchemy support for Postgres itree datatype"""
from sqlalchemy.dialects.postgresql import ARRAY
from sqlalchemy.dialects.postgresql.base import PGTypeCompiler, ischema_names
from sqlalchemy.sql import expression
from sqlalchemy.types import Concatenable, UserDefinedType


class ITree:
    """Python representation of the Postgres itree 18 byte value with logical int segments and a string representation."""
    ITREE_MAX_LEVELS = 16
    def __init__(self, path_or_itree):
        if isinstance(path_or_itree, str):
            self.validate(path_or_itree)
            self.path = path_or_itree.strip()
        elif isinstance(path_or_itree, ITree):
            self.path = path_or_itree.path
        else:
            raise TypeError(f"itree must be initialized with a string or another ITree instance not {type(path_or_itree).__name__}")

    @classmethod
    def validate(cls, path: str):
        if not path:
            raise ValueError("itree path cannot be empty")
        if not isinstance(path, str):
            raise ValueError("itree path must be a string")

        segments_str = path.strip().split('.')
        if len(segments_str) > cls.ITREE_MAX_LEVELS:
            raise ValueError("itree must have max 16 integer segments")

        total_bytes = 0
        for seg_str in segments_str:
            if not seg_str:
                raise ValueError("itree path contains empty segment(s)")
            try:
                seg = int(seg_str)
            except ValueError:
                raise ValueError(f"itree segment '{seg_str}' is not a valid integer")
            if not (0 < seg < 65536):
                raise ValueError(f"itree segment '{seg}' must be in range 1..65535")
            total_bytes += 2 if seg > 255 else 1
            if total_bytes > 16:
                raise ValueError("itree must have a total length of 16 bytes or less")

    @property
    def value(self):
        """Get the binary representation of an itree instance."""
        control = 0xFFFF
        data = bytearray(16)
        data_offset = 0

        segments = [int(seg) for seg in self.path.split('.')]
        for seg in segments:
            seg_bytes = seg.to_bytes(2 if seg > 255 else 1, 'big')
            seg_len = len(seg_bytes)

            data[data_offset:data_offset + seg_len] = seg_bytes
            if seg_len == 2:
                # Mark the second byte as a continuation (bit=0)
                control &= ~(1 << (15 - (data_offset + 1)))
            data_offset += seg_len

        # Compose the final 18-byte value
        return control.to_bytes(2, 'big') + data

    @classmethod
    def from_bytes(cls, value: bytes):
        """Create an ITree instance from a binary value.
            Args:
                value (bytes): The 18-byte binary value from Postgres.
                1.Control bits are in the first 2 bytes.
                2.Data segments follow, where each segment can be 1 or 2 bytes.
        """
        if not isinstance(value, bytes) or len(value) != 18:
            raise ValueError("itree must be 18 bytes")

        control = int.from_bytes(value[:2], 'big')
        data = value[2:]

        def get_control_bit(control, n):
            return (control >> (15 - n)) & 1

        byte_pos = 0
        seg_count = 0

        segments = []
        while byte_pos < len(data):
            if seg_count >= cls.ITREE_MAX_LEVELS:
                raise ValueError("itree cannot have more than 16 segments")

            if get_control_bit(control, byte_pos) == 1:
                # Check if the next byte is a continuation, read 2 bytes
                if byte_pos +1 < cls.ITREE_MAX_LEVELS and get_control_bit(control, byte_pos + 1) == 0:
                    seg = int.from_bytes(data[byte_pos:byte_pos + 2], 'big')
                    byte_pos += 2
                else: # Single byte segment
                    seg = data[byte_pos]
                    byte_pos += 1
            else:
                raise ValueError(f"Invalid control bit at position {byte_pos}")

            if seg == 0:
                continue

            segments.append(seg)
            seg_count += 1

        path = '.'.join(str(seg) for seg in segments)
        itree = cls(path)
        return itree


    def __str__(self):
        return self.path

    def __repr__(self):
        return f'{self.__class__.__name__}({self.path!r})'

    def __len__(self):
        return len(self.path.split('.'))

    def index(self, other):
        subpath = ITree(other).path.split('.')
        parts = self.path.split('.')
        for index, _ in enumerate(parts):
            if parts[index:len(subpath) + index] == subpath:
                return index
        raise ValueError('subpath not found')

    def descendant_of(self, other):
        """
        is left argument a descendant of right (or equal)?

        ::

            assert ITree('1.2.3.4.5').descendant_of('1.2.3')
        """
        subpath = self[:len(ITree(other))]
        return subpath == other

    def ancestor_of(self, other):
        """
        is left argument an ancestor of right (or equal)?

        ::

            assert ITree('1.2.3').ancestor_of('1.2.3.4.5')
        """
        subpath = ITree(other)[:len(self)]
        return subpath == self

    def __getitem__(self, key):
        if isinstance(key, int):
            return ITree(self.path.split('.')[key])
        elif isinstance(key, slice):
            return ITree('.'.join(self.path.split('.')[key]))
        raise TypeError(f'ITree indices must be integers, not {key.__class__.__name__}')

    def __add__(self, other):
        return ITree(self.path + '.' + ITree(other).path)

    def __radd__(self, other):
        return ITree(other) + self

    def __eq__(self, other):
        if isinstance(other, ITree):
            return self.path == other.path
        elif isinstance(other, str):
            return self.path == other
        else:
            return NotImplemented

    def __ne__(self, other):
        return not (self == other)

    def __hash__(self):
        return hash(self.path)

    def __contains__(self, label):
        if isinstance(label,int):
            label = str(label)

        return label in self.path.split('.')

    def __gt__(self, other):
        return [int(s) for s in self.path.split('.')] > [int(s) for s in other.path.split('.')]

    def __lt__(self, other):
        return [int(s) for s in self.path.split('.')] < [int(s) for s in other.path.split('.')]

    def __ge__(self, other):
        return [int(s) for s in self.path.split('.')] >= [int(s) for s in other.path.split('.')]

    def __le__(self, other):
        return [int(s) for s in self.path.split('.')] <= [int(s) for s in other.path.split('.')]

    def __iter__(self):
        return iter(int(self.path.split('.')))

class ITreeType(Concatenable, UserDefinedType):
    cache_ok = True

    class comparator_factory(Concatenable.Comparator):
        def ancestor_of(self, other):
            if isinstance(other, list):
                return self.op('@>')(expression.cast(other, ARRAY(ITreeType)))
            else:
                return self.op('@>')(other)

        def descendant_of(self, other):
            if isinstance(other, list):
                return self.op('<@')(expression.cast(other, ARRAY(ITreeType)))
            else:
                return self.op('<@')(other)

    def bind_processor(self, dialect):
        def process(value):
            if value:
                return value.path
        return process

    def result_processor(self, dialect, coltype):
        def process(value):
            return self._coerce(value)
        return process

    def literal_processor(self, dialect):
        def process(value):
            value = value.replace("'", "''")
            return f"'{value}'"
        return process

    __visit_name__ = 'ITREE'

    def _coerce(self, value):
        if value:
            return ITree(value)

def visit_ITREE(self, type_, **kw):
    return 'ITREE'

ischema_names['itree'] = ITreeType
PGTypeCompiler.visit_ITREE = visit_ITREE