import re, operator, array
from collections import namedtuple

class Argument(object):
    def __init__(self, viewtype, begin, end=None):
        self.type = viewtype
        self.begin = int(begin)
        self.end = None
        if end is not None:
            self.end = int(end)

class Lram(bytearray):
    pass

class Pram(Lram):
    def __init__(self):
        self.instruction = re.compile(r'\[(?P<out>[^\]]+)\](?P<op>\w+)\((?P<in>[^\)]+)\)')
        self.operation = dict(add=operator.add, mul=operator.mul, mod=operator.mod, sub=operator.sub, div=operator.truediv)
        self.type = dict(i8='b', u8='B', i16='h', u16='H', i32='l', u32='L', f32='f')

    def _parse_arguments(self, op, lram):
        arguments = [Argument(*x.split(':')) for x in op.split(',')]
        return [memoryview(lram)[x.begin:x.end].cast(self.type[x.type]) for x in arguments]

    def _vectorize(self, op, output, inputs):
        for x in zip(range(len(output)), *inputs):
            output[x[0]] = op(*x[1:])

    def run(self, lram):
        operations = self.instruction.findall(str(self).replace(' ', ''))
        for op in operations:
            outputs = self._parse_arguments(op[0], lram)
            inputs = self._parse_arguments(op[2], lram)
            self._vectorize(self.operation[op[1]], outputs[0], inputs)

class Unit(object):
    def __init__(self):
        self.lram = Lram()
        self.pram = Pram()

class Ctrl(list):
    def __init__(self, units):
        self.units = units

    def wait(self):
        if len(self) == 0:
            return []

        number = self.pop(0);
        unit = self.units[number]
        unit.pram.run(unit.lram)

        return [number]

class Device(object):
    def __init__(self, units):
        self.units = [Unit() for _ in range(units)];
        self.ctrl = Ctrl(self.units)
