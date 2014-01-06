import openshot

# Create a empty clip
t = openshot.Timeline(720, 480, openshot.Fraction(24,1), 44100, 2)

# lower layer
lower = openshot.ImageReader("/home/jonathan/apps/libopenshot/src/examples/back.png")
c1 = openshot.Clip(lower)
c1.Layer(1)
t.AddClip(c1)

# higher layer
higher = openshot.ImageReader("/home/jonathan/apps/libopenshot/src/examples/front3.png")
c2 = openshot.Clip(higher)
c2.Layer(2)
#c2.alpha = openshot.Keyframe(0.5)
t.AddClip(c2)

# Wipe / Transition
brightness = openshot.Keyframe()
brightness.AddPoint(1, 100.0, openshot.BEZIER)
brightness.AddPoint(24, -100.0, openshot.BEZIER)

contrast = openshot.Keyframe()
contrast.AddPoint(1, 20.0, openshot.BEZIER)
contrast.AddPoint(24, 20.0, openshot.BEZIER)

e = openshot.Wipe("/home/jonathan/apps/libopenshot/src/examples/mask.png", brightness, contrast)
e.Layer(2)
e.End(60)
t.AddEffect(e)

e1 = openshot.Wipe("/home/jonathan/apps/libopenshot/src/examples/mask2.png", brightness, contrast)
e1.Layer(2)
e1.Order(2)
e1.End(60)
#t.AddEffect(e1)


for n in range(1,25):
 print n
 t.GetFrame(n).Save("%s.png" % n, 1.0)
