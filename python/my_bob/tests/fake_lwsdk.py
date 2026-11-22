"""A stand-in for LightWave's lwsdk module, for tests run outside LightWave."""
import sys
import types

lwsdk = types.ModuleType("lwsdk")

# Base classes plug-ins derive from: accept anything, do nothing.
class _Base(object):
    def __init__(self, *args, **kwargs):
        pass

for _name in ("IItemMotion", "IGeneric", "IMaster", "IChannel",
              "IMeshDeformer", "IImageFilter", "ICustomObj"):
    setattr(lwsdk, _name, type(_name, (_Base,), {}))

# Factories only need to be hashable dictionary keys.
def _factory(name, cls):
    return (name, cls)

for _name in ("ItemMotionFactory", "GenericFactory", "MasterFactory",
              "ChannelFactory", "MeshDeformerFactory", "ImageFilterFactory"):
    setattr(lwsdk, _name, _factory)

# Constants: any value works, as long as each name exists.
lwsdk.LWIP_POSITION = 1
lwsdk.SRVTAG_USERNAME = 0
lwsdk.LANGID_USENGLISH = 0x0409

sys.modules["lwsdk"] = lwsdk


class FakeMotionAccess(object):
    """Records what a motion handler reads and writes."""
    def __init__(self, time, position):
        self.time = time
        self.position = list(position)

    def getParam(self, param):
        assert param == lwsdk.LWIP_POSITION
        return tuple(self.position)

    def setParam(self, param, value):
        assert param == lwsdk.LWIP_POSITION
        self.position = list(value)


class FakeState(object):
    """In-memory save/load state with the lwsave_fp / lwload_fp calls."""
    def __init__(self):
        self.values = []

    def lwsave_fp(self, value):
        self.values.append(float(value))

    def lwload_fp(self, count):
        value = self.values.pop(0)
        return 1, value
