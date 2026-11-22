__lwver__ = "2025"
import math
import lwsdk

class Bob(lwsdk.IItemMotion):
    def __init__(self, context):
        super(Bob, self).__init__()
        self.item = context          # the item this is applied to
        self.height = 0.25           # metres
        self.period = 2.0            # seconds

    def evaluate(self, ma):
        pos = list(ma.getParam(lwsdk.LWIP_POSITION))
        pos[1] += self.height * math.sin(2.0 * math.pi * ma.time / self.period)
        ma.setParam(lwsdk.LWIP_POSITION, pos)

    def inst_descln(self):
        return "Bob (%.2f m, %.1f s)" % (self.height, self.period)

    def inst_copy(self, source):
        self.height, self.period = source.height, source.period

    def inst_save(self, state):
        state.lwsave_fp(self.height); state.lwsave_fp(self.period)

    def inst_load(self, state):
        ok, self.height = state.lwload_fp(1)
        ok, self.period = state.lwload_fp(1)

ServerTagInfo = [("My Bob", lwsdk.SRVTAG_USERNAME | lwsdk.LANGID_USENGLISH)]
ServerRecord = { lwsdk.ItemMotionFactory("MY_Bob", Bob): ServerTagInfo }
