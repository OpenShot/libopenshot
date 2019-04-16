import openshot

# Create an empty timeline
t = openshot.Timeline(720, 480, openshot.Fraction(24,1), 44100, 2, openshot.LAYOUT_STEREO)
t.Open()

# lower layer
lower = openshot.QtImageReader("back.png")
c1 = openshot.Clip(lower)
c1.Layer(1)
t.AddClip(c1)

# higher layer
higher = openshot.QtImageReader("front3.png")
c2 = openshot.Clip(higher)
c2.Layer(2)
#c2.alpha = openshot.Keyframe(0.5)
t.AddClip(c2)

# Wipe / Transition
brightness = openshot.Keyframe()
brightness.AddPoint(1, 1.0, openshot.BEZIER)
brightness.AddPoint(24, -1.0, openshot.BEZIER)

contrast = openshot.Keyframe()
contrast.AddPoint(1, 20.0, openshot.BEZIER)
contrast.AddPoint(24, 20.0, openshot.BEZIER)

reader = openshot.QtImageReader("mask.png")
e = openshot.Mask(reader, brightness, contrast)
e.Layer(2)
e.End(60)
t.AddEffect(e)

reader1 = openshot.QtImageReader("mask2.png")
e1 = openshot.Mask(reader1, brightness, contrast)
e1.Layer(2)
e1.Order(2)
e1.End(60)
#t.AddEffect(e1)

for n in range(1,25):
 print(n, end=" ", flush=1)
 t.GetFrame(n).Save("%s.png" % n, 1.0)
