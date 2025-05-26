import pytest
from sqlalchemy import create_engine, text
from sqlalchemy.exc import ProgrammingError
from itree.type import ITree

DATABASE_URL = "postgresql+psycopg://api:secret@localhost:5432/app"

engine = create_engine(DATABASE_URL, echo=True, future=True)

def test_itree_db_support() -> bool:
    """Check if the database supports itree."""
    try:
        with engine.connect() as connection:
            result = connection.execute(text("SELECT '1.2.3.4'::itree;"))
            value = result.scalar()
            assert value == '1.2.3.4'
    except ProgrammingError:
        pytest.fail("itree type is not supported in the database")

def visualize_itree(itree: "ITree"):
    value = itree.value
    control = int.from_bytes(value[:2], 'big')
    data = value[2:]

    print(f"Path: {itree.path}")
    print(f"Binary (hex): {value.hex()}")
    print("Logical segments:", [int(seg) for seg in itree.path.split('.')])
    print("Value:")
    for n in range(16):
        print(f" Pos:  {n}: control bit  {(control >> (15 - n)) & 1} Byte: {data[n]:08b} (0x{data[n]:02x})")

def test_itree() -> None:
    """Test the ITree class."""
    # Test valid ITree creation
    itree = ITree('1.2.3')
    assert itree.path == '1.2.3'


    # Test invalid ITree creation
    with pytest.raises(ValueError):
        ITree('..') # Invalid path with empty segments
        ITree('1.2..3')
        ITree('1.2.3.')
        ITree('1.2.3.4.5.6.7.8.9.10.11.12.13.14.15.16.17')  # More than 16 segments

    # Test ITree equality
    assert ITree('1.2.3') == ITree('1.2.3')
    assert ITree('1.2.3') != ITree('1.2.4')
    assert ITree('1.2.3') == '1.2.3'

    # Test ITree length
    assert len(ITree('1.2')) == 2

    # Test subpath index
    assert ITree('1.2.3').index('2.3') == 1
    # Test getting a segment by index
    assert ITree('1.2.3.4.5.6')[2] == ITree('3')
    #print(f" ITree('1.2.3')[2:4] = {ITree('1.2.3')[2:4]}")
    assert ITree('1.2.3.4.5.6')[2:4] == ITree('3.4')

    # Test __contains__ method
    assert 2 in ITree('1.2.3')
    assert '7' not in ITree('1.2.3')

    #Test comparison operators
    assert ITree('1.2.3') < ITree('1.2.4')
    assert ITree('1.2.3') <= ITree('1.2.3')
    assert ITree('1.2.3') <= ITree('1.2.4')
    assert ITree('1.2.3') > ITree('1.2.2')
    assert ITree('1.2.3') >= ITree('1.2.3')
    assert ITree('1.2.3') >= ITree('1.2.2')
    assert ITree('1.2.3') != ITree('1.2.4')
    assert ITree('1.2.3') == ITree('1.2.3')
    assert ITree('1.2.3') != ITree('1.2.4')
    assert ITree('1.2.3') == '1.2.3'
    assert ITree('1.2.3') != '1.2.4'


    #Addition
    assert ITree('1.2.3') + ITree('4.5') == ITree('1.2.3.4.5')
    assert ITree('1.2.3') + '4.5' == ITree('1.2.3.4.5')

    #ancestry
    assert ITree('1.2.3').descendant_of('1.2')
    assert not ITree('1.2.3').descendant_of('1.3')
    assert ITree('1.2.3').ancestor_of('1.2.3.4')
    assert not ITree('1.2.3').ancestor_of('1.2.4')

def test_itree_binary():
    itree = ITree('1.2.300.4.500')
    value = itree.value
    #print("Binary value (hex):", value.hex())
    assert value == bytes.fromhex("edff0102012c0401f4000000000000000000")

    visualize_itree(itree)

    itree2 = ITree.from_bytes(value)
    assert itree2 == itree, "ITree from bytes should match original ITree"
