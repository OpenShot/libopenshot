import openshot
import gtk

# Create Reader
r = openshot.FFmpegReader("/home/jonathan/Videos/sintel-1024-stereo.mp4")

# Create Player
p = openshot.Player()

# Set the Reader
p.SetReader(r)

# Create callback method
def FrameDone(frame, width, height, pixels):
	print "------------"
	print "frame ready for display: %d" % frame
	
	# Create pixbuf
	#pixbuf = gtk.gdk.Pixbuf(gtk.gdk.COLORSPACE_RGB, False, 8, width, height)
	#pixel_array = pixbuf.get_pixels_array()
	
	# Loop through pixel array of pixbuf, and set values
	#for row in range(height):
	#	for col in range(width):
	#		for color in range(3):
	#			pixel_array[row][col][color] = pixels[row][col][color]
	
	# Save pixbuf for debugging
	#pixbuf.save("test.png", "png")
	print "pixbuf created for frame: %d" % frame

# Hook up python callback
p.set_pymethod(FrameDone)